#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>

// A firing scope may temporarily expose a native acquisition result to native
// consumers which read the unit directly. The caller resolves full ownership
// before both operations; this class never retains authority over a raw pointer.
// A native change inside the scope wins over restoration of the older result.
template<std::size_t Size>
struct NativeShotTargetLease
{
    static_assert(Size>0);
    bool active=false;
    uint32_t generation=0,owner=UINT32_MAX;
    const void* storage=nullptr;
    unsigned char saved[Size]{},written[Size]{};

    bool Apply(uint32_t liveGeneration,uint32_t liveOwner,void* resolved,
        const void* nativeResult) noexcept
    {
        if(active || !liveGeneration || liveOwner==UINT32_MAX || !resolved || !nativeResult)
            return false;
        std::memcpy(saved,resolved,Size);
        std::memcpy(written,nativeResult,Size);
        generation=liveGeneration;owner=liveOwner;storage=resolved;
        // Arm before the write so the adapter's exception cleanup can inspect
        // the lease even if storage disappears during the native transition.
        active=true;
        std::memcpy(resolved,written,Size);
        return true;
    }

    bool Restore(uint32_t liveGeneration,uint32_t liveOwner,void* resolved) noexcept
    {
        if(!active)return true;
        active=false;
        if(generation!=liveGeneration || owner!=liveOwner || storage!=resolved || !resolved)
            return false;
        if(std::memcmp(resolved,written,Size)!=0)return false;
        std::memcpy(resolved,saved,Size);
        return true;
    }
};
