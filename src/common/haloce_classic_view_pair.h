#pragma once

#include "haloce_render_logic.h"
#include <cstring>

namespace halo_ce
{
// E-CE-1: Classic constructs BOTH native frusta from the two Camera values
// in each 0xAC window. Staging one camera would leave visibility and drawing
// with different head positions. This object contains private bytes only.
struct ClassicViewPair
{
    Window source;
    Window eyes[2];
    Tracking tracking;
    Cover cover;
    uint64_t rendererEpoch{};
};

enum class ClassicPairResult : uint8_t
{
    Ready, InvalidWindow, InvalidTracking, InvalidRendererEpoch,
    InvalidCover, InvalidEye
};

inline ClassicPairResult StageClassicViewPair(const Window& source,
    const Tracking& tracking,const Reference& reference,uint64_t rendererEpoch,
    float unitsPerMeter,bool positional,ClassicViewPair& out) noexcept
{
    if (!ValidPrimary(source)) return ClassicPairResult::InvalidWindow;
    if (!tracking.serial||!tracking.generation||!tracking.spaceEpoch||
        tracking.generation!=reference.generation||
        tracking.spaceEpoch!=reference.spaceEpoch)
        return ClassicPairResult::InvalidTracking;
    if (!rendererEpoch) return ClassicPairResult::InvalidRendererEpoch;

    ClassicViewPair staged{};
    staged.source=source;
    staged.tracking=tracking;
    staged.rendererEpoch=rendererEpoch;
    if (!BuildCover(tracking,source.raster.viewport,staged.cover))
        return ClassicPairResult::InvalidCover;
    for (int eye=0;eye<2;++eye)
    {
        staged.eyes[eye]=source;
        if (!BuildEye(source.render,tracking,reference,eye,unitsPerMeter,
                positional,staged.cover,staged.eyes[eye].render)||
            !BuildEye(source.raster,tracking,reference,eye,unitsPerMeter,
                positional,staged.cover,staged.eyes[eye].raster)||
            !ValidPrimary(staged.eyes[eye]))
            return ClassicPairResult::InvalidEye;
    }
    out=staged;
    return ClassicPairResult::Ready;
}

// Stock-camera bytes and all identity fields must still match before native
// execution starts. Mode switches/recenter/title entry revoke the pair even
// if the head is stationary and the camera happens to be bit-identical.
inline bool ClassicPairCurrent(const ClassicViewPair& pair,const Window& source,
    uint32_t generation,uint64_t spaceEpoch,uint64_t serial,
    uint64_t rendererEpoch) noexcept
{
    return generation&&spaceEpoch&&serial&&rendererEpoch&&
        pair.tracking.generation==generation&&pair.tracking.spaceEpoch==spaceEpoch&&
        pair.tracking.serial==serial&&pair.rendererEpoch==rendererEpoch&&
        std::memcmp(&pair.source,&source,sizeof(source))==0;
}
}
