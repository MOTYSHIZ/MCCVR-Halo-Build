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
