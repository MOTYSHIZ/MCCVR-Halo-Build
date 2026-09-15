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

// CE's native bitmap renderer masks writes to RGB (B0EC20 -> render state
// A8=7 -> D3D11_BLEND_DESC.RenderTarget[0].RenderTargetWriteMask). The private
// capture is cleared transparent, so its untouched alpha is not visibility.
constexpr uint32_t ReticleVisibleInk(uint32_t alphaInk,uint32_t colorInk) noexcept
{ return colorInk>alphaInk?colorInk:alphaInk; }

// Repair is authorized by the authored-upload call site, never merely by a
// 512-square eye/menu texture. Keep the normal CE image path byte-preserving.
constexpr bool ReticleUploadNeedsAlphaRepair(bool ceTitle,bool authoredUpload,
    uint32_t sourceWidth,uint32_t sourceHeight,
    uint32_t destinationWidth,uint32_t destinationHeight) noexcept
{
    return ceTitle&&authoredUpload&&sourceWidth==512&&sourceHeight==512&&
        destinationWidth==512&&destinationHeight==512;
}
}
