#pragma once

#include <cstddef>
#include <cstdint>

namespace halo_ce
{
// E-CE-6: CE +0x204C40 consumes this request from the +0x45E2B0
// per-view output call. These are native surface wrappers, NOT D3D pointers.
// +0xAD5F0 selects the actual wrapper before the native copy reads its +0xE0
// D3D resource. Reading sourceSurface+0xE0 directly can select the wrong eye.
struct SurfaceTransfer
{
    uint64_t sourceSurface{},destinationSurface{};
    int32_t sourceX{},sourceY{},sourceMip{},sourceArraySlice{};
    int32_t destinationX{},destinationY{},destinationMip{},destinationArraySlice{};
    int32_t width{},height{};
};
static_assert(sizeof(SurfaceTransfer)==0x38);
static_assert(offsetof(SurfaceTransfer,sourceX)==0x10);
static_assert(offsetof(SurfaceTransfer,destinationX)==0x20);
static_assert(offsetof(SurfaceTransfer,width)==0x30);

// A shape guard for the exact native eye-output call, not a GPU capture or
// ownership predicate on its own. The adapter must separately establish the
// native caller, current view pair/serial, live resolved source, immediate D3D
// context, compatible preallocated texture, and completion before submission.
inline bool IsPrimaryEyeTransfer(const SurfaceTransfer& copy,int eye,
    uint32_t expectedWidth,uint32_t expectedHeight) noexcept
{
    return eye>=0&&eye<2&&expectedWidth>0&&expectedWidth<=16384&&
        expectedHeight>0&&expectedHeight<=16384&&
        copy.sourceSurface!=0&&copy.destinationSurface!=0&&
        copy.sourceSurface!=copy.destinationSurface&&
        copy.sourceX==0&&copy.sourceY==0&&copy.sourceMip==0&&
        copy.sourceArraySlice==0&&copy.destinationX==0&&
        copy.destinationY==eye*static_cast<int32_t>(expectedHeight)&&
        copy.destinationMip==0&&copy.destinationArraySlice==0&&
        copy.width==static_cast<int32_t>(expectedWidth)&&
        copy.height==static_cast<int32_t>(expectedHeight);
}
}
