// Which way the left stick walks: head, left controller or right controller (src/common/movement_direction_logic.h).
#include "../src/common/movement_direction_logic.h"
#include <cmath>
#include <cstdio>

using namespace movement_direction;

static int g_failed = 0;
#define CHECK(condition) \
    do { if (!(condition)) { ++g_failed; std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #condition); } } while (0)

static constexpr float kDeg = 3.14159265f / 180.0f;
static bool Near(float a, float b) { return std::fabs(a - b) < 0.001f; }

// An OpenXR orientation turned `rightDeg` to the right (about +Y), then pitched `upDeg` up (about its own +X).
struct Q { float v[4]; };
static Q Pose(float rightDeg, float upDeg = 0.0f)
{
    const float yaw = -rightDeg * kDeg * 0.5f, pitch = upDeg * kDeg * 0.5f;
    const float yw = std::cos(yaw), yy = std::sin(yaw), pw = std::cos(pitch), px = std::sin(pitch);
    // (0, yy, 0, yw) * (px, 0, 0, pw)
    return {{yw * px, yy * pw, -yy * px, yw * pw}};
}

static void HeadingIsMeasuredToTheRight()
{
    float yaw = 0.0f;
    CHECK(ForwardYaw(Pose(0).v, yaw) && Near(yaw, 0.0f));
    CHECK(ForwardYaw(Pose(30).v, yaw) && Near(yaw, 30 * kDeg));
    CHECK(ForwardYaw(Pose(-90).v, yaw) && Near(yaw, -90 * kDeg));
    // Pitch does not change the heading until it is too steep to have one.
    CHECK(ForwardYaw(Pose(40, -45).v, yaw) && Near(yaw, 40 * kDeg));
    CHECK(!ForwardYaw(Pose(40, -85).v, yaw));
    CHECK(!ForwardYaw(Pose(0, 88).v, yaw));
}

static void HeadSourceAddsNothing()
{
    Tracker tracker;
    CHECK(tracker.Offset(Head, true, Pose(10).v, true, Pose(70).v, 100) == 0.0f);
    CHECK(tracker.Offset(7, true, Pose(10).v, true, Pose(70).v, 100) == 0.0f); // unknown value = head
    CHECK(Clamp(-1) == Head && Clamp(3) == Head && Clamp(LeftHand) == LeftHand && Clamp(RightHand) == RightHand);
}

static void HandSourceTurnsTheHeadingToTheHand()
{
    Tracker tracker;
    CHECK(Near(tracker.Offset(RightHand, true, Pose(0).v, true, Pose(30).v, 100), 30 * kDeg));
    CHECK(Near(tracker.Offset(RightHand, true, Pose(90).v, true, Pose(0).v, 110), -90 * kDeg));
    // Across the back: head 170 right, hand 170 left is 20 degrees further right, not 340 left.
    CHECK(Near(tracker.Offset(LeftHand, true, Pose(170).v, true, Pose(-170).v, 120), 20 * kDeg));
    // No head pose: nothing to turn from.
    CHECK(tracker.Offset(LeftHand, false, Pose(0).v, true, Pose(30).v, 130) == 0.0f);
}

static void PointingAtTheFloorKeepsTheLastHeading()
{
    Tracker tracker;
    CHECK(Near(tracker.Offset(LeftHand, true, Pose(0).v, true, Pose(45).v, 100), 45 * kDeg));
    // Hand pointed straight down for a long time while tracked: still walks the last way it pointed,
    // measured from where the head now looks.
    CHECK(Near(tracker.Offset(LeftHand, true, Pose(15).v, true, Pose(80, -89).v, 5000), 30 * kDeg));
    // Never had a heading: the head's.
    Tracker fresh;
    CHECK(fresh.Offset(LeftHand, true, Pose(0).v, true, Pose(80, -89).v, 100) == 0.0f);
}

static void ShortTrackingLossHoldsThenFallsBackToTheHead()
{
    Tracker tracker;
    CHECK(Near(tracker.Offset(RightHand, true, Pose(0).v, true, Pose(60).v, 1000), 60 * kDeg));
    CHECK(Near(tracker.Offset(RightHand, true, Pose(0).v, false, Pose(0).v, 1000 + Tracker::kLostHoldMs), 60 * kDeg));
    CHECK(tracker.Offset(RightHand, true, Pose(0).v, false, Pose(0).v, 1001 + Tracker::kLostHoldMs) == 0.0f);
    // Back in view: its new heading at once.
    CHECK(Near(tracker.Offset(RightHand, true, Pose(0).v, true, Pose(-20).v, 2000), -20 * kDeg));
}

static void ChangingTheSourceForgetsTheOldHand()
{
    Tracker tracker;
    CHECK(Near(tracker.Offset(RightHand, true, Pose(0).v, true, Pose(60).v, 100), 60 * kDeg));
    // Now the left hand, pointing at the floor: it has no heading of its own yet, so the head's.
    CHECK(tracker.Offset(LeftHand, true, Pose(0).v, true, Pose(0, -89).v, 110) == 0.0f);
}

// Halo CE turns the head by -offset about +Y (haloce_controls_logic.h HeadRelativeMovement): check that sign.
static void CeTurnCarriesTheHeadingByTheOffset()
{
    static constexpr float kOffsetsDeg[] = {-120.0f, -30.0f, 0.0f, 45.0f, 170.0f};
    for (float offsetDeg : kOffsetsDeg)
    {
        const float half = -0.5f * offsetDeg * kDeg;
        const float turn[4]{0.0f, std::sin(half), 0.0f, std::cos(half)};
        float yaw = 0.0f;
        CHECK(ForwardYaw(turn, yaw) && Near(WrapPi(yaw - offsetDeg * kDeg), 0.0f));
    }
}

int main()
{
    HeadingIsMeasuredToTheRight();
    HeadSourceAddsNothing();
    HandSourceTurnsTheHeadingToTheHand();
    PointingAtTheFloorKeepsTheLastHeading();
    ShortTrackingLossHoldsThenFallsBackToTheHead();
    ChangingTheSourceForgetsTheOldHand();
    CeTurnCarriesTheHeadingByTheOffset();
    if (g_failed) { std::printf("%d movement direction check(s) failed\n", g_failed); return 1; }
    std::printf("movement direction tests passed\n");
    return 0;
}
