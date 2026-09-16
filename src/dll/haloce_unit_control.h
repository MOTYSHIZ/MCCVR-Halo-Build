#pragma once
#include <cstddef>
#include <cstdint>

struct HaloCELocalPlayerState;
namespace halo_ce { struct RenderContext; }

bool HaloCEUnitControl_Poll(uintptr_t base,size_t size,uint32_t generation,bool active) noexcept;
// Read-only admission for the optional seated crosshair, using the exact
// vehicle proof and native ownership checks used by controller steering.
bool HaloCEUnitControl_VehicleAimCurrent(const HaloCELocalPlayerState& player,
    const halo_ce::RenderContext& context) noexcept;
