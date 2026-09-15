#pragma once

#include "haloce_anniversary_logic.h"

namespace halo_ce
{
// E-CE-4/5. This is the bounded prefix of CE's native view list, not the
// complete list. Native construction, allocation and destruction stay native.
// +0x4547E0 creates the primary/secondary records through +0x2EA720 before
// +0x455170 submits culling. No whole-frame render replay is needed here.
struct SaberView
{
    uint32_t flags{};
    uint8_t native04[8]{};
    int32_t viewIndex{-1};
    uint8_t native10[0x20]{};
    SaberCamera camera;
};
struct SaberViewPair
{
    uint32_t flags{};
    uint32_t native04{};
    uint32_t count{};
    uint32_t native0c{};
    SaberView views[2];
};
static_assert(sizeof(SaberView)==0x3c8);
static_assert(offsetof(SaberView,camera)==0x30);
static_assert(offsetof(SaberView,viewIndex)==0x0c);
static_assert(offsetof(SaberViewPair,views)==0x10);
static_assert(sizeof(SaberViewPair)==0x7a0);
// Native append stores index*0x3c8+0x10; the next array begins at +0xbd20.
// Culling can append reflection/shadow views after the two primary records.
inline constexpr uint32_t kNativeViewCapacity=(0xbd20-0x10)/sizeof(SaberView);
static_assert(kNativeViewCapacity==50);

enum class PairStageResult : uint8_t
{
    Staged,
    InvalidNativePair,
    InvalidTracking,
    RebuildFailed,
    InvalidRebuiltCamera,
    AwaitingRaster,
};

struct StagedViewPair
{
    uint64_t serial{},spaceEpoch{};
    uint32_t generation{};
    SaberCamera cameras[2];
    Cover covers[2];
};

// Only accepts the specific two-primary-view shape built by +0x4547E0.
// Reflections, cubemap passes, extra views and an already enabled native stereo
// shift are not this path. The 0x1000 bit is selected by native stock zoom.
inline bool ValidNativePair(const SaberViewPair& pair)
{
    if (pair.count!=2||pair.flags!=1) return false;
    for (int eye=0;eye<2;++eye)
    {
        const SaberView& view=pair.views[eye];
        const uint32_t expected=eye==0?0x10bu:0x20bu;
        if ((view.flags&~0x1000u)!=expected||view.viewIndex!=eye)
            return false;
        Camera camera{};
        if (!NativeCameraFromSaber(view.camera,camera)) return false;
    }
    const SaberCamera& left=pair.views[0].camera;
    const SaberCamera& right=pair.views[1].camera;
    // Both are copies of the one primary camera in single-player. Never
    // replace an actual split-screen player's independently authored view.
    return std::memcmp(&left.pose,&right.pose,sizeof(SaberPose))==0&&
        left.viewportWidth==right.viewportWidth&&
        left.viewportHeight==right.viewportHeight&&
        left.horizontalFovDegrees==right.horizontalFovDegrees&&
        left.verticalFovDegrees==right.verticalFovDegrees;
}

// E-CE-11: the native copy can expose a half-height eye source while the
// original camera still describes the full desktop raster. Select the proven
// GPU dimensions BEFORE deriving the eye FOV/projection, never at submission.
// The caller supplies dimensions of its cold, generation-bound eye cache;
// capture must still verify them against the actual current native source.
inline bool SelectNativeEyeRaster(const SaberViewPair& native,uint32_t width,
    uint32_t height,SaberViewPair& out) noexcept
{
    if (!ValidNativePair(native)||!width||width>16384||!height||height>16384)
        return false;
    SaberViewPair result=native;
    for (auto& view:result.views)
    {
        view.camera.viewportWidth=static_cast<float>(width);
        view.camera.viewportHeight=static_cast<float>(height);
    }
    out=result;
    return true;
}

// Rebuild is supplied by the CE adapter only after independently verifying
// CE's view (+0x11ABA0), projection (+0x11AE90) and culling (+0x11E5F0)
// functions. It operates on private copies and must return false on failure.
// Neither this helper nor its tests install a runtime hook. A Staged result
// means two prepared cameras, not rendered/captured/submitted OpenXR eyes.
template<class Rebuild>
PairStageResult StageNativeViewPair(const SaberViewPair& native,
    const Tracking& tracking,const Reference& reference,float unitsPerMeter,
    bool positional,Rebuild&& rebuild,StagedViewPair& out)
{
    if (!ValidNativePair(native)) return PairStageResult::InvalidNativePair;
    StagedViewPair staged{};
    staged.serial=tracking.serial;
    staged.spaceEpoch=tracking.spaceEpoch;
    staged.generation=tracking.generation;
    // Finish both mathematical stages before calling either native builder.
    for (int eye=0;eye<2;++eye)
        if (!StageSaberEye(native.views[eye].camera,tracking,reference,eye,
                unitsPerMeter,positional,staged.cameras[eye],staged.covers[eye]))
            return PairStageResult::InvalidTracking;
    for (int eye=0;eye<2;++eye)
    {
        SaberCamera& camera=staged.cameras[eye];
        const SaberPose requestedPose=camera.pose;
        if (!rebuild(camera)) return PairStageResult::RebuildFailed;
        Camera checked{};
        if (!NativeCameraFromSaber(camera,checked)||!FiniteSaberDerivedCamera(camera))
            return PairStageResult::InvalidRebuiltCamera;
        // Reject a changed raster/FOV instead of submitting a projection that
        // disagrees with the image. The adapter still needs actual GPU proof.
        const SaberCamera& original=native.views[eye].camera;
        constexpr float degreesToRadians=0.017453292519943295f;
        for (int component=0;component<16;++component)
            if (std::fabs(camera.pose.matrix[component]-
                    requestedPose.matrix[component])>0.0001f)
                return PairStageResult::InvalidRebuiltCamera;
        if (camera.nearPlane!=original.nearPlane||
            camera.farPlane!=original.farPlane||
            camera.viewportWidth!=original.viewportWidth||
            camera.viewportHeight!=original.viewportHeight||
            !std::isfinite(camera.horizontalFovDegrees)||
            std::fabs(camera.horizontalFovDegrees*degreesToRadians-
                2*staged.covers[eye].halfX)>0.0001f||
            std::fabs(camera.verticalFovDegrees*degreesToRadians-
                2*staged.covers[eye].halfY)>0.0001f)
            return PairStageResult::InvalidRebuiltCamera;
    }
    out=staged;
    return PairStageResult::Staged;
}
}
