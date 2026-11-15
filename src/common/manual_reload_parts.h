#pragma once
// Halo 3 / ODST manual reload: which part of each first-person gun is its magazine and which part the hand
// works to chamber a round, from the official editing kits (manual_reload_parts.generated.h), and how each
// weapon feeds (the Campaign Evolved VR mod's per-weapon traits, vr-game-conversion references/manual-reload.md
// §2-§3). A weapon without an entry here reloads the game's way: manual reload never traps the player in a
// reload a weapon cannot finish.
#include "reload_ammo_model.h"
#include "runtime_types.h"
#include "weapon_model_catalog.h"
#include <cstdint>
#include <cstring>

namespace manual_reload
{
enum class RackKind : uint8_t { None, Handle, Slide, Pump };
struct Parts
{
    GameTitle title;
    uint64_t identity;      // the render model's import checksum (weapon_model::Find's identity)
    const char* name;       // the render model's name in the kit
    int nodeCount;
    int magazineNode;       // -1: none
    int rackNode;           // -1: none
    RackKind rack;
};
} // namespace manual_reload

#include "manual_reload_parts.generated.h"

namespace manual_reload
{
inline const Parts* FindParts(GameTitle title, uint64_t identity) noexcept
{
    for (const Parts& parts : kParts)
        if (parts.title == title && parts.identity == identity) return &parts;
    // Project Reclaimer runs ODST's levels on the Halo 3 engine: the active title is Halo 3 while the guns are
    // ODST's own models. Their identities are the kits' pinned checksums, so the ODST rows are safe to try.
    if (title == GameTitle::Halo3) return FindParts(GameTitle::Halo3ODST, identity);
    return nullptr;
}
// The same fallback for the reload catalogue's model (its name and magazine geometry).
inline const weapon_model::Model* FindModel(GameTitle title, uint64_t identity) noexcept
{
    const auto* model = weapon_model::Find(title, identity);
    if (!model && title == GameTitle::Halo3) model = weapon_model::Find(GameTitle::Halo3ODST, identity);
    return model;
}

// How a weapon reloads by hand. Off (`enabled` false) for anything not listed: those reload the game's way.
struct Weapon
{
    bool enabled = false;
    reload_ammo::Traits traits{};
    // The rack part chambers a round after a magazine went into an empty gun (Settings::rack decides whether
    // the player works it; with racking off, seating chambers the round).
    bool rackChambers = false;
    float insertDepth = 0.08f;   // metres below the well where a magazine is first taken onto the axis
};

// The built-in weapons: magazine-fed guns whose magazine goes up into the gun and whose kit model has both a
// magazine and a chambering part. Matched on the kit's render model name, so Halo 3 and ODST share a row.
inline Weapon BuiltInWeapon(const char* name) noexcept
{
    Weapon w{};
    if (!name) return w;
    struct Row { const char* name; int chamberRounds; };
    static constexpr Row kRows[]{
        {"fp_assault_rifle", 1}, {"fp_battle_rifle", 3}, {"fp_smg", 1}, {"fp_smg_silenced", 1},
        {"fp_sniper_rifle", 1}, {"fp_magnum", 1}, {"fp_automag", 1}};
    for (const Row& row : kRows)
        if (std::strcmp(row.name, name) == 0)
        {
            w.enabled = true;
            w.traits.feed = reload_ammo::Feed::Magazine;
            w.traits.cycle = reload_ammo::Cycle::Auto;
            w.traits.chamber = true;
            w.traits.chamberRounds = row.chamberRounds;
            w.rackChambers = true;
            return w;
        }
    return w;
}
} // namespace manual_reload
