// PROJECT RECLAIMER: ODST'S VISR GETS THE HEADSET EYE'S FIELD OF VIEW (included by game.cpp, after the player tags).
//
// Reclaimer 0.10 draws ODST's VISR itself. It builds VISR's screen-to-view constants (exe+0x377B7B0 in 0.10.4) from a
// camera it asks for by view (exe+0x3775E80): {some, vertical field of view, near, far}, read from the game camera
// at that moment, and the picture's width over height. In the headset the eye's projection is the mod's own, set
// only while each eye renders; the field of view the client read was the game's narrower one (79 degrees against the
// eye's 95 in the user's 2026-11-15 run, read live), so with VISR on the whole world looked zoomed and stretched for as
// long as VISR was on. The picture's width over height already matches the eye's (1.387 both).
//
// The fix answers that one question: the camera the client asks for VISR keeps its near and far, and its vertical
// field of view becomes the eye's (g_reclaimerEyeVerticalFov, written by each eye's render). Only VISR asks for it
// (its one caller, exe+0x377C830, builds VISR's state), so nothing else the client draws changes.
//
// THE WIDTH TOO. With the eye's vertical field of view VISR still looked stretched (user, 15:21): its constants take
// the horizontal half-width as the picture's width over height (2912/2100 = 1.387) times the vertical one, while the
// eye's projection is nearly square (1.376 wide for 1.428 high). VISR's screen-to-view constants (exe+0x377B7B0 in
// 0.10.4, a 0x110-byte block: +0x10 = 2 tanX, +0x24 = -2 tanY, +0x40 = -tanX, +0x44 = tanY, +0x48 = 1) are corrected
// after the client builds them: +0x10 and +0x40 get the eye's own tanX. Only a block that holds exactly that shape,
// built from the eye's vertical field of view, is touched.
//
// FOUND BY ITS SHAPE (both 0.10.0 and 0.10.4 checked offline, unique in each): the camera's three reads,
// mov e?x,[r?x+0x28]; mov e?x,[r?x+0x64]; mov e?x,[r?x+0x68]; test e?x,e?x, followed by the stores into the answer,
// mov [r?x+4]; mov [r?x+8]; mov [r?x+0xC]. The hook goes on the function whose extent holds both. Not found, or more
// than once: StockFallback, said once, and VISR stays as the client draws it.

