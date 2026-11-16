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

} // namespace

// Install thread, once per process while Project Reclaimer's recompiled engine is in use. The finds are
// reclaimer_client_finds.h's, the same the offline check runs (tests/reclaimer_client_file_check.cpp). False with a
// reason logged when VISR's camera is not found; its constants are hooked only when found AND the two finds agree
// (the camera's one caller also builds the constants), else VISR keeps the vertical fix alone.
bool InstallReclaimerVisrCamera()
{
    static bool tried = false;
    if (tried || !g_hostIsProjectReclaimer || !lifted::Active())
        return g_reclaimerViewCameraTarget != nullptr;
    tried = true;
    reclaimer_client::Image image;
    if (!reclaimer_client::ClientImageInMemory(image))
    {
        LOG("Project Reclaimer VISR: StockFallback: the client executable's headers do not read; VISR keeps the "
            "client's field of view");
        return false;
    }
    const auto exe = reinterpret_cast<uintptr_t>(image.base);
    const reclaimer_client::Find camera = reclaimer_client::FindVisrCamera(image);
    if (camera.missing)
    {
        LOG("Project Reclaimer VISR: StockFallback: its camera: %s not found; VISR keeps the client's field of view",
            camera.missing);
        return false;
    }
    void* const target = reinterpret_cast<void*>(exe + camera.function);
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
    LOG("Project Reclaimer VISR: installed on the camera its VISR asks for (exe+0x%X): VISR gets the headset eye's "
        "vertical field of view", camera.function);

    // Its constants' width (see the header).
    const reclaimer_client::Find constants = reclaimer_client::FindVisrConstants(image);
    const char* disagree = constants.missing ? nullptr
                                             : reclaimer_client::VisrAgree(image, camera.function, constants.function);
    if (constants.missing || disagree)
    {
        LOG("Project Reclaimer VISR: StockFallback: its constants: %s %s; VISR's width stays the picture's",
            constants.missing ? constants.missing : disagree, constants.missing ? "not found" : "does not hold");
        return true;
    }
    void* const builderTarget = reinterpret_cast<void*>(exe + constants.function);
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
    LOG("Project Reclaimer VISR: installed on its constants' builder (exe+0x%X, built by the camera's one caller "
        "exe+0x%X too): VISR's width becomes the eye's", constants.function,
        reclaimer_client::CallingFunctions(image, camera.function, 1).empty()
            ? 0u
            : reclaimer_client::CallingFunctions(image, camera.function, 1)[0]);
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
