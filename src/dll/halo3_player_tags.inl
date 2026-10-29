// PLAYER TAGS, Halo 3 (player_tags_show / _scale / _opacity / _max_distance_m; halo3_player_tags_logic.h has the
// game's side and the rules). Included in game.cpp after halo3_hud_note.inl.
//
// One hook, on chud_draw_navpoints: around the original, each teammate's tag gets its alpha multiplier and its size
// from the player's settings, and both are put back after it. With every setting at its default the detour only
// forwards. Halo 3 in MCC and in Project Reclaimer (which loads the same halo3.dll and enlarges the HUD block the
// navpoints live in: the draw takes them by pointer, so nothing here depends on that block's size).
//
// PROOF (resolved + guarded, nothing hardcoded): the draw by its entry signature and again by its one caller's call
// operand; the projection by the draw's own call; the view by the projection's own read; the entry layout by the
// draw's own instructions that read the position, the size, the alpha and the loop's stride and count. The same
// in-body checks run again on the draw's first call, after any host has patched the image: a miss there leaves the
// game's own tags for the rest of the level, said once.
//
// THREAD. The draw runs inside the HUD draw. The only fields written are the two the draw reads and the game
// rewrites every update (+0x14 alpha, +0x38 size); each is put back only where it still holds what was written, so
// an update that ran meanwhile keeps its own value. Nothing here is read by a shot, an aim or the network.

#include "../common/halo3_player_tags_logic.h"

using Halo3NavpointsDrawFn = void(__fastcall*)(uint8_t* navpoints, int user);

struct Halo3PlayerTagsLane
{
    void* target = nullptr;
    Halo3NavpointsDrawFn original = nullptr;
    uintptr_t body = 0;                 // the draw, for the first-call recheck
    uintptr_t base = 0;                 // halo3.dll, for the same
    size_t size = 0;
    uint8_t* const* view = nullptr;     // the view pointer the projection reads
    std::atomic<bool> ready{false};
    std::atomic<int> checked{0};        // 0 not yet, 1 proven on the live code, -1 refused
    std::atomic<uint32_t> callbacks{0};
    std::atomic<uint32_t> faults{0};
    // Cold reporting: the last settings the draw applied (halo3_player_tags::Fingerprint), logged once per change.
    std::atomic<uint32_t> appliedFingerprint{0};
    uint32_t reportedFingerprint = 0;
    std::atomic<uint32_t> lastPlayerTags{0}, lastHidden{0};
};
Halo3PlayerTagsLane g_halo3PlayerTags;

