// MANUAL RELOAD SOUNDS (Halo 3 / ODST, MCC). Research 2026-11-15 (docs/MANUAL-RELOAD-PORT-2026-11-15.md):
//
// - The game's reload sound is one ~2 s clip fired at frame 0 of the first-person reload animation, through the
//   first-person animation event handler `fp_anim_event(user, event, slot)` (H3 +0x2C2A94, ODST +0x2EB030): an event
//   with type 1 (+4) plays its sound datum (+8) on the held weapon. The first-person weapon state exists only for
//   local users and the handler reaches no network sender. While the manual reload has the gun and the game reloads
//   unseen, its sound events are dropped here, so the player does not hear a reload that is not shown.
// - `play_2d(sound, scale)` (H3 +0x158244, ODST +0x185CA4) plays a sound for this machine only: ODST's own per-player
//   code calls it for a local player and sends a network event instead for a remote one, and the ammo-pickup click
//   (the weapon definition's pickup sound, H3 +0x3AC, ODST +0x3BC) is played through it for a local user only. It
//   creates no object. It reads the game thread's thread-local data: it is called from the palette hook only.
//
// Neither touches the simulation: multiplayer-safe (AGENTS.md). Not installed in Project Reclaimer.
namespace manual_reload_sound
{
using EventFn = void(__fastcall*)(int32_t user, const uint8_t* event, int32_t slot);
using Play2dFn = void(__fastcall*)(int32_t sound, float scale);
constexpr const char* kEventHandler =
    "48 8B C4 48 89 58 08 48 89 68 10 48 89 ?? 18 4C 89 70 20 41 57 48 83 EC 70 65 48 8B 04 25 58 00 00 00 48 8B DA "
    "44 8B 0D ?? ?? ?? ?? 48 63 D1 4E 8B 14 C8 B8 ?? ?? 00 00 48 8D 0C 52 4A 8B 04 10 83 BC 88 04 06 00 00 00";
constexpr const char* kPlay2d =
    "40 55 48 8D 6C 24 A9 48 81 EC A0 00 00 00 F3 0F 10 05 ?? ?? ?? ?? 33 D2 83 C8 FF F3 0F 11 4D DB";

struct Lane
{
    const char* title;
    GameTitle game;
    size_t pickupSound;          // the weapon definition's pickup sound datum
    void* target = nullptr;
    EventFn original = nullptr;
    Play2dFn play = nullptr;
    std::atomic<bool> ready{false};
    std::atomic<uint32_t> callbacks{0};
    std::atomic<uint64_t> silenced{0}, played{0}, faults{0};
};
Lane g_halo3{"Halo 3", GameTitle::Halo3, 0x3AC};
Lane g_odst{"ODST", GameTitle::Halo3ODST, 0x3BC};
Lane& For(GameTitle title) { return title == GameTitle::Halo3ODST ? g_odst : g_halo3; }

template <Lane& L>
__declspec(noinline) void __fastcall Detour(int32_t user, const uint8_t* event, int32_t slot)
{
    L.callbacks.fetch_add(1, std::memory_order_acq_rel);
    bool drop = false;
    if (event && L.ready.load(std::memory_order_acquire) && user == 0 && slot == 0 && g_config.manual_reload_sounds &&
        VR_ManualReloadHidesReload(L.game))
    {
        __try { drop = *reinterpret_cast<const int32_t*>(event + 4) == 1; }
        __except (EXCEPTION_EXECUTE_HANDLER) { L.faults.fetch_add(1, std::memory_order_relaxed); }
    }
    if (drop) L.silenced.fetch_add(1, std::memory_order_relaxed);
    else L.original(user, event, slot);
    L.callbacks.fetch_sub(1, std::memory_order_acq_rel);
}

// The palette hook (the game's thread): the held gun's own pickup click, for this player only.
void PlayClick(GameTitle title, const uint8_t* definition, float scale)
{
    Lane& lane = For(title);
    if (!lane.ready.load(std::memory_order_acquire) || !lane.play || !definition) return;
    __try
    {
        const int32_t sound = *reinterpret_cast<const int32_t*>(definition + lane.pickupSound);
        if (sound == -1) return;
        lane.play(sound, scale);
        lane.played.fetch_add(1, std::memory_order_relaxed);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        lane.faults.fetch_add(1, std::memory_order_relaxed);
        lane.ready.store(false, std::memory_order_release);
    }
}

bool Install(Lane& lane, void* detour, uintptr_t base, size_t size, uint32_t generation)
{
    lane.ready.store(false, std::memory_order_release);
    if (lane.target)
    {
        LOG("MANUAL RELOAD SOUND %s: StockFallback: a previous hook has not retired", lane.title);
        return false;
    }
    if (lifted::Active())
    {
        LOG("MANUAL RELOAD SOUND %s: not installed in Project Reclaimer (the game's reload sound plays)", lane.title);
        return false;
    }
    const uintptr_t handler = mcc_clock::FindUnique(base, size, kEventHandler);
    const uintptr_t play = mcc_clock::FindUnique(base, size, kPlay2d);
    if (!handler || !play)
    {
        LOG("MANUAL RELOAD SOUND %s: StockFallback: %s did not prove", lane.title,
            !handler ? "the first-person animation event handler" : "the local 2D sound call");
        return false;
    }
    DWORD64 image = 0;
    if (!RtlLookupFunctionEntry(reinterpret_cast<DWORD64>(detour), &image, nullptr))
    {
        LOG("MANUAL RELOAD SOUND %s: StockFallback: the detour has no unwind entry", lane.title);
        return false;
    }
    void* const target = reinterpret_cast<void*>(handler);
    if (MH_CreateHook(target, detour, reinterpret_cast<void**>(&lane.original)) != MH_OK)
    {
        lane.original = nullptr;
        LOG("MANUAL RELOAD SOUND %s: StockFallback: the hook was not created", lane.title);
        return false;
    }
    lane.target = target;
    lane.play = lifted::Callable(reinterpret_cast<Play2dFn>(play));
    if (MH_EnableHook(target) != MH_OK)
    {
        LOG("MANUAL RELOAD SOUND %s: StockFallback: the hook did not enable", lane.title);
        (void)MH_RemoveHook(target);
        lane.target = nullptr;
        lane.original = nullptr;
        lane.play = nullptr;
        return false;
    }
    lane.ready.store(true, std::memory_order_release);
    LOG("MANUAL RELOAD SOUND %s: Installed (generation %u): first-person animation events +0x%llX (sound events "
        "dropped only while the manual reload hides the game's reload), local 2D sound +0x%llX (the gun's pickup "
        "click at the seat and the rack)",
        lane.title, generation, static_cast<unsigned long long>(handler - base),
        static_cast<unsigned long long>(play - base));
    return true;
}

bool Remove(Lane& lane, void* detour)
{
    lane.ready.store(false, std::memory_order_release);
    if (!lane.target) return true;
    const auto status = MCCVR_DisableHookForRetirement(lane.target);
    if (status != MH_OK && status != MH_ERROR_DISABLED && status != MH_ERROR_NOT_CREATED)
    {
        LOG("MANUAL RELOAD SOUND %s CleanupRequired: the hook did not disable (%d)", lane.title, static_cast<int>(status));
        return false;
    }
    const void* functions[]{detour};
    const void* trampolines[]{reinterpret_cast<const void*>(lane.original)};
    if (!WaitForNativeDetourQuiescence(functions, trampolines, 1, lane.callbacks))
    {
        LOG("MANUAL RELOAD SOUND %s CleanupRequired: the detour is busy", lane.title);
        return false;
    }
    const auto removed = MH_RemoveHook(lane.target);
    if (removed != MH_OK && removed != MH_ERROR_NOT_CREATED)
    {
        LOG("MANUAL RELOAD SOUND %s CleanupRequired: the hook was not removed (%d)", lane.title, static_cast<int>(removed));
        return false;
    }
    lane.target = nullptr;
    lane.original = nullptr;
    lane.play = nullptr;
    return true;
}

void Report(Lane& lane)
{
    static uint64_t last[2][3]{};
    const int i = lane.game == GameTitle::Halo3ODST ? 1 : 0;
    const uint64_t now[3]{lane.silenced.load(), lane.played.load(), lane.faults.load()};
    if (std::memcmp(now, last[i], sizeof(now)))
    {
        std::memcpy(last[i], now, sizeof(now));
        LOG("MANUAL RELOAD SOUND %s: the game's hidden reload sounds dropped %llu, clicks played %llu, faults %llu",
            lane.title, static_cast<unsigned long long>(now[0]), static_cast<unsigned long long>(now[1]),
            static_cast<unsigned long long>(now[2]));
    }
}
} // namespace manual_reload_sound

bool InstallHalo3ManualReloadSound(uintptr_t base, size_t size, uint32_t generation)
{
    return manual_reload_sound::Install(manual_reload_sound::g_halo3,
        reinterpret_cast<void*>(&manual_reload_sound::Detour<manual_reload_sound::g_halo3>), base, size, generation);
}
bool RemoveHalo3ManualReloadSound()
{
    return manual_reload_sound::Remove(manual_reload_sound::g_halo3,
        reinterpret_cast<void*>(&manual_reload_sound::Detour<manual_reload_sound::g_halo3>));
}
bool InstallOdstManualReloadSound(uintptr_t base, size_t size, uint32_t generation)
{
    return manual_reload_sound::Install(manual_reload_sound::g_odst,
        reinterpret_cast<void*>(&manual_reload_sound::Detour<manual_reload_sound::g_odst>), base, size, generation);
}
bool RemoveOdstManualReloadSound()
{
    return manual_reload_sound::Remove(manual_reload_sound::g_odst,
        reinterpret_cast<void*>(&manual_reload_sound::Detour<manual_reload_sound::g_odst>));
}
