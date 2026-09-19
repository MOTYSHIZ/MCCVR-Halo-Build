#pragma once
#include "weapon_interaction_logic.h"
#include <cstring>

namespace weapon_accessory
{
using weapon_interaction::Vec;
using weapon_interaction::Quat;
struct Pose { Vec position{}; Quat orientation{}; };
struct Presentation
{
    const weapon_model::Model* model{};
    Pose pose{};
    GameTitle title{};
    uint32_t generation{};
    uint64_t space{}, sampled{},identity{};
};
inline Presentation Build(const weapon_interaction::Sample& sample,
    const weapon_interaction::Settings& settings,
    const weapon_interaction::Output& output, Quat supportRotation) noexcept
{
    using namespace weapon_interaction;
    const auto* model=weapon_model::Find(sample.title,sample.weaponGraph);
    if(!model&&sample.weaponGraph&&settings.genericVisual) model=&weapon_model::kGenericReloadModel;
    // These H4 kit models have articulated reload assemblies, not a detached
    // clip node. A known zero-vertex model must still receive a visible item.
    if(model&&model->title==GameTitle::Halo4&&!model->vertexCount&&
        std::strstr(model->name,"forerunner_")) model=&weapon_model::kPrometheanReloadModel;
    Vec pouch{},holster{};
    if(!settings.reload||!sample.ready||sample.dualWield||!sample.now||
        !sample.generation||!sample.space||!model||!model->vertexCount||
        !Zones(sample,settings,pouch,holster)||!Normal(supportRotation)) return {};
    Pose pose{};
    if(output.holdingMagazine)
    {
        pose.orientation=supportRotation;
        pose.position=HeldMagazineCenter(sample,supportRotation);
    }
    else
    {
        // Derive the same horizontal basis as the grab zone, including when
        // looking vertically down at it. No engine or world-space transform.
        Vec forward=Rotate(sample.headRotation,{0,0,-1});forward.y=0;
        if(Dot(forward,forward)<0.04f)
        {
            const Vec right=Rotate(sample.headRotation,{1,0,0});
            forward={right.z,0,-right.x};
        }
        const float yaw=std::atan2(-forward.x,-forward.z);
        pose.orientation={0,std::sin(yaw*0.5f),0,std::cos(yaw*0.5f)};
        pose.position=pouch;
    }
    return {model,pose,sample.title,sample.generation,sample.space,sample.now,sample.weaponGraph};
}
inline bool Current(const Presentation& p,GameTitle title,uint32_t generation,
    uint64_t space,uint64_t identity,uint64_t now,bool enabled,bool gameplay) noexcept
{
    return enabled&&p.model&&p.identity==identity&&p.space==space&&
        weapon_interaction::Fresh(now,p.sampled,title,p.title,generation,p.generation,gameplay);
}
// Shader layout: six float4s. OpenXR is right handed and looks along -Z.
struct Constants { float objectQ[4],objectP[4],eyeQ[4],eyeP[4],tangents[4],color[4]; };
inline bool Projection(const Presentation& p,const Pose& eye,
    float left,float right,float down,float up,Constants& out) noexcept
{
    using namespace weapon_interaction;
    if(!p.model||!Finite(p.pose.position)||!Normal(p.pose.orientation)||
        !Finite(eye.position)||!Normal(eye.orientation)) return false;
    for(float a:{left,right,down,up}) if(!std::isfinite(a)||std::fabs(a)>=1.56f) return false;
    const float l=std::tan(left),r=std::tan(right),d=std::tan(down),u=std::tan(up);
    if(r-l<0.01f||u-d<0.01f) return false;
    const auto q=p.pose.orientation;const auto e=eye.orientation;
    out={{q.x,q.y,q.z,q.w},{p.pose.position.x,p.pose.position.y,p.pose.position.z,0},
        {-e.x,-e.y,-e.z,e.w},{eye.position.x,eye.position.y,eye.position.z,0},{l,r,d,u},
        {1,1,1,1}};
    if(p.model==&weapon_model::kGenericReloadModel)
    { out.color[0]=.12f;out.color[1]=.45f;out.color[2]=.65f; }
    if(p.model==&weapon_model::kPrometheanReloadModel)
    { out.color[0]=.95f;out.color[1]=.38f;out.color[2]=.06f; }
    return true;
}
}
