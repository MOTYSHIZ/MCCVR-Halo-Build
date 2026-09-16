#include "../src/common/input_logic.h"
#include "../src/common/weapon_hand_logic.h"
#include "../src/dll/vr.h"
#include <cmath>
#include <cstdio>
#include <initializer_list>
#include <limits>

namespace
{
unsigned checks = 0, failures = 0;
void Check(bool value, const char* name)
{
    ++checks;
    if (!value) { ++failures; std::printf("FAIL: %s\n", name); }
}

void Sample(VrPadState& pad, bool enabled, bool touched, bool inputReady)
{
    const auto selected = ConsumeThumbrestDpad(enabled, touched, inputReady,
        pad.turnX, pad.turnY);
    pad.thumbrestDpad = selected.active;
    pad.dpadX = selected.x;
    pad.dpadY = selected.y;
}
}

int main()
{
    const float head[3]{}, diagonalInside[3]{0.2f, 0.2f, 0},
        diagonalOutside[3]{0.18f, 0.18f, 0.18f};
    const float nan = std::numeric_limits<float>::quiet_NaN();
    const float infinity = std::numeric_limits<float>::infinity();
    struct RadiusCase { float radius, distance; bool inside; };
    const RadiusCase radii[]{
        {0.30f, 0.299f, true}, {0.30f, 0.30f, false}, {0.30f, 0.301f, false},
        {0.10f, 0.099f, true}, {0.10f, 0.10f, false},
        {0.50f, 0.499f, true}, {0.50f, 0.50f, false},
        {-1.0f, 0.099f, true}, {-1.0f, 0.101f, false},
        {2.0f, 0.499f, true}, {2.0f, 0.501f, false},
        {nan, 0.299f, true}, {nan, 0.301f, false},
        {infinity, 0.299f, true}, {-infinity, 0.301f, false}};
    for (const auto& test : radii)
    {
        const float controller[3]{test.distance, 0, 0};
        Check(DpadHeadWithinRadius(head, controller, test.radius) == test.inside,
            "radius preserves strict boundary, limits and invalid-setting fallback");
    }
    Check(DpadHeadWithinRadius(head, diagonalInside, 0.30f) &&
        !DpadHeadWithinRadius(head, diagonalOutside, 0.30f), "head radius measures all three axes");
    Check(!DpadHeadWithinRadius(nullptr, head, 0.30f) &&
        !DpadHeadWithinRadius(head, nullptr, 0.30f), "missing pose cannot activate gesture");
    for (float invalid : {nan, infinity, -infinity})
    {
        const float badPose[3]{0, invalid, 0};
        Check(!DpadHeadWithinRadius(head, badPose, 0.30f) &&
            !DpadHeadWithinRadius(badPose, head, 0.30f), "invalid tracked pose stays inactive");
    }

    VrPadState defaults{};
    Check(!defaults.thumbrestDpad && defaults.dpadX == 0 && defaults.dpadY == 0,
        "existing default pad has no optional gesture");
    for (unsigned flags = 0; flags < 8; ++flags)
    {
        VrPadState pad{};
        pad.valid = true; pad.moveX = -0.7f; pad.moveY = 0.8f;
        pad.turnX = 0.9f; pad.turnY = -0.8f; pad.clickL = pad.clickR = true;
        pad.trigL = 0.2f; pad.trigR = 0.9f; pad.menu = pad.a = true;
        Sample(pad, (flags & 1) != 0, (flags & 2) != 0, (flags & 4) != 0);
        const bool active = flags == 7;
        Check(pad.thumbrestDpad == active, "option, touch and focused ready input are all required");
        Check(pad.turnX == (active ? 0.0f : 0.9f) && pad.turnY == (active ? 0.0f : -0.8f),
            "only selected physical right axes are consumed");
        Check(pad.dpadX == (active ? 0.9f : 0.0f) && pad.dpadY == (active ? -0.8f : 0.0f),
            "inactive or unavailable touch cannot retain direction");
        Check(pad.moveX == -0.7f && pad.moveY == 0.8f && pad.clickL && pad.clickR &&
            pad.menu && pad.a && pad.trigL == 0.2f && pad.trigR == 0.9f,
            "left movement, head-gesture clicks and other actions are preserved");
    }

    for (float x : {-0.6f, -0.5f, 0.0f, 0.5f, 0.6f})
        for (float y : {-0.6f, -0.5f, 0.0f, 0.5f, 0.6f})
        {
            const uint16_t expectedX = x == -0.6f ? 0x4 : x == 0.6f ? 0x8 : 0;
            const uint16_t expectedY = y == -0.6f ? 0x2 : y == 0.6f ? 0x1 : 0;
            Check(DpadDirectionButtons(x, y) == (expectedX | expectedY),
                "cardinals, diagonals and strict half-stick deadzone");
        }
    for (float invalid : {nan, infinity, -infinity})
    {
        Check(DpadDirectionButtons(invalid, invalid) == 0 &&
            DpadDirectionButtons(invalid, -0.8f) == 0x2,
            "nonfinite direction axes cannot fabricate buttons or suppress a valid axis");
        VrPadState pad{}; pad.turnX = invalid; pad.turnY = 2.0f;
        Sample(pad, true, true, true);
        Check(pad.thumbrestDpad && pad.dpadX == 0 && pad.dpadY == 1 &&
            pad.turnX == 0 && pad.turnY == 0 && DpadDirectionButtons(pad.dpadX, pad.dpadY) == 0x1,
            "invalid axis is neutral and finite out-of-range axis clamps before publication");
    }
    for (bool leftHanded : {false, true})
    {
        Check(PhysicalHandForWeaponSlot(0, leftHanded) == (leftHanded ? 0 : 1),
            "representative weapon roles differ with handedness");
        VrPadState pad{}; pad.moveX = -0.75f; pad.turnX = 0.8f;
        Sample(pad, true, true, true);
        Check(pad.moveX == -0.75f && pad.turnX == 0 && pad.dpadX == 0.8f,
            "D-pad selects physical right stick independently of weapon role");
        pad.turnX = -0.9f; pad.turnY = 0.7f; // next actual action sample
        Sample(pad, true, false, true);
        Check(!pad.thumbrestDpad && pad.dpadX == 0 && pad.dpadY == 0 &&
            pad.turnX == -0.9f && pad.turnY == 0.7f,
            "touch release clears D-pad and restores next right-stick turn/zoom sample");
    }
    std::printf("D-pad input: %u checks, %u failures\n", checks, failures);
    return failures ? 1 : 0;
}
