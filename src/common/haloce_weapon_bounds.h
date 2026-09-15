#pragma once
#include "haloce_first_person_logic.h"
#include <span>

namespace halo_ce
{
struct WeaponNodeBounds { uint16_t node{};Vec3 minimum{},maximum{}; };
struct WeaponMeshBounds
{
    uint64_t graphIdentity{};
    uint16_t graphCount{};
    uint16_t gunNode{};
    std::span<const WeaponNodeBounds> nodes;
    const char* name{};
};
inline constexpr unsigned kCeWeaponBoundsSamples=14;
}
#include "haloce_weapon_bounds.generated.h"

namespace halo_ce
{
inline const WeaponMeshBounds* FindWeaponMeshBounds(const FirstPersonBinding& binding) noexcept
{
    if (!binding.nodeIdentity) return nullptr;
    for (const auto& model:kCeWeaponMeshBounds)
        if (model.graphIdentity==binding.nodeIdentity&&model.graphCount==binding.count&&
            model.gunNode==binding.gun) return &model;
    return nullptr;
}

// Every positively weighted vertex was enclosed in each influencing node's
// bind-local box. After posing those boxes, their union also encloses every
// weighted vertex: linear skinning is a convex combination of its influences.
// Produce stable gun-local corners/face centres, not moving world-axis labels.
inline bool BuildWeaponMeshBoundsSamples(const FirstPersonBinding& binding,
    const NodeMatrix* palette,Vec3 (&samples)[kCeWeaponBoundsSamples]) noexcept
{
    const auto* mesh=FindWeaponMeshBounds(binding);
    if (!mesh||!palette||binding.gun<0||binding.gun>=binding.count||
        !Valid(palette[binding.gun])) return false;
    const auto& carrier=palette[binding.gun];
    Vec3 minimum{},maximum{};bool seeded=false;
    for (const auto& node:mesh->nodes)
    {
        if (node.node>=binding.count||!(binding.gunMask&(uint64_t{1}<<node.node))||
            !Valid(palette[node.node])||!Finite(node.minimum)||!Finite(node.maximum)||
            node.minimum.x>node.maximum.x||node.minimum.y>node.maximum.y||node.minimum.z>node.maximum.z)
            return false;
        for (unsigned corner=0;corner<8;++corner)
        {
            const Vec3 local{corner&1?node.maximum.x:node.minimum.x,
                corner&2?node.maximum.y:node.minimum.y,corner&4?node.maximum.z:node.minimum.z};
            const auto& transform=palette[node.node];
            const Vec3 world=transform.position+TransformDirection(transform,local)*transform.scale;
            const Vec3 gunLocal=InverseDirection(carrier,world-carrier.position)*(1/carrier.scale);
            if (!Finite(gunLocal)||Dot(gunLocal,gunLocal)>10000) return false;
            if (!seeded) { minimum=maximum=gunLocal;seeded=true; }
            else for (unsigned axis=0;axis<3;++axis)
            {
                if ((&gunLocal.x)[axis]<(&minimum.x)[axis]) (&minimum.x)[axis]=(&gunLocal.x)[axis];
                if ((&gunLocal.x)[axis]>(&maximum.x)[axis]) (&maximum.x)[axis]=(&gunLocal.x)[axis];
            }
        }
    }
    if (!seeded) return false;
    Vec3 staged[kCeWeaponBoundsSamples]{};
    for (unsigned corner=0;corner<8;++corner)
        staged[corner]={corner&1?maximum.x:minimum.x,corner&2?maximum.y:minimum.y,
            corner&4?maximum.z:minimum.z};
    const Vec3 center=(minimum+maximum)*0.5f;
    for (unsigned axis=0;axis<3;++axis)
    {
        staged[8+axis*2]=staged[9+axis*2]=center;
        (&staged[8+axis*2].x)[axis]=(&minimum.x)[axis];
        (&staged[9+axis*2].x)[axis]=(&maximum.x)[axis];
    }
    for (auto& sample:staged)
    {
        sample=carrier.position+TransformDirection(carrier,sample)*carrier.scale;
        if (!Finite(sample)) return false;
    }
    std::memcpy(samples,staged,sizeof(staged));return true;
}
}
