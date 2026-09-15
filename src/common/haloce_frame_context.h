#pragma once
#include "haloce_render_logic.h"

namespace halo_ce
{
// One native center camera and one immutable XR sample/reference. The caller
// supplies the stock camera before first-person or per-eye changes; consumers
// must never reconstruct this from separately sampled global hand/head poses.
struct RenderContext
{
    Tracking tracking;
    Reference reference;
    Camera camera;
    float unitsPerMeter{};
    bool positional{};
    uint64_t referenceRevision{},rendererEpoch{};
};
}
