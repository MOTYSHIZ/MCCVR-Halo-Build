#pragma once

#include "halo4_world_collision_logic.h"
#include <bit>
#include <cstring>
#include <span>

// H4EK render_model +0x80 compression block; independently matched to the
// retail model renderer (344534 -> 387AB8). These are H4-only cache facts.
inline constexpr uint32_t kHalo4RuntimeCompressionBlockOffset = 0x80;
inline constexpr size_t kHalo4RuntimeCompressionRecordBytes = 52;
inline constexpr int kHalo4RuntimeCompressionMaxRecords = 64;

inline bool Halo4DecodeRuntimeCompressionBounds(
    std::span<const uint8_t> bytes, Halo4WeaponCollisionBounds& output) noexcept
{
    if (bytes.size() != kHalo4RuntimeCompressionRecordBytes) return false;
    uint16_t flags{};
    memcpy(&flags, bytes.data(), sizeof(flags));
    if (!(flags & 1u)) return false; // no authored compressed-position bounds
    Halo4WeaponCollisionBounds next{};
    for (int axis=0; axis<3; ++axis)
    {
        float minimum{}, maximum{};
        if (flags & 4u)
        {
            // H4EK 1572A0: optimized union stores extent XYZ at +4 and
            // minimum XYZ at +14, rather than interleaved min/max pairs.
            float extent{};
            memcpy(&extent, bytes.data()+4+axis*4, sizeof(float));
            memcpy(&minimum, bytes.data()+0x14+axis*4, sizeof(float));
            if (!std::isfinite(extent) || extent<0.0f) return false;
            maximum=minimum+extent;
        }
        else
        {
            memcpy(&minimum, bytes.data()+4+axis*8, sizeof(float));
            memcpy(&maximum, bytes.data()+8+axis*8, sizeof(float));
        }
        if (!std::isfinite(minimum) || !std::isfinite(maximum) ||
            minimum>maximum || !std::isfinite(maximum-minimum)) return false;
        next.minimum[axis]=minimum;
        next.maximum[axis]=maximum;
    }
    if (next.minimum[0]==next.maximum[0] &&
        next.minimum[1]==next.maximum[1] &&
        next.minimum[2]==next.maximum[2]) return false;
    output=next;
    return true;
}

inline uint32_t Halo4RuntimeWeaponShapeId(
    uint16_t tag, const Halo4WeaponCollisionBounds& bounds) noexcept
{
    uint32_t hash=2166136261u;
    const auto mix=[&](uint32_t word) { hash=(hash^word)*16777619u; };
    mix(tag);
    mix(bounds.runtimeImportChecksum);
    for (int axis=0; axis<3; ++axis)
    {
        mix(std::bit_cast<uint32_t>(bounds.minimum[axis]));
        mix(std::bit_cast<uint32_t>(bounds.maximum[axis]));
    }
    return hash ? hash : 1u;
}
