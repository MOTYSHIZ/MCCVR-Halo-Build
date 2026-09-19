#pragma once
#include <cstdint>
#include <cstring>

// H4EK player_mapping.cpp / UnitInstance::Update, matched to retail
// 959A8, 95BB4, 43AD8, 5DA400 and 5ECCB4. Not another title's layout.
struct Halo4VehicleInputState
{
    uint32_t unit{UINT32_MAX},parent{UINT32_MAX};
    int16_t seat{-1};
    bool seated{};
};
template<class T> inline T Halo4VehicleRead(const uint8_t* bytes,size_t offset) noexcept
{ T result{};std::memcpy(&result,bytes+offset,sizeof(result));return result; }
inline const uint8_t* Halo4VehicleObject(const uint8_t* table,uint32_t handle,uint32_t kinds) noexcept
{
    if (!table||handle==UINT32_MAX||!(handle>>16)||!table[0x31]||
        Halo4VehicleRead<uint64_t>(table,0x20)!=0x18) return nullptr;
    const auto count=Halo4VehicleRead<int32_t>(table,0x44);
    const auto storage=Halo4VehicleRead<uintptr_t>(table,0x50);
    if (count<=0||count>0x10000||(handle&0xffff)>=uint32_t(count)||!storage||
        storage>UINTPTR_MAX-size_t(count)*0x18) return nullptr;
    const auto* entry=reinterpret_cast<const uint8_t*>(storage)+(handle&0xffff)*0x18;
    if (Halo4VehicleRead<uint16_t>(entry,0)!=uint16_t(handle>>16)||
        entry[4]>=32||!(kinds&(1u<<entry[4]))) return nullptr;
    return Halo4VehicleRead<const uint8_t*>(entry,0x10);
}

// Caller owns the module lifetime, proves the native output-user unit, and
// wraps these memory reads in SEH. A unique input mapping distinguishes the
// local player from spectator/debug output overrides and remote co-op units.
inline bool Halo4ReadVehicleInputMemory(const uint8_t* tls,uint32_t outputUnit,
    Halo4VehicleInputState& output) noexcept
{
    if (!tls||outputUnit==UINT32_MAX||!(outputUnit>>16)) return false;
    const auto* mapping=Halo4VehicleRead<const uint8_t*>(tls,0x138);
    const auto* table=Halo4VehicleRead<const uint8_t*>(tls,0x18);
    if (!mapping||Halo4VehicleRead<uint32_t>(mapping,0xc8)!=outputUnit) return false;
    const auto player=Halo4VehicleRead<uint32_t>(mapping,0xb8);
    if (player==UINT32_MAX||!(player>>16)) return false;
    int matches=0;
    for (int input=0;input<4;++input)
        if (Halo4VehicleRead<uint32_t>(mapping,4+input*4)==player&&
            Halo4VehicleRead<uint32_t>(mapping,0x14+input*4)==outputUnit) ++matches;
    if (matches!=1) return false;
    const auto* biped=Halo4VehicleObject(table,outputUnit,1);
    if (!biped) return false;
    Halo4VehicleInputState next{};next.unit=outputUnit;
    next.parent=Halo4VehicleRead<uint32_t>(biped,0x24);
    next.seat=Halo4VehicleRead<int16_t>(biped,0x2c);
    if (next.parent!=UINT32_MAX)
    {
        if (next.parent==outputUnit||next.seat<0||
            !Halo4VehicleObject(table,next.parent,0x2003)) return false;
        next.seated=true;
    }
    else if (next.seat!=-1) return false;
    output=next;return true;
}
