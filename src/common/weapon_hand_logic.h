#pragma once
#include <cstdint>

// Full engine handles only. Inventory slots are engine roles: primary=0,
// secondary=1. The physical controller for each role is a user preference.
constexpr int ResolveEquippedWeaponSlot(uint32_t weapon, uint32_t primary,
    uint32_t secondary, bool primaryValid, bool secondaryValid) noexcept
{
    if (weapon == UINT32_MAX || !(weapon >> 16) || primary == secondary)
        return -1;
    if (primaryValid && primary == weapon) return 0;
    if (primaryValid && secondaryValid && secondary == weapon) return 1;
    return -1;
}

constexpr int PhysicalHandForWeaponSlot(int slot, bool leftHanded) noexcept
{
    return slot == 0 ? (leftHanded ? 0 : 1) :
           slot == 1 ? (leftHanded ? 1 : 0) : -1;
}