namespace
{
constexpr char kReclaimerViewCameraReads[] = "8B ?? 28 8B ?? 64 8B ?? 68 85";
constexpr char kReclaimerViewCameraStores[] = "89 ?? 04 89 ?? 08 89 ?? 0C";
// In VISR's constant builder: movss [r+0x40],-tanX; movss [r+0x44],tanY; mov dword [r+0x48],1.0.
constexpr char kReclaimerVisrConstantsStores[] = "0F 11 ?? 40 F3 ?? 0F 11 ?? 44 ?? C7 ?? 48 00 00 80 3F";

// The builder: (out Vec of {name: &str, data: Vec<u8>}, width, height, ?, VISR's state).
using ReclaimerVisrConstantsFn = void*(__fastcall*)(void* out, uint32_t width, uint32_t height, void* r9,
                                                     void* state);
ReclaimerVisrConstantsFn g_reclaimerVisrConstantsOriginal = nullptr;
void* g_reclaimerVisrConstantsTarget = nullptr;
std::atomic<uint32_t> g_reclaimerVisrWidthsFixed{0};
std::atomic<uint32_t> g_reclaimerVisrWidthWasBits{0}, g_reclaimerVisrWidthNowBits{0};

__declspec(noinline) void* __fastcall ReclaimerVisrConstantsDetour(void* out, uint32_t width, uint32_t height,
                                                                  void* r9, void* state)
{
    void* const result = g_reclaimerVisrConstantsOriginal(out, width, height, r9, state);
    __try
    {
        const float tanX = g_reclaimerEyeTanX.load(std::memory_order_relaxed);
        const float fov = g_reclaimerEyeVerticalFov.load(std::memory_order_relaxed);
        if (!out || !(tanX > 0.05f && tanX < 20.0f) || !(fov > 0.1f && fov < 3.0f) ||
            !g_enabled.load(std::memory_order_relaxed) || !VR_IsStereoEnabled())
            return result;
        const float tanY = tanf(0.5f * fov);
        const uint64_t* vec = static_cast<const uint64_t*>(out);   // {capacity, items, count}
        const auto* items = reinterpret_cast<const uint8_t*>(vec[1]);
        const uint64_t count = vec[2];
        for (uint64_t i = 0; items && i < count && i < 64; ++i)
        {
            const uint8_t* item = items + i * 0x28;
            float* c = *reinterpret_cast<float* const*>(item + 0x18);
            if (*reinterpret_cast<const uint64_t*>(item + 0x20) != 0x110 || !c)
                continue;
            const float twoX = c[0x10 / 4], minusTwoY = c[0x24 / 4], minusX = c[0x40 / 4], y = c[0x44 / 4];
            const bool shape = c[0x48 / 4] == 1.0f && twoX > 0.0f && fabsf(minusX + 0.5f * twoX) <= 1e-4f * twoX &&
                               fabsf(minusTwoY + 2.0f * y) <= 1e-4f * fabsf(minusTwoY) && y > 0.0f;
            if (!shape || fabsf(y - tanY) > 0.02f * tanY)
                continue;   // not VISR's block, or not built from the eye's field of view
            uint32_t was = 0, now = 0;
            memcpy(&was, &twoX, sizeof(was));
            c[0x10 / 4] = 2.0f * tanX;
            c[0x40 / 4] = -tanX;
            memcpy(&now, &c[0x10 / 4], sizeof(now));
            g_reclaimerVisrWidthWasBits.store(was, std::memory_order_relaxed);
            g_reclaimerVisrWidthNowBits.store(now, std::memory_order_relaxed);
            g_reclaimerVisrWidthsFixed.fetch_add(1, std::memory_order_relaxed);
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
    }
    return result;
}

using ReclaimerViewCameraFn = void*(__fastcall*)(void* out, uint64_t view);
ReclaimerViewCameraFn g_reclaimerViewCameraOriginal = nullptr;
void* g_reclaimerViewCameraTarget = nullptr;
std::atomic<uint32_t> g_reclaimerViewCameraGiven{0};
std::atomic<uint32_t> g_reclaimerViewCameraGameBits{0}, g_reclaimerViewCameraEyeBits{0};

__declspec(noinline) void* __fastcall ReclaimerViewCameraDetour(void* out, uint64_t view)
{
    void* const result = g_reclaimerViewCameraOriginal(out, view);
    __try
    {
        // The answer: u32 some (1) / none (0), then f32 field of view, near, far.
        auto* answer = static_cast<uint32_t*>(out);
        const float eye = g_reclaimerEyeVerticalFov.load(std::memory_order_relaxed);
        if (answer && answer[0] == 1 && g_enabled.load(std::memory_order_relaxed) && VR_IsStereoEnabled() &&
            eye > 0.1f && eye < 3.0f)
        {
            g_reclaimerViewCameraGameBits.store(answer[1], std::memory_order_relaxed);
            uint32_t bits = 0;
            memcpy(&bits, &eye, sizeof(bits));
            answer[1] = bits;
            g_reclaimerViewCameraEyeBits.store(bits, std::memory_order_relaxed);
            g_reclaimerViewCameraGiven.fetch_add(1, std::memory_order_relaxed);
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
    }
    return result;
}

// The client executable's code section.
bool ReclaimerExeCode(uintptr_t& start, size_t& size)
{
    const auto exe = reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(exe);
    const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(exe + dos->e_lfanew);
    const auto* section = IMAGE_FIRST_SECTION(nt);
    for (unsigned i = 0; i < nt->FileHeader.NumberOfSections; ++i, ++section)
        if (memcmp(section->Name, ".text", 6) == 0)
        {
            start = exe + section->VirtualAddress;
            size = section->Misc.VirtualSize;
            return true;
        }
    return false;
}
} // namespace

// Install thread, once per process while Project Reclaimer's recompiled engine is in use. False with a reason logged.
bool InstallReclaimerVisrCamera()
{
    static bool tried = false;
    if (tried || !g_hostIsProjectReclaimer || !lifted::Active())
        return g_reclaimerViewCameraTarget != nullptr;
    tried = true;
    uintptr_t code = 0;
    size_t codeSize = 0;
    const char* why = nullptr;
    uintptr_t read = 0, function = 0;
    DWORD64 image = 0;
    const PRUNTIME_FUNCTION extent =
        !ReclaimerExeCode(code, codeSize)                                       ? nullptr
        : !(read = sig::Find(code, codeSize, kReclaimerViewCameraReads))       ? nullptr
                                                                               : RtlLookupFunctionEntry(read, &image,
                                                                                                        nullptr);
    if (!code)
        why = "the client's code section";
    else if (!read)
        why = "its VISR camera's reads of field of view, near and far";
    else if (sig::Find(read + 1, codeSize - (read + 1 - code), kReclaimerViewCameraReads))
        why = "a single VISR camera read (more than one matched)";
    else if (!extent || !(function = static_cast<uintptr_t>(image + extent->BeginAddress)))
        why = "the VISR camera's function extent";
    else if (!sig::Find(read, static_cast<size_t>(image + extent->EndAddress - read), kReclaimerViewCameraStores))
        why = "the VISR camera's answer (field of view, near, far)";
    if (why)
    {
        LOG("Project Reclaimer VISR: StockFallback: %s not found; VISR keeps the client's field of view", why);
        return false;
    }
    void* const target = reinterpret_cast<void*>(function);
    if (MH_CreateHook(target, reinterpret_cast<void*>(&ReclaimerViewCameraDetour),
                      reinterpret_cast<void**>(&g_reclaimerViewCameraOriginal)) != MH_OK ||
        MH_EnableHook(target) != MH_OK)
    {
        (void)MH_RemoveHook(target);
        g_reclaimerViewCameraOriginal = nullptr;
        LOG("Project Reclaimer VISR: StockFallback: its camera could not be hooked; VISR keeps the client's field of "
            "view");
        return false;
    }
    g_reclaimerViewCameraTarget = target;
    const auto exe = reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
    LOG("Project Reclaimer VISR: installed on the camera its VISR asks for (exe+0x%llX): VISR gets the headset eye's "
        "vertical field of view", static_cast<unsigned long long>(function - exe));

    // Its constants' width (see the header). Without it VISR keeps the vertical fix alone.
    const uintptr_t stores = sig::Find(code, codeSize, kReclaimerVisrConstantsStores);
    DWORD64 builderImage = 0;
    const PRUNTIME_FUNCTION builder = stores ? RtlLookupFunctionEntry(stores, &builderImage, nullptr) : nullptr;
    if (!stores || sig::Find(stores + 1, codeSize - (stores + 1 - code), kReclaimerVisrConstantsStores) || !builder)
    {
        LOG("Project Reclaimer VISR: StockFallback: its constants' builder %s; VISR's width stays the picture's",
            !stores ? "not found" : !builder ? "has no extent" : "matched more than once");
        return true;
    }
    void* const builderTarget = reinterpret_cast<void*>(static_cast<uintptr_t>(builderImage + builder->BeginAddress));
    if (MH_CreateHook(builderTarget, reinterpret_cast<void*>(&ReclaimerVisrConstantsDetour),
                      reinterpret_cast<void**>(&g_reclaimerVisrConstantsOriginal)) != MH_OK ||
        MH_EnableHook(builderTarget) != MH_OK)
    {
        (void)MH_RemoveHook(builderTarget);
        g_reclaimerVisrConstantsOriginal = nullptr;
        LOG("Project Reclaimer VISR: StockFallback: its constants' builder could not be hooked; VISR's width stays "
            "the picture's");
        return true;
    }
    g_reclaimerVisrConstantsTarget = builderTarget;
    LOG("Project Reclaimer VISR: installed on its constants' builder (exe+0x%llX): VISR's width becomes the eye's",
        static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(builderTarget) - exe));
    return true;
}

// Cold worker: once, when VISR first asked and was answered with the eye's field of view.
void ReportReclaimerVisrCamera()
{
    static bool reportedWidth = false;
    if (!reportedWidth && g_reclaimerVisrWidthsFixed.load(std::memory_order_relaxed))
    {
        reportedWidth = true;
        float was = 0.0f, now = 0.0f;
        const uint32_t wasBits = g_reclaimerVisrWidthWasBits.load(std::memory_order_relaxed);
        const uint32_t nowBits = g_reclaimerVisrWidthNowBits.load(std::memory_order_relaxed);
        memcpy(&was, &wasBits, sizeof(was));
        memcpy(&now, &nowBits, sizeof(now));
        LOG("Project Reclaimer VISR: its width set to the eye's: %.1f deg across instead of %.1f",
            2.0f * atanf(0.5f * now) * 57.29578f, 2.0f * atanf(0.5f * was) * 57.29578f);
    }
    static bool reported = false;
    if (reported || !g_reclaimerViewCameraGiven.load(std::memory_order_relaxed))
        return;
    reported = true;
    float game = 0.0f, eye = 0.0f;
    const uint32_t gameBits = g_reclaimerViewCameraGameBits.load(std::memory_order_relaxed);
    const uint32_t eyeBits = g_reclaimerViewCameraEyeBits.load(std::memory_order_relaxed);
    memcpy(&game, &gameBits, sizeof(game));
    memcpy(&eye, &eyeBits, sizeof(eye));
    LOG("Project Reclaimer VISR: on; its camera's field of view %.1f deg is given as the eye's %.1f deg",
        game * 57.29578f, eye * 57.29578f);
}
