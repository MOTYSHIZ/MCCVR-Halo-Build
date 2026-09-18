#pragma once
#include <cstdint>
#include <cstddef>
#include <cstring>

namespace halo2_datum
{
// H2EK datum_get and retail allocator 67AFA0: capacity +20, stride +24,
// valid +29, relative storage +48, full handle salt at the record start.
// The caller owns the memory-access guard and the module lifetime.
inline const uint8_t* Record(const uint8_t* table,uint32_t handle,
    uint32_t stride,uint32_t maximum) noexcept
{
    if (!table || handle==UINT32_MAX || !(handle>>16) || !table[0x29]) return nullptr;
    uint32_t capacity{},actualStride{};uintptr_t offset{};
    std::memcpy(&capacity,table+0x20,4);
    std::memcpy(&actualStride,table+0x24,4);
    std::memcpy(&offset,table+0x48,sizeof(offset));
    const auto base=reinterpret_cast<uintptr_t>(table);
    if (!stride || !capacity || capacity>maximum || actualStride!=stride ||
        (handle&0xffff)>=capacity || !offset ||
        uint64_t(capacity)*stride>UINTPTR_MAX-base ||
        offset>UINTPTR_MAX-base-size_t(capacity)*stride) return nullptr;
    const auto* record=table+offset+size_t(handle&0xffff)*stride;
    uint16_t salt{};std::memcpy(&salt,record,2);
    return salt==uint16_t(handle>>16) ? record : nullptr;
}
}
