#pragma once
#include <array>
#include "anatomical_palette_logic.h"
#include "controller_finger_input.h"
#include "finger_joint_identity.h"

// H3ODSTEK's recon/ONI FP graphs, independently exported. These are the
// render-model palette's own indices, not combined-animation weapon indices.
// Halo 3's masterchief/fp (H3EK export, docs/PLAYER-IK-PARITY-GAP-2026-11-13.md)
// has the same 37 nodes, names, order and parents, so the same tables serve it;
// each title's bind pose comes from its own loaded tag at run time.
namespace odst_fingers {
using Matrix=Halo4FloatingTransform; // shared math value; no H4 binding
constexpr unsigned Count=37;
using Pose=std::array<Matrix,Count>;
inline constexpr int16_t Parents[Count]{-1,0,0,1,2,3,4,5,5,5,5,5,6,6,6,6,6,
    7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26};
inline constexpr unsigned Chains[2][5][3]{
    {{7,17,27},{8,18,28},{9,19,29},{10,20,30},{11,21,31}},
    {{12,22,32},{13,23,33},{14,24,34},{15,25,35},{16,26,36}}};
// Optional per-finger curls (a support-hand track's hand shape), in the
// track file's order: index, middle, ring, pinky, thumb. When set they replace
// the trigger/grip curls for that side; 0 is the bind shape, 1 closed.
inline constexpr float kMinCurl=-0.25f,kMaxCurl=1.0f;
// Chains[side][finger] digit order is index, middle, pinky, ring, thumb.
inline constexpr unsigned kChainToCurl[5]{0,1,3,2,4};
struct Request {
    uint32_t checksum{};unsigned count{};
    bool allowed[2]{};ControllerFingerInput fingers[2]{}; // anatomical sides
    bool curlsValid[2]{};float curls[2][5]{};
};
// The rigs this solve knows: ODST's recon and ONI first-person models, and
// Halo 3's masterchief/fp (checksum 504041493, 37 nodes).
inline bool PalmMarkers(uint32_t checksum,unsigned count,AnatomicalPalmMarkers& markers) noexcept {
    if(count!=Count)return false;
    if(OdstAnatomicalPalmMarkers(checksum,int(count),markers))return true;
    return checksum==504041493u&&Halo3AnatomicalPalmMarkers(checksum,int(count),markers);
}
inline GameTitle RigTitle(uint32_t checksum) noexcept
{ return checksum==504041493u?GameTitle::Halo3:GameTitle::Halo3ODST; }
// Per-finger curls for one anatomical side (a hand shape, a gesture). The side
// must already be allowed.
inline bool SetSideCurls(Request& request,unsigned side,const float (&curls)[5]) noexcept {
    if(side>1||!request.allowed[side])return false;
    for(const float c:curls)if(!std::isfinite(c)||c<kMinCurl||c>kMaxCurl)return false;
    request.curlsValid[side]=true;
    for(unsigned f=0;f<5;++f)request.curls[side][f]=curls[f];
    return true;
}
inline bool SetSupportCurls(Request& request,const float (&curls)[5]) noexcept {
    for(const float c:curls)if(!std::isfinite(c)||c<kMinCurl||c>kMaxCurl)return false;
    for(unsigned side=0;side<2;++side)if(request.allowed[side]) {
        request.curlsValid[side]=true;
        for(unsigned f=0;f<5;++f)request.curls[side][f]=curls[f];
        return true;
    }
    return false;
}
inline bool Describe(uint32_t checksum,unsigned count,unsigned side,finger_joint::Inventory& out) noexcept {
    if(side>1||count!=Count)return false;
    AnatomicalPalmMarkers markers{};if(!PalmMarkers(checksum,count,markers))return false;
    const unsigned lengths[5]{3,3,3,3,3};
    using D=finger_joint::Digit;const D digits[5]{D::Index,D::Middle,D::Pinky,D::Ring,D::Thumb};
    return finger_joint::Build({RigTitle(checksum),checksum,Count,finger_joint::Palette::FirstPerson},
        side,side+5,Parents,Chains[side],lengths,digits,out);
}
inline bool ConfigureFreeSupport(Request& request,bool handAlignment,bool secondaryPresent,
    bool attached,const ControllerFingerInput& input) noexcept {
    if(secondaryPresent||attached||!input.valid||!std::isfinite(input.trigger)||!std::isfinite(input.grip)||
       input.trigger<0||input.trigger>1||input.grip<0||input.grip>1)return false;
    Request candidate=request;candidate.allowed[0]=candidate.allowed[1]=false;
    const unsigned side=FingerAnatomicalSupportSide(handAlignment);
    candidate.allowed[side]=true;candidate.fingers[side]=input;request=candidate;return true;
}
// The gun hand, while the gun is stowed (put away): its fingers are free too.
// Adds that side without touching the support side's request.
inline bool ConfigureStowedPrimary(Request& request,bool handAlignment,const ControllerFingerInput& input) noexcept {
    if(!input.valid||!std::isfinite(input.trigger)||!std::isfinite(input.grip)||
       input.trigger<0||input.trigger>1||input.grip<0||input.grip>1)return false;
    const unsigned side=1u-FingerAnatomicalSupportSide(handAlignment);
    request.allowed[side]=true;request.fingers[side]=input;request.curlsValid[side]=false;
    return true;
}
inline bool Descendant(unsigned node,unsigned root) noexcept {
    for(unsigned steps=0;node<Count&&steps<Count;++steps) {
        if(node==root)return true;
        const int parent=Parents[node];if(parent<0)return false;node=unsigned(parent);
    }return false;
}
inline bool IsFinger(unsigned node,unsigned side) noexcept {
    return side<2&&node!=side+5&&Descendant(node,side+5);
}
inline bool Normalize(float (&v)[3]) noexcept {
    const float length=std::sqrt(v[0]*v[0]+v[1]*v[1]+v[2]*v[2]);
    if(!std::isfinite(length)||length<1e-6f)return false;
    for(float& value:v)value/=length;return true;
}
inline void Cross(const float* a,const float* b,float (&out)[3]) noexcept {
    out[0]=a[1]*b[2]-a[2]*b[1];out[1]=a[2]*b[0]-a[0]*b[2];out[2]=a[0]*b[1]-a[1]*b[0];
}
inline bool RotateJoint(Pose& candidate,unsigned joint,const float* axis,float angle) noexcept {
    if(joint>=Count||!std::isfinite(angle))return false;
    Matrix rotation{};const float x=axis[0],y=axis[1],z=axis[2],c=std::cos(angle),s=std::sin(angle),t=1-c;
    const float basis[]{t*x*x+c,t*x*y+s*z,t*x*z-s*y,t*x*y-s*z,t*y*y+c,t*y*z+s*x,t*x*z+s*y,t*y*z-s*x,t*z*z+c};
    std::memcpy(rotation.rotation,basis,sizeof(basis));
    for(unsigned a=0;a<3;++a)rotation.translation[a]=candidate[joint].translation[a]-
        (basis[a]*candidate[joint].translation[0]+basis[a+3]*candidate[joint].translation[1]+basis[a+6]*candidate[joint].translation[2]);
    for(unsigned node=0;node<Count;++node)if(Descendant(node,joint)) {
        Matrix next{};if(!Halo4ComposeFloatingTransforms(rotation,candidate[node],next))return false;
        candidate[node]=next;
    }return true;
}
inline bool Solve(const Pose& source,const Pose& inverseBind,const Request& request,Pose& output) noexcept {
    AnatomicalPalmMarkers markers{};
    if(!PalmMarkers(request.checksum,request.count,markers))return false;
    Pose candidate=source;bool any=false;
    for(unsigned side=0;side<2;++side) {
        if(!request.allowed[side])continue;
        const auto& raw=request.fingers[side];
        if(!raw.valid)continue;
        if(!std::isfinite(raw.trigger)||!std::isfinite(raw.grip)||raw.trigger<0||raw.trigger>1||raw.grip<0||raw.grip>1)return false;
        const unsigned wrist=side+5;
        finger_joint::Inventory inventory{};if(!Describe(request.checksum,request.count,side,inventory))return false;
        if(!Halo4FloatingTransformValid(source[wrist])||!Halo4FloatingTransformValid(inverseBind[wrist]))return false;
        // Reset free fingers from their own authored bind, relative to the
        // already committed wrist. This supports an open hand at zero and
        // cannot accumulate curl across repeat submissions or stereo eyes.
        for(unsigned node=0;node<Count;++node)if(IsFinger(node,side)) {
            Matrix bind{},relative{};
            if(!Halo4InvertFloatingTransform(inverseBind[node],bind)||
               !Halo4ComposeFloatingTransforms(inverseBind[wrist],bind,relative)||
               !Halo4ComposeFloatingTransforms(source[wrist],relative,candidate[node]))return false;
        }
        Matrix palm{};
        if(!Halo4ComposeFloatingTransforms(source[wrist],side?markers.right:markers.left,palm))return false;
        const float down[]{-palm.rotation[6],-palm.rotation[7],-palm.rotation[8]};
        for(unsigned finger=0;finger<5;++finger) {
            const auto& chain=Chains[side][finger];
            float direction[]{candidate[chain[2]].translation[0]-candidate[chain[0]].translation[0],
                candidate[chain[2]].translation[1]-candidate[chain[0]].translation[1],
                candidate[chain[2]].translation[2]-candidate[chain[0]].translation[2]};
            float axis[3]{};Cross(direction,down,axis);
            if(!Normalize(axis))return false;
            float curl=finger==0?raw.trigger:raw.grip;
            if(request.curlsValid[side]) {
                curl=request.curls[side][kChainToCurl[finger]];
                if(!std::isfinite(curl)||curl<kMinCurl||curl>kMaxCurl)return false;
            }
            constexpr float flex[3]{.42f,.60f,.74f};
            for(unsigned joint=0;joint<3;++joint)if(!RotateJoint(candidate,inventory.FindNative(finger,joint)->paletteIndex,axis,curl*flex[joint]))return false;
        }
        any=true;
    }
    if(!any)return false;
    output=candidate;return true;
}
}
