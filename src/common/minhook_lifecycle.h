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
#ifdef __cplusplus
}
#endif
