// HALO 3 PART DRAW: a vehicle camera's "hideParts" and "fadeParts" -- the parts of the player's own vehicle whose
// material it names are not drawn (a hornet's glass inside its hull mesh, the Longsword's glass beside its screen),
// or are drawn with less of their tint and glare. Included by game.cpp after the avatar (halo3_avatar.inl); the cold
// worker's half is halo3_vehicle_parts.inl, the fading rule halo3_glass_fade.h.
//
// THE PART DRAW. halo3.dll draws one part of one mesh of one object per call of its part draw (+0x2B5590 on retail,
// found by a unique signature; docs/HALO3-VEHICLE-PARTS-AND-GLASS-2026-10-20.md section 3), from the opaque and the
// transparent passes alike; neither caller reads a result. Its record (rcx) names the object's row (+8: info, whose
// +4 is the render model's tag index and +0x48 the object), the mesh (+0x1A) and the part (+0x18); the part's
// material is the first int16 of the render model's mesh -> parts entry. A call whose object, model and material
// the seat's camera hides is not passed on; every other call is, untouched.
//
// THE BIND (fading). Inside that call the material's bind (+0x289B90) copies the render method's real parameters
// into a buffer of its own and calls +0x2894B0 (twice at most, found by its signature) to apply the animated
// functions to that copy, then uploads it. The part draw notes "this part fades" for its own thread; the hook on
// +0x2894B0 lets the game write the copy, then scales the parameters the fade names -- once per bind (a parameter
// already scaled to this value is left), and only when the copy is of the postprocess entry the cold worker read
// for that material (its seventh argument is that entry + 0x50 at both call sites), so a material the game swapped
// in for this draw is left alone. Nothing shared is written.
//
// INSTALLED ONLY WHILE WANTED. The report (every 2 s) installs the part draw hook while the camera file asks any
// Halo 3 camera for "hideParts" or "fadeParts", the bind hook while it asks for "fadeParts", and removes each when
// the file no longer does; retirement with Halo 3 removes both first. While installed and nothing is published,
// their cost per call is an atomic load or two and a thread-local test.
//
// CO-OP. Only this machine's drawing: nothing the simulation reads is touched.

