#pragma once
#include <cstring>
#include "contact_melee_motion.h"
#include "halo4_render_logic.h"

// Use Halo 4's own carrier mapping, including its mirror and sign controls.
// Contact coordinates are body-relative metres. Current actor translation/yaw
// live only in transform, so locomotion never contributes to strike velocity.
// The caller supplies an independent physical controller, never two-hand aim.
inline bool Halo4PreparePhysicalContactFrame(
    const Halo4ControllerWorldPoseInput& physical, bool trackingValid,
    uint64_t serial, int64_t timeNs, uint64_t trackingEpoch,
    uint32_t generation, uint32_t unit, uint64_t shape,
    uint64_t settingsEpoch, contact_melee::Frame& output) noexcept
{
    output={};
    output.serial=serial;
    output.timeNs=timeNs;
    output.unit=unit;
    output.shape=shape;
    output.rigidMotion=true;
    if(!trackingValid || !serial || timeNs<=0 || !trackingEpoch ||
        !generation || unit==UINT32_MAX || !shape || !settingsEpoch ||
        !std::isfinite(physical.gameYawReference) ||
        !std::isfinite(physical.worldScale) || physical.worldScale<=0)
        return false;

    auto local=physical;
    local.gameYawReference=0;
    local.worldScale=1;
    for(float& value:local.bodyOrigin) value=0;
    // Mesh offsets belong to the current collider points. They are not a
    // moving controller origin; rotating a long gun should move its tip.
    local.forwardTrim=local.verticalTrim=local.lateralTrim=0;
    Halo4ControllerWorldPose controller{};
    if(!Halo4BuildControllerWorldPose(local,controller)) return false;
    auto& pose=output.controllerPose;
    pose.origin={controller.position[0],controller.position[1],controller.position[2]};
    for(unsigned axis=0;axis<3;++axis)
        pose.axis[axis]={controller.basis[axis*3],controller.basis[axis*3+1],
                        controller.basis[axis*3+2]};

    const float c=std::cos(physical.gameYawReference);
    const float s=std::sin(physical.gameYawReference);
    auto& transform=output.transform;
    transform.axis[0]={c,s,0};
    transform.axis[1]={-s,c,0};
    transform.axis[2]={0,0,1};
    transform.origin={physical.bodyOrigin[0],physical.bodyOrigin[1],physical.bodyOrigin[2]};
    transform.unitsPerMetre=physical.worldScale;
    if(!pose.Valid() || !transform.Valid()) return false;

    uint64_t epoch=(14695981039346656037ull^trackingEpoch)*1099511628211ull;
    epoch=(epoch^generation)*1099511628211ull;
    epoch=(epoch^settingsEpoch)*1099511628211ull;
    epoch=(epoch^uint64_t(physical.mirrored))*1099511628211ull;
    const float references[]{physical.trackingOriginPosition[0],
        physical.trackingOriginPosition[1],physical.trackingOriginPosition[2],
        physical.headYawReference,physical.yawSign,physical.pitchSign,
        physical.worldScale};
    for(float value:references)
    {
        uint32_t bits=0;
        std::memcpy(&bits,&value,sizeof(bits));
        epoch=(epoch^bits)*1099511628211ull;
    }
    output.referenceEpoch=epoch ? epoch : 1;
    return true;
}
