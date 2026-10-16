#pragma once
// WHERE THE CAMERA IN USE IS, THIS FRAME: the selected camera (vehicle_camera_select.h) laid onto the vehicle a
// game reports (vehicle_camera_frame_logic.h has the geometry and the space). What a game supplies is Inputs;
// what it gets back is the world point its seat view is to be seen from, before the player's own head movement.
//
// Pure: no engine, no clock, no allocation. State is one camera hook's own.

#include <cmath>
#include <cstdint>

#include "vehicle_camera_frame_logic.h"
#include "vehicle_camera_select.h"

namespace vehicle_camera_place
{
struct Inputs
{
    // The vehicle as it is DRAWN this frame: a camera placed from a frame the picture is not drawn from shakes
    // against the vehicle at speed.
    float vehicleOrigin[3]{};
    vehicle_camera_frame::Basis vehicle{};
    // The seat's own point and the seated character's head, where the game has them.
    bool seatValid = false;
    float seat[3]{};
    bool headValid = false;
    float head[3]{};
    // Where the game's own third-person camera rests for this seat ("origin": "thirdperson"): the point it
    // turns about, its own distance from that point in world units, and how much of that distance is height
    // above the point (a game's chase camera looking level usually stands above the point it turns about; the
    // rest of the distance is behind). A game that cannot say leaves restValid false, and such a camera then
    // stands behind the vehicle's origin at a distance of the mod's own.
    bool restValid = false;
    float rest[3]{};
    float restDistance = 0.0f;
    float restRise = 0.0f;
    // The heading the player's room faces (the stick turns it; his head does not): what an orbiting camera
    // ("locationTracking": "view") rides.
    float viewYaw = 0.0f;
};

// The mod's own stand-in for a third-person rest place the game could not give: behind and above the
// vehicle's origin, in cm.
inline constexpr float kStandInBehindCm = 450.0f;
inline constexpr float kStandInAboveCm = 180.0f;

struct State
{
    bool latched = false;
    uint32_t changes = 0;
    // The world heading an offset keeps while its mode does not follow the vehicle's yaw: the vehicle's own at
    // the moment the camera or mode was chosen, so the camera starts where a following one would be.
    float freeYaw = 0.0f;
    // The last placement of a "thirdperson" camera used the stand-in above: for the caller's log.
    bool standIn = false;
};

inline bool Finite3(const float v[3]) noexcept
{
    return std::isfinite(v[0]) && std::isfinite(v[1]) && std::isfinite(v[2]);
}

// What else a placement settled, for a caller that needs it: the point the camera is measured from (its origin,
// as this frame resolved it -- a camera pulled in from a wall moves toward it), the frame its offset was laid out
// in, and the offset as laid out, in cm along that frame's forward, right and up (a "thirdperson" camera's
// includes the game's own distance behind).
struct Placed
{
    float pivot[3]{};
    vehicle_camera_frame::Basis carrier{};
    float offsetCm[3]{};
};

// The camera's point in the world. False (and `out` untouched) when no chase camera is selected or an input is
// not usable: the caller keeps the view it has without this.
inline bool Place(const vehicle_camera_select::ActiveCamera& camera, const Inputs& in, State& state,
                  float out[3], Placed* placed = nullptr) noexcept
{
    using vehicle_camera_file::Origin;
    if (!camera.valid || !camera.chase || !vehicle_camera_frame::Usable(in.vehicle) || !Finite3(in.vehicleOrigin) ||
        !std::isfinite(in.viewYaw))
        return false;

    const bool seatUsable = in.seatValid && Finite3(in.seat);
    const bool headUsable = in.headValid && Finite3(in.head);
    const float* origin = in.vehicleOrigin;
    float offsetCm[3]{camera.offsetCm[0], camera.offsetCm[1], camera.offsetCm[2]};
    bool standIn = false;
    switch (static_cast<Origin>(camera.origin))
    {
    case Origin::ThirdPerson:
        // The game's own distance from its own turning point, laid along whatever carries this camera's offset:
        // its height above the point, and the rest of it behind. So the camera rests where the game's does when
        // it looks level, and does not swing with the aim as the game's does.
        if (in.restValid && Finite3(in.rest) && std::isfinite(in.restDistance) && in.restDistance > 0.0f)
        {
            float rise = std::isfinite(in.restRise) ? in.restRise : 0.0f;
            rise = rise > in.restDistance ? in.restDistance : (rise < -in.restDistance ? -in.restDistance : rise);
            const float back = std::sqrt(in.restDistance * in.restDistance - rise * rise);
            origin = in.rest;
            offsetCm[0] -= back / vehicle_camera_frame::kWorldUnitsPerCm;
            offsetCm[2] += rise / vehicle_camera_frame::kWorldUnitsPerCm;
        }
        else
        {
            offsetCm[0] -= kStandInBehindCm;
            offsetCm[2] += kStandInAboveCm;
            standIn = true;
        }
        break;
    case Origin::Seat:
        if (seatUsable)
            origin = in.seat;
        break;
    case Origin::Head:
        if (headUsable)
            origin = in.head;
        else if (seatUsable)
            origin = in.seat;
        break;
    default: // the vehicle, or a marker the game has not resolved
        break;
    }

    if (!state.latched || state.changes != camera.changes)
    {
        state.freeYaw = vehicle_camera_frame::Heading(in.vehicle);
        state.changes = camera.changes;
        state.latched = true;
    }
    const vehicle_camera_frame::Basis carrier =
        camera.locView ? vehicle_camera_frame::FromAngles(in.viewYaw, 0.0f, 0.0f)
                       : vehicle_camera_frame::Tracked(in.vehicle, camera.locYaw, camera.locPitch, camera.locRoll,
                                                       state.freeYaw);
    float offset[3];
    vehicle_camera_frame::OffsetToWorld(carrier, offsetCm, 1.0f, offset);
    const float point[3]{origin[0] + offset[0], origin[1] + offset[1], origin[2] + offset[2]};
    if (!Finite3(point))
        return false;
    state.standIn = standIn;
    out[0] = point[0];
    out[1] = point[1];
    out[2] = point[2];
    if (placed)
    {
        for (int axis = 0; axis < 3; ++axis)
        {
            placed->pivot[axis] = origin[axis];
            placed->offsetCm[axis] = offsetCm[axis];
        }
        placed->carrier = carrier;
    }
    return true;
}

// THE LEASH on the player's head (the camera's "leashMin" / "leashMax", vehicle_camera_frame::LeashToBasis):
// `head` is how far, in world units, the head has moved the view from the camera's point; it comes back with
// whatever crosses a limit taken off, measured along the frame the camera's offset is laid out in. `reach` is how
// much of the offset the camera stands at (1, unless a wall holds it in). False (and `head` unchanged) when the
// camera sets no leash.
inline bool LeashHead(const vehicle_camera_select::ActiveCamera& camera, const Placed& placed, float reach,
                      float head[3]) noexcept
{
    if (!vehicle_camera_file::LeashSet(camera.leashMinCm) && !vehicle_camera_file::LeashSet(camera.leashMaxCm))
        return false;
    if (!Finite3(head) || !vehicle_camera_frame::Usable(placed.carrier))
        return false;
    const float at = std::isfinite(reach) ? reach : 1.0f;
    const float cameraCm[3]{placed.offsetCm[0] * at, placed.offsetCm[1] * at, placed.offsetCm[2] * at};
    const vehicle_camera_frame::LeashBox box =
        vehicle_camera_frame::LeashToBasis(camera.leashMinCm, camera.leashMaxCm, cameraCm);
    float shift[3];
    vehicle_camera_frame::LeashShift(head, placed.carrier, box, shift);
    for (int axis = 0; axis < 3; ++axis)
        head[axis] -= shift[axis];
    return true;
}

// HELD SHORT OF A WALL ("collide"): how much of the way from the pivot to the camera's place it may go, from a
// trace along that line. `rangeWu` is where the trace met something (meaningful only with `hit`), `lengthWu` the
// whole way, `marginCm` how far short to stop. 1 = all the way.
inline float CollideReach(bool hit, float rangeWu, float lengthWu, float marginCm) noexcept
{
    if (!hit || !std::isfinite(rangeWu) || !std::isfinite(lengthWu) || !(lengthWu > 0.0f) || rangeWu >= lengthWu)
        return 1.0f;
    const float margin = std::isfinite(marginCm) && marginCm > 0.0f ? marginCm * vehicle_camera_frame::kWorldUnitsPerCm
                                                                      : 0.0f;
    const float allowed = rangeWu - margin;
    return allowed > 0.0f ? allowed / lengthWu : 0.0f;
}

// ...eased: drawn in at once, let out at `releasePerSecond` of the whole way a second, so a gap between two walls
// does not throw the camera out and back. `held` is the caller's, 1 to start.
inline float EaseReach(float& held, float wanted, float seconds, float releasePerSecond) noexcept
{
    if (!std::isfinite(wanted))
        wanted = 1.0f;
    wanted = wanted < 0.0f ? 0.0f : (wanted > 1.0f ? 1.0f : wanted);
    if (!std::isfinite(held))
        held = 1.0f;
    if (wanted <= held)
        held = wanted;
    else
    {
        const float step = std::isfinite(seconds) && seconds > 0.0f ? seconds * releasePerSecond : 0.0f;
        held = held + step >= wanted ? wanted : held + step;
    }
    return held;
}

// LAG ("lag", "lagDamping"): the camera follows its place on a spring and damper instead of being carried there
// rigidly. A camera on a long boom is swung through a wide arc by every turn and bump of the vehicle; the spring
// lets those swings through slowly. It damps against the place's own velocity, so at a steady speed it sits on its
// place and only a change of motion lags -- but against that velocity SMOOTHED over the lag's seconds and held to
// a vehicle's speed: steered by the raw one, a bump's jolt reaches the camera within a twentieth of a second, and a
// place that jumps in one frame (a speed no vehicle has) throws it past. World units.
struct LagState
{
    bool valid = false;
    uint32_t changes = 0; // the camera the state belongs to (ActiveCamera::changes)
    float pos[3]{};
    float vel[3]{};
    float place[3]{};    // the place at the last step, for its velocity
    float placeVel[3]{}; // the place's velocity, smoothed
};
inline constexpr float kLagStiffness = 3.0f;      // the spring's frequency is this over the lag's seconds
inline constexpr float kLagMaxPlaceSpeedWu = 20.0f; // about 60 m/s: past any vehicle, or a boom end in a hard turn
inline constexpr float kLagSubstepSeconds = 0.004f;
inline constexpr float kLagMaxStepSeconds = 0.1f; // a longer frame is stepped as this long
inline constexpr float kLagRestartSeconds = 0.5f; // a longer gap starts over on the place
inline constexpr float kLagRestartWu = 3.0f;      // so does a place this far from the camera (about 9 m)

// The camera's point this frame, in `out`. `seconds` since the last call: 0 (the same frame again) moves nothing.
// A camera without lag, a different camera, a long gap or a far jump puts the point on its place.
inline void FollowLag(const vehicle_camera_select::ActiveCamera& camera, const float place[3], float seconds,
                      LagState& state, float out[3]) noexcept
{
    const auto restart = [&]() {
        state.valid = Finite3(place);
        state.changes = camera.changes;
        for (int axis = 0; axis < 3; ++axis)
        {
            state.pos[axis] = place[axis];
            state.vel[axis] = 0.0f;
            state.place[axis] = place[axis];
            state.placeVel[axis] = 0.0f;
            out[axis] = place[axis];
        }
    };
    const float lag = camera.lagSeconds;
    if (!(lag > 0.0f) || !std::isfinite(lag) || !Finite3(place) || !state.valid || state.changes != camera.changes ||
        !std::isfinite(seconds) || seconds < 0.0f || seconds > kLagRestartSeconds)
    {
        restart();
        if (!(lag > 0.0f))
            state.valid = false;
        return;
    }
    float apart = 0.0f;
    for (int axis = 0; axis < 3; ++axis)
        apart += (place[axis] - state.pos[axis]) * (place[axis] - state.pos[axis]);
    if (!(apart <= kLagRestartWu * kLagRestartWu))
    {
        restart();
        return;
    }
    if (seconds > 0.0f)
    {
        const float step = seconds > kLagMaxStepSeconds ? kLagMaxStepSeconds : seconds;
        const float w = kLagStiffness / lag;
        float zeta = camera.lagDamping;
        if (!std::isfinite(zeta))
            zeta = 1.0f;
        zeta = zeta < vehicle_camera_file::kMinLagDamping ? vehicle_camera_file::kMinLagDamping
             : zeta > vehicle_camera_file::kMaxLagDamping ? vehicle_camera_file::kMaxLagDamping
                                                          : zeta;
        int count = static_cast<int>(step / kLagSubstepSeconds) + 1;
        const float h = step / static_cast<float>(count);
        // The place's velocity this frame, held to a vehicle's speed, folded into the smoothed one.
        float raw[3];
        float rawSpeed = 0.0f;
        for (int axis = 0; axis < 3; ++axis)
        {
            raw[axis] = (place[axis] - state.place[axis]) / seconds;
            rawSpeed += raw[axis] * raw[axis];
        }
        rawSpeed = std::sqrt(rawSpeed);
        const float hold = rawSpeed > kLagMaxPlaceSpeedWu ? kLagMaxPlaceSpeedWu / rawSpeed : 1.0f;
        const float blend = 1.0f - std::exp(-seconds / lag);
        for (int axis = 0; axis < 3; ++axis)
            state.placeVel[axis] += (raw[axis] * hold - state.placeVel[axis]) * blend;
        // The place moves in a straight line across the frame; each substep pulls toward where it is at its start.
        for (int i = 0; i < count; ++i)
        {
            const float t = h * static_cast<float>(i) / step; // 0..1 across the stepped part of the frame
            for (int axis = 0; axis < 3; ++axis)
            {
                const float at = state.place[axis] + (place[axis] - state.place[axis]) * t;
                const float accel = w * w * (at - state.pos[axis]) +
                                    2.0f * zeta * w * (state.placeVel[axis] - state.vel[axis]);
                state.vel[axis] += accel * h;
                state.pos[axis] += state.vel[axis] * h;
            }
        }
        if (!Finite3(state.pos) || !Finite3(state.vel))
        {
            restart();
            return;
        }
        for (int axis = 0; axis < 3; ++axis)
            state.place[axis] = place[axis];
    }
    for (int axis = 0; axis < 3; ++axis)
        out[axis] = state.pos[axis];
}
} // namespace vehicle_camera_place