namespace halo3_part_draw
{
using DrawFn = void(__fastcall*)(void*, uint32_t);
using BindFn = uint64_t(__fastcall*)(uint32_t, void*, void*, void*, void*, void*, void*);
struct Hook
{
    void* target = nullptr;
    void* original = nullptr;
    bool refused = false; // its signature was not found for this binding: not tried again
};
struct Runtime
{
    std::atomic<uint32_t> callbacks{0};
    std::atomic<uint64_t> skipped{0}, faded{0}, swapped{0};
    Hook draw, bind;
    uintptr_t base = 0;
    size_t size = 0;
} runtime;

// What the part draw notes for the bind inside it, on its own thread.
struct FadeScope
{
    bool active = false;
    Halo3PartFade fade{};
    uint32_t writtenMask = 0;
    float written[halo3_glass_fade::kMaxParameters][4]{};
};
thread_local FadeScope fadeScope;

// ADDR-HYGIENE: resolved. The part draw's own prologue and its first reads (the record's info, the tag instance
// table, the info's render model index), unique in retail halo3.dll; the hook goes where it matches.
inline constexpr const char* kDrawPattern =
    "48 8B C4 48 89 58 08 48 89 68 18 48 89 70 20 89 50 10 57 41 54 41 55 41 56 41 57 48 83 EC 50 48 8B 41 08 33 ED "
    "48 8B F1 44 8B FD 48 8B 0D ?? ?? ?? ?? 44 0F B7 40 04";
// ADDR-HYGIENE: resolved. The animated-parameter step's prologue and first writes (its fifth argument into r13, the
// buffer into rbp, the fourth argument's word cleared), unique in retail halo3.dll.
inline constexpr const char* kBindPattern =
    "48 8B C4 48 89 58 10 48 89 68 18 48 89 70 20 89 48 08 57 41 54 41 55 41 56 41 57 48 83 EC 30 4C 8B AC 24 80 00 "
    "00 00 45 33 DB 48 8B EA 66 45 89 19";

// The object, render model and part material this call draws. Guarded: a read that faults is not a part of ours.
__declspec(noinline) bool ReadPart(const void* record, uint32_t& object, uint32_t& model, uint32_t& material)
{
    __try
    {
        const auto* r = static_cast<const uint8_t*>(record);
        const uint8_t* info = nullptr;
        std::memcpy(&info, r + 8, sizeof(info));
        if (!info)
            return false;
        uint32_t flags = 0;
        uint16_t tag = 0, meshIndex = 0, partIndex = 0;
        std::memcpy(&object, info + 0x48, 4);
        std::memcpy(&flags, info + 0x58, 4);
        std::memcpy(&tag, info + 4, 2);
        if (flags & 0x10)
            return false; // a first-person row
        const uint8_t* render = Halo3LoadedTagDefinition(tag);
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
        int16_t index = -1;
        std::memcpy(&index, tagBase + static_cast<size_t>(partAddress) * 4 + static_cast<size_t>(partIndex) * 0x10, 2);
        if (index < 0)
            return false;
        model = tag;
        material = static_cast<uint32_t>(index);
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

// The player's own view of a seat (the seated body's gate, without the avatar's own state).
bool SeatView()
{
    const RuntimeMode mode = TitleAdapter_GetRuntimeMode();
    return g_enabled.load() && VR_IsStereoEnabled() && !g_scopeRenderActive.load() && !VR_IsCutsceneTheaterActive() &&
           TitleAdapter_GetActiveTitle() == GameTitle::Halo3 &&
           (mode == RuntimeMode::Vehicle || mode == RuntimeMode::Turret);
}

void __fastcall DrawHook(void* record, uint32_t entry)
{
    runtime.callbacks.fetch_add(1, std::memory_order_acq_rel);
    __try
    {
        bool skip = false;
        const bool hides = g_halo3VehicleHide.partsAny.load(std::memory_order_relaxed) != 0;
        const bool fades = g_halo3VehicleFade.any.load(std::memory_order_relaxed) != 0;
        uint32_t object = 0, model = 0, material = 0;
        if ((hides || fades) && ReadPart(record, object, model, material) && SeatView())
        {
            const uint32_t generation = g_halo3RuntimeGeneration.load(std::memory_order_acquire);
            if (hides && Halo3VehicleHidesMaterial(generation, object, model, material))
                skip = true;
            else if (fades && Halo3VehicleFadeFor(generation, object, model, material, fadeScope.fade))
            {
                fadeScope.active = true;
                fadeScope.writtenMask = 0;
            }
        }
        if (skip)
            runtime.skipped.fetch_add(1, std::memory_order_relaxed);
        else
            reinterpret_cast<DrawFn>(runtime.draw.original)(record, entry);
    }
    __finally
    {
        fadeScope.active = false;
        runtime.callbacks.fetch_sub(1, std::memory_order_acq_rel);
    }
}

// The bind's copy of the parameters (`parameters`, one float4 each), with the noted fade applied once. `entry` is
// the bind's seventh argument, the postprocess entry + 0x50. Guarded.
__declspec(noinline) void FadeCopy(void* parameters, const void* entry)
{
    __try
    {
        FadeScope& scope = fadeScope;
        if (reinterpret_cast<uintptr_t>(entry) - 0x50 != scope.fade.postprocess)
        {
            runtime.swapped.fetch_add(1, std::memory_order_relaxed);
            return;
        }
        auto* values = static_cast<float(*)[4]>(parameters);
        uint32_t count = scope.fade.parameters;
        if (count > static_cast<uint32_t>(halo3_glass_fade::kMaxParameters))
            count = halo3_glass_fade::kMaxParameters;
        bool any = false;
        for (uint32_t k = 0; k < count; ++k)
        {
            const auto op = static_cast<halo3_glass_fade::Op>(scope.fade.op[k]);
            if (op == halo3_glass_fade::Op::None)
                continue;
            if ((scope.writtenMask & (1u << k)) && std::memcmp(values[k], scope.written[k], sizeof(values[k])) == 0)
                continue; // this bind's copy already holds the faded value
            halo3_glass_fade::Apply(op, scope.fade.kept, values[k]);
            std::memcpy(scope.written[k], values[k], sizeof(values[k]));
            scope.writtenMask |= 1u << k;
            any = true;
        }
        if (any)
            runtime.faded.fetch_add(1, std::memory_order_relaxed);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
    }
}

uint64_t __fastcall BindHook(uint32_t a1, void* parameters, void* a3, void* a4, void* a5, void* a6, void* entry)
{
    runtime.callbacks.fetch_add(1, std::memory_order_acq_rel);
    uint64_t result = 0;
    __try
    {
        result = reinterpret_cast<BindFn>(runtime.bind.original)(a1, parameters, a3, a4, a5, a6, entry);
        if (fadeScope.active && parameters && entry)
            FadeCopy(parameters, entry);
    }
    __finally
    {
        runtime.callbacks.fetch_sub(1, std::memory_order_acq_rel);
    }
    return result;
}

bool Remove(Hook& hook, const void* detour, const char* what)
{
    if (!hook.target)
        return true;
    const MH_STATUS disabled = MCCVR_DisableHookForRetirement(hook.target);
    if (disabled != MH_OK && disabled != MH_ERROR_DISABLED && disabled != MH_ERROR_NOT_CREATED)
    {
        LOG("Halo 3 vehicle parts CleanupRequired: the %s hook did not disable (MinHook status %d)", what,
            static_cast<int>(disabled));
        return false;
    }
    const void* hooks[] = {detour};
    const void* originals[] = {hook.original};
    if (!WaitForNativeDetourQuiescence(hooks, originals, 1, runtime.callbacks))
    {
        LOG("Halo 3 vehicle parts CleanupRequired: the %s hook is still being run", what);
        return false;
    }
    const MH_STATUS removed = MH_RemoveHook(hook.target);
    if (removed != MH_OK && removed != MH_ERROR_NOT_CREATED)
    {
        LOG("Halo 3 vehicle parts CleanupRequired: the %s hook was not removed (MinHook status %d)", what,
            static_cast<int>(removed));
        return false;
    }
    hook.target = nullptr;
    hook.original = nullptr;
    return true;
}

bool Install(Hook& hook, const char* pattern, void* detour, const char* what)
{
    if (hook.target || !runtime.base || hook.refused)
        return hook.target != nullptr;
    const uintptr_t hit = sig::Find(runtime.base, runtime.size, pattern);
    if (!hit || sig::Find(hit + 1, runtime.base + runtime.size - hit - 1, pattern))
    {
        hook.refused = true;
        LOG("Halo 3 vehicle parts: the %s is %s; that part of the camera file is not applied", what,
            hit ? "ambiguous" : "not found");
        return false;
    }
    void* target = reinterpret_cast<void*>(hit);
    const MH_STATUS created = MH_CreateHook(target, detour, &hook.original);
    if (created != MH_OK)
    {
        hook.refused = true;
        LOG("Halo 3 vehicle parts: the %s hook was not created (MinHook status %d)", what, static_cast<int>(created));
        return false;
    }
    hook.target = target;
    const MH_STATUS enabled = MH_EnableHook(target);
    if (enabled != MH_OK)
    {
        LOG("Halo 3 vehicle parts: the %s hook was not enabled (MinHook status %d)", what, static_cast<int>(enabled));
        (void)Remove(hook, detour, what);
        hook.refused = true;
        return false;
    }
    LOG("Halo 3 vehicle parts: the %s hook is in (+0x%llX)", what, static_cast<unsigned long long>(hit - runtime.base));
    return true;
}
} // namespace halo3_part_draw

// The Halo 3 install: where halo3.dll is. Nothing is hooked until a camera asks for "hideParts" or "fadeParts".
void BindHalo3PartDraw(uintptr_t base, size_t size, uint32_t)
{
    using namespace halo3_part_draw;
    runtime.base = base;
    runtime.size = size;
    runtime.draw.refused = false;
    runtime.bind.refused = false;
}

// Halo 3's retirement, before halo3.dll goes: the bind hook first (it only runs inside the part draw).
bool RemoveHalo3PartDraw()
{
    using namespace halo3_part_draw;
    if (!Remove(runtime.bind, reinterpret_cast<const void*>(BindHook), "material bind") ||
        !Remove(runtime.draw, reinterpret_cast<const void*>(DrawHook), "part draw"))
        return false;
    runtime.base = 0;
    runtime.size = 0;
    return true;
}

// Every report (about 2 s): each hook in while wanted and out when not, and what they did.
void ReportHalo3PartDraw()
{
    using namespace halo3_part_draw;
    const bool fade = Halo3VehicleCameraFadePartsWanted();
    const bool draw = fade || Halo3VehicleCameraHidePartsWanted();
    if (draw && !runtime.draw.target)
        (void)Install(runtime.draw, kDrawPattern, reinterpret_cast<void*>(DrawHook), "part draw");
    if (fade && runtime.draw.target && !runtime.bind.target)
        (void)Install(runtime.bind, kBindPattern, reinterpret_cast<void*>(BindHook), "material bind");
    if (!fade && runtime.bind.target)
        (void)Remove(runtime.bind, reinterpret_cast<const void*>(BindHook), "material bind");
    if (!draw && runtime.draw.target && !runtime.bind.target)
        (void)Remove(runtime.draw, reinterpret_cast<const void*>(DrawHook), "part draw");
    static uint64_t shown[3]{};
    static ULONGLONG logged = 0;
    const uint64_t now[3]{runtime.skipped.load(std::memory_order_relaxed), runtime.faded.load(std::memory_order_relaxed),
                          runtime.swapped.load(std::memory_order_relaxed)};
    const ULONGLONG clock = GetTickCount64();
    if (std::memcmp(now, shown, sizeof(now)) != 0 && clock - logged >= 5000)
    {
        logged = clock;
        LOG("Halo 3 vehicle parts: since the last line, %llu parts of the seat's vehicle left out (hideParts), %llu "
            "binds of its glass faded (fadeParts), %llu binds left as they were (another material bound for the part)",
            static_cast<unsigned long long>(now[0] - shown[0]), static_cast<unsigned long long>(now[1] - shown[1]),
            static_cast<unsigned long long>(now[2] - shown[2]));
        std::memcpy(shown, now, sizeof(now));
    }
}
