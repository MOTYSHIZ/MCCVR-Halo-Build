#pragma once
#include "haloce_snapshot.h"
#include "runtime_types.h"
#include <cstdint>

namespace weapon_model
{
struct Observation
{
    GameTitle title{};
    uint32_t generation{};
    uint64_t identity{},space{},at{};
};
class Observations
{
    halo_ce::Snapshot<Observation> titles_[6];
public:
    bool Publish(const Observation& o) noexcept
    {
        const int i=static_cast<int>(o.title)-1;
        return i>=0&&i<6&&o.generation&&o.space&&o.at&&titles_[i].Publish(o);
    }
    uint64_t Read(GameTitle title,uint32_t generation,uint64_t space,uint64_t now) const noexcept
    {
        const int i=static_cast<int>(title)-1;
        Observation o{};
        if(i<0||i>=6||!titles_[i].Read(o)||o.title!=title||o.generation!=generation||
            o.space!=space||!o.at||now<o.at||now-o.at>150) return 0;
        return o.identity;
    }
};
}
