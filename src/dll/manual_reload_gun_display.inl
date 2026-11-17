// MANUAL RELOAD ON THE GUN'S OWN COUNTER (Halo 3 / ODST, MCC): the assault rifle's and the battle rifle's digits show
// the player's count.
//
// Research 2026-11-16 (docs/MANUAL-RELOAD-PORT-2026-11-15.md): each counter part's shader (the AR's `ones`, `tens`
// and `display`, the BR's `ones` and `tens`, first and third person) picks its digit frame from the object function
// inputs `primary_ammunition_ones` / `primary_ammunition_tens` (string ids H3 0x214 / 0x215, ODST 0x219 / 0x21A).
// The weapon type's compute-function-value routine (H3 +0x36177C, ODST +0x3A78E8) works them out from the weapon's
// rounds loaded: ones = (n % 10) / 10, tens = ((n / 10) % 10) / 10. Both official kits were scanned whole: only those
// display shaders name these two inputs (ODST's cinematic AR props export them from a constant of their own), and
// no engine code uses their ids outside this routine. So changing the two digits for the gun the manual reload owns
// changes the picture only (AGENTS.md multiplayer rule). `primary_ammunition` itself (loaded / maximum) is NEVER
// changed: other guns' tags use it to switch their firing effects. Not installed in Project Reclaimer.
namespace manual_reload_gun_display
{
using ComputeFn = char(__fastcall*)(uint32_t object, int32_t name, uint32_t p3, float* out, bool* nonzero, uint64_t p6);
constexpr const char* kEntry =
    "48 8B C4 48 89 58 18 4C 89 48 20 55 56 57 41 54 41 55 41 56 41 57 48 83 EC 60 44 8B 15 ?? ?? ?? ?? 44 8B DA "
    "0F 29 70 B8 33 DB 0F 29 78 A8 4D 8B ?? 65 48 8B 04 25 58 00 00 00 45 8B E0 4C 8B 0D ?? ?? ?? ?? 41 BD ?? 00 00 00 "
    "8B E9";
// The routine's ammunition-name comparisons (H3 +0x361A5D, ODST +0x3A7BC9): four 0x2xx immediates.
constexpr const char* kAmmoIds =
    "B8 ?? 02 00 00 44 3B D8 0F 84 ?? ?? ?? ?? 41 81 FB ?? 02 00 00 0F 84 ?? ?? ?? ?? 41 81 FB ?? 02 00 00 74 ?? "
    "41 81 FB ?? 02 00 00 0F 84";
// mov eax,imm32 at +0x00, then cmp r11d,imm32 at +0x0E, +0x1B and +0x24 (their imm32 at +0x11, +0x1E, +0x27); read
// from both retail binaries (2026-11-17): 0x213, 0x214, 0x215, 0x216 (H3) and 0x218-0x21B (ODST), in that order.
constexpr size_t kIdAt[4]{0x01, 0x11, 0x1E, 0x27};

struct Lane
{
    const char* title;
    GameTitle game;
    int32_t primary;          // primary_ammunition's string id (ones = +1, tens = +2), from the string table
    void* target = nullptr;
    ComputeFn original = nullptr;
    std::atomic<bool> ready{false};
    std::atomic<uint32_t> callbacks{0};
    std::atomic<uint64_t> shown{0}, faults{0};
};
Lane g_halo3{"Halo 3", GameTitle::Halo3, 0x213};
Lane g_odst{"ODST", GameTitle::Halo3ODST, 0x218};

template <Lane& L>
__declspec(noinline) char __fastcall Detour(uint32_t object, int32_t name, uint32_t p3, float* out, bool* nonzero,
                                            uint64_t p6)
{
    L.callbacks.fetch_add(1, std::memory_order_acq_rel);
    char handled = 0;
    __try
    {
    handled = CallNativeWithSpareSlots(L.original, object, name, p3, out, nonzero, p6);
    // Every function query of every weapon passes here: the name is tested first. Not in a simulation another
    // machine shares (lockstep co-op): there the digits show the game's count, as a flat player's would.
    if (handled && (name == L.primary + 1 || name == L.primary + 2) && out && L.ready.load(std::memory_order_acquire) &&
        g_config.manual_reload && Sight_FireTimeChangesAllowed())
    {
        int loaded = 0, reserve = 0;
        if (VR_ManualReloadHudCount(L.game, object, loaded, reserve))
        {
            const int n = loaded < 0 ? 0 : loaded;
            float v = name == L.primary + 1 ? float(n % 10) / 10.0f : float((n / 10) % 10) / 10.0f;
            v = v < 0 ? 0 : v > 1 ? 1 : v;
            __try
            {
                *out = v;
                if (nonzero) *nonzero = v > 0;
                L.shown.fetch_add(1, std::memory_order_relaxed);
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                L.faults.fetch_add(1, std::memory_order_relaxed);
                L.ready.store(false, std::memory_order_release);
            }
        }
    }
    }
    __finally
    {
        L.callbacks.fetch_sub(1, std::memory_order_acq_rel);
    }
    return handled;
}

bool Install(Lane& lane, void* detour, uintptr_t base, size_t size, uint32_t generation)
{
    lane.ready.store(false, std::memory_order_release);
    if (lane.target)
    {
        LOG("MANUAL RELOAD GUN COUNTER %s: StockFallback: a previous hook has not retired", lane.title);
        return false;
    }
    if (lifted::Active())
    {
        LOG("MANUAL RELOAD GUN COUNTER %s: not installed in Project Reclaimer (the gun shows the game's count)",
            lane.title);
        return false;
    }
    const char* why = nullptr;
    const uintptr_t entry = mcc_clock::FindUnique(base, size, kEntry);
    const uintptr_t ids = mcc_clock::FindUnique(base, size, kAmmoIds);
    if (!entry)
        why = "the weapon's compute-function-value routine";
    else if (!ids || ids < entry || ids - entry > 0x1000)
        why = "its ammunition-name comparisons";
    else
    {
        // The names must be the ones this routine compares, in its order (primary, ones, tens, then the next): an
        // order this was not read from fails closed.
        for (int k = 0; k < 4 && !why; ++k)
        {
            int32_t id = 0;
            std::memcpy(&id, reinterpret_cast<const void*>(ids + kIdAt[k]), sizeof(id));
            if (id != lane.primary + k) why = "the ammunition names' ids in the routine";
        }
    }
    if (why)
    {
        LOG("MANUAL RELOAD GUN COUNTER %s: StockFallback: %s did not prove; the guns show the game's count", lane.title,
            why);
        return false;
    }
    DWORD64 image = 0;
    if (!RtlLookupFunctionEntry(reinterpret_cast<DWORD64>(detour), &image, nullptr))
    {
        LOG("MANUAL RELOAD GUN COUNTER %s: StockFallback: the detour has no unwind entry", lane.title);
        return false;
    }
    void* const target = reinterpret_cast<void*>(entry);
    if (MH_CreateHook(target, detour, reinterpret_cast<void**>(&lane.original)) != MH_OK)
    {
        lane.original = nullptr;
        LOG("MANUAL RELOAD GUN COUNTER %s: StockFallback: the hook was not created", lane.title);
        return false;
    }
    lane.target = target;
    lane.shown.store(0, std::memory_order_relaxed);
    lane.faults.store(0, std::memory_order_relaxed);
    if (MH_EnableHook(target) != MH_OK)
    {
        LOG("MANUAL RELOAD GUN COUNTER %s: StockFallback: the hook did not enable", lane.title);
        (void)MH_RemoveHook(target);
        lane.target = nullptr;
        lane.original = nullptr;
        return false;
    }
    lane.ready.store(true, std::memory_order_release);
    LOG("MANUAL RELOAD GUN COUNTER %s: Installed (generation %u): weapon function values +0x%llX; only the digit "
        "inputs (ids 0x%X, 0x%X) of the gun the manual reload owns show the player's count",
        lane.title, generation, static_cast<unsigned long long>(entry - base), lane.primary + 1, lane.primary + 2);
    return true;
}

bool Remove(Lane& lane, void* detour)
{
    lane.ready.store(false, std::memory_order_release);
    if (!lane.target) return true;
    const auto status = MCCVR_DisableHookForRetirement(lane.target);
    if (status != MH_OK && status != MH_ERROR_DISABLED && status != MH_ERROR_NOT_CREATED)
    {
        LOG("MANUAL RELOAD GUN COUNTER %s CleanupRequired: the hook did not disable (%d)", lane.title,
            static_cast<int>(status));
        return false;
    }
    const void* functions[]{detour};
    const void* trampolines[]{reinterpret_cast<const void*>(lane.original)};
    if (!WaitForNativeDetourQuiescence(functions, trampolines, 1, lane.callbacks))
    {
        LOG("MANUAL RELOAD GUN COUNTER %s CleanupRequired: the detour is busy", lane.title);
        return false;
    }
    const auto removed = MH_RemoveHook(lane.target);
    if (removed != MH_OK && removed != MH_ERROR_NOT_CREATED)
    {
        LOG("MANUAL RELOAD GUN COUNTER %s CleanupRequired: the hook was not removed (%d)", lane.title,
            static_cast<int>(removed));
        return false;
    }
    lane.target = nullptr;
    lane.original = nullptr;
    return true;
}

void Report(Lane& lane)
{
    static uint64_t lastShown[2]{}, lastFaults[2]{};
    static ULONGLONG at[2]{};
    const int i = lane.game == GameTitle::Halo3ODST ? 1 : 0;
    const uint64_t shown = lane.shown.load(std::memory_order_relaxed);
    const uint64_t faults = lane.faults.load(std::memory_order_relaxed);
    const ULONGLONG now = GetTickCount64();
    if ((shown != lastShown[i] && now - at[i] >= 30000) || faults != lastFaults[i])
    {
        lastShown[i] = shown;
        lastFaults[i] = faults;
        at[i] = now;
        LOG("MANUAL RELOAD GUN COUNTER %s: the gun's digits showed the player's count %llu times; faults %llu",
            lane.title, static_cast<unsigned long long>(shown), static_cast<unsigned long long>(faults));
    }
}
} // namespace manual_reload_gun_display

bool InstallHalo3ManualReloadGunDisplay(uintptr_t base, size_t size, uint32_t generation)
{
    return manual_reload_gun_display::Install(manual_reload_gun_display::g_halo3,
        reinterpret_cast<void*>(&manual_reload_gun_display::Detour<manual_reload_gun_display::g_halo3>), base, size,
        generation);
}
bool RemoveHalo3ManualReloadGunDisplay()
{
    return manual_reload_gun_display::Remove(manual_reload_gun_display::g_halo3,
        reinterpret_cast<void*>(&manual_reload_gun_display::Detour<manual_reload_gun_display::g_halo3>));
}
bool InstallOdstManualReloadGunDisplay(uintptr_t base, size_t size, uint32_t generation)
{
    return manual_reload_gun_display::Install(manual_reload_gun_display::g_odst,
        reinterpret_cast<void*>(&manual_reload_gun_display::Detour<manual_reload_gun_display::g_odst>), base, size,
        generation);
}
bool RemoveOdstManualReloadGunDisplay()
{
    return manual_reload_gun_display::Remove(manual_reload_gun_display::g_odst,
        reinterpret_cast<void*>(&manual_reload_gun_display::Detour<manual_reload_gun_display::g_odst>));
}
