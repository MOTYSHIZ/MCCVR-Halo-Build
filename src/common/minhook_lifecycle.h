#pragma once
#include <MinHook.h>

#ifdef __cplusplus
extern "C" {
#endif
// Worker only. Disables an exact live MinHook patch normally, or marks an
// absent mapping / already restored entry disabled WITHOUT writing it.
// This never frees a trampoline. Callers must prove detour quiescence before
// MH_RemoveHook, including when this returns MH_ERROR_DISABLED.
MH_STATUS WINAPI MCCVR_DisableHookForRetirement(LPVOID target);

// One of this mod's ENABLED MinHook patches: the bytes it overwrote (`address`, `size` of them) and what stood
// there before (`original`). A normal hook rewrites five bytes at the target (E9 rel32); a hot-patch hook rewrites
// the five bytes above it and two at it.
typedef struct MCCVR_HOOK_PATCH
{
    const unsigned char* address;
    unsigned char size;
    unsigned char original[8];
} MCCVR_HOOK_PATCH;
// Any thread. Copies up to `capacity` enabled patches that intersect [base, base + size) and returns how many
// there are in all (a return above `capacity` means the copy is incomplete). This is how a byte-pattern proof
// reads the GAME's code where this mod has hooked it: a signature is a statement about the image as it shipped,
// and after MH_EnableHook the head of a hooked function reads our jump instead (sigscan.h, sig::Find).
UINT WINAPI MCCVR_CopyEnabledHookPatches(LPCVOID base, SIZE_T size, MCCVR_HOOK_PATCH* out, UINT capacity);

// ENABLES DEFERRED TO ONE FREEZE. MH_EnableHook suspends every other thread of the process, one at a time, reads
// each one's context and resumes them: one whole-process freeze per hook. A title install that enables its optional
// hooks one by one pays that for every hook, at the moment a level starts, when the game's threads are busiest and
// slowest to suspend, and the game stalls for the length of each freeze.
//
// While a deferral is open on the calling thread, MCCVR_EnableHook queues the hook instead (MH_QueueEnableHook's
// state) and returns what MH_EnableHook returns for a created, disabled hook: MH_OK. The next MH_ApplyQueued
// enables it in the same single freeze as everything else queued. Refused as MH_EnableHook refuses:
// MH_ERROR_NOT_CREATED, MH_ERROR_ENABLED. Enabled at once, exactly as MH_EnableHook, when no deferral is open on
// this thread (every other thread and caller), the target is MH_ALL_HOOKS, the deferral is full, or the hook's
// patch bytes meet another hook's that is enabled or queued (two overlapping writes then keep their old order).
//
// A hook queued here is not live until MH_ApplyQueued. Its trampoline is valid at once (MH_CreateHook made it),
// the image still holds the game's own bytes there, and MCCVR_DisableHookForRetirement and MH_RemoveHook both take
// it off the queue.
//
// Begin returns FALSE (and changes nothing) when a deferral is already open on this thread. End closes it:
// `applyFollows` TRUE = the caller calls MH_ApplyQueued next; FALSE = every hook this deferral queued that is still
// queued is taken off the queue and enabled now, one at a time (the old path). Returns how many hooks were
// deferred, and in `immediate` how many inside the deferral were enabled at once for one of the reasons above.
BOOL WINAPI MCCVR_BeginHookEnableDeferral(VOID);
MH_STATUS WINAPI MCCVR_EnableHook(LPVOID target);
UINT WINAPI MCCVR_EndHookEnableDeferral(BOOL applyFollows, UINT* immediate);
#ifdef __cplusplus
}
#endif
