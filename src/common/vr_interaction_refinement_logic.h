#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>

// Preserve short, real tracking peaks between native input polls. The buffer
// never amplifies velocity and expires independently of the reader's cadence.
struct PhysicalMeleeSpeedHistory
{
    struct Sample { uint64_t atMs=0; float speed=0; } samples[32]{};
    unsigned next=0;
    void Update(uint64_t now, bool valid, float speed) noexcept
    {
        if(!valid || !now || !std::isfinite(speed) || speed<0 || speed>100)
        { *this={}; return; }
        samples[next]={now,speed}; next=(next+1)%32;
    }
    bool Read(uint64_t now, float& speed) const noexcept
    {
        speed=0;
        bool found=false;
        for(const auto& sample:samples)
            if(sample.atMs && now>=sample.atMs && now-sample.atMs<=60)
            { speed=std::max(speed,sample.speed); found=true; }
        return found;
    }
};

// Smooth only release along a continuing correction direction. New/deeper
// contact is immediate. Retain at most 10 mm extra clearance, never weaken
// the current native correction or invent a surface normal from a point hit.
struct ContactReleaseSmoothing
{
    uint64_t atMs=0;
    float previous[3]{};
    void Reset() noexcept { *this={}; }
    void Apply(uint64_t now, bool contact, float worldUnitsPerMetre,
               float correction[3]) noexcept
    {
        if(!correction) { Reset(); return; }
        float lengthSquared=0,oldSquared=0,dot=0;
        for(int axis=0;axis<3;++axis)
        {
            if(!std::isfinite(correction[axis])) { Reset(); return; }
            lengthSquared+=correction[axis]*correction[axis];
            oldSquared+=previous[axis]*previous[axis];
            dot+=correction[axis]*previous[axis];
        }
        if(!contact || !now || !std::isfinite(worldUnitsPerMetre) ||
            worldUnitsPerMetre<=0 || !std::isfinite(lengthSquared) || lengthSquared<=1e-12f)
        { Reset(); return; }
        const float length=std::sqrt(lengthSquared),oldLength=std::sqrt(oldSquared);
        if(atMs && now>=atMs && now-atMs<=100 && oldLength>length &&
            dot>0.95f*length*oldLength)
        {
            const float retained=(oldLength-length)*std::exp(-float(now-atMs)/40.0f);
            const float extra=std::min(retained,0.01f*worldUnitsPerMetre);
            for(int axis=0;axis<3;++axis) correction[axis]*=(length+extra)/length;
        }
        atMs=now;
        for(int axis=0;axis<3;++axis) previous[axis]=correction[axis];
    }
};

// Visibility only: five degrees beyond each raster edge. Keep the submitted
// OpenXR projection unchanged and preserve already wider authored coverage.
inline float ExpandVisibilityTangent(float tangent) noexcept
{
    if(!std::isfinite(tangent) || tangent<=0) return tangent;
    constexpr float pi=3.14159265358979323846f;
    const float half=std::atan(tangent);
    if(half>=85.0f*pi/180.0f) return tangent;
    return std::tan(std::min(half+5.0f*pi/180.0f,85.0f*pi/180.0f));
}
