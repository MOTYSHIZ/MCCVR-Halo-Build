#pragma once
#include "haloce_anniversary_logic.h"

namespace halo_ce
{
// E-CE-FLARE-1: the native flare projector divides by the current eye's
// forward depth without clipping. Its sprite draw grows with projected
// distance, so a light at/behind the tracked near plane is not drawable.
enum class SaberFlareDisposition { Unproven, Visible, Clipped };
inline SaberFlareDisposition ClassifySaberFlare(const SaberCamera& camera,Vec3 light,
    const float (&screen)[2]) noexcept
{
    const auto* pose=camera.pose.matrix;
    const Vec3 position{pose[12],pose[13],pose[14]},forward{pose[8],pose[9],pose[10]};
    if (!Finite(light)||!Finite(position)||!Finite(forward)||
        std::fabs(Dot(forward,forward)-1)>=.05f||
        !std::isfinite(camera.nearPlane)||camera.nearPlane<=0)
        return SaberFlareDisposition::Unproven;
    const float depth=Dot(light-position,forward);
    if (!std::isfinite(depth)) return SaberFlareDisposition::Unproven;
    return depth<camera.nearPlane||!std::isfinite(screen[0])||!std::isfinite(screen[1])?
        SaberFlareDisposition::Clipped:SaberFlareDisposition::Visible;
}
}
