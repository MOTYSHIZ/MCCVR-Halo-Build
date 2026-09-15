#pragma once
#include <cstdint>

namespace halo_ce
{
// Anniversary workers prepare the frame before the late native HUD callback.
// Keep the same bounded receipt age as the CE core, after that core has checked
// the title, renderer, reference, tracking space and callback owner.
constexpr bool ReticleReceiptSerialCurrent(uint64_t receipt,uint64_t displayed) noexcept
{
    return receipt&&receipt<=displayed&&displayed-receipt<=8;
}

// Completing a native draw is not proof of visible compositor pixels. Until
// measured artwork has also been published, show the shared controller marker.
constexpr bool ReticleNeedsProceduralBootstrap(bool ownsNativeScope,bool measuredArt,
    bool publishedArt) noexcept
{
    return ownsNativeScope&&(!measuredArt||!publishedArt);
}

// Coverage belongs to the texture queued with the GPU query. Keep that exact
// source unchanged until its result has been consumed, including before the
// first successful compositor upload.
constexpr bool ReticleCanReplaceCapture(bool coveragePending) noexcept
{ return !coveragePending; }
}
