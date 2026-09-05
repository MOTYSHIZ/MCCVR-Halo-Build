#pragma once

#include <cmath>
#include <cstdint>
#include <cstring>
#include <span>

// Loaded retail mode record, verified against the live fp_battle_rifle and
// independently matched to its official H2EK compression bounds. The import
// block address occupies +0x10; it is not the compression count.
inline bool Halo2ReadWeaponCompressionHeader(
    std::span<const uint8_t> definition, int32_t& byteOffset) noexcept
{
    byteOffset=0;
    if(definition.size()<0x1C) return false;
    int32_t count=0,offset=0;
    std::memcpy(&count,definition.data()+0x14,sizeof(count));
    std::memcpy(&offset,definition.data()+0x18,sizeof(offset));
    if(count!=1 || !offset) return false;
    byteOffset=offset;
    return true;
}

// Pinned H2 loaded-tag getter ends in ADD RAX,[RIP+disp32] at +0x12.
// The displacement begins at +0x15 and RIP is +0x19, not +0x14/+0x18.
inline uintptr_t Halo2CollisionTagBaseSlot(
    uintptr_t moduleBase, size_t moduleSize, uint32_t getterRva,
    std::span<const uint8_t> getterBytes) noexcept
{
    if (!moduleBase || moduleSize < 8 || getterBytes.size() < 26 ||
        getterBytes[0x12] != 0x48 || getterBytes[0x13] != 0x03 ||
        getterBytes[0x14] != 0x05 || getterBytes[0x19] != 0xC3)
        return 0;
    int32_t displacement = 0;
    std::memcpy(&displacement, getterBytes.data() + 0x15, sizeof(displacement));
    const int64_t slotRva = static_cast<int64_t>(getterRva) + 0x19 + displacement;
    if (slotRva < 0 || static_cast<uint64_t>(slotRva) > moduleSize - 8 ||
        static_cast<uint64_t>(slotRva) > UINTPTR_MAX - moduleBase)
        return 0;
    return moduleBase + static_cast<uintptr_t>(slotRva);
}

// Halo 2 publishes one stable root plus min/max X/Y/Z samples selected from
// the title's final, already root-composed first-person packet.  The packet is
// the engine's own visible geometry in both Classic and Anniversary, so this
// does not invent a weapon length or copy Halo 4's render-model layout.
inline constexpr int kHalo2WorldCollisionSampleCount = 7;
inline constexpr int kHalo2WeaponCollisionBoundsSampleCount = 14;
inline constexpr int kHalo2WorldCollisionMaxSamples =
    kHalo2WorldCollisionSampleCount +
    kHalo2WeaponCollisionBoundsSampleCount;

struct Halo2WorldCollisionResolution
{
    bool valid = false;
    bool contact = false;
    float accepted[3]{};
    float correction[3]{};
};

inline bool Halo2WorldCollisionFinite(const float value[3]) noexcept
{
    return value && std::isfinite(value[0]) && std::isfinite(value[1]) &&
        std::isfinite(value[2]);
}

inline float Halo2WorldCollisionDistanceSquared(
    const float first[3], const float second[3]) noexcept
{
    if (!Halo2WorldCollisionFinite(first) ||
        !Halo2WorldCollisionFinite(second))
        return -1.0f;
    const float x = second[0] - first[0];
    const float y = second[1] - first[1];
    const float z = second[2] - first[2];
    const float result = x * x + y * y + z * z;
    return std::isfinite(result) ? result : -1.0f;
}

inline int Halo2SelectWorldCollisionExtrema(
    const float root[3], const float* points, int pointCount,
    float output[][3], int outputCapacity) noexcept
{
    if (!Halo2WorldCollisionFinite(root) || !points || pointCount <= 0 ||
        !output || outputCapacity < kHalo2WorldCollisionSampleCount)
        return 0;
    for (int axis = 0; axis < 3; ++axis)
        output[0][axis] = root[axis];
    int extrema[6]{-1, -1, -1, -1, -1, -1};
    float values[6]{INFINITY, -INFINITY, INFINITY, -INFINITY,
                    INFINITY, -INFINITY};
    for (int point = 0; point < pointCount; ++point)
    {
        const float* candidate = points + point * 3;
        if (!Halo2WorldCollisionFinite(candidate)) continue;
        for (int axis = 0; axis < 3; ++axis)
        {
            const int minimum = axis * 2;
            const int maximum = minimum + 1;
            if (candidate[axis] < values[minimum])
            {
                values[minimum] = candidate[axis];
                extrema[minimum] = point;
            }
            if (candidate[axis] > values[maximum])
            {
                values[maximum] = candidate[axis];
                extrema[maximum] = point;
            }
        }
    }
    for (int slot = 0; slot < 6; ++slot)
    {
        if (extrema[slot] < 0) return 0;
        const float* candidate = points + extrema[slot] * 3;
        for (int axis = 0; axis < 3; ++axis)
            output[slot + 1][axis] = candidate[axis];
    }
    return kHalo2WorldCollisionSampleCount;
}

inline bool Halo2WorldCollisionMovementIsTeleport(
    const float accepted[3], const float desired[3], float worldScale,
    float maximumMetres = 1.5f) noexcept
{
    if (!std::isfinite(worldScale) || worldScale <= 0.0f ||
        !std::isfinite(maximumMetres) || maximumMetres <= 0.0f)
        return true;
    const float distanceSquared =
        Halo2WorldCollisionDistanceSquared(accepted, desired);
    const float maximum = maximumMetres * worldScale;
    return distanceSquared < 0.0f || distanceSquared > maximum * maximum;
}

inline Halo2WorldCollisionResolution Halo2ResolveWorldCollision(
    const float start[3], const float desired[3], bool hit,
    float hitFraction, float worldScale, float skinMetres = 0.015f) noexcept
{
    Halo2WorldCollisionResolution result{};
    if (!Halo2WorldCollisionFinite(start) ||
        !Halo2WorldCollisionFinite(desired) || !std::isfinite(worldScale) ||
        worldScale <= 0.0f || !std::isfinite(skinMetres) || skinMetres < 0.0f)
        return result;
    const float distanceSquared =
        Halo2WorldCollisionDistanceSquared(start, desired);
    if (distanceSquared < 0.0f) return result;
    result.valid = true;
    if (!hit)
    {
        for (int axis = 0; axis < 3; ++axis)
            result.accepted[axis] = desired[axis];
        return result;
    }
    if (!std::isfinite(hitFraction) || hitFraction < 0.0f ||
        hitFraction > 1.0f)
    {
        return Halo2WorldCollisionResolution{};
    }
    const float distance = std::sqrt(distanceSquared);
    float fraction = hitFraction;
    if (distance > 1.0e-6f)
        fraction = std::fmax(
            0.0f, hitFraction - skinMetres * worldScale / distance);
    for (int axis = 0; axis < 3; ++axis)
    {
        result.accepted[axis] = start[axis] +
            (desired[axis] - start[axis]) * fraction;
        result.correction[axis] = result.accepted[axis] - desired[axis];
        if (!std::isfinite(result.accepted[axis]) ||
            !std::isfinite(result.correction[axis]))
            return Halo2WorldCollisionResolution{};
    }
    result.contact = true;
    return result;
}
