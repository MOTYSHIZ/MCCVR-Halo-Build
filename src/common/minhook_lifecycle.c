// Compile the pinned upstream implementation unchanged in this translation
// unit so the narrow retirement extension can use its lock and hook records.
#include "hook.c"
#include "minhook_lifecycle.h"

MH_STATUS WINAPI MCCVR_DisableHookForRetirement(LPVOID target)
{
    MH_STATUS status = MH_ERROR_NOT_INITIALIZED;
    EnterSpinLock();
    if (g_hHeap != NULL && target != MH_ALL_HOOKS)
    {
        UINT pos = FindHookEntry(target);
        status = MH_ERROR_NOT_CREATED;
        if (pos != INVALID_HOOK_POS)
        {
            PHOOK_ENTRY hook = &g_hooks.pItems[pos];
            LPBYTE patch = (LPBYTE)target;
            SIZE_T size = sizeof(JMP_REL);
            MEMORY_BASIC_INFORMATION memory;
            if (hook->patchAbove)
            {
                patch -= sizeof(JMP_REL);
                size += sizeof(JMP_REL_SHORT);
            }
            status = MH_ERROR_MEMORY_PROTECT;
            // Do not make an unreadable mapping writable just to restore
            // bytes from a previous module instance.
            if (VirtualQuery(patch, &memory, sizeof(memory)) == sizeof(memory))
            {
                BOOL absent = memory.State != MEM_COMMIT;
                BOOL restored = FALSE;
                BOOL owned = FALSE;
                if (!absent && !(memory.Protect & (PAGE_NOACCESS | PAGE_GUARD)) &&
                    (SIZE_T)(patch - (LPBYTE)memory.BaseAddress) + size <= memory.RegionSize)
                {
                    __try
                    {
                        JMP_REL jump;
                        jump.opcode = 0xE9;
                        jump.operand = (UINT32)((LPBYTE)hook->pDetour -
                            (patch + sizeof(JMP_REL)));
                        restored = memcmp(patch, hook->backup, size) == 0;
                        owned = memcmp(patch, &jump, sizeof(jump)) == 0 &&
                            (!hook->patchAbove ||
                             (((LPBYTE)target)[0] == 0xEB &&
                              ((LPBYTE)target)[1] == (UINT8)-7));
                    }
                    __except (EXCEPTION_EXECUTE_HANDLER) { }
                }
                if (absent || restored)
                {
                    hook->isEnabled = FALSE;
                    hook->queueEnable = FALSE;
                    status = MH_ERROR_DISABLED;
                }
                else if (owned && hook->isEnabled)
                {
                    FROZEN_THREADS threads;
                    status = Freeze(&threads, pos, ACTION_DISABLE);
                    if (status == MH_OK)
                    {
                        // The loader may remove a mapping after VirtualQuery.
                        __try { status = EnableHookLL(pos, FALSE); }
                        __except (EXCEPTION_EXECUTE_HANDLER)
                        { status = MH_ERROR_MEMORY_PROTECT; }
                        Unfreeze(&threads);
                    }
                }
                // Foreign bytes are never overwritten, even if upstream's
                // bookkeeping says this entry is already disabled.
            }
        }
    }
    LeaveSpinLock();
    return status;
}

UINT WINAPI MCCVR_CopyEnabledHookPatches(LPCVOID base, SIZE_T size, MCCVR_HOOK_PATCH* out, UINT capacity)
{
    UINT count = 0;
    const ULONG_PTR lo = (ULONG_PTR)base;
    const ULONG_PTR hi = lo + size;
    if (!size || hi < lo)
        return 0;
    EnterSpinLock();
    if (g_hHeap != NULL)
    {
        UINT i;
        for (i = 0; i < g_hooks.size; ++i)
        {
            const PHOOK_ENTRY hook = &g_hooks.pItems[i];
            ULONG_PTR patch = (ULONG_PTR)hook->pTarget;
            SIZE_T length = sizeof(JMP_REL);
            if (!hook->isEnabled)
                continue;
            if (hook->patchAbove)
            {
                patch -= sizeof(JMP_REL);
                length += sizeof(JMP_REL_SHORT);
            }
            if (patch >= hi || patch + length <= lo)
                continue;
            if (out && count < capacity)
            {
                out[count].address = (const unsigned char*)patch;
                out[count].size = (unsigned char)length;
                memcpy(out[count].original, hook->backup, sizeof(out[count].original));
            }
            ++count;
        }
    }
    LeaveSpinLock();
    return count;
}

