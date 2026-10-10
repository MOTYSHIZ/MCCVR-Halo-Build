// Where the camera in use is (src/common/vehicle_camera_place_logic.h): the origin a camera is measured from,
// which of the vehicle's rotations carry its offset, an offset that keeps a world heading, and an orbiting one.
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include "../src/common/vehicle_camera_place_logic.h"

using namespace vehicle_camera_place;
using vehicle_camera_file::Origin;
using vehicle_camera_frame::FromAngles;
using vehicle_camera_frame::kWorldUnitsPerCm;
using vehicle_camera_select::ActiveCamera;

static unsigned g_checks = 0;
static void Check(bool condition, const char* message)
{
    ++g_checks;
    if (!condition)
    {
        std::fprintf(stderr, "FAIL: %s\n", message);
        std::exit(1);
    }
}

static bool Near(float a, float b)
{
    return std::fabs(a - b) <= 1.0e-4f;
}

static bool Near3(const float v[3], float x, float y, float z)
{
    return Near(v[0], x) && Near(v[1], y) && Near(v[2], z);
}

constexpr float kHalfTurn = 3.14159265358979323846f;
constexpr float kQuarterTurn = kHalfTurn / 2.0f;

// A chase camera 300 cm behind, 60 cm to the right and 150 cm up, rigid to the vehicle.
static ActiveCamera Behind()
{
    ActiveCamera camera;
    camera.valid = true;
    camera.chase = true;
    camera.origin = static_cast<uint8_t>(Origin::Vehicle);
    camera.offsetCm[0] = -300.0f;
    camera.offsetCm[1] = 60.0f;
    camera.offsetCm[2] = 150.0f;
    camera.locYaw = camera.locPitch = camera.locRoll = true;
    camera.changes = 1;
    return camera;
}

static Inputs At(float x, float y, float z, float yaw, float pitch, float roll)
{
    Inputs in;
    in.vehicleOrigin[0] = x;
    in.vehicleOrigin[1] = y;
    in.vehicleOrigin[2] = z;
    in.vehicle = FromAngles(yaw, pitch, roll);
    return in;
}

static void TestRigid()
{
    const float back = 300.0f * kWorldUnitsPerCm, right = 60.0f * kWorldUnitsPerCm, up = 150.0f * kWorldUnitsPerCm;
    State state;
    float point[3]{};

    Check(Place(Behind(), At(10.0f, 20.0f, 3.0f, 0.0f, 0.0f, 0.0f), state, point), "a chase camera on a level vehicle");
    Check(Near3(point, 10.0f - back, 20.0f - right, 3.0f + up),
          "sits behind, to the right (minus left) and above the vehicle's origin, in world units");

    // The vehicle faces +Y: behind is -Y, right is +X.
    Check(Place(Behind(), At(10.0f, 20.0f, 3.0f, kQuarterTurn, 0.0f, 0.0f), state, point), "the vehicle turned left");
    Check(Near3(point, 10.0f + right, 20.0f - back, 3.0f + up), "carries a rigid offset round with it");

    // Nose straight up: behind is below, up is back along -X.
    Check(Place(Behind(), At(0.0f, 0.0f, 0.0f, 0.0f, kQuarterTurn, 0.0f), state, point), "the vehicle nose up");
    Check(Near3(point, -up, -right, -back), "pitches a rigid offset with it");
}

static void TestYawOnlyStaysLevel()
{
    ActiveCamera camera = Behind();
    camera.locPitch = camera.locRoll = false;
    State state;
    float point[3]{};
    Check(Place(camera, At(0.0f, 0.0f, 0.0f, kQuarterTurn, 0.5f, -0.4f), state, point), "a yaw-only offset on a tilted vehicle");
    Check(Near3(point, 60.0f * kWorldUnitsPerCm, -300.0f * kWorldUnitsPerCm, 150.0f * kWorldUnitsPerCm),
          "turns with the vehicle and stays level");
}

