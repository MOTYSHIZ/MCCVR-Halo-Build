#pragma once
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <span>

namespace native_reload
{
// Each row is independently matched to its official kit and retail module.
enum Title : unsigned { CE,H2,H3,ODST,Reach,H4,Count };
inline unsigned Options(bool manual,bool disableAuto,bool fullSkip,bool shortened) noexcept
{return manual ? (disableAuto?1u:0u)|(fullSkip?2u:0u)|(!fullSkip&&shortened?4u:0u) : 0u;}
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
inline bool ReloadAction(unsigned title,uint32_t action) noexcept
{
    return title<Count && (title==CE ? (action==9 || action==10 || action==18 || action==19) :
        (action>=7 && action<=12));
}
// A seek is admitted only when an authored insertion leaves a real native tail.
// Keep the transfer countdown running through that tail: this also keeps a
// nonempty magazine from becoming interruptible before the chambering finishes.
// No ammo, reserve, state or initial-duration copy is written.
inline int TailTicks(unsigned title,std::span<const uint8_t> data,int magazine,int elapsed) noexcept
{
    if(title>=Count || magazine<0 || magazine>=2 || elapsed<=0) return 0;
    const auto& l=layouts[title];const size_t b=l.base+size_t(magazine)*l.stride;
    if(data.size()<b+l.stride) return 0;
    int16_t state,total;std::memcpy(&state,data.data()+b,2);std::memcpy(&total,data.data()+b+2,2);
    if((state!=1 && (title==CE || state!=3)) || total<=elapsed) return 0;
    return total-elapsed;
}
inline bool RetainTail(unsigned title,std::span<uint8_t> data,int magazine,int elapsed) noexcept
{
    const int tail=TailTicks(title,data,magazine,elapsed);
    if(!tail) return false;
    const auto& l=layouts[title];const size_t b=l.base+size_t(magazine)*l.stride;
    const int16_t remaining=int16_t(tail),zero=0;
    std::memcpy(data.data()+b+2,&remaining,2);
    if(title!=CE) std::memcpy(data.data()+b+(title==H2?12:16),&remaining,2);
    // H3/ODST's post-transfer delay is +20; Reach/H4 moved it to +22.
    // Transfer now occurs at the end of the retained tail, so it adds no second
    // copy of the same wait. Reach/H4 +20 is a separate native event countdown.
    if(title>=H3) std::memcpy(data.data()+b+(title>=Reach?22:20),&zero,2);
    if(title>=Reach) {
        int16_t event;std::memcpy(&event,data.data()+b+20,2);
        if(event>0) {event=int16_t(event>elapsed?event-elapsed:1);std::memcpy(data.data()+b+20,&event,2);}
    }
    return true;
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
