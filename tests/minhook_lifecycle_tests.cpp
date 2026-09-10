#include <windows.h>
#include <cstdio>
#include <cstring>
#include "../src/common/minhook_lifecycle.h"
#include "../src/common/manual_vr_recovery.h"

static int failures = 0;
static void Check(bool condition, const char* description)
{
    if (!condition) { std::fprintf(stderr, "FAIL: %s\n", description); ++failures; }
}
__declspec(noinline) static int Detour() { return 42; }

int main()
{
    Check(MH_Initialize() == MH_OK, "initialize local test hooks");
    // Entirely synthetic executable memory in THIS test process. No MCC
    // process, module, config, installation or game data is accessed.
    auto* page = static_cast<unsigned char*>(VirtualAlloc(nullptr, 4096,
        MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE));
    if (!page) return 1;
    const unsigned char native[]{0xB8, 7, 0, 0, 0, 0xC3}; // mov eax,7; ret
    using Function = int(*)();
    const auto function = reinterpret_cast<Function>(page);
    for (int scenario = 0; scenario < 6; ++scenario)
    {
        std::memset(page, 0x90, 4096);
        std::memcpy(page, native, sizeof(native));
        Function original = nullptr;
        Check(MH_CreateHook(page, reinterpret_cast<void*>(&Detour),
            reinterpret_cast<void**>(&original)) == MH_OK, "create hook");
        Check(MH_EnableHook(page) == MH_OK && function() == 42, "live detour");
        if (scenario == 1) // MCC released the mapping before the worker polled
            Check(VirtualFree(page, 4096, MEM_DECOMMIT) != FALSE, "decommit target");
        if (scenario == 2) // Reload at the same address, original bytes restored
            std::memcpy(page, native, sizeof(native));
        if (scenario == 3) // A foreign replacement must never be overwritten
        {
            std::memset(page, 0xCC, sizeof(native));
            Check(MCCVR_DisableHookForRetirement(page) == MH_ERROR_MEMORY_PROTECT,
                "reject unknown replacement");
            Check(page[0] == 0xCC && page[5] == 0xCC, "foreign bytes unchanged");
            std::memcpy(page, native, sizeof(native));
        }
        if (scenario == 4) // Already disabled by a prior partial cleanup
            Check(MH_DisableHook(page) == MH_OK, "ordinary disable");
        if (scenario == 5) // Failed install before its queued hooks went live
        {
            Check(MH_DisableHook(page) == MH_OK, "prepare staged hook");
            Check(MH_QueueEnableHook(page) == MH_OK, "queue future activation");
        }
        const auto retired = MCCVR_DisableHookForRetirement(page);
        Check(retired == MH_OK || retired == MH_ERROR_DISABLED, "retire safely");
        if (scenario == 5)
            Check(MH_ApplyQueued() == MH_OK && function() == 7,
                "retirement cancels queued activation");
        if (scenario != 1)
        {
            Check(std::memcmp(page, native, sizeof(native)) == 0, "native bytes intact");
            Check(function() == 7 && original() == 7, "trampoline retained until removal");
        }
        // No other threads call these test functions, so they are quiescent.
        Check(MH_RemoveHook(page) == MH_OK, "retired record can be removed");
        Check(MCCVR_DisableHookForRetirement(page) == MH_ERROR_NOT_CREATED,
            "removed record is absent");
        if (scenario == 1)
            Check(VirtualAlloc(page, 4096, MEM_COMMIT, PAGE_EXECUTE_READWRITE) == page,
                "reuse original address");
    }
    VirtualFree(page, 0, MEM_RELEASE);
    Check(MH_Uninitialize() == MH_OK, "shutdown");
    wchar_t eventName[96]{};
    ManualVrRecoveryEventName(eventName, GetCurrentProcessId());
    HANDLE receiver = CreateEventW(nullptr, FALSE, FALSE, eventName);
    HANDLE sender = OpenEventW(EVENT_MODIFY_STATE, FALSE, eventName);
    Check(receiver && sender, "launcher and DLL agree on recovery endpoint");
    if (receiver && sender)
    {
        Check(SetEvent(sender) && SetEvent(sender), "coalesce repeated recovery clicks");
        Check(WaitForSingleObject(receiver, 0) == WAIT_OBJECT_0,
            "worker receives recovery request");
        Check(WaitForSingleObject(receiver, 0) == WAIT_TIMEOUT,
            "request is consumed once");
    }
    if (sender) CloseHandle(sender);
    if (receiver) CloseHandle(receiver);
    return failures ? 1 : 0;
}