static void TestWorldHeadingIsKept()
{
    ActiveCamera camera = Behind();
    camera.offsetCm[1] = 0.0f;
    camera.offsetCm[2] = 0.0f;
    camera.locYaw = camera.locPitch = camera.locRoll = false;
    const float back = 300.0f * kWorldUnitsPerCm;
    State state;
    float point[3]{};

    Check(Place(camera, At(0.0f, 0.0f, 0.0f, kQuarterTurn, 0.0f, 0.0f), state, point), "an offset that follows no rotation");
    Check(Near3(point, 0.0f, -back, 0.0f), "starts behind the vehicle as it faces when the camera is chosen");
    Check(Place(camera, At(5.0f, 0.0f, 0.0f, kHalfTurn, 0.3f, 0.0f), state, point), "the vehicle turning and moving on");
    Check(Near3(point, 5.0f, -back, 0.0f), "moves the camera with the vehicle but keeps the offset's world heading");

    camera.changes = 2;
    Check(Place(camera, At(5.0f, 0.0f, 0.0f, kHalfTurn, 0.0f, 0.0f), state, point), "choosing a camera again");
    Check(Near3(point, 5.0f + back, 0.0f, 0.0f), "takes the vehicle's heading of that moment");
}

static void TestOrbit()
{
    ActiveCamera camera = Behind();
    camera.offsetCm[1] = 0.0f;
    camera.offsetCm[2] = 0.0f;
    camera.locView = true;
    const float back = 300.0f * kWorldUnitsPerCm;
    State state;
    float point[3]{};
    Inputs in = At(0.0f, 0.0f, 1.0f, 0.3f, 0.6f, 0.2f);
    in.viewYaw = kQuarterTurn;
    Check(Place(camera, in, state, point), "an orbiting camera");
    Check(Near3(point, 0.0f, -back, 1.0f), "sits behind the way the player faces, level, whatever the vehicle does");
    in.viewYaw = kHalfTurn;
    Check(Place(camera, in, state, point) && Near3(point, back, 0.0f, 1.0f), "and swings round as he turns");
}

static void TestOrigins()
{
    ActiveCamera camera = Behind();
    camera.offsetCm[0] = camera.offsetCm[1] = camera.offsetCm[2] = 0.0f;
    State state;
    float point[3]{};
    Inputs in = At(1.0f, 2.0f, 3.0f, 0.0f, 0.0f, 0.0f);
    in.seat[0] = 4.0f;
    in.seat[1] = 5.0f;
    in.seat[2] = 6.0f;
    in.head[0] = 7.0f;
    in.head[1] = 8.0f;
    in.head[2] = 9.0f;

    camera.origin = static_cast<uint8_t>(Origin::Seat);
    Check(Place(camera, in, state, point) && Near3(point, 1.0f, 2.0f, 3.0f), "a seat origin the game has no point for falls back to the vehicle");
    in.seatValid = true;
    Check(Place(camera, in, state, point) && Near3(point, 4.0f, 5.0f, 6.0f), "a seat origin is the seat's point");

    camera.origin = static_cast<uint8_t>(Origin::Head);
    Check(Place(camera, in, state, point) && Near3(point, 4.0f, 5.0f, 6.0f), "a head origin with no head falls back to the seat");
    in.headValid = true;
    Check(Place(camera, in, state, point) && Near3(point, 7.0f, 8.0f, 9.0f), "a head origin is the head");

    camera.origin = static_cast<uint8_t>(Origin::Marker);
    Check(Place(camera, in, state, point) && Near3(point, 1.0f, 2.0f, 3.0f), "a marker the game has not resolved is the vehicle's origin");
}

static void TestRefusals()
{
    State state;
    float point[3]{42.0f, 42.0f, 42.0f};
    const Inputs good = At(0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);

    ActiveCamera none;
    Check(!Place(none, good, state, point), "no camera selected places nothing");
    ActiveCamera seatView = Behind();
    seatView.chase = false;
    Check(!Place(seatView, good, state, point), "the seat view places nothing: it is the game's and the mod's own");

    Inputs bad = good;
    bad.vehicle.up[2] = 0.0f;
    Check(!Place(Behind(), bad, state, point), "a frame that is not a frame places nothing");
    bad = good;
    bad.vehicleOrigin[1] = std::numeric_limits<float>::quiet_NaN();
    Check(!Place(Behind(), bad, state, point), "an origin that is not a number places nothing");
    bad = good;
    bad.viewYaw = std::numeric_limits<float>::infinity();
    Check(!Place(Behind(), bad, state, point), "a heading that is not a number places nothing");
    Check(point[0] == 42.0f && point[1] == 42.0f && point[2] == 42.0f, "and a refusal leaves the caller's point alone");
    Check(!state.latched, "and takes no heading");
}

int main()
{
    TestRigid();
    TestYawOnlyStaysLevel();
    TestWorldHeadingIsKept();
    TestOrbit();
    TestOrigins();
    TestRefusals();
    std::printf("PASS: %u vehicle camera placement checks\n", g_checks);
    return 0;
}
