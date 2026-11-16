// MANUAL RELOAD ON THE HUD (Halo 3 / ODST, MCC): the CHUD's ammo readout shows the player's own count.
//
// The game is let reload ahead of the player (manual_reload_logic.h), so its rounds-loaded word is the game's. The
// CHUD builds each frame's weapon slots from one getter, `weapon_get_display_record(weapon, out)` (H3 +0x362A60,
// ODST +0x3A8C6C), which fills a stack record of the weapon's display values. It has four callers in each title, all
// display: the CHUD weapon-slot fill (H3 +0x2CD8E8, ODST +0x2F7F58), a CHUD pickup check, and the training hints.
// Firing, AI and the network serializer never call it (the serializer reads the weapon datum directly). Every
// 16-bit reader of the rounds fields was listed in both binaries: no other CHUD code reads rounds loaded.
// (Research 2026-11-15: docs/MANUAL-RELOAD-PORT-2026-11-15.md.)
//
// The detour calls the original and then, ONLY for the call that returns into the CHUD fill (found by the fill's
// call-site signature, the same in both titles) and only for the gun the manual reload owns, writes the player's
// count into that stack record: rounds loaded (H3 +0x2C, ODST +0x30, capped at the record's maximum beside it) and
// the reserve (H3 +0x30, ODST +0x34). The meter, the digits and the low/empty flash then follow the player's count.
// Nothing of the game's weapon is written: multiplayer-safe (AGENTS.md). Not installed in Project Reclaimer, whose
// lifted engine runs its own copy of this code.
namespace manual_reload_hud
{
using GetterFn = void(__fastcall*)(uint32_t weapon, void* out);
// The CHUD fill's call of the getter: E8 at +0x29, the return address at +0x2E (H3 +0x2CD927, ODST +0x2F7F93).
constexpr const char* kFillCallSite =
    "41 8B CF BB ?? 00 00 00 4A 8B 04 D0 44 0F B7 D2 48 8D 55 B0 49 6B F6 54 48 8B 1C 18 4F 8D 1C 52 48 8B 43 48 "
    "4A 8B 5C D8 10 E8 ?? ?? ?? ?? 0F B7 03 45 33 C9";
constexpr size_t kCallRel = 0x2A, kReturn = 0x2E;
// The getter's own entry, as a cross-check of the call's target.
constexpr const char* kHalo3Getter =
    "48 8B C4 53 55 56 57 41 54 41 55 41 56 41 57 48 81 EC B8 00 00 00 44 8B 05 ?? ?? ?? ?? 48 8B F2";
constexpr const char* kOdstGetter =
    "48 8B C4 55 53 56 57 41 54 41 55 41 56 41 57 48 8D 68 A1 48 81 EC E8 00 00 00 44 8B 05 ?? ?? ?? ?? 48 8B F2";

struct Lane
{
    const char* title;
    GameTitle game;
    const char* getterPattern;
    size_t loaded, maximum, reserve;   // magazine 0's words in the display record
    void* target = nullptr;
    GetterFn original = nullptr;
    uintptr_t returnAt = 0;
    std::atomic<bool> ready{false};
    std::atomic<uint32_t> callbacks{0};
    std::atomic<uint64_t> shown{0}, faults{0};
};
Lane g_halo3{"Halo 3", GameTitle::Halo3, kHalo3Getter, 0x2C, 0x2E, 0x30};
Lane g_odst{"ODST", GameTitle::Halo3ODST, kOdstGetter, 0x30, 0x32, 0x34};

bool Substitute(Lane& lane, uint32_t weapon, uint8_t* out)
{
    int loaded = 0, reserve = 0;
    if (!VR_ManualReloadHudCount(lane.game, weapon, loaded, reserve)) return false;
    __try
    {
        const int16_t maximum = *reinterpret_cast<const int16_t*>(out + lane.maximum);
        if (maximum <= 0) return false;
        *reinterpret_cast<int16_t*>(out + lane.loaded) = int16_t(loaded < 0 ? 0 : loaded > maximum ? maximum : loaded);
        *reinterpret_cast<int16_t*>(out + lane.reserve) = int16_t(reserve < 0 ? 0 : reserve > 32767 ? 32767 : reserve);
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        lane.faults.fetch_add(1, std::memory_order_relaxed);
        lane.ready.store(false, std::memory_order_release);
        return false;
    }
}

template <Lane& L>
__declspec(noinline) void __fastcall Detour(uint32_t weapon, void* out)
{
    L.callbacks.fetch_add(1, std::memory_order_acq_rel);
    const uintptr_t caller = reinterpret_cast<uintptr_t>(_ReturnAddress());
    L.original(weapon, out);
    if (out && caller == L.returnAt && L.ready.load(std::memory_order_acquire) && g_config.manual_reload &&
        Substitute(L, weapon, static_cast<uint8_t*>(out)))
        L.shown.fetch_add(1, std::memory_order_relaxed);
    L.callbacks.fetch_sub(1, std::memory_order_acq_rel);
}

bool Install(Lane& lane, void* detour, uintptr_t base, size_t size, uint32_t generation)
{
    lane.ready.store(false, std::memory_order_release);
    if (lane.target)
    {
        LOG("MANUAL RELOAD HUD %s: StockFallback: a previous hook has not retired", lane.title);
        return false;
    }
    if (lifted::Active())
    {
        LOG("MANUAL RELOAD HUD %s: not installed in Project Reclaimer (the HUD shows the game's count)", lane.title);
        return false;
    }
    const char* why = nullptr;
    uintptr_t site = 0, getter = 0;
    if (!(site = mcc_clock::FindUnique(base, size, kFillCallSite)))
        why = "the CHUD fill's call of the display getter";
    else if (reinterpret_cast<const uint8_t*>(site)[kCallRel - 1] != 0xE8 ||
             !(getter = sig::RipTarget(site + kCallRel, site + kReturn)) || getter < base || getter >= base + size)
        why = "the call's target";
    else if (mcc_clock::FindUnique(base, size, lane.getterPattern) != getter)
        why = "the getter's entry (the call goes elsewhere)";
    if (why)
    {
        LOG("MANUAL RELOAD HUD %s: StockFallback: %s did not prove; the HUD shows the game's count", lane.title, why);
        return false;
    }
    DWORD64 image = 0;
    if (!RtlLookupFunctionEntry(reinterpret_cast<DWORD64>(detour), &image, nullptr))
    {
        LOG("MANUAL RELOAD HUD %s: StockFallback: the detour has no unwind entry", lane.title);
        return false;
    }
    void* const target = reinterpret_cast<void*>(getter);
    if (MH_CreateHook(target, detour, reinterpret_cast<void**>(&lane.original)) != MH_OK)
    {
        lane.original = nullptr;
        LOG("MANUAL RELOAD HUD %s: StockFallback: the hook was not created", lane.title);
        return false;
    }
    lane.target = target;
    lane.returnAt = site + kReturn;
    lane.shown.store(0, std::memory_order_relaxed);
    lane.faults.store(0, std::memory_order_relaxed);
    if (MH_EnableHook(target) != MH_OK)
    {
        LOG("MANUAL RELOAD HUD %s: StockFallback: the hook did not enable", lane.title);
        (void)MH_RemoveHook(target);
        lane.target = nullptr;
        lane.original = nullptr;
        return false;
    }
    lane.ready.store(true, std::memory_order_release);
    LOG("MANUAL RELOAD HUD %s: Installed (generation %u): display getter +0x%llX; only its call from the CHUD fill "
        "(returning to +0x%llX) gets the player's count (loaded +0x%zX, reserve +0x%zX of the stack record)",
        lane.title, generation, static_cast<unsigned long long>(getter - base),
        static_cast<unsigned long long>(lane.returnAt - base), lane.loaded, lane.reserve);
    return true;
}

bool Remove(Lane& lane, void* detour)
{
    lane.ready.store(false, std::memory_order_release);
    if (!lane.target) return true;
    const auto status = MCCVR_DisableHookForRetirement(lane.target);
    if (status != MH_OK && status != MH_ERROR_DISABLED && status != MH_ERROR_NOT_CREATED)
    {
        LOG("MANUAL RELOAD HUD %s CleanupRequired: the hook did not disable (%d)", lane.title, static_cast<int>(status));
        return false;
    }
    const void* functions[]{detour};
    const void* trampolines[]{reinterpret_cast<const void*>(lane.original)};
    if (!WaitForNativeDetourQuiescence(functions, trampolines, 1, lane.callbacks))
    {
        LOG("MANUAL RELOAD HUD %s CleanupRequired: the detour is busy", lane.title);
        return false;
    }
    const auto removed = MH_RemoveHook(lane.target);
    if (removed != MH_OK && removed != MH_ERROR_NOT_CREATED)
    {
        LOG("MANUAL RELOAD HUD %s CleanupRequired: the hook was not removed (%d)", lane.title, static_cast<int>(removed));
        return false;
    }
    lane.target = nullptr;
    lane.original = nullptr;
    lane.returnAt = 0;
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
        LOG("MANUAL RELOAD HUD %s: the CHUD showed the player's count %llu times; faults %llu", lane.title,
            static_cast<unsigned long long>(shown), static_cast<unsigned long long>(faults));
    }
}
} // namespace manual_reload_hud

bool InstallHalo3ManualReloadHud(uintptr_t base, size_t size, uint32_t generation)
{
    return manual_reload_hud::Install(manual_reload_hud::g_halo3,
        reinterpret_cast<void*>(&manual_reload_hud::Detour<manual_reload_hud::g_halo3>), base, size, generation);
}
bool RemoveHalo3ManualReloadHud()
{
    return manual_reload_hud::Remove(manual_reload_hud::g_halo3,
        reinterpret_cast<void*>(&manual_reload_hud::Detour<manual_reload_hud::g_halo3>));
}
bool InstallOdstManualReloadHud(uintptr_t base, size_t size, uint32_t generation)
{
    return manual_reload_hud::Install(manual_reload_hud::g_odst,
        reinterpret_cast<void*>(&manual_reload_hud::Detour<manual_reload_hud::g_odst>), base, size, generation);
}
bool RemoveOdstManualReloadHud()
{
    return manual_reload_hud::Remove(manual_reload_hud::g_odst,
        reinterpret_cast<void*>(&manual_reload_hud::Detour<manual_reload_hud::g_odst>));
}
