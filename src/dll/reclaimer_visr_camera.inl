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
// FOUND BY ITS SHAPE (both 0.10.0 and 0.10.4 checked offline, unique in each): the camera's three reads,
// mov e?x,[r?x+0x28]; mov e?x,[r?x+0x64]; mov e?x,[r?x+0x68]; test e?x,e?x, followed by the stores into the answer,
// mov [r?x+4]; mov [r?x+8]; mov [r?x+0xC]. The hook goes on the function whose extent holds both. Not found, or more
// than once: StockFallback, said once, and VISR stays as the client draws it.

namespace
{
constexpr char kReclaimerViewCameraReads[] = "8B ?? 28 8B ?? 64 8B ?? 68 85";
constexpr char kReclaimerViewCameraStores[] = "89 ?? 04 89 ?? 08 89 ?? 0C";

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
    return true;
}

// Cold worker: once, when VISR first asked and was answered with the eye's field of view.
void ReportReclaimerVisrCamera()
{
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
