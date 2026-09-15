#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>

namespace halo_ce
{
// A VR-side raster transform, not a claimed CE native curvature field.
// The caller owns one proven gameplay HUD scope and excludes authored reticles.
struct HudLayoutViewport
{
    float x{}, y{}, width{}, height{}, minDepth{}, maxDepth{1.0f};
};
struct HudLayoutRect { int32_t left{}, top{}, right{}, bottom{}; };
struct HudLayoutAffine
{
    float horizontal{1.0f}, vertical{1.0f}, offsetX{}, offsetY{};
};

inline bool ValidHudLayoutViewport(const HudLayoutViewport& v) noexcept
{
    return std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.width)&&
        std::isfinite(v.height)&&std::isfinite(v.minDepth)&&std::isfinite(v.maxDepth)&&
        v.width>0.0f&&v.height>0.0f&&v.width<=32767.0f&&v.height<=32767.0f&&
        v.x>=-32768.0f&&v.y>=-32768.0f&&v.x+v.width<=32767.0f&&v.y+v.height<=32767.0f&&
        v.minDepth>=0.0f&&v.maxDepth<=1.0f&&v.minDepth<=v.maxDepth;
}

inline bool ComputeHudLayoutAffine(float size,float aspect,float heightPixels,
    float gameAspect,const float* eyeFov,const HudLayoutViewport& root,
    HudLayoutAffine& out) noexcept
{
    if (!std::isfinite(size)||size<0.30f||size>1.0f||
        !std::isfinite(aspect)||aspect<0.50f||aspect>2.0f||
        !std::isfinite(heightPixels)||heightPixels<-300.0f||heightPixels>300.0f||
        !ValidHudLayoutViewport(root)) return false;
    float horizontal=size,vertical=size;
    // Match game.cpp ComputeHudSafeFramePair, including its optional FOV
    // correction. Absent/invalid optical data retains the native aspect.
    if (eyeFov&&std::isfinite(gameAspect)&&gameAspect>0.0f&&
        std::isfinite(eyeFov[0])&&std::isfinite(eyeFov[1])&&
        std::isfinite(eyeFov[2])&&std::isfinite(eyeFov[3]))
    {
        const float tanX=std::tan(std::max(-eyeFov[0],eyeFov[1]));
        const float tanY=std::tan(std::max(eyeFov[2],-eyeFov[3]));
        if (std::isfinite(tanX)&&std::isfinite(tanY)&&tanX>0.01f&&tanY>0.01f)
        {
            const float correction=gameAspect/(tanX/tanY);
            if (std::isfinite(correction)&&correction>=0.25f&&correction<=4.0f)
            {
                if (correction>1.0f) vertical=size/correction;
                else horizontal=size*correction;
            }
        }
    }
    horizontal=std::clamp(horizontal*aspect,0.15f,1.0f);
    vertical=std::clamp(vertical,0.15f,1.0f);
    // Screen Y grows downward. Positive height raises by output pixels,
    // independently of scale, matching the native H3 anchor-height semantics.
    out={horizontal,vertical,(root.x+root.width*0.5f)*(1.0f-horizontal),
        (root.y+root.height*0.5f)*(1.0f-vertical)-heightPixels};
    return true;
}

inline bool ValidHudLayoutAffine(const HudLayoutAffine& a) noexcept
{
    return std::isfinite(a.horizontal)&&std::isfinite(a.vertical)&&
        std::isfinite(a.offsetX)&&std::isfinite(a.offsetY)&&
        a.horizontal>=0.15f&&a.horizontal<=1.0f&&a.vertical>=0.15f&&a.vertical<=1.0f;
}

inline bool ApplyHudLayoutViewport(const HudLayoutAffine& a,
    const HudLayoutViewport& in,HudLayoutViewport& out) noexcept
{
    if (!ValidHudLayoutAffine(a)||!ValidHudLayoutViewport(in)) return false;
    const HudLayoutViewport result{in.x*a.horizontal+a.offsetX,
        in.y*a.vertical+a.offsetY,in.width*a.horizontal,in.height*a.vertical,
        in.minDepth,in.maxDepth};
    if (!ValidHudLayoutViewport(result)) return false;
    out=result;
    return true;
}

inline bool ApplyHudLayoutScissor(const HudLayoutAffine& a,
    const HudLayoutRect& in,HudLayoutRect& out) noexcept
{
    if (!ValidHudLayoutAffine(a)||in.left>in.right||in.top>in.bottom) return false;
    const double left=std::floor(double(in.left)*a.horizontal+a.offsetX);
    const double top=std::floor(double(in.top)*a.vertical+a.offsetY);
    const double right=std::ceil(double(in.right)*a.horizontal+a.offsetX);
    const double bottom=std::ceil(double(in.bottom)*a.vertical+a.offsetY);
    constexpr double lo=double(std::numeric_limits<int32_t>::min());
    constexpr double hi=double(std::numeric_limits<int32_t>::max());
    if (left<lo||top<lo||right>hi||bottom>hi) return false;
    HudLayoutRect result{int32_t(left),int32_t(top),int32_t(right),int32_t(bottom)};
    // Outward rounding preserves clipped edge pixels, but never opens an
    // originally empty scissor rectangle.
    if (in.left==in.right) result.right=result.left;
    if (in.top==in.bottom) result.bottom=result.top;
    out=result;
    return true;
}
}
