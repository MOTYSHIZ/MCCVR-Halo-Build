#pragma once
#include <d3d11.h>
#include "haloce_snapshot.h"

namespace halo_ce
{
// Descriptors are recorded at native resource creation/import, while the
// caller owns the texture. No COM reference is carried into a render callback.
// Native release revokes the wrapper BEFORE releasing its resource. A hash
// collision declines capture; it never lends another wrapper's descriptor.
class ResourceRegistry
{
public:
    // Cold retirement, after native callbacks have drained. Pointer values
    // from an earlier module/device lifetime are never descriptor evidence.
    void InvalidateAll() noexcept
    {
        for (auto& slot:slots_) slot.revision.fetch_add(1,std::memory_order_acq_rel);
    }
    struct Record
    {
        uintptr_t wrapper{},resource{};
        uint64_t revision{};
        D3D11_TEXTURE2D_DESC descriptor{};
    };
private:
    struct Slot { std::atomic<uint64_t> revision{}; Snapshot<Record> value; };
    Slot slots_[8192];
    static size_t Index(uintptr_t pointer) noexcept
    { return ((pointer>>4)^(pointer>>15))%8192; }
public:
    void Forget(uintptr_t wrapper) noexcept
    {
        if (!wrapper) return;
        auto& slot=slots_[Index(wrapper)];
        Record value{};
        if (slot.value.Read(value)&&value.wrapper==wrapper)
        {
            auto revision=value.revision;
            slot.revision.compare_exchange_strong(revision,revision+1,std::memory_order_acq_rel);
        }
    }
    uint64_t Revoke(uintptr_t wrapper) noexcept
    {
        if (!wrapper) return 0;
        return slots_[Index(wrapper)].revision.fetch_add(1,std::memory_order_acq_rel)+1;
    }
    bool Publish(uintptr_t wrapper,uintptr_t resource,uint64_t revision,
        const D3D11_TEXTURE2D_DESC& descriptor) noexcept
    {
        if (!wrapper||!resource||!revision) return false;
        auto& slot=slots_[Index(wrapper)];
        if (slot.revision.load(std::memory_order_acquire)!=revision) return false;
        return slot.value.Publish({wrapper,resource,revision,descriptor})&&
            slot.revision.load(std::memory_order_acquire)==revision;
    }
    bool Read(uintptr_t wrapper,uintptr_t resource,Record& out) const noexcept
    {
        if (!wrapper||!resource) return false;
        const auto& slot=slots_[Index(wrapper)];
        const uint64_t before=slot.revision.load(std::memory_order_acquire);
        Record record{};
        if (!slot.value.Read(record)||record.wrapper!=wrapper||record.resource!=resource||
            record.revision!=before||!before||
            slot.revision.load(std::memory_order_acquire)!=before) return false;
        out=record; return true;
    }
};
}