// ENABLES DEFERRED TO ONE FREEZE (minhook_lifecycle.h). The calling thread's own state: only the thread that opened
// the deferral defers, every other thread's MCCVR_EnableHook is MH_EnableHook as before.
#define MCCVR_DEFERRAL_CAPACITY 64
static __declspec(thread) BOOL t_mccvrDeferring = FALSE;
static __declspec(thread) UINT t_mccvrDeferredCount = 0;
static __declspec(thread) UINT t_mccvrImmediateCount = 0;
static __declspec(thread) LPVOID t_mccvrDeferred[MCCVR_DEFERRAL_CAPACITY];

// The bytes a hook's jump rewrites: five at the target, or five above it and two at it.
static VOID MccvrPatchRange(const HOOK_ENTRY* hook, ULONG_PTR* lo, ULONG_PTR* hi)
{
    *lo = (ULONG_PTR)hook->pTarget;
    *hi = *lo + sizeof(JMP_REL);
    if (hook->patchAbove)
    {
        *lo -= sizeof(JMP_REL);
        *hi = (ULONG_PTR)hook->pTarget + sizeof(JMP_REL_SHORT);
    }
}

// Under the lock. Whether the hook at `pos` rewrites a byte that another hook already rewrites, or is queued to.
static BOOL MccvrPatchOverlapsAnother(UINT pos)
{
    ULONG_PTR lo, hi;
    UINT i;
    MccvrPatchRange(&g_hooks.pItems[pos], &lo, &hi);
    for (i = 0; i < g_hooks.size; ++i)
    {
        ULONG_PTR otherLo, otherHi;
        const HOOK_ENTRY* other = &g_hooks.pItems[i];
        if (i == pos || (!other->isEnabled && !other->queueEnable))
            continue;
        MccvrPatchRange(other, &otherLo, &otherHi);
        if (otherLo < hi && lo < otherHi)
            return TRUE;
    }
    return FALSE;
}

BOOL WINAPI MCCVR_BeginHookEnableDeferral(VOID)
{
    if (t_mccvrDeferring)
        return FALSE;
    t_mccvrDeferring = TRUE;
    t_mccvrDeferredCount = 0;
    t_mccvrImmediateCount = 0;
    return TRUE;
}

MH_STATUS WINAPI MCCVR_EnableHook(LPVOID target)
{
    if (t_mccvrDeferring && target != MH_ALL_HOOKS)
    {
        MH_STATUS status = MH_ERROR_NOT_INITIALIZED;
        BOOL answered = TRUE;
        EnterSpinLock();
        if (g_hHeap != NULL)
        {
            const UINT pos = FindHookEntry(target);
            if (pos == INVALID_HOOK_POS)
                status = MH_ERROR_NOT_CREATED;
            else if (g_hooks.pItems[pos].isEnabled)
                status = MH_ERROR_ENABLED;
            else if (t_mccvrDeferredCount < MCCVR_DEFERRAL_CAPACITY && !MccvrPatchOverlapsAnother(pos))
            {
                g_hooks.pItems[pos].queueEnable = TRUE;
                t_mccvrDeferred[t_mccvrDeferredCount++] = target;
                status = MH_OK;
            }
            else
                answered = FALSE;
        }
        LeaveSpinLock();
        if (answered)
            return status;
        // No room, or its bytes meet another hook's: enabled now, exactly as without a deferral.
        ++t_mccvrImmediateCount;
    }
    return MH_EnableHook(target);
}

UINT WINAPI MCCVR_EndHookEnableDeferral(BOOL applyFollows, UINT* immediate)
{
    UINT i;
    const UINT count = t_mccvrDeferredCount;
    if (immediate)
        *immediate = t_mccvrImmediateCount;
    // Closed first: the enables below are the ordinary ones.
    t_mccvrDeferring = FALSE;
    t_mccvrDeferredCount = 0;
    t_mccvrImmediateCount = 0;
    if (applyFollows)
        return count;
    for (i = 0; i < count; ++i)
    {
        const LPVOID target = t_mccvrDeferred[i];
        BOOL stillQueued = FALSE;
        EnterSpinLock();
        if (g_hHeap != NULL)
        {
            const UINT pos = FindHookEntry(target);
            if (pos != INVALID_HOOK_POS && !g_hooks.pItems[pos].isEnabled && g_hooks.pItems[pos].queueEnable)
            {
                // Taken off the queue so a later MH_ApplyQueued (another install's) does not enable it as a side
                // effect, then enabled on its own, as the feature asked.
                g_hooks.pItems[pos].queueEnable = FALSE;
                stillQueued = TRUE;
            }
        }
        LeaveSpinLock();
        if (stillQueued)
            (void)MH_EnableHook(target);
    }
    return count;
}
