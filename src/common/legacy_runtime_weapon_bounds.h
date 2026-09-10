#pragma once

#include "halo4_world_collision_logic.h"
#include <bit>
#include <cstring>
#include <span>

enum class LegacyBoundsLayout { Halo3, Odst, Reach };

// Independently verified in H3EK 6F5250, ODSTEK 7535B0 and HREK 3FF3A0.
// H3/ODST records are 44 bytes; Reach records are 52. All three position
// fields are interleaved min/max, unlike H4's optional optimized union.
inline constexpr size_t LegacyCompressionRecordBytes(LegacyBoundsLayout layout)
{
    return layout == LegacyBoundsLayout::Reach ? 52 : 44;
}

inline bool LegacyDecodeCompressionBounds(LegacyBoundsLayout layout,
    std::span<const uint8_t> bytes, Halo4WeaponCollisionBounds& output) noexcept
{
    if (bytes.size()!=LegacyCompressionRecordBytes(layout)) return false;
    uint16_t flags{};
    memcpy(&flags,bytes.data(),sizeof(flags));
    if (!(flags&1u)) return false;
    Halo4WeaponCollisionBounds next{};
    for (int axis=0;axis<3;++axis)
    {
        memcpy(&next.minimum[axis],bytes.data()+4+axis*8,sizeof(float));
        memcpy(&next.maximum[axis],bytes.data()+8+axis*8,sizeof(float));
        if (!std::isfinite(next.minimum[axis]) || !std::isfinite(next.maximum[axis]) ||
            next.minimum[axis]>next.maximum[axis] ||
            !std::isfinite(next.maximum[axis]-next.minimum[axis])) return false;
    }
    if (next.minimum[0]==next.maximum[0] && next.minimum[1]==next.maximum[1] &&
        next.minimum[2]==next.maximum[2]) return false;
    output=next;
    return true;
}

// Learn only a complete body prefix containing BOTH native wrists. A gun,
// incomplete LOD remap or camera-control entry cannot establish this boundary.
inline int LegacyBodyPrefixFromRemap(std::span<const int32_t> remap,
    int sourceCount, int cameraControl, int rightWrist, int leftWrist) noexcept
{
    if (remap.empty() || remap.size()>=64 || sourceCount<=0 || sourceCount>64 ||
        cameraControl<=0 || cameraControl>=sourceCount || rightWrist<=0 ||
        leftWrist<=0 || rightWrist==leftWrist ||
        rightWrist>=int(remap.size()) || leftWrist>=int(remap.size())) return -1;
    uint64_t mask=0;
    for (int source:remap)
    {
        if (source<0 || source>=int(remap.size()) || source>=cameraControl ||
            (mask&(uint64_t{1}<<source))) return -1;
        mask|=uint64_t{1}<<source;
    }
    return mask==((uint64_t{1}<<remap.size())-1) ? int(remap.size()) : -1;
}

inline bool LegacyRemapIsAppendedWeapon(std::span<const int32_t> remap,
    int bodyEnd, int cameraControl, int sourceCount) noexcept
{
    if (remap.empty() || remap.size()>64 || bodyEnd<=0 ||
        cameraControl<=bodyEnd || cameraControl>=sourceCount || sourceCount>64 ||
        remap[0]!=bodyEnd) return false;
    // The native appended model's root must be the start of the held graph.
    // Unmapped optional nodes (-1) are legal; a mapped arm/camera is not.
    for (int source:remap)
        if (source!=-1 && (source<bodyEnd || source>=cameraControl)) return false;
    return true;
}

inline uint64_t LegacyRuntimeWeaponShapeId(uint16_t tag, int root,
    const Halo4WeaponCollisionBounds& bounds) noexcept
{
    uint64_t hash=14695981039346656037ull;
    const auto mix=[&](uint32_t word) { hash=(hash^word)*1099511628211ull; };
    mix(tag); mix(uint32_t(root)); mix(bounds.runtimeImportChecksum);
    for (int axis=0;axis<3;++axis)
    { mix(std::bit_cast<uint32_t>(bounds.minimum[axis]));
      mix(std::bit_cast<uint32_t>(bounds.maximum[axis])); }
    return hash ? hash : 1;
}
