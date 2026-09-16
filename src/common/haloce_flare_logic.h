#pragma once
#include "haloce_anniversary_logic.h"

namespace halo_ce
{
// E-CE-FLARE-1: the native flare projector divides by the current eye's
// forward depth without clipping. Its sprite draw grows with projected
// distance, so a light at/behind the tracked near plane is not drawable.
enum class SaberFlareDisposition { Unproven, Visible, Clipped, Offscreen };
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
    if (depth<camera.nearPlane||!std::isfinite(screen[0])||!std::isfinite(screen[1]))
        return SaberFlareDisposition::Clipped;
    // The native sprite envelope falls to zero at radius .8, in units of
    // render width on BOTH axes. Its source-centered halo is an exception:
    // it retains alpha while its radius continues growing offscreen. Bound
    // that residual draw at the native envelope, outside the visible raster.
    // Do not infer a raster from an invalid camera or affect stock callbacks.
    const float width=camera.viewportWidth,height=camera.viewportHeight;
    if (!std::isfinite(width)||!std::isfinite(height)||width<1||height<1||
        width>16384||height>16384||camera.viewportX!=0||camera.viewportY!=0||
        std::floor(width)!=width||std::floor(height)!=height)
        return SaberFlareDisposition::Unproven;
    const double x=(double(screen[0])-double(width)*.5)/width;
    const double y=(double(screen[1])-double(height)*.5)/width;
    // Preserve every in-raster source even on unusual tall render targets.
    const bool outside=screen[0]<0||screen[0]>width||screen[1]<0||screen[1]>height;
    return outside&&x*x+y*y>double(.8f)*double(.8f)?
        SaberFlareDisposition::Offscreen:SaberFlareDisposition::Visible;
}
}