namespace
{
// The in-body checks (halo3_player_tags_logic.h): the entry layout the writes below depend on, and the projection.
// Null when every one holds; otherwise what failed. `projection` gets the projection's address.
const char* Halo3PlayerTagsBodyProof(uintptr_t draw, uintptr_t base, size_t size, uintptr_t& projection)
{
    using namespace halo3_player_tags;
    const size_t span = (draw + kDrawBodyBytes <= base + size) ? kDrawBodyBytes : 0;
    if (!span)
        return "the draw's extent";
    if (!mcc_clock::FindUnique(draw, span, kRecordPattern))
        return "the record and position reads";
    if (!mcc_clock::FindUnique(draw, span, kScaleReadPattern))
        return "the size read";
    if (!sig::Find(draw, span, kAlphaReadPattern))
        return "the alpha read";
    if (!mcc_clock::FindUnique(draw, span, kLoopPattern))
        return "the loop's stride and count";
    const uintptr_t call = mcc_clock::FindUnique(draw, span, kProjectionCallPattern);
    if (!call)
        return "the projection call";
    projection = sig::RipTarget(call + kProjectionCallRel, call + kProjectionCallRel + 4);
    if (projection < base || projection >= base + size)
        return "the projection's address";
    return nullptr;
}

// Runs the in-body checks once on the live code, from inside the draw. Guarded: no C++ object here.
bool Halo3PlayerTagsLiveProof(Halo3PlayerTagsLane& lane)
{
    const int state = lane.checked.load(std::memory_order_acquire);
    if (state)
        return state > 0;
    bool ok = false;
    __try
    {
        uintptr_t projection = 0;
        ok = !Halo3PlayerTagsBodyProof(lane.body, lane.base, lane.size, projection);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        ok = false;
    }
    lane.checked.store(ok ? 1 : -1, std::memory_order_release);
    if (!ok)
        LOG("PLAYER TAGS Halo 3: StockFallback: the navpoint draw changed after install (another program patched "
            "it?); teammates' tags stay as the game draws them for the rest of this level");
    return ok;
}

halo3_player_tags::Settings Halo3PlayerTagsSettings()
{
    halo3_player_tags::Settings s{};
    s.show = g_config.player_tags_show;
    s.scale = g_config.player_tags_scale;
    s.opacity = g_config.player_tags_opacity;
    s.maxDistanceM = g_config.player_tags_max_distance_m;
    return halo3_player_tags::Clamped(s);
}

struct Halo3PlayerTagsSaved
{
    float alpha[halo3_player_tags::kEntryCount];
    float scale[halo3_player_tags::kEntryCount];
    float wroteAlpha[halo3_player_tags::kEntryCount];
    float wroteScale[halo3_player_tags::kEntryCount];
    bool changed[halo3_player_tags::kEntryCount];
};

// Applies the settings to every player tag. Guarded: no C++ object here.
void Halo3PlayerTagsApply(Halo3PlayerTagsLane& lane, uint8_t* navpoints, const halo3_player_tags::Settings& s,
                          Halo3PlayerTagsSaved& saved)
{
    using namespace halo3_player_tags;
    uint32_t players = 0, hidden = 0;
    __try
    {
        float eye[3]{};
        bool haveEye = false;
        if (s.maxDistanceM > 0.0f && lane.view)
        {
            const uint8_t* view = *lane.view;
            if (view)
            {
                std::memcpy(eye, view + kViewPositionOffset, sizeof(eye));
                haveEye = std::isfinite(eye[0]) && std::isfinite(eye[1]) && std::isfinite(eye[2]);
            }
        }
        for (int i = 0; i < kEntryCount; ++i)
        {
            uint8_t* entry = navpoints + i * kEntryStride;
            saved.changed[i] = false;
            if (!entry[kValidOffset])
                continue;
            int32_t icon = 0;
            uint32_t flags = 0;
            std::memcpy(&icon, entry + kIconOffset, sizeof(icon));
            std::memcpy(&flags, entry + kFlagsOffset, sizeof(flags));
            if (!IsPlayerTag(icon, flags))
                continue;
            ++players;
            float distanceM = -1.0f;
            if (haveEye)
            {
                float position[3];
                std::memcpy(position, entry + kPositionOffset, sizeof(position));
                distanceM = DistanceM(position, eye);
            }
            const Adjust adjust = AdjustFor(s, distanceM);
            if (adjust.alpha <= 0.0f)
                ++hidden;
            float alpha = 0.0f, scale = 0.0f;
            std::memcpy(&alpha, entry + kAlphaOffset, sizeof(alpha));
            std::memcpy(&scale, entry + kScaleOffset, sizeof(scale));
            saved.alpha[i] = alpha;
            saved.scale[i] = scale;
            saved.wroteAlpha[i] = alpha * adjust.alpha;
            saved.wroteScale[i] = scale * adjust.scale;
            std::memcpy(entry + kAlphaOffset, &saved.wroteAlpha[i], sizeof(float));
            std::memcpy(entry + kScaleOffset, &saved.wroteScale[i], sizeof(float));
            saved.changed[i] = true;
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        lane.faults.fetch_add(1, std::memory_order_relaxed);
    }
    lane.lastPlayerTags.store(players, std::memory_order_relaxed);
    lane.lastHidden.store(hidden, std::memory_order_relaxed);
}

// Puts back each written field that still holds what was written. Guarded: no C++ object here.
void Halo3PlayerTagsRestore(Halo3PlayerTagsLane& lane, uint8_t* navpoints, const Halo3PlayerTagsSaved& saved)
{
    using namespace halo3_player_tags;
    __try
    {
        for (int i = 0; i < kEntryCount; ++i)
        {
            if (!saved.changed[i])
                continue;
            uint8_t* entry = navpoints + i * kEntryStride;
            float now = 0.0f;
            std::memcpy(&now, entry + kAlphaOffset, sizeof(now));
            if (now == saved.wroteAlpha[i])
                std::memcpy(entry + kAlphaOffset, &saved.alpha[i], sizeof(float));
            std::memcpy(&now, entry + kScaleOffset, sizeof(now));
            if (now == saved.wroteScale[i])
                std::memcpy(entry + kScaleOffset, &saved.scale[i], sizeof(float));
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        lane.faults.fetch_add(1, std::memory_order_relaxed);
    }
}
} // namespace

__declspec(noinline) void __fastcall Halo3NavpointsDrawDetour(uint8_t* navpoints, int user)
{
    auto& lane = g_halo3PlayerTags;
    lane.callbacks.fetch_add(1, std::memory_order_acq_rel);
    __try
    {
        const halo3_player_tags::Settings settings = Halo3PlayerTagsSettings();
        const bool apply = navpoints && lane.ready.load(std::memory_order_acquire) &&
                           !halo3_player_tags::IsStock(settings) && Halo3PlayerTagsLiveProof(lane);
        Halo3PlayerTagsSaved saved;
        if (apply)
        {
            Halo3PlayerTagsApply(lane, navpoints, settings, saved);
            lane.appliedFingerprint.store(halo3_player_tags::Fingerprint(settings), std::memory_order_relaxed);
        }
        if (lane.original)
            CallNativeWithSpareSlots(lane.original, navpoints, user);
        if (apply)
            Halo3PlayerTagsRestore(lane, navpoints, saved);
    }
    __finally
    {
        lane.callbacks.fetch_sub(1, std::memory_order_acq_rel);
    }
}

// Install thread. Its own transaction: any miss leaves the game's own tags, said once.
bool InstallHalo3PlayerTags(uintptr_t base, size_t size, uint32_t generation)
{
    using namespace halo3_player_tags;
    auto& lane = g_halo3PlayerTags;
    lane.ready.store(false, std::memory_order_release);
    if (lane.target)
    {
        LOG("PLAYER TAGS Halo 3: StockFallback: a previous navpoint hook has not retired");
        return false;
    }
    const char* why = nullptr;
    uintptr_t draw = 0, caller = 0, projection = 0, viewRead = 0;
    if (!(draw = mcc_clock::FindUnique(base, size, kDrawEntryPattern)))
        why = "the navpoint draw's entry";
    else if (!(caller = mcc_clock::FindUnique(base, size, kCallerPattern)) ||
             sig::RipTarget(caller + kCallerCallRel, caller + kCallerCallRel + 4) != draw)
        why = "the HUD draw's call of it";
    else if (const char* body = Halo3PlayerTagsBodyProof(draw, base, size, projection))
        why = body;
    else if (!(viewRead = mcc_clock::FindUnique(projection, kProjectionHeadBytes, kViewReadPattern)))
        why = "the projection's view read";
    if (why)
    {
        LOG("PLAYER TAGS Halo 3: StockFallback: %s did not prove; teammates' tags stay as the game draws them", why);
        return false;
    }
    const uintptr_t view = sig::RipTarget(viewRead + kViewReadDisp, viewRead + kViewReadDisp + 4);
    lane.view = (view >= base && view < base + size) ? reinterpret_cast<uint8_t* const*>(view) : nullptr;
    lane.body = draw;
    lane.base = base;
    lane.size = size;
    lane.checked.store(0, std::memory_order_release);
    lane.faults.store(0, std::memory_order_relaxed);
    void* const target = reinterpret_cast<void*>(draw);
    if (MH_CreateHook(target, reinterpret_cast<void*>(&Halo3NavpointsDrawDetour),
                      reinterpret_cast<void**>(&lane.original)) != MH_OK)
    {
        lane.original = nullptr;
        LOG("PLAYER TAGS Halo 3: StockFallback: the navpoint draw's hook was not created");
        return false;
    }
    lane.target = target;
    lane.ready.store(true, std::memory_order_release);
    if (MH_EnableHook(target) != MH_OK)
    {
        LOG("PLAYER TAGS Halo 3: StockFallback: the navpoint draw's hook did not enable");
        (void)RemoveHalo3PlayerTags();
        return false;
    }
    LOG("PLAYER TAGS Halo 3: Installed (generation %u): chud_draw_navpoints +0x%llX (called once, from +0x%llX) draws "
        "the twenty navpoints of a user's HUD; around it each teammate's tag gets its alpha and size from the player "
        "tag settings and both are put back after. Projection +0x%llX, its view %s. Proven: the entry and its "
        "caller, and in the draw the position, size and alpha reads and the loop of 20 x 0x74 (checked again on the "
        "first draw). Settings: show %d, size %.2f, opacity %.2f, max distance %.0f m (0 = none)",
        generation, static_cast<unsigned long long>(draw - base), static_cast<unsigned long long>(caller - base),
        static_cast<unsigned long long>(projection - base),
        lane.view ? "bound (distance limit available)" : "not bound (the distance limit does nothing)",
        g_config.player_tags_show ? 1 : 0, g_config.player_tags_scale, g_config.player_tags_opacity,
        g_config.player_tags_max_distance_m);
    return true;
}

// Worker: retire the hook (disable, prove nothing is inside, remove). Idempotent; false = try again.
bool RemoveHalo3PlayerTags()
{
    auto& lane = g_halo3PlayerTags;
    lane.ready.store(false, std::memory_order_release);
    if (!lane.target)
        return true;
    const auto status = MCCVR_DisableHookForRetirement(lane.target);
    if (status != MH_OK && status != MH_ERROR_DISABLED && status != MH_ERROR_NOT_CREATED)
    {
        LOG("PLAYER TAGS Halo 3 CleanupRequired: the navpoint hook did not disable (%d)", static_cast<int>(status));
        return false;
    }
    const void* functions[]{reinterpret_cast<const void*>(&Halo3NavpointsDrawDetour)};
    const void* trampolines[]{reinterpret_cast<const void*>(lane.original)};
    if (!WaitForNativeDetourQuiescence(functions, trampolines, 1, lane.callbacks))
    {
        LOG("PLAYER TAGS Halo 3 CleanupRequired: the navpoint detour is busy");
        return false;
    }
    const auto removed = MH_RemoveHook(lane.target);
    if (removed != MH_OK && removed != MH_ERROR_NOT_CREATED)
    {
        LOG("PLAYER TAGS Halo 3 CleanupRequired: the navpoint hook was not removed (%d)", static_cast<int>(removed));
        return false;
    }
    lane.target = nullptr;
    lane.original = nullptr;
    lane.view = nullptr;
    lane.body = 0;
    lane.checked.store(0, std::memory_order_release);
    return true;
}

// Cold worker: one line when the applied settings change, with what the last draw saw.
void ReportHalo3PlayerTags()
{
    auto& lane = g_halo3PlayerTags;
    const uint32_t applied = lane.appliedFingerprint.load(std::memory_order_relaxed);
    if (!applied || applied == lane.reportedFingerprint)
        return;
    lane.reportedFingerprint = applied;
    LOG("PLAYER TAGS Halo 3: applied show %d, size %.2f, opacity %.2f, max distance %.0f m: last draw %u teammate "
        "tag(s), %u hidden; %u fault(s)",
        g_config.player_tags_show ? 1 : 0, g_config.player_tags_scale, g_config.player_tags_opacity,
        g_config.player_tags_max_distance_m, lane.lastPlayerTags.load(std::memory_order_relaxed),
        lane.lastHidden.load(std::memory_order_relaxed), lane.faults.load(std::memory_order_relaxed));
}
