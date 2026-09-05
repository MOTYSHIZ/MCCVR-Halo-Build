#pragma once
#include <cmath>
#include <cstdint>

// A completed on-foot stereo pair owns one presented controller ray. Never
// converge from the head to a far reticle point: those lines only cross once.
inline bool ReachBuildHandShotDirection(
    uint32_t generation,uint32_t sampleGeneration,
    int32_t firingUnit,int32_t currentUnit,int32_t sampleUnit,
    uint64_t nowMs,uint64_t sampleMs,const float origin[3],
    const float target[3],float direction[3]) noexcept
{
    if(!generation || generation!=sampleGeneration || firingUnit==-1 ||
        firingUnit!=currentUnit || firingUnit!=sampleUnit ||
        !sampleMs || sampleMs>nowMs || nowMs-sampleMs>100 ||
        !origin || !target || !direction) return false;
    float delta[3]{},lengthSquared=0;
    for(int axis=0;axis<3;++axis)
    {
        if(!std::isfinite(origin[axis]) || !std::isfinite(target[axis])) return false;
        delta[axis]=target[axis]-origin[axis];
        lengthSquared+=delta[axis]*delta[axis];
    }
    if(!std::isfinite(lengthSquared) || lengthSquared<1.0e-8f) return false;
    const float inverse=1.0f/std::sqrt(lengthSquared);
    for(int axis=0;axis<3;++axis) direction[axis]=delta[axis]*inverse;
    return true;
}
