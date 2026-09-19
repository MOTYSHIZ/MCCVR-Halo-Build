#pragma once
#include <cstdint>

namespace flashlight_input
{
inline constexpr const char* kButtons="Right bumper\0Left bumper\0D-pad up\0D-pad down\0D-pad left\0D-pad right\0X\0Y\0A\0B\0Left stick click\0Right stick click\0Back / View\0Left trigger\0Right trigger\0";
inline constexpr uint32_t kMasks[]{0x200,0x100,1,2,4,8,0x4000,0x8000,0x1000,0x2000,0x40,0x80,0x20,1u<<16,1u<<17};
inline constexpr int kCount=15;
inline constexpr uint32_t Mask(int index) noexcept
{return index>=0&&index<kCount?kMasks[index]:0;}

// Runs on the final merged gamepad, without changing the XR grip sample used
// by two-hand aim, magazines, holsters or the F1 chord. A consumed hold must
// release before disabling/changing this option can expose a new native press.
struct Filter
{
    uint32_t held{},title{};
    template<class Pad> void Apply(Pad& pad,bool enabled,bool gameplay,uint32_t currentTitle,int button) noexcept
    {
        if(title!=currentTitle) {held=0;title=currentTitle;}
        const uint32_t pressed=pad.wButtons|(pad.bLeftTrigger?1u<<16:0)|(pad.bRightTrigger?1u<<17:0);
        held&=pressed;
        if(!gameplay) return;
        const uint32_t blocked=enabled?Mask(button):0;
        held|=pressed&blocked;
        const uint32_t remove=blocked|held;
        pad.wButtons=static_cast<uint16_t>(pad.wButtons&~remove);
        if(remove&(1u<<16)) pad.bLeftTrigger=0;
        if(remove&(1u<<17)) pad.bRightTrigger=0;
    }
};
}
