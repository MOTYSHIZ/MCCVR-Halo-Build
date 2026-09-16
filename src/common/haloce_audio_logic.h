#pragma once
#include "haloce_frame_context.h"
#include <array>
#include <cstddef>

namespace halo_ce
{
// E-CE-ORIENTATION-2: sound_manager builds this packet before its backend
// callback. Preserve native position, velocity, environment and matrix origin.
struct AudioListenerPacket
{
    Vec3 position,forward,up,velocity;
    std::array<std::byte,0x18> environment;
    float scale;
    Vec3 matrixForward,matrixLeft,matrixUp,matrixPosition;
    std::array<std::byte,4> tail;
};
static_assert(sizeof(AudioListenerPacket)==0x80);
static_assert(offsetof(AudioListenerPacket,scale)==0x48);
static_assert(offsetof(AudioListenerPacket,matrixPosition)==0x70);

inline bool BuildHeadListener(const RenderContext& context,
    const AudioListenerPacket& source,AudioListenerPacket& output) noexcept
{
    if (!context.tracking.serial||!context.tracking.generation||!context.tracking.spaceEpoch||
        !context.referenceRevision||context.tracking.generation!=context.reference.generation||
        context.tracking.spaceEpoch!=context.reference.spaceEpoch||
        !Valid(context.tracking.headOrientation)) return false;
    Camera frame{};Quat inverse{};
    if (!BuildTrackingFrame(context.camera,context.reference,frame,inverse)) return false;
    const Quat head=Multiply(inverse,context.tracking.headOrientation);
    const Vec3 forward=ToNative(frame,Rotate(head,{0,0,-1}));
    const Vec3 up=ToNative(frame,Rotate(head,{0,1,0}));
    Camera check=context.camera;check.forward=forward;check.up=up;
    if (!Valid(check)||!Finite(source.velocity)||!Finite(source.matrixLeft)) return false;
    Camera sourceBasis=context.camera;
    sourceBasis.forward=source.matrixForward;sourceBasis.up=source.matrixUp;
    if (!Valid(sourceBasis)||Dot(source.matrixLeft-Cross(source.matrixUp,source.matrixForward),
        source.matrixLeft-Cross(source.matrixUp,source.matrixForward))>0.0001f) return false;
    AudioListenerPacket candidate=source;
    candidate.forward=candidate.matrixForward=forward;
    candidate.up=candidate.matrixUp=up;
    // Native BA24EC uses up x forward for the second matrix axis.
    candidate.matrixLeft=Cross(up,forward);
    // ABCC54 maps local velocity (x,y,z) to (x,z,-y), then transforms by
    // the listener matrix. Re-express that vector in the new basis so head
    // rotation cannot change the native world velocity/Doppler handoff.
    const Vec3 nativeVelocity=source.matrixForward*source.velocity.x+
        source.matrixLeft*source.velocity.z-source.matrixUp*source.velocity.y;
    candidate.velocity={Dot(nativeVelocity,forward),-Dot(nativeVelocity,up),
        Dot(nativeVelocity,candidate.matrixLeft)};
    if (!Finite(candidate.velocity)) return false;
    output=candidate;return true;
}
}
