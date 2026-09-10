#pragma once

#include "halo2_render_logic.h"
#include <algorithm>
#include <cmath>

// H2 keeps a native body-yaw camera. A stick-selected target lets the view
// snap immediately while native RX brings the body to that same heading.
// No controller-aim feedback is used to choose an on-foot turn target.
struct Halo2SnapTurnState
{
    uint32_t generation=0;
    uint64_t serial=0;
    bool latched=false, pending=false;
    float targetYaw=0;

    static float Wrap(float yaw) noexcept
    { return std::remainder(yaw,6.28318530718f); }

    bool Update(uint32_t epoch, uint64_t frame, float engineYaw, bool ownsStick,
        bool snapMode, bool padValid, float x, float degrees) noexcept
    {
        if (!epoch || !frame || !std::isfinite(engineYaw)) return false;
        if (generation!=epoch)
        { *this={}; generation=epoch; targetYaw=engineYaw; }
        if (serial==frame) return false;
        serial=frame;
        if (!padValid || !std::isfinite(x)) { latched=true; return false; }
        if (std::fabs(x)<0.3f) latched=false;
        if (!ownsStick || !snapMode)
        { if (std::fabs(x)>0.6f) latched=true; return false; }
        if (latched || std::fabs(x)<=0.6f || !std::isfinite(degrees)) return false;
        latched=true;
        targetYaw=Wrap((pending ? targetYaw:engineYaw) -
            std::copysign(std::clamp(degrees,5.0f,90.0f)*0.0174532925199f,x));
        pending=true;
        return true;
    }

    float ViewOffset(float engineYaw) const noexcept
    { return pending && std::isfinite(engineYaw) ? Wrap(targetYaw-engineYaw):0; }
};

inline bool Halo2SnapReference(const float reference[4], float gameYawOffset,
    float output[4]) noexcept
{
    float normalized[4]{};
    if (!std::isfinite(gameYawOffset) || !Halo2NormalizeQuaternion(reference,normalized))
        return false;
    const float turn[4]{0,-std::sin(gameYawOffset*0.5f),0,std::cos(gameYawOffset*0.5f)};
    float candidate[4]{};
    Halo2MultiplyQuaternion(turn,normalized,candidate);
    return Halo2NormalizeQuaternion(candidate,output);
}
