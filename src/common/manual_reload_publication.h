#pragma once
// What the palette hook (a game call, render thread) hands the VR input thread for the manual reload, and what
// goes back. Pure.
//
// Game -> VR: the held gun's magazine count (read inside the game call: in Project Reclaimer the game's
// thread-local data exists only there), and the places of its well and rack part, stored RELATIVE TO THE
// GUN-HAND CONTROLLER (as weapon_reload_target.h does) so a later controller sample carries them with the hand.
// VR -> game: hide the gun's own magazine part, and how far its rack part is drawn back. Both act only on the
// local first-person palette (multiplayer safety, AGENTS.md).
#include "contact_melee_motion.h"
#include "manual_reload_logic.h"
#include "reload_ammo_model.h"
#include "weapon_interaction_logic.h"
#include <cmath>
#include <cstdint>

namespace manual_reload
{
struct GameSample
{
    GameTitle title{};
    uint32_t generation = 0;
    uint64_t identity = 0;   // the held fp gun's render model (weapon_model identity)
    uint64_t space = 0, at = 0;
    bool leftHanded = false;
    uint32_t weapon = 0;     // the held weapon's object handle
    bool simValid = false;
    reload_ammo::Sim sim{};
    bool wellValid = false;
    Vec well{}, wellAxis{};  // controller-local
    bool rackValid = false;
    Vec rack{}, rackBack{};  // controller-local
};

// World point/direction -> gun-hand-controller-local, through the palette's own tracking transform.
inline bool LocalPoint(const contact_melee::TrackingToWorld& world, const contact_melee::TrackingToWorld& controller,
                       const float point[3], Vec& out) noexcept
{
    if (!point || !world.Valid() || !controller.Valid()) return false;
    const auto local = controller.Tracking(world.Tracking({point[0], point[1], point[2]}));
    if (!contact_melee::Finite(local) || contact_melee::Dot(local, local) > 4.f) return false;
    out = {local.x, local.y, local.z};
    return true;
}
inline bool LocalDirection(const contact_melee::TrackingToWorld& world, const contact_melee::TrackingToWorld& controller,
                           const float origin[3], const float direction[3], Vec& out) noexcept
{
    if (!origin || !direction) return false;
    const float tip[3]{origin[0] + direction[0], origin[1] + direction[1], origin[2] + direction[2]};
    Vec a{}, b{};
    if (!world.Valid() || !controller.Valid()) return false;
    const auto la = controller.Tracking(world.Tracking({origin[0], origin[1], origin[2]}));
    const auto lb = controller.Tracking(world.Tracking({tip[0], tip[1], tip[2]}));
    a = {la.x, la.y, la.z};
    b = {lb.x, lb.y, lb.z};
    out = Normalized(b - a);
    return weapon_interaction::Finite(out) && weapon_interaction::Dot(out, out) > 0.5f;
}

// The sample, if it is this game's, current (100 ms) and for this hand, as tracking-space geometry.
inline bool ReadGameSample(const GameSample& s, GameTitle title, uint32_t generation, uint64_t space,
                           bool leftHanded, uint64_t now, Vec primary, Quat primaryRotation,
                           Geometry& geometry, uint32_t& weapon, reload_ammo::Sim& sim, bool& simValid) noexcept
{
    geometry = {};
    simValid = false;
    weapon = 0;
    if (s.title != title || !generation || s.generation != generation || s.space != space ||
        s.leftHanded != leftHanded || !s.at || now < s.at || now - s.at > 100 ||
        !weapon_interaction::Finite(primary) || !weapon_interaction::Normal(primaryRotation)) return false;
    weapon = s.weapon;
    sim = s.sim;
    simValid = s.simValid && s.weapon;
    if (s.wellValid)
    {
        geometry.wellValid = true;
        geometry.well = primary + weapon_interaction::Rotate(primaryRotation, s.well);
        geometry.wellAxis = Normalized(weapon_interaction::Rotate(primaryRotation, s.wellAxis));
    }
    if (s.rackValid)
    {
        geometry.rackValid = true;
        geometry.rack = primary + weapon_interaction::Rotate(primaryRotation, s.rack);
        geometry.rackBack = Normalized(weapon_interaction::Rotate(primaryRotation, s.rackBack));
    }
    return true;
}

// VR -> game.
struct Presentation
{
    GameTitle title{};
    uint32_t generation = 0;
    uint64_t identity = 0;
    uint64_t at = 0;
    bool hideMagazine = false;
    float rackPull = 0;      // 0..1
    float rackTravel = 0;    // metres
};
inline bool CurrentPresentation(const Presentation& p, GameTitle title, uint32_t generation, uint64_t identity,
                                uint64_t now) noexcept
{
    return p.title == title && generation && p.generation == generation && p.identity == identity && p.at &&
           now >= p.at && now - p.at <= 250;
}
} // namespace manual_reload
