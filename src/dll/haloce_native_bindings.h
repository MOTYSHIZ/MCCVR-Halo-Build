#pragma once
#include "../common/haloce_anniversary_logic.h"
#include "../common/haloce_view_pair.h"
#include "../common/haloce_contracts.generated.h"
#include <span>

namespace halo_ce
{
struct NativeContractSet
{
    std::span<const contract::Entry> entries;
    std::span<const contract::Witness> witnesses;
    std::span<const contract::Relative> relatives;
    std::span<const contract::Pointer> pointers;
};
// Cold, read-only verification for one independent feature. A failed optional
// set never mutates the camera bindings or any other feature's lifecycle.
bool VerifyNativeFeatureBindings(uintptr_t base,size_t size,uint32_t generation,
    const NativeContractSet& contracts,const char*& failure) noexcept;
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
