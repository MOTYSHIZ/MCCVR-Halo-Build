#pragma once
#include "../common/haloce_anniversary_logic.h"
#include "../common/haloce_view_pair.h"

namespace halo_ce
{
struct NativeBindings
{
    uintptr_t base{};
    size_t size{};
    uint32_t generation{};
    uintptr_t viewRebuild{},projectionRebuild{},cullingRebuild{};
    uintptr_t prepare{},pairBuilder{},frame{},output{},transfer{},surfaceSelector{};
};

// Cold worker only, with the module retained by the caller for this entire call
// and until all users of the result retire. Verifies mapped x64 PE identity,
// every unique executable signature, unwind entry, operand and body witness.
// Does not install a hook or grant capabilities. Output is cleared on failure.
bool ResolveNativeBindings(uintptr_t base,size_t size,uint32_t generation,
    NativeBindings& out,const char*& failure) noexcept;

// Caller retains the verified binding's module generation. Private camera only:
// CE's three one-argument rebuild routines run once in native dependency order.
// No native camera/resource ownership is transferred to this byte copy.
bool RebuildNativeCamera(const NativeBindings& bindings,SaberCamera& camera) noexcept;
PairStageResult StageBoundNativePair(const NativeBindings& bindings,
    const SaberViewPair& source,const Tracking& tracking,const Reference& reference,
    float unitsPerMeter,bool positional,StagedViewPair& out) noexcept;
}
