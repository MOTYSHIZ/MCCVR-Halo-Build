#pragma once
#include "weapon_interaction_logic.h"
#include "haloce_snapshot.h"
#include "contact_melee_motion.h"

namespace weapon_interaction
{
// Native adapters supply a rendered point and the exact tracking-to-world
// transform used for that palette. Store it relative to the primary controller
// so a later XR sample follows hand motion instead of using a stale world point.
struct ReloadTarget
{
    GameTitle title{};
    uint32_t generation{};
    uint64_t identity{},space{},at{};
    bool leftHanded{};
    Vec primaryLocal{};
};
inline bool BuildReloadTarget(GameTitle title,uint32_t generation,uint64_t identity,
    uint64_t space,uint64_t at,bool leftHanded,contact_melee::Point world,
    const contact_melee::TrackingToWorld& transform,
    const contact_melee::TrackingToWorld& controller,ReloadTarget& out) noexcept
{
    const auto* model=weapon_model::Find(title,identity);
    if(!model||!model->vertexCount||!generation||!space||!at||
        !contact_melee::Finite(world)||!transform.Valid()||!controller.Valid()||
        std::abs(controller.unitsPerMetre-1.f)>.00001f) return false;
    const auto local=controller.Tracking(transform.Tracking(world));
    if(!contact_melee::Finite(local)||contact_melee::Dot(local,local)>4.f) return false;
    out={title,generation,identity,space,at,leftHanded,{local.x,local.y,local.z}};
    return true;
}
class ReloadTargets
{
    halo_ce::Snapshot<ReloadTarget> target_;
public:
    bool Publish(const ReloadTarget& target) noexcept { return target_.Publish(target); }
    bool Read(Sample& sample,bool leftHanded) const noexcept
    {
        sample.receiverValid=false;
        ReloadTarget target{};
        if(!target_.Read(target)||target.title!=sample.title||target.generation!=sample.generation||
            target.identity!=sample.weaponGraph||target.space!=sample.space||
            target.leftHanded!=leftHanded||!target.at||sample.now<target.at||
            sample.now-target.at>100||!Finite(sample.primary)||!Normal(sample.primaryRotation)) return false;
        sample.receiver=sample.primary+Rotate(sample.primaryRotation,target.primaryLocal);
        sample.receiverValid=Finite(sample.receiver);
        return sample.receiverValid;
    }
};
}
