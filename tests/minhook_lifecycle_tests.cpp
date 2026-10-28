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
__declspec(noinline) static int OtherDetour() { return 43; }

// MCCVR_EnableHook inside and outside a deferral (minhook_lifecycle.h), on two synthetic functions in `page`.
static void DeferralTests(unsigned char* page)
{
    using Function = int(*)();
    const unsigned char seven[]{0xB8, 7, 0, 0, 0, 0xC3};  // mov eax,7; ret
    const unsigned char nine[]{0xB8, 9, 0, 0, 0, 0xC3};   // mov eax,9; ret
    unsigned char* const a = page;
    unsigned char* const b = page + 0x200;
    const auto fa = reinterpret_cast<Function>(a);
    const auto fb = reinterpret_cast<Function>(b);
    const auto reset = [&] {
        std::memset(page, 0x90, 4096);
        std::memcpy(a, seven, sizeof(seven));
        std::memcpy(b, nine, sizeof(nine));
    };
    const auto create = [&] {
        Function oa = nullptr, ob = nullptr;
        return MH_CreateHook(a, reinterpret_cast<void*>(&Detour), reinterpret_cast<void**>(&oa)) == MH_OK &&
               MH_CreateHook(b, reinterpret_cast<void*>(&OtherDetour), reinterpret_cast<void**>(&ob)) == MH_OK;
    };
    const auto remove = [&] {
        (void)MCCVR_DisableHookForRetirement(a);
        (void)MCCVR_DisableHookForRetirement(b);
        return MH_RemoveHook(a) == MH_OK && MH_RemoveHook(b) == MH_OK;
    };
    UINT immediate = 99;

    // Outside a deferral it is MH_EnableHook.
    reset();
    Check(create(), "deferral: create two hooks");
    Check(MCCVR_EnableHook(a) == MH_OK && fa() == 42, "no deferral: enabled at once");
    Check(MCCVR_EnableHook(a) == MH_ERROR_ENABLED, "no deferral: an enabled hook is refused as MinHook refuses it");
    Check(remove(), "deferral: remove");

    // Deferred, then the caller's MH_ApplyQueued takes them in one pass.
    reset();
    Check(create(), "deferral: create two hooks again");
    Check(MCCVR_BeginHookEnableDeferral() == TRUE, "deferral opens");
    Check(MCCVR_BeginHookEnableDeferral() == FALSE, "a second open on the same thread changes nothing");
    Check(MCCVR_EnableHook(a) == MH_OK && MCCVR_EnableHook(b) == MH_OK, "deferred enables report MH_OK");
    Check(fa() == 7 && fb() == 9 && a[0] == 0xB8 && b[0] == 0xB8, "deferred: the bytes are still the game's");
    Check(MCCVR_EnableHook(page + 0x400) == MH_ERROR_NOT_CREATED, "deferred: no hook there is refused");
    Check(MCCVR_EndHookEnableDeferral(TRUE, &immediate) == 2 && immediate == 0, "two deferred, none at once");
    Check(fa() == 7 && fb() == 9, "still not live before the apply");
    Check(MH_ApplyQueued() == MH_OK && fa() == 42 && fb() == 43, "one MH_ApplyQueued enables both");
    Check(MCCVR_EnableHook(a) == MH_ERROR_ENABLED, "closed: the ordinary path again");
    Check(remove() && fa() == 7 && fb() == 9, "retired: the game's own bytes");

    // Closed with nothing to apply: each is enabled on its own, and nothing stays queued.
    reset();
    Check(create(), "deferral: create for the no-apply close");
    Check(MCCVR_BeginHookEnableDeferral() == TRUE, "deferral opens again");
    Check(MCCVR_EnableHook(a) == MH_OK && MCCVR_EnableHook(b) == MH_OK, "deferred again");
    Check(MCCVR_EndHookEnableDeferral(FALSE, &immediate) == 2 && fa() == 42 && fb() == 43,
          "a close with no apply to follow enables each now");
    Check(MH_ApplyQueued() == MH_OK && fa() == 42 && fb() == 43, "and leaves nothing queued to flip");
    Check(remove(), "deferral: remove after the no-apply close");

    // A deferred hook a feature retires before the apply stays the game's.
    reset();
    Check(create(), "deferral: create for the retire-before-apply case");
    Check(MCCVR_BeginHookEnableDeferral() == TRUE, "deferral opens a third time");
    Check(MCCVR_EnableHook(a) == MH_OK && MCCVR_EnableHook(b) == MH_OK, "deferred a third time");
    Check(MCCVR_DisableHookForRetirement(a) == MH_ERROR_DISABLED, "a retired deferred hook was never live");
    Check(MCCVR_EndHookEnableDeferral(TRUE, &immediate) == 2, "the deferral counted both");
    Check(MH_ApplyQueued() == MH_OK && fa() == 7 && fb() == 43, "the apply skips the retired one");
    Check(remove(), "deferral: remove after the retire case");

    // Another thread's enable is never deferred by this thread's deferral.
    reset();
    Check(create(), "deferral: create for the other-thread case");
    Check(MCCVR_BeginHookEnableDeferral() == TRUE, "deferral opens on this thread");
    struct Arg { unsigned char* target; MH_STATUS status; } arg{b, MH_UNKNOWN};
    HANDLE other = CreateThread(nullptr, 0, [](LPVOID p) -> DWORD {
        auto* x = static_cast<Arg*>(p);
        x->status = MCCVR_EnableHook(x->target);
        return 0;
    }, &arg, 0, nullptr);
    Check(other && WaitForSingleObject(other, 5000) == WAIT_OBJECT_0, "the other thread ran");
    if (other)
        CloseHandle(other);
    Check(arg.status == MH_OK && fb() == 43, "another thread's enable is live at once");
    Check(MCCVR_EndHookEnableDeferral(TRUE, &immediate) == 0, "and was not this deferral's");
    Check(remove(), "deferral: remove after the other-thread case");
}

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
    DeferralTests(page);
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
