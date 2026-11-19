// src/common/reclaimer_client_finds.h on a made-up image: each find, its agreement check, and what it says when
// something is missing. The real executables are checked by tests/reclaimer_client_file_check.cpp.

#include "../src/common/reclaimer_client_finds.h"

#include <algorithm>
#include <array>
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

    // VISR's view user: the wrapper at 0x1000 stores the player at 0x3F00, tests the enable at 0x3F10, addresses the
    // flags at 0x3F20 and sets the player back to -1; the object pass at 0x1400 reads the player and the flags.
    {
        Fake f;
        f.Function(0x1000, 0x1100);
        const auto rel = [](uint32_t target, uint32_t end) {
            const int32_t r = static_cast<int32_t>(target) - static_cast<int32_t>(end);
            return std::array<uint8_t, 4>{uint8_t(r), uint8_t(r >> 8), uint8_t(r >> 16), uint8_t(r >> 24)};
        };
        const auto put = [&](Fake& g, uint32_t at, std::initializer_list<uint8_t> head, uint32_t target,
                             std::initializer_list<uint8_t> tail = {}) {
            g.Put(at, head);
            const uint32_t dispAt = at + static_cast<uint32_t>(head.size());
            const auto r = rel(target, dispAt + 4 + static_cast<uint32_t>(tail.size()));
            std::copy(r.begin(), r.end(), g.image.begin() + dispAt);
            std::copy(tail.begin(), tail.end(), g.image.begin() + dispAt + 4);
        };
        put(f, 0x1010, {0x89, 0x05}, 0x3F00);
        f.Put(0x1016, {0x83, 0xF8, 0x03, 0x77, 0x17});
        put(f, 0x101B, {0x0F, 0xB6, 0x05}, 0x3F10);
        f.Put(0x1022, {0x84, 0xC0, 0x74, 0x0C});
        put(f, 0x1026, {0x48, 0x8D, 0x05}, 0x3F20);
        f.Put(0x102D, {0x41, 0xC6, 0x04, 0x07, 0x01});
        put(f, 0x1080, {0xC7, 0x05}, 0x3F00, {0xFF, 0xFF, 0xFF, 0xFF});
        f.Function(0x1400, 0x1500);
        put(f, 0x1410, {0x8B, 0x1D}, 0x3F00);
        f.Put(0x1416, {0x83, 0xFB, 0x03, 0x77, 0x1A});
        put(f, 0x1440, {0x4C, 0x8D, 0x35}, 0x3F20);
        const VisrViewUser u = FindVisrViewUser(f.View());
        CHECK(!u.missing && u.wrapper == 0x1000 && u.at == 0x1010 && u.user == 0x3F00 && u.enable == 0x3F10 &&
              u.flags == 0x3F20 && u.reader == 0x1400);
        // No reset to -1: refused.
        Fake g = f;
        g.Put(0x1086, {0x00, 0x00, 0x00, 0x00});
        CHECK(FindVisrViewUser(g.View()).missing != nullptr);
        // No object pass reading the same flags: refused.
        Fake h = f;
        put(h, 0x1440, {0x4C, 0x8D, 0x35}, 0x3F30);
        CHECK(FindVisrViewUser(h.View()).missing != nullptr);
        // A second wrapper of the same shape: refused.
        Fake k = f;
        k.Function(0x2000, 0x2100);
        std::copy(f.image.begin() + 0x1010, f.image.begin() + 0x1032, k.image.begin() + 0x2010);
        CHECK(FindVisrViewUser(k.View()).missing != nullptr);
        // The player global inside the code: refused.
        Fake m = f;
        put(m, 0x1010, {0x89, 0x05}, 0x1200);
        CHECK(FindVisrViewUser(m.View()).missing != nullptr);
        // 0.10.6's object pass: mov ecx,[USER]; cmp ecx,3; two stores; ja (the compiler's order): still the reader.
        Fake n = f;
        std::fill(n.image.begin() + 0x1410, n.image.begin() + 0x1420, 0xCC);
        put(n, 0x1410, {0x8B, 0x0D}, 0x3F00);
        n.Put(0x1416, {0x83, 0xF9, 0x03, 0x48, 0x89, 0xBD, 0xA8, 0x04, 0x00, 0x00, 0x48, 0x89, 0x8D, 0xE0, 0x04, 0x00,
                       0x00, 0x77, 0x1A});
        const VisrViewUser nu = FindVisrViewUser(n.View());
        CHECK(!nu.missing && nu.reader == 0x1400);
        // ... but a compare of another register is not this read's test.
        Fake o = n;
        o.Put(0x1416, {0x83, 0xFA, 0x03});
        CHECK(FindVisrViewUser(o.View()).missing != nullptr);
        // ... nor a load into r9 (REX.R) compared as ecx.
        Fake q = n;
        q.Put(0x140F, {0x44});
        CHECK(FindVisrViewUser(q.View()).missing != nullptr);
    }

    // Project Reclaimer 0.10.6's framed pass-through (exe+0x579E020 to exe+0x4EBF450, 2026-11-19).
    {
        const uint8_t framed[] = {0x55, 0x48, 0x83, 0xEC, 0x30, 0x48, 0x8D, 0x6C, 0x24, 0x30, 0x48, 0xC7, 0x45, 0xF8,
                                  0xFE, 0xFF, 0xFF, 0xFF, 0xE8, 0x19, 0x14, 0x72, 0xFF, 0x90, 0x48, 0x83, 0xC4, 0x30,
                                  0x5D, 0xC3, 0x66, 0x90};
        int64_t offset = 0;
        CHECK(halo3_player_tags::ThunkCallee(framed, sizeof(framed), offset) && 0x579E020 + offset == 0x4EBF450);
        uint8_t noPop[sizeof(framed)];
        std::memcpy(noPop, framed, sizeof(framed));
        noPop[28] = 0x90;   // the frame not undone: a real function
        CHECK(!halo3_player_tags::ThunkCallee(noPop, sizeof(noPop), offset));
        uint8_t otherFrame[sizeof(framed)];
        std::memcpy(otherFrame, framed, sizeof(framed));
        otherFrame[27] = 0x40;   // add rsp,0x40 after sub rsp,0x30
        CHECK(!halo3_player_tags::ThunkCallee(otherFrame, sizeof(otherFrame), offset));
        CHECK(!halo3_player_tags::ThunkCallee(framed, 20, offset));   // cut short
    }

    // 0.10.6's navpoint version: a framed pass-through at 0x3000 to the version 0x3100, which is the draw itself and
    // reads the count (no draw of its own); a second version 0x3400 reads the same count.
    {
        Fake f;
        f.Function(0x3000, 0x3020);
        f.Put(0x3000, {0x55, 0x48, 0x83, 0xEC, 0x30, 0x48, 0x8D, 0x6C, 0x24, 0x30, 0x48, 0xC7, 0x45, 0xF8, 0xFE, 0xFF,
                       0xFF, 0xFF});
        f.Call(0x3012, 0x3100);
        f.Put(0x3017, {0x90, 0x48, 0x83, 0xC4, 0x30, 0x5D, 0xC3});
        f.Function(0x3100, 0x3300);
        // mov rax,[rip+X]; test rax,rax; je; mov [rbp+0x50],rax; imul rax,[rax],0x74
        f.Put(0x3180, {0x48, 0x8B, 0x05, 0x00, 0x0B, 0x00, 0x00, 0x48, 0x85, 0xC0, 0x0F, 0x84, 0x09, 0x1F, 0x00, 0x00,
                       0x48, 0x89, 0x45, 0x50, 0x48, 0x6B, 0x00, 0x74});
        const Image image = f.View();
        const NavpointDraw v = FindNavpointDrawFromVersion(image, 0x3000);
        CHECK(!v.missing && v.versionIsDraw && v.version == 0x3100 && v.draw == 0x3100 && v.countDisp == 0x3183);
        const NavpointDraw offline = FindNavpointDraw(image);
        CHECK(!offline.missing && offline.versionIsDraw && offline.draw == 0x3100);
        // A second version reading another count global: no single count, refused offline.
        Fake g = f;
        g.Function(0x3400, 0x3420);
        std::copy(f.image.begin() + 0x3000, f.image.begin() + 0x3012, g.image.begin() + 0x3400);
        g.Call(0x3412, 0x3500);
        g.Put(0x3417, {0x90, 0x48, 0x83, 0xC4, 0x30, 0x5D, 0xC3});
        g.Function(0x3500, 0x3600);
        g.Put(0x3580, {0x48, 0x8B, 0x05, 0x00, 0x0C, 0x00, 0x00, 0x48, 0x85, 0xC0, 0x0F, 0x84, 0x09, 0x1F, 0x00, 0x00,
                       0x48, 0x6B, 0x00, 0x74});
        CHECK(FindNavpointDraw(g.View()).missing != nullptr);
        // The version with no count read and no call of a draw: refused.
        Fake h = f;
        h.Put(0x3194, {0x90, 0x90, 0x90, 0x90});
        CHECK(FindNavpointDrawFromVersion(h.View(), 0x3000).missing != nullptr);
    }

    if (g_failures)
        return 1;
    std::printf("reclaimer client finds: all passed\n");
    return 0;
}
