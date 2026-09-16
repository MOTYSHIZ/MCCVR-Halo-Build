#pragma once
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <span>

namespace native_reload
{
// Each row is independently matched to its official kit and retail module.
enum Title : unsigned { CE,H2,H3,ODST,Reach,H4,Count };
struct MagazineLayout { uint16_t base,stride; uint8_t timerCount; uint8_t timers[4]; };
inline constexpr MagazineLayout layouts[]{
    {0x280,0x14,1,{2}}, {0x228,0x10,2,{2,12}},
    {0x230,0x18,3,{2,16,20}}, {0x228,0x18,3,{2,16,20}},
    {0x2c0,0x1a,4,{2,16,20,22}}, {0x5a4,0x1a,4,{2,16,20,22}},
};
inline bool SuppressAction(unsigned title,uint32_t action) noexcept
{
    if (title>=Count) return false;
    if (title==CE) return action==9 || action==10 || action==18 || action==19 || action==12;
    if (action>=7 && action<=12) return true;
    if (title==Reach) return action==23 || action==24;
    if (title==H4) return action==25 || action==26;
    return action==20 || action==21;
}
// Shortens only an admitted native reload state. Never changes state, ammo,
// reserve, regeneration, cooling or ownership. Engine update performs transfer.
inline bool ShortenMagazine(unsigned title,std::span<uint8_t> data,int magazine) noexcept
{
    if (title>=Count || magazine<0 || magazine>=2) return false;
    const auto& layout=layouts[title];
    const size_t base=layout.base+size_t(magazine)*layout.stride;
    if (data.size()<base+layout.stride) return false;
    int16_t state=-1;
    std::memcpy(&state,data.data()+base,2);
    if (state<1 || state>(title==CE ? 1 : 3)) return false;
    const int16_t zero=0;
    for (unsigned i=0;i<layout.timerCount;++i)
        std::memcpy(data.data()+base+layout.timers[i],&zero,2);
    return true;
}
}
