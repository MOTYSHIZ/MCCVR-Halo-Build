#pragma once
#include "contact_melee_motion.h"
#include "halo2_render_logic.h"

// Derive the contact transform from Halo 2's accepted carrier mapping, including
// its recenter frame. Both old and current physical points use this current
// actor transform; native locomotion cannot manufacture controller velocity.
inline bool Halo2BuildContactTrackingTransform(const Halo2ObserverPosePublication& publication,
    float worldScale,contact_melee::TrackingToWorld& output) noexcept
{
    const float identity[]{0,0,0,1};
    Halo2CameraBasis localStock=Halo2ControllerReference(publication);
    for(float& position:localStock.position) position=0;
    Halo2CameraBasis samples[4]{};
    for(unsigned sample=0;sample<4;++sample)
    {
        float point[3]{};
        if(sample) point[sample-1]=1;
        if(!Halo2BuildStableControllerCarrier(localStock,
                publication.referenceOrientation,publication.referencePosition,
                identity,point,worldScale,0,0,0,samples[sample])) return false;
    }
    contact_melee::TrackingToWorld candidate{};
    candidate.unitsPerMetre=worldScale;
    candidate.origin={samples[0].position[0]+publication.stock.position[0],
        samples[0].position[1]+publication.stock.position[1],
        samples[0].position[2]+publication.stock.position[2]};
    if(!std::isfinite(worldScale) || worldScale<=0) return false;
    for(unsigned axis=0;axis<3;++axis)
        candidate.axis[axis]={
            (samples[axis+1].position[0]-samples[0].position[0])/worldScale,
            (samples[axis+1].position[1]-samples[0].position[1])/worldScale,
            (samples[axis+1].position[2]-samples[0].position[2])/worldScale};
    if(!candidate.Valid()) return false;
    output=candidate;
    return true;
}

// Freeze the same observer publication used to place the visible packets.
// The physical right controller remains independent of the support-hand aim
// solver: moving only the support hand must not manufacture a right-hand swing.
inline bool Halo2PreparePhysicalContactFrame(const Halo2ObserverPosePublication& publication,
    unsigned hand,uint32_t unit,uint64_t shape,float worldScale,uint64_t settingsEpoch,
    contact_melee::Frame& output) noexcept
{
    output={};
    output.serial=publication.serial;
    output.timeNs=publication.snapshot.predictedDisplayTimeNs;
    output.unit=unit;
    output.shape=shape;
    output.rigidMotion=true;
    const auto& snapshot=publication.snapshot;
    if(hand>1 || !publication.generation || !publication.serial || !snapshot.valid ||
        !snapshot.trackingSpaceEpoch || snapshot.predictedDisplayTimeNs<=0 ||
        unit==UINT32_MAX || !shape || !settingsEpoch ||
        (hand==0 ? !snapshot.leftControllerValid : !snapshot.independentRightAimValid) ||
        !Halo2BuildContactTrackingTransform(publication,worldScale,output.transform) ||
        !output.controllerPose.SetPose(hand==0 ? snapshot.leftControllerOrientation :
            snapshot.independentRightAimOrientation,hand==0 ? snapshot.leftControllerPosition :
            snapshot.rightAimPosition)) return false;
    uint64_t epoch=(14695981039346656037ull^snapshot.trackingSpaceEpoch)*1099511628211ull;
    epoch=(epoch^publication.generation)*1099511628211ull;
    epoch=(epoch^settingsEpoch)*1099511628211ull;
    for(float value:publication.referenceOrientation)
    { uint32_t bits=0; std::memcpy(&bits,&value,sizeof(bits)); epoch=(epoch^bits)*1099511628211ull; }
    for(float value:publication.referencePosition)
    { uint32_t bits=0; std::memcpy(&bits,&value,sizeof(bits)); epoch=(epoch^bits)*1099511628211ull; }
    output.referenceEpoch=epoch ? epoch : 1;
    return true;
}
