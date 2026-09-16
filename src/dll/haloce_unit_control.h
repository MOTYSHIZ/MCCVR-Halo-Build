#pragma once
#include <cstddef>
#include <cstdint>

bool HaloCEUnitControl_Poll(uintptr_t base,size_t size,uint32_t generation,bool active) noexcept;
