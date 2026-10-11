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
    // turns about, and its own distance behind that point in world units. A game that cannot say leaves
    // restValid false, and such a camera then stands behind the vehicle's origin at a distance of the mod's own.
    bool restValid = false;
    float rest[3]{};
    float restDistance = 0.0f;
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

// The camera's point in the world. False (and `out` untouched) when no chase camera is selected or an input is
// not usable: the caller keeps the view it has without this.
inline bool Place(const vehicle_camera_select::ActiveCamera& camera, const Inputs& in, State& state,
                  float out[3]) noexcept
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
        // The game's own distance lies behind its own turning point, along whatever carries this camera's
        // offset, so the camera rests where the game's does and does not swing with the aim as the game's does.
        if (in.restValid && Finite3(in.rest) && std::isfinite(in.restDistance) && in.restDistance > 0.0f)
        {
            origin = in.rest;
            offsetCm[0] -= in.restDistance / vehicle_camera_frame::kWorldUnitsPerCm;
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
    return true;
}
} // namespace vehicle_camera_place
