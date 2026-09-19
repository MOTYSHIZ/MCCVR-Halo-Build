#pragma once
#include <cstdint>

namespace flashlight_input
{
// Keep stored explicit-button indices stable; append the corrected default.
inline constexpr int kGripDefault=15;
inline constexpr uint32_t kMasks[]{0x200,0x100,1,2,4,8,0x4000,0x8000,0x1000,0x2000,0x40,0x80,0x20,1u<<16,1u<<17,0x100};
inline constexpr int kCount=16;
inline const char* Label(int index,bool leftHanded,bool reachSwap) noexcept
{
    switch(index) {
    case 0:return leftHanded?"Left grip (RB)":"Right grip (RB)";
    case 1:return leftHanded?"Right grip (LB)":"Left grip (LB)";
    case 2:return "D-pad gesture: up";
    case 3:return "D-pad gesture: down";
    case 4:return "D-pad gesture: left";
    case 5:return "D-pad gesture: right";
    case 6:return reachSwap?(leftHanded?"Right trigger (X)":"Left trigger (X)"):"Left controller X";
    case 7:return "Left controller Y";
    case 8:return "Right controller A";
    case 9:return "Right controller B";
    case 10:return "Left stick click";
    case 11:return "Right stick click";
    case 12:return "Gamepad View (no default Quest button)";
    case 13:return reachSwap?"Left controller X (LT)":(leftHanded?"Right trigger (LT)":"Left trigger (LT)");
    case 14:return leftHanded?"Left trigger (RT)":"Right trigger (RT)";
    case 15:return leftHanded?"Support grip: right (recommended)":"Support grip: left (recommended)";
    default:return "Unknown";
    }
}
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
