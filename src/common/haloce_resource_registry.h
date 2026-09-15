#pragma once
#include <d3d11.h>
#include "haloce_snapshot.h"

namespace halo_ce
{
// Descriptors are recorded at native resource creation/import, while the
// caller owns the texture. No COM reference is carried into a render callback.
// Native release revokes the wrapper BEFORE releasing its resource. Independent
// live identities sharing a hash keep independent records. A full bucket refuses
// the new record instead of evicting a live eye/depth target until its next creation.
class ResourceRegistry
{
public:
    // Cold retirement, after core native callbacks have drained. Shared
    // creation/presentation publishers have separate lifetimes; concurrent
    // publication may lose availability and must still pass revision proof.
    // Pointer values from an earlier lifetime are never descriptor evidence.
    void InvalidateAll() noexcept
    {
        for (auto& bucket:slots_) for (auto& slot:bucket)
        {
            slot.revision.fetch_add(1,std::memory_order_acq_rel);
            slot.owner.store(0,std::memory_order_release);
        }
    }
    struct Record
    {
        uintptr_t wrapper{},resource{};
        uint64_t revision{};
        D3D11_TEXTURE2D_DESC descriptor{};
    };
private:
    struct Slot
    {
        std::atomic<uintptr_t> owner{};
        std::atomic<uint64_t> revision{};
        Snapshot<Record> value;
    };
    // Slots retain their owner until drained module/device retirement. This
    // avoids a released-pointer ABA or moving entries under render readers.
    Slot slots_[4096][8];
    static size_t Index(uintptr_t pointer) noexcept
    { return ((pointer>>4)^(pointer>>15))%4096; }
    const Slot* Find(uintptr_t wrapper) const noexcept
    {
        for (const auto& slot:slots_[Index(wrapper)])
            if (slot.owner.load(std::memory_order_acquire)==wrapper) return &slot;
        return nullptr;
    }
    Slot* Reserve(uintptr_t wrapper) noexcept
    {
        for (auto& slot:slots_[Index(wrapper)])
        {
            auto owner=slot.owner.load(std::memory_order_acquire);
            if (owner==wrapper) return &slot;
            if (!owner&&(slot.owner.compare_exchange_strong(owner,wrapper,
                    std::memory_order_acq_rel)||owner==wrapper)) return &slot;
        }
        return nullptr;
    }
public:
    void Forget(uintptr_t wrapper) noexcept
    {
        if (!wrapper) return;
        auto* selected=const_cast<Slot*>(Find(wrapper));
        if (!selected) return;
        auto& slot=*selected;
        // Revoke even an in-flight publication: consulting the last payload
        // can miss a newer reserved revision and resurrect a released texture.
        slot.revision.fetch_add(1,std::memory_order_acq_rel);
    }
    uint64_t Revoke(uintptr_t wrapper) noexcept
    {
        if (!wrapper) return 0;
        auto* slot=Reserve(wrapper);
        return slot?slot->revision.fetch_add(1,std::memory_order_acq_rel)+1:0;
    }
    bool Publish(uintptr_t wrapper,uintptr_t resource,uint64_t revision,
        const D3D11_TEXTURE2D_DESC& descriptor) noexcept
    {
        if (!wrapper||!resource||!revision) return false;
        auto* selected=const_cast<Slot*>(Find(wrapper));
        if (!selected) return false;
        auto& slot=*selected;
        if (slot.revision.load(std::memory_order_acquire)!=revision) return false;
        return slot.value.Publish({wrapper,resource,revision,descriptor})&&
            slot.revision.load(std::memory_order_acquire)==revision;
    }
    bool Read(uintptr_t wrapper,uintptr_t resource,Record& out) const noexcept
    {
        if (!wrapper||!resource) return false;
        const auto* selected=Find(wrapper);
        if (!selected) return false;
        const auto& slot=*selected;
        const uint64_t before=slot.revision.load(std::memory_order_acquire);
        Record record{};
        if (!slot.value.Read(record)||record.wrapper!=wrapper||record.resource!=resource||
            record.revision!=before||!before||
            slot.revision.load(std::memory_order_acquire)!=before) return false;
        out=record; return true;
    }
};
}
