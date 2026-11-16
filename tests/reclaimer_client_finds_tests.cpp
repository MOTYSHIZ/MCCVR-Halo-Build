// src/common/reclaimer_client_finds.h on a made-up image: each find, its agreement check, and what it says when
// something is missing. The real executables are checked by tests/reclaimer_client_file_check.cpp.

#include "../src/common/reclaimer_client_finds.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <utility>
#include <vector>

static int g_failures = 0;
#define CHECK(x)                                                                                                       \
    do                                                                                                                 \
    {                                                                                                                  \
        if (!(x))                                                                                                      \
        {                                                                                                              \
            std::printf("FAILED line %d: %s\n", __LINE__, #x);                                                         \
            ++g_failures;                                                                                              \
        }                                                                                                              \
    } while (0)

using namespace reclaimer_client;

namespace
{
struct Fake
{
    std::vector<uint8_t> image = std::vector<uint8_t>(0x4000, 0xCC);
    std::vector<std::pair<uint32_t, uint32_t>> functions;

    void Put(uint32_t at, std::initializer_list<uint8_t> bytes)
    {
        std::copy(bytes.begin(), bytes.end(), image.begin() + at);
    }
    void Call(uint32_t at, uint32_t target)
    {
        image[at] = 0xE8;
        const int32_t rel = static_cast<int32_t>(target) - static_cast<int32_t>(at + 5);
        std::memcpy(&image[at + 1], &rel, 4);
    }
    void Function(uint32_t begin, uint32_t end) { functions.push_back({begin, end}); }

    static bool Extent(const void* context, uint32_t rva, uint32_t& begin, uint32_t& end)
    {
        for (const auto& f : static_cast<const Fake*>(context)->functions)
            if (rva >= f.first && rva < f.second)
            {
                begin = f.first;
                end = f.second;
                return true;
            }
        return false;
    }
    Image View() const
    {
        Image view;
        view.base = image.data();
        view.size = image.size();
        view.codeRva = 0x100;
        view.codeSize = 0x3E00;
        view.extent = &Extent;
        view.context = this;
        return view;
    }
};

// VISR's camera at 0x1000, its constants at 0x1400, their one caller at 0x1800.
Fake Visr()
{
    Fake f;
    f.Function(0x1000, 0x1100);
    f.Put(0x1040, {0x8B, 0x53, 0x28, 0x8B, 0x43, 0x64, 0x8B, 0x4B, 0x68, 0x85, 0xD2});
    f.Put(0x1060, {0x89, 0x56, 0x04, 0x89, 0x46, 0x08, 0x89, 0x4E, 0x0C});
    f.Function(0x1400, 0x1500);
    f.Put(0x1480, {0xF3, 0x41, 0x0F, 0x11, 0x5D, 0x40, 0xF3, 0x41, 0x0F, 0x11, 0x45, 0x44, 0x41, 0xC7, 0x45, 0x48, 0x00,
                   0x00, 0x80, 0x3F});
    f.Function(0x1800, 0x1900);
    f.Call(0x1810, 0x1000);
    f.Call(0x1850, 0x1400);
    return f;
}
} // namespace

int main()
{
    // Patterns.
    const std::vector<int> p = ParsePattern("8B ?? 28 ff");
    CHECK(p.size() == 4 && p[0] == 0x8B && p[1] == -1 && p[2] == 0x28 && p[3] == 0xFF);

    // VISR: both finds, and they agree.
    {
        const Fake f = Visr();
        const Image image = f.View();
        const Find camera = FindVisrCamera(image);
        CHECK(!camera.missing && camera.function == 0x1000 && camera.at == 0x1040);
        const Find constants = FindVisrConstants(image);
        CHECK(!constants.missing && constants.function == 0x1400);
        CHECK(VisrAgree(image, camera.function, constants.function) == nullptr);
        CHECK(CallingFunctions(image, 0x1000).size() == 1 && Calls(image, 0x1800, 0x1400));
    }
    // ... and each way they do not.
    {
        Fake f = Visr();
        f.Put(0x1060, {0x90, 0x90, 0x90});   // the answer's stores gone
        CHECK(FindVisrCamera(f.View()).missing != nullptr);
    }
    {
        Fake f = Visr();
        f.Function(0x2000, 0x2100);
        f.Put(0x2040, {0x8B, 0x53, 0x28, 0x8B, 0x43, 0x64, 0x8B, 0x4B, 0x68, 0x85, 0xD2});
        CHECK(FindVisrCamera(f.View()).missing != nullptr);   // two of them: neither is taken
    }
    {
        Fake f = Visr();
        f.Put(0x1850, {0x90, 0x90, 0x90, 0x90, 0x90});   // the caller no longer builds the constants
        CHECK(VisrAgree(f.View(), 0x1000, 0x1400) != nullptr);
    }
    {
        Fake f = Visr();
        f.Function(0x2400, 0x2500);
        f.Call(0x2410, 0x1000);   // a second caller of the camera
        CHECK(VisrAgree(f.View(), 0x1000, 0x1400) != nullptr);
    }
    {
        Fake f = Visr();
        f.functions.clear();   // no extents: nothing is hooked by a guess
        CHECK(FindVisrCamera(f.View()).missing != nullptr && FindVisrConstants(f.View()).missing != nullptr);
    }

    // The widget draw's crosshair gate: the draw at 0x2000 calls the gate 0x2400 and, after the class test, the
    // crosshair check 0x2600; a pass-through at 0x2800 calls the draw.
    {
        Fake f;
        f.Function(0x2000, 0x2200);
        f.Function(0x2400, 0x2480);
        f.Function(0x2600, 0x2680);
        f.Function(0x2800, 0x280F);
        f.Call(0x2050, 0x2400);
        f.Put(0x2055, {0x8B, 0x4C, 0x24, 0x34, 0x84, 0xC0, 0x74, 0x18, 0x66, 0x83, 0xFD, 0x02, 0x75, 0x12});
        f.Call(0x2063, 0x2600);
        f.Put(0x2800, {0x48, 0x83, 0xEC, 0x28});
        f.Call(0x2804, 0x2000);
        f.Put(0x2809, {0x90, 0x48, 0x83, 0xC4, 0x28, 0xC3});
        const Image image = f.View();
        const WidgetGate g = FindWidgetCrosshairGate(image);
        CHECK(!g.missing && g.widgetDraw == 0x2000 && g.gate == 0x2400 && g.crosshair == 0x2600);
        CHECK(PastPassThroughs(image, 0x2800) == 0x2000);
        const WidgetGate inDraw = FindWidgetCrosshairGate(image, 0x2000);
        CHECK(!inDraw.missing && inDraw.gate == 0x2400);
        CHECK(FindWidgetCrosshairGate(image, 0x2400).missing != nullptr);   // not the function holding it
        // The gate asked from somewhere else too: refused.
        Fake g2 = f;
        g2.Function(0x3800, 0x3880);
        g2.Call(0x3810, 0x2400);
        CHECK(FindWidgetCrosshairGate(g2.View()).missing != nullptr);
        // The crosshair check not at a function's start: refused.
        Fake g3 = f;
        g3.Call(0x2063, 0x2604);
        CHECK(FindWidgetCrosshairGate(g3.View()).missing != nullptr);
    }

    // The navpoint draw: a pass-through at 0x3000 to the version at 0x3100, which calls the draw at 0x3400.
    {
        Fake f;
        f.Function(0x3000, 0x300F);
        f.Put(0x3000, {0x48, 0x83, 0xEC, 0x28});
        f.Call(0x3004, 0x3100);
        f.Put(0x3009, {0x90, 0x48, 0x83, 0xC4, 0x28, 0xC3});
        f.Function(0x3100, 0x3180);
        f.Put(0x3113, {0x4C, 0x8B, 0x71, 0x08, 0x48, 0x8B, 0x59, 0x10, 0x4C, 0x89, 0xF1, 0x89, 0xDA});
        f.Call(0x3120, 0x3400);
        f.Put(0x3125, {0x84, 0xC0});
        f.Function(0x3400, 0x3600);
        f.Put(0x3480, {0x48, 0x8B, 0x05, 0x10, 0x20, 0x30, 0x40, 0x48, 0x85, 0xC0, 0x0F, 0x84, 0x09, 0x1F, 0x00, 0x00,
                       0x48, 0x89, 0x84, 0x24, 0x90, 0x00, 0x00, 0x00, 0x48, 0x6B, 0x00, 0x74});
        const Image image = f.View();
        const NavpointDraw fromThunk = FindNavpointDrawFromVersion(image, 0x3000);
        CHECK(!fromThunk.missing && fromThunk.version == 0x3100 && fromThunk.draw == 0x3400 &&
              fromThunk.countDisp == 0x3483);
        const NavpointDraw offline = FindNavpointDraw(image);
        CHECK(!offline.missing && offline.draw == 0x3400 && offline.version == 0x3100);
        // The count read gone: the draw is not taken.
        Fake g = f;
        g.Put(0x3498, {0x90, 0x90, 0x90, 0x90});
        CHECK(FindNavpointDrawFromVersion(g.View(), 0x3000).missing != nullptr);
        CHECK(FindNavpointDraw(g.View()).missing != nullptr);
        // A call into the middle of a function is no draw.
        Fake h = f;
        h.Call(0x3120, 0x3404);
        CHECK(FindNavpointDrawFromVersion(h.View(), 0x3100).missing != nullptr);
    }

    if (g_failures)
        return 1;
    std::printf("reclaimer client finds: all passed\n");
    return 0;
}
