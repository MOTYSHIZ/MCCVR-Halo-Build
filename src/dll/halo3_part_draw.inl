// HALO 3 PART DRAW: a vehicle camera's "hideParts" -- the parts of the player's own vehicle whose material it names
// (a hornet's glass inside its hull mesh, the Longsword's glass beside its screen) are not drawn in that camera.
// Included by game.cpp after the avatar (halo3_avatar.inl); the cold worker's half is halo3_vehicle_parts.inl.
//
// THE HOOK. halo3.dll draws one part of one mesh of one object per call of its part draw (+0x2B5590 on retail,
// found by a unique signature; docs/HALO3-VEHICLE-PARTS-AND-GLASS-2026-10-20.md section 3), from the opaque and the
// transparent passes alike; neither caller reads a result. Its record (rcx) names the object's row (+8: info, whose
// +4 is the render model's tag index and +0x48 the object), the mesh (+0x1A) and the part (+0x18); the part's
// material is the first int16 of the render model's mesh -> parts entry. A call whose object, model and material
// the seat's camera hides is not passed on; every other call is, untouched.
//
// INSTALLED ONLY WHILE WANTED. The report (every 2 s) installs the hook while the camera file asks any Halo 3 camera
// for "hideParts", and removes it when the file no longer does; retirement with Halo 3 removes it first. While it is
// installed and nothing is published its cost per part is one atomic load.
//
// CO-OP. Only this machine's drawing: nothing the simulation reads is touched.

