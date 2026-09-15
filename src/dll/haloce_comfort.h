#pragma once
#include <cstddef>
#include <cstdint>

// Optional native camera-motion-blur setting. Failure leaves this effect stock.
bool HaloCEComfort_Poll(uintptr_t base,size_t size,uint32_t generation,bool active) noexcept;
