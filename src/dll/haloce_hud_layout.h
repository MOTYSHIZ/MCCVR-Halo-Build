#pragma once
#include <d3d11.h>
#include <cstddef>
#include <cstdint>

bool HaloCEHudLayout_Poll(uintptr_t base,size_t size,uint32_t generation,bool active) noexcept;
void HaloCEHudLayout_SetObservationAvailable(bool available) noexcept;
// Output arrays must hold D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE.
// Always observe accepted native state; return true to submit transformed output.
bool HaloCEHudLayout_PrepareViewports(ID3D11DeviceContext*,UINT,const D3D11_VIEWPORT*,D3D11_VIEWPORT*) noexcept;
bool HaloCEHudLayout_PrepareScissors(ID3D11DeviceContext*,UINT,const D3D11_RECT*,D3D11_RECT*) noexcept;
void HaloCEHudLayout_InvalidateState(ID3D11DeviceContext*) noexcept;
// Bracket the complete native crosshair call, including stock fallback.
void HaloCEHudLayout_Suspend() noexcept;
void HaloCEHudLayout_Resume() noexcept;
// During Anniversary replay this remains in authored (full-height) pixels.
// Private reticle capture uses that raster and restores it through the observed
// native setters, which apply the eye transform once. Do not pre-scale it.
bool HaloCEHudLayout_CopyState(ID3D11DeviceContext*,UINT* viewportCount,D3D11_VIEWPORT*,
    UINT* scissorCount,D3D11_RECT*) noexcept;
void HaloCEHudLayout_BeginPrivateRaster() noexcept;
void HaloCEHudLayout_EndPrivateRaster() noexcept;
// The native Anniversary HUD is authored at full desktop height. Its callback
// can be replayed into one verified half-height eye source after its preamble.
// Caller restores the native backend target stack after EndEyeReplay.
bool HaloCEHudLayout_BeginEyeReplay(ID3D11DeviceContext*,UINT nativeWidth,UINT nativeHeight,
    UINT eyeWidth,UINT eyeHeight) noexcept;
bool HaloCEHudLayout_EndEyeReplay() noexcept;