namespace halo3_part_draw
{
using DrawFn = void(__fastcall*)(void*, uint32_t);
struct Runtime
{
    std::atomic<bool> enabled{false};
    std::atomic<uint32_t> callbacks{0};
    std::atomic<uint64_t> skipped{0};
    void* target = nullptr;
    DrawFn original = nullptr;
    uintptr_t base = 0;
    size_t size = 0;
    bool refused = false; // the signature was not found once for this binding: not tried again
} runtime;

// ADDR-HYGIENE: resolved. The part draw's own prologue and its first reads (the record's info, the tag instance
// table, the info's render model index), unique in retail halo3.dll; the hook goes where it matches.
inline constexpr const char* kDrawPattern =
    "48 8B C4 48 89 58 08 48 89 68 18 48 89 70 20 89 50 10 57 41 54 41 55 41 56 41 57 48 83 EC 50 48 8B 41 08 33 ED "
    "48 8B F1 44 8B FD 48 8B 0D ?? ?? ?? ?? 44 0F B7 40 04";

// Whether the seat's camera hides this call's part. Guarded: a read that faults draws the part.
__declspec(noinline) bool Hidden(const void* record)
{
    __try
    {
        const auto* r = static_cast<const uint8_t*>(record);
        const uint8_t* info = nullptr;
        std::memcpy(&info, r + 8, sizeof(info));
        if (!info)
            return false;
        uint32_t object = 0, flags = 0;
        uint16_t model = 0, meshIndex = 0, partIndex = 0;
        std::memcpy(&object, info + 0x48, 4);
        std::memcpy(&flags, info + 0x58, 4);
        std::memcpy(&model, info + 4, 2);
        if (flags & 0x10)
            return false; // a first-person row
        const uint8_t* render = Halo3LoadedTagDefinition(model);
        const uint8_t* tagBase = g_halo3TagDataBase ? static_cast<const uint8_t*>(*g_halo3TagDataBase) : nullptr;
        if (!render || !tagBase)
            return false;
        std::memcpy(&meshIndex, r + 0x1A, 2);
        std::memcpy(&partIndex, r + 0x18, 2);
        uint32_t meshCount = 0, meshAddress = 0, partCount = 0, partAddress = 0;
        std::memcpy(&meshCount, render + 0x68, 4);
        std::memcpy(&meshAddress, render + 0x6C, 4);
        if (meshIndex >= meshCount || !meshAddress)
            return false;
        const uint8_t* mesh = tagBase + static_cast<size_t>(meshAddress) * 4 + static_cast<size_t>(meshIndex) * 0x4C;
        std::memcpy(&partCount, mesh, 4);
        std::memcpy(&partAddress, mesh + 4, 4);
        if (partIndex >= partCount || !partAddress)
            return false;
        int16_t material = -1;
        std::memcpy(&material, tagBase + static_cast<size_t>(partAddress) * 4 + static_cast<size_t>(partIndex) * 0x10, 2);
        if (material < 0 ||
            !Halo3VehicleHidesMaterial(g_halo3RuntimeGeneration.load(std::memory_order_acquire), object, model,
                                       static_cast<uint32_t>(material)))
            return false;
        // The player's own view of a seat only (the seated body's gate, without the avatar's own state).
        const RuntimeMode mode = TitleAdapter_GetRuntimeMode();
        return g_enabled.load() && VR_IsStereoEnabled() && !g_scopeRenderActive.load() &&
               !VR_IsCutsceneTheaterActive() && TitleAdapter_GetActiveTitle() == GameTitle::Halo3 &&
               (mode == RuntimeMode::Vehicle || mode == RuntimeMode::Turret);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

void __fastcall DrawHook(void* record, uint32_t entry)
{
    runtime.callbacks.fetch_add(1, std::memory_order_acq_rel);
    __try
    {
        if (g_halo3VehicleHide.partsAny.load(std::memory_order_relaxed) && Hidden(record))
            runtime.skipped.fetch_add(1, std::memory_order_relaxed);
        else
            runtime.original(record, entry);
    }
    __finally
    {
        runtime.callbacks.fetch_sub(1, std::memory_order_acq_rel);
    }
}

bool Remove()
{
    Runtime& r = runtime;
    r.enabled.store(false, std::memory_order_release);
    if (!r.target)
        return true;
    const MH_STATUS disabled = MCCVR_DisableHookForRetirement(r.target);
    if (disabled != MH_OK && disabled != MH_ERROR_DISABLED && disabled != MH_ERROR_NOT_CREATED)
    {
        LOG("Halo 3 vehicle parts CleanupRequired: the part draw hook did not disable (MinHook status %d)",
            static_cast<int>(disabled));
        return false;
    }
    const void* hooks[] = {reinterpret_cast<const void*>(DrawHook)};
    const void* originals[] = {reinterpret_cast<const void*>(r.original)};
    if (!WaitForNativeDetourQuiescence(hooks, originals, 1, r.callbacks))
    {
        LOG("Halo 3 vehicle parts CleanupRequired: the part draw hook is still being run");
        return false;
    }
    const MH_STATUS removed = MH_RemoveHook(r.target);
    if (removed != MH_OK && removed != MH_ERROR_NOT_CREATED)
    {
        LOG("Halo 3 vehicle parts CleanupRequired: the part draw hook was not removed (MinHook status %d)",
            static_cast<int>(removed));
        return false;
    }
    r.target = nullptr;
    r.original = nullptr;
    return true;
}

bool Install()
{
    Runtime& r = runtime;
    if (r.target || !r.base || r.refused)
        return r.target != nullptr;
    const uintptr_t hit = sig::Find(r.base, r.size, kDrawPattern);
    if (!hit || sig::Find(hit + 1, r.base + r.size - hit - 1, kDrawPattern))
    {
        r.refused = true;
        LOG("Halo 3 vehicle parts: the part draw is %s; a camera's hideParts is not applied",
            hit ? "ambiguous" : "not found");
        return false;
    }
    void* target = reinterpret_cast<void*>(hit);
    const MH_STATUS created =
        MH_CreateHook(target, reinterpret_cast<void*>(DrawHook), reinterpret_cast<void**>(&r.original));
    if (created != MH_OK)
    {
        r.refused = true;
        LOG("Halo 3 vehicle parts: the part draw hook was not created (MinHook status %d)", static_cast<int>(created));
        return false;
    }
    r.target = target;
    const MH_STATUS enabled = MH_EnableHook(target);
    if (enabled != MH_OK)
    {
        LOG("Halo 3 vehicle parts: the part draw hook was not enabled (MinHook status %d)", static_cast<int>(enabled));
        (void)Remove();
        r.refused = true;
        return false;
    }
    r.enabled.store(true, std::memory_order_release);
    LOG("Halo 3 vehicle parts: the part draw hook is in (+0x%llX): a camera's hideParts leaves those parts of your own "
        "vehicle out of your view",
        static_cast<unsigned long long>(hit - r.base));
    return true;
}
} // namespace halo3_part_draw

// The Halo 3 install: where halo3.dll is. Nothing is hooked until a camera asks for "hideParts".
void BindHalo3PartDraw(uintptr_t base, size_t size, uint32_t)
{
    halo3_part_draw::runtime.base = base;
    halo3_part_draw::runtime.size = size;
    halo3_part_draw::runtime.refused = false;
}

// Halo 3's retirement, before halo3.dll goes.
bool RemoveHalo3PartDraw()
{
    if (!halo3_part_draw::Remove())
        return false;
    halo3_part_draw::runtime.base = 0;
    halo3_part_draw::runtime.size = 0;
    return true;
}

// Every report (about 2 s): the hook in while wanted and out when not, and what it did.
void ReportHalo3PartDraw()
{
    using namespace halo3_part_draw;
    const bool wanted = Halo3VehicleCameraHidePartsWanted();
    if (wanted && !runtime.target)
        (void)Install();
    else if (!wanted && runtime.target)
        (void)Remove();
    static uint64_t shown = 0;
    static ULONGLONG logged = 0;
    const uint64_t now = runtime.skipped.load(std::memory_order_relaxed);
    const ULONGLONG clock = GetTickCount64();
    if (now != shown && clock - logged >= 5000)
    {
        logged = clock;
        LOG("Halo 3 vehicle parts: %llu parts of the seat's vehicle left out for its camera's hideParts (since the "
            "last line)",
            static_cast<unsigned long long>(now - shown));
        shown = now;
    }
}
