#pragma once
#include <cmath>
#include <cstddef>
#include <cstdint>

// Presentation only. Callers provide verified hand-only node masks after all
// gun, contact, collision and reload computations. Matrices are already composed
// independently: translating a hand node must never propagate to a gun child.
namespace visual_hand
{
struct Offset {float x=0,y=0,z=0;};
inline bool Active(const Offset& value) noexcept
{return value.x!=0||value.y!=0||value.z!=0;}
inline bool Delta(const float basis[9],float unitsPerMeter,const Offset& offset,float result[3]) noexcept
{
    if(!basis||!result||!std::isfinite(unitsPerMeter)||unitsPerMeter<=0||unitsPerMeter>10)return false;
    const float values[]{offset.x,offset.y,offset.z};
    for(float value:values)if(!std::isfinite(value)||std::abs(value)>.2f)return false;
    for(int axis=0;axis<3;++axis)
    {
        float length=0;for(int row=0;row<3;++row){const float v=basis[axis*3+row];if(!std::isfinite(v))return false;length+=v*v;}
        if(std::abs(length-1)>.05f)return false;
    }
    for(int row=0;row<3;++row)
    {
        result[row]=0;for(int column=0;column<3;++column)result[row]+=basis[column*3+row]*values[column]*unitsPerMeter;
        if(!std::isfinite(result[row]))return false;
    }
    return true;
}
// Both deltas are computed before any mutation. In the released role-swapped
// left-handed mode the right mesh follows the physical left controller;
// anatomical alignment restores each mesh to its physical side.
inline bool Deltas(const float leftBasis[9],const float rightBasis[9],float unitsPerMeter,
    Offset physicalLeft,Offset physicalRight,bool leftHanded,bool anatomical,float left[3],float right[3]) noexcept
{
    const bool swap=leftHanded&&!anatomical;
    return Delta(leftBasis,unitsPerMeter,swap?physicalRight:physicalLeft,left)&&
        Delta(rightBasis,unitsPerMeter,swap?physicalLeft:physicalRight,right);
}
template<class Classify,class ReadPosition,class WritePosition>
bool ApplyClassified(size_t count,const float left[3],const float right[3],Classify classify,
    ReadPosition read,WritePosition write) noexcept
{
    if(!count||count>128)return false;
    float staged[128][3]{};uint8_t sides[128]{};
    for(size_t node=0;node<count;++node)
    {
        const auto side=classify(node);if(side<0||side>2)return false;sides[node]=uint8_t(side);if(!side)continue;
        const auto* position=read(node);const float* delta=side==1?left:right;
        if(!position)return false;
        for(int row=0;row<3;++row){staged[node][row]=position[row]+delta[row];if(!std::isfinite(staged[node][row]))return false;}
    }
    for(size_t node=0;node<count;++node)
        if(sides[node])write(node,staged[node]);
    return true;
}
template<class ReadPosition,class WritePosition>
bool Apply(size_t count,uint64_t leftMask,uint64_t rightMask,const float left[3],const float right[3],
    ReadPosition read,WritePosition write) noexcept
{
    if(!count||count>64||(leftMask&rightMask)||!leftMask||!rightMask||
        (count<64&&((leftMask|rightMask)>>count)))return false;
    return ApplyClassified(count,left,right,[&](size_t node){const auto bit=uint64_t{1}<<node;return leftMask&bit?1:rightMask&bit?2:0;},read,write);
}
}
