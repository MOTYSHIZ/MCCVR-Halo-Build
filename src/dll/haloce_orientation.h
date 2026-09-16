#pragma once
#include <cstddef>
#include <cstdint>

// Optional CE view-comfort and spatial-audio transactions. Their admission and
// installation never own or revoke stereo, gameplay controls, or OpenXR.
bool HaloCEOrientation_Poll(uintptr_t base,size_t size,uint32_t generation,bool active) noexcept;
