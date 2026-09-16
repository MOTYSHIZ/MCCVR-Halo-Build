#pragma once
#include "runtime_types.h"
#include <cmath>
#include <cstdint>
#include <cstddef>

namespace weapon_model
{
inline uint64_t LiveIdentity(uint32_t checksum,uint32_t tag) noexcept
{ return checksum?checksum:(0x8000000000000000ull|uint64_t(tag)); }
struct Model
{
    GameTitle title;
    uint64_t identity;
    const char* name;
    bool needles;
    unsigned nodeCount, firstVertex, vertexCount;
    float minimum[3],maximum[3]; // H2's independently proven compression identity.
};
struct Vertex { float position[3],normal[3]; };
// Unfamiliar held models get a mod-authored interaction token. It does not
// claim the gun takes a box magazine; the native game still decides reloads.
inline constexpr Model kGenericReloadModel{GameTitle::None,0,"generic reload token",false,0,0,36,{},{}};
}
#include "weapon_models.generated.h"
namespace weapon_model
{
inline const Model* Find(GameTitle title,uint64_t identity) noexcept
{
    if(!identity) return nullptr;
    for(const auto& m:kModels) if(m.title==title&&m.identity==identity) return &m;
    return nullptr;
}
inline const Model* FindHalo2(const float minimum[3],const float maximum[3],unsigned nodes) noexcept
{
    const Model* result=nullptr;
    for(int a=0;a<3;++a)
        if(!std::isfinite(minimum[a])||!std::isfinite(maximum[a])||minimum[a]>maximum[a]) return nullptr;
    for(const auto& m:kModels)
    {
        if(m.title!=GameTitle::Halo2||m.nodeCount!=nodes) continue;
        bool match=true;
        for(int a=0;a<3;++a)
            match=match&&std::fabs(minimum[a]-m.minimum[a])<=0.000002f&&
                std::fabs(maximum[a]-m.maximum[a])<=0.000002f;
        if(!match) continue;
        if(result) return nullptr; // ambiguous identities never select a mesh
        result=&m;
    }
    return result;
}
inline uint64_t Halo2LiveIdentity(const float minimum[3],const float maximum[3],unsigned nodes,uint32_t tag) noexcept
{
    if(!nodes||nodes>64||tag==UINT32_MAX) return 0;
    for(int a=0;a<3;++a)
        if(!std::isfinite(minimum[a])||!std::isfinite(maximum[a])||minimum[a]>maximum[a]||
            maximum[a]-minimum[a]>2.0f) return 0;
    if(const auto* model=FindHalo2(minimum,maximum,nodes)) return model->identity;
    // Bounded native identity, not an attempt to guess weapon type from size.
    // Include the tag so switching between differently authored custom guns
    // with identical bounds still cancels a carried reload interaction.
    uint64_t hash=14695981039346656037ull;
    const auto append=[&](const void* data,size_t count) {
        const auto* bytes=static_cast<const unsigned char*>(data);
        for(size_t i=0;i<count;++i) hash=(hash^bytes[i])*1099511628211ull;
    };
    append(minimum,sizeof(float)*3);append(maximum,sizeof(float)*3);
    append(&nodes,sizeof(nodes));append(&tag,sizeof(tag));
    return hash|0x8000000000000000ull;
}
}
