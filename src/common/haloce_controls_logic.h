#pragma once
#include "haloce_frame_context.h"
#include "halo3_vehicle_logic.h"
#include "vr_turn_mode.h"

namespace halo_ce
{
struct ControlAdmission
{
    bool hasControlledUnit{},onFoot{},nativeFirstPerson{},inputBlocked{true},
        lookBlocked{true},paused{},cinematic{},presentationBlocked{};
};
inline bool OnFootControls(const ControlAdmission& state) noexcept
{
    return state.hasControlledUnit&&state.onFoot&&state.nativeFirstPerson&&
        !state.inputBlocked&&!state.lookBlocked&&!state.paused&&!state.cinematic&&
        !state.presentationBlocked;
}

// H3's accepted snap latch is reused directly. Smooth turn has H3's 0.15
// deadzone, configured degree rate and capped high-resolution elapsed time.
// A native input call may repeat inside one XR serial: consume that sample
// once, keeping its elapsed interval intact for the next actual sample.
struct ControlTurnState
{
    uint32_t generation{};
    uint64_t space{},reference{},renderer{},serial{};
    double previousSeconds{};
    bool latched{true};

    float Step(const RenderContext& context,bool admitted,double nowSeconds,bool seated=false) noexcept
    {
        const auto& rig=context.tracking.controllers;
        if (!std::isfinite(nowSeconds)||nowSeconds<=0) return 0;
        const bool fresh=context.tracking.generation&&context.tracking.serial&&
            context.tracking.spaceEpoch&&context.referenceRevision;
        if (!fresh) { *this={};return 0; }
        if (generation!=context.tracking.generation||space!=context.tracking.spaceEpoch||
            reference!=context.referenceRevision||renderer!=context.rendererEpoch)
        {
            *this={};generation=context.tracking.generation;space=context.tracking.spaceEpoch;
            reference=context.referenceRevision;renderer=context.rendererEpoch;previousSeconds=nowSeconds;
        }
        if (!admitted||!rig.padValid||!std::isfinite(rig.turnX)||
            !std::isfinite(rig.turnSnapDeg)||!std::isfinite(rig.turnSmoothDegS))
        {
            if (!rig.padValid||!std::isfinite(rig.turnX)) latched=true;
            else Halo3ConsumeSnapTurn(false,rig.turnX,latched);
            previousSeconds=nowSeconds;serial=std::max(serial,context.tracking.serial);return 0;
        }
        if (serial>=context.tracking.serial) return 0;
        serial=context.tracking.serial;
        const float elapsed=static_cast<float>(std::clamp(nowSeconds-previousSeconds,0.0,0.1));
        previousSeconds=nowSeconds;
        constexpr float radians=0.0174532925199433f;
        const float x=std::clamp(rig.turnX,-1.0f,1.0f);
        if (UseSmoothVrTurn(rig.turnSmooth,rig.vehicleSmoothTurn,seated))
        {
            Halo3ConsumeSnapTurn(false,x,latched);
            return std::fabs(x)>0.15f?-x*std::clamp(rig.turnSmoothDegS,30.0f,360.0f)*radians*elapsed:0;
        }
        return Halo3ConsumeSnapTurn(true,x,latched)?
            -std::copysign(std::clamp(rig.turnSnapDeg,5.0f,90.0f)*radians,x):0;
    }
};

// Add actual hull yaw to the native following-camera angles. Never use the
// desired hand direction here: doing so would feed steering back into itself.
// First samples, seat changes, reference changes and gaps establish a new
// baseline without rotating the view. Follow-off still observes the hull so
// enabling follow cannot replay accumulated rotation.
struct VehicleTurnFollow
{
    uint32_t generation{},unit{},parent{};
    int16_t seat{-1};
    uint64_t space{},reference{},renderer{};
    double previousSeconds{};
    float yaw{};
    bool following{};
    float Step(const RenderContext& context,uint32_t nextUnit,uint32_t nextParent,
        int16_t nextSeat,float nextYaw,double nowSeconds) noexcept
    {
        if (!std::isfinite(nextYaw)||!std::isfinite(nowSeconds)||nowSeconds<=0||
            !context.tracking.generation||!context.tracking.spaceEpoch||
            !context.referenceRevision||!context.rendererEpoch||
            nextUnit==UINT32_MAX||!(nextUnit>>16)||nextParent==UINT32_MAX||
            !(nextParent>>16)||nextSeat<0)
        { *this={};return 0; }
        const bool follow=context.tracking.controllers.vehicleViewFollow;
        const bool continuous=generation==context.tracking.generation&&
            unit==nextUnit&&parent==nextParent&&seat==nextSeat&&
            space==context.tracking.spaceEpoch&&reference==context.referenceRevision&&
            renderer==context.rendererEpoch&&previousSeconds>0&&
            nowSeconds>=previousSeconds&&nowSeconds-previousSeconds<0.25&&following&&follow;
        const float delta=continuous?std::remainder(nextYaw-yaw,6.283185307179586f):0;
        generation=context.tracking.generation;unit=nextUnit;parent=nextParent;seat=nextSeat;
        space=context.tracking.spaceEpoch;reference=context.referenceRevision;
        renderer=context.rendererEpoch;previousSeconds=nowSeconds;yaw=nextYaw;following=follow;
        return delta;
    }
};

// Project both headings onto CE's independently established world-Z-up plane.
// Preserve stick magnitude while expressing head-relative input in the native
// body-camera heading. A vertical/invalid physical heading leaves the caller
// untouched; a native camera at its pitch pole uses the shared frame fallback.
inline bool HeadRelativeMovement(const RenderContext& context,float x,float y,
    float& outputX,float& outputY) noexcept
{
    if (!context.tracking.serial||!context.tracking.generation||!context.referenceRevision||
        !Valid(context.camera)||!Valid(context.reference.orientation)||
        !Valid(context.tracking.headOrientation)||!std::isfinite(x)||!std::isfinite(y)||
        std::fabs(x)>1.001f||std::fabs(y)>1.001f||
        context.tracking.generation!=context.reference.generation||
        context.tracking.spaceEpoch!=context.reference.spaceEpoch) return false;
    Camera frame{};Quat inverse{};
    if (!BuildTrackingFrame(context.camera,context.reference,frame,inverse)) return false;
    const Vec3 head=ToNative(frame,Rotate(Multiply(inverse,
        context.tracking.headOrientation),{0,0,-1}));
    Vec3 forward=frame.forward,heading{head.x,head.y,0};
    const float fLength=std::sqrt(Dot(forward,forward)),hLength=std::sqrt(Dot(heading,heading));
    if (!std::isfinite(fLength)||!std::isfinite(hLength)||fLength<0.001f||hLength<0.001f) return false;
    forward=forward*(1/fLength);heading=heading*(1/hLength);
    const Vec3 right=Cross(forward,{0,0,1}),headRight=Cross(heading,{0,0,1});
    const Vec3 movement=heading*y+headRight*x;
    const float candidateX=Dot(movement,right),candidateY=Dot(movement,forward);
    if (!std::isfinite(candidateX)||!std::isfinite(candidateY)) return false;
    outputX=candidateX;outputY=candidateY;return true;
}
}
