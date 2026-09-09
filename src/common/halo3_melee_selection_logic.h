#pragma once
#include <cstdint>

// H3EK A5DE20 / retail 35A9A4. Values, not engine pointers or copied tag structs.
struct Halo3MeleeResponse
{
    uint8_t material=3;
    uint32_t damage=UINT32_MAX, effect=UINT32_MAX;
    uint32_t clashDamage=UINT32_MAX, clashEffect=UINT32_MAX;
};

inline Halo3MeleeResponse Halo3SelectPhysicalMeleeResponse(
    uint32_t unitDamage, bool armed, uint8_t material,
    uint32_t weaponDamage, uint32_t weaponEffect,
    const Halo3MeleeResponse& authored, bool charged) noexcept
{
    if(!armed) return {3,unitDamage};
    Halo3MeleeResponse result=authored;
    result.material=material&0x3f;
    if(charged) result.clashDamage=result.clashEffect=UINT32_MAX;
    if(result.damage==UINT32_MAX) result.damage=weaponDamage;
    if(result.effect==UINT32_MAX) result.effect=weaponEffect;
    if(result.damage==UINT32_MAX) result.damage=unitDamage;
    return result;
}
