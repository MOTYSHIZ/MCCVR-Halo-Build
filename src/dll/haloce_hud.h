#pragma once
#include <cstddef>
#include <cstdint>
#include "haloce_hud_target.h"

bool HaloCEHud_Poll(uintptr_t base,size_t size,uint32_t generation,bool active) noexcept;
// Native scope is independent of optional private capture resources. Layout
// requires it so even a stock-fallback crosshair keeps its original framing.
bool HaloCEHud_HasCrosshairScope() noexcept;
bool HaloCEHud_CapturedCrosshair() noexcept;
uint64_t HaloCEHud_CrosshairKey() noexcept;
bool HaloCEHud_ReadTarget(ID3D11DeviceContext* context,CeHudTargetSnapshot& snapshot) noexcept;
