#pragma once
// MANUAL RELOAD BY HAND (Halo 3 / ODST), ported from the Halo: Campaign Evolved VR mod's reload engine
// (core/reload, Gesture.cpp; design: vr-game-conversion references/manual-reload.md).
//
//   tap reload      the magazine drops out of the gun (hidden on the gun). The game is sent its own reload
//                   button at once and reloads by itself, unseen; the player's gun keeps its own count
//                   (reload_ammo_model.h) and the trigger is held back on it.
//   support grip    at the pouch takes a magazine (drawn in the hand).
//   bring it up     to the well under the gun: once it has travelled from the pouch and comes within reach of
//                   the well's axis, turned the right way up, it locks onto the axis and the hand pushes it home.
//   rack            a gun that ran dry needs its handle or slide pulled back once the magazine is in.
//
// Input level only: the game sees the trigger held back and its reload button, never a written count.
// Pure: no game or plugin includes, no heap. One owner thread (the VR input thread).
#include "manual_reload_parts.h"
#include "reload_ammo_model.h"
#include "weapon_interaction_logic.h"
#include <cmath>
#include <cstdint>

namespace manual_reload
{
using weapon_interaction::Vec;
using weapon_interaction::Quat;

enum class Phase : uint8_t { Idle, MagOut, MagHeld, Pushing, Racking };
inline constexpr const char* kPhaseNames[]{"idle", "magazine out", "magazine in hand", "pushing in", "racking"};

struct Settings
{
    bool rack = true;               // the player racks a gun that ran dry (else seating chambers the round)
    float pouchRadius = 0.20f;      // the pouch's reach (the weapon gestures' body zone)
    float seatTravel = 0.08f;       // the magazine must have moved this far from where it was taken
    float seatLateral = 0.10f;      // ... come within this of the well's axis
    float seatAlignDeg = 90.0f;     // ... turned no more than this from the right way up
    float seatKeep = 0.30f;         // a push is abandoned if the magazine strays this far off the axis
    float seatDone = 0.01f;         // seated within this of home
    float rackRadius = 0.10f;       // the rack part's grab reach
    float rackTravel = 0.06f;       // the rack's full pull
    uint32_t reloadButton = 0x4000; // the game's reload binding (weapon_interaction::ReloadButton)
};

// Where the gun's parts are this frame, in tracking space (metres), from the palette hook.
struct Geometry
{
    bool wellValid = false;
    Vec well{};          // the magazine's rest place (the bottom of the well)
    Vec wellAxis{};      // unit: the way a magazine travels going in
    bool rackValid = false;
    Vec rack{};          // the rack part's rest place
    Vec rackBack{};      // unit: the way the rack part is pulled
};

struct Input
{
    uint64_t now = 0;
    bool ready = false;              // focused on-foot gameplay with tracked hands, single weapon
    uint32_t weaponId = 0;           // the held weapon's object handle (each gun keeps its own count)
    Weapon weapon{};                 // BuiltInWeapon for the held model; disabled: the game's own reload
    bool simValid = false;
    reload_ammo::Sim sim{};
    bool reloadTap = false;          // a tap of the reload button since the last update
    Vec support{};
    Quat supportRotation{};
    float supportGrip = 0;
    Vec pouch{};
    bool pouchValid = false;
    Geometry geometry{};
};

struct Output
{
    bool owned = false;              // manual reload runs for the held weapon: its reload tap is ours
    bool triggerHeld = false;        // hold the gun hand's trigger back
    uint32_t buttons = 0;            // the game's reload press, while pulsing
    uint64_t pulseUntil = 0;
    Phase phase = Phase::Idle;
    bool hideMagazine = false;       // the gun's own magazine part is not drawn
    bool magazineInHand = false;     // a magazine is drawn in the support hand
    bool magazineOnAxis = false;     // ... locked onto the well's axis (pushing in)
    Vec magazine{};                  // its centre
    float rackPull = 0;              // 0..1: how far the rack part is drawn back
    bool consumeSupport = false;     // the support grip is the reload's
    bool blockTwoHand = false;       // the two-handed hold may not take the support hand
    float supportHaptic = 0, primaryHaptic = 0;
    // What happened this update (the log).
    bool dropped = false, grabbed = false, seated = false, racked = false, pressed = false, lost = false;
    int loaded = 0, reserve = 0, chamber = 0;
};

inline float Length(Vec v) noexcept { return std::sqrt(weapon_interaction::Dot(v, v)); }
inline Vec Normalized(Vec v) noexcept
{
    const float n = Length(v);
    return std::isfinite(n) && n > 1e-6f ? v * (1.0f / n) : Vec{};
}

class Engine
{
public:
    static constexpr uint64_t kPulseMs = 120;        // a reload press (110 ms is CampE's; 120 is this mod's pulse)
    static constexpr uint64_t kPressCheckMs = 400;   // the game must start reloading within this of a press
    static constexpr uint64_t kCatchUpMs = 1500;     // between catch-up presses
    static constexpr int kCatchUpPresses = 3;
    static constexpr uint64_t kLateGripMs = 300;     // a grip closed this long before reaching a zone still takes
    static constexpr float kGripOn = 0.55f, kGripOff = 0.35f;
    static constexpr float kRackDone = 0.92f, kRackRearm = 0.50f;
    static constexpr float kMagazineTopLocal[3]{0, 1, 0}; // the drawn magazine's top, in the controller's frame

    void Reset() noexcept { *this = Engine{}; }
    const reload_ammo::Model& Ammo() const noexcept { return ammo_; }
    Phase CurrentPhase() const noexcept { return phase_; }

    Output Update(const Input& in, const Settings& c) noexcept
    {
        Output out{};
        const bool owned = in.weapon.enabled && in.simValid && in.weaponId && in.sim.capacity > 0;
        // The support grip, with its fresh-press edge (armed below kGripOff, closed above kGripOn).
        const bool gripValid = std::isfinite(in.supportGrip) && in.supportGrip >= 0 && in.supportGrip <= 1;
        const bool gripHeld = gripValid && in.supportGrip > kGripOn;
        if (gripValid && in.supportGrip < kGripOff) { gripArmed_ = true; gripOwned_ = false; }
        bool freshGrip = false;
        if (gripHeld && gripArmed_) { gripArmed_ = false; gripClosedAt_ = in.now; freshGrip = true; }
        const bool lateGrip = gripHeld && !gripOwned_ && gripClosedAt_ && in.now >= gripClosedAt_ &&
                              in.now - gripClosedAt_ <= kLateGripMs;

        if (!owned)
        {
            // Not ours: the game's reload. A record the player left with its magazine out keeps it out; it comes
            // back when the weapon is ours again (each gun keeps its own count).
            if (phase_ != Phase::Idle) { phase_ = Phase::Idle; out.lost = true; }
            ammo_.ReleaseHeld();
            pulseUntil_ = 0;
            return out;
        }
        reload_ammo::Traits traits = in.weapon.traits;
        traits.chamberOnSeat = !(c.rack && in.weapon.rackChambers);
        ammo_.Sample(in.weaponId, in.sim, traits);
        out.owned = true;

        if (in.weaponId != weaponId_)
        {
            // Another gun in the hand: whatever the hand held is gone; that gun's own record says where its
            // magazine is.
            if (phase_ == Phase::MagHeld || phase_ == Phase::Pushing || phase_ == Phase::Racking) out.lost = true;
            phase_ = ammo_.MagIn() ? Phase::Idle : Phase::MagOut;
            weaponId_ = in.weaponId;
            pressPending_ = false;
            catchUp_ = 0;
            pressedSinceDrop_ = false;
        }
        if (!in.ready)
        {
            // A menu, a cutscene, a seat, lost tracking: a magazine in the hand is let go, nothing else changes.
            if (phase_ == Phase::MagHeld || phase_ == Phase::Pushing || phase_ == Phase::Racking)
            {
                out.lost = true;
                phase_ = ammo_.MagIn() ? Phase::Idle : Phase::MagOut;
            }
        }
        else
        {
            switch (phase_)
            {
            case Phase::Idle:
                if (in.reloadTap && ammo_.DropMag())
                {
                    phase_ = Phase::MagOut;
                    out.dropped = true;
                    out.primaryHaptic = 0.6f;
                    pressedSinceDrop_ = false;
                    // The game reloads now, unseen, while the hands fetch a magazine.
                    if (ammo_.GameCanReload()) Press(in.now, out);
                    pressRetried_ = false;
                    break;
                }
                if (in.weapon.rackChambers && c.rack && RackHasWork() && in.geometry.rackValid &&
                    (freshGrip || lateGrip) &&
                    weapon_interaction::Near(in.support, in.geometry.rack, c.rackRadius))
                {
                    phase_ = Phase::Racking;
                    rackFrom_ = in.support;
                    rackArmed_ = true;
                    gripOwned_ = true;
                    out.supportHaptic = 0.5f;
                }
                break;
            case Phase::MagOut:
                if (in.pouchValid && (freshGrip || lateGrip) &&
                    weapon_interaction::Near(in.support, in.pouch, c.pouchRadius))
                {
                    phase_ = Phase::MagHeld;
                    grabbedAt_ = Magazine(in);
                    gripOwned_ = true;
                    out.grabbed = true;
                    out.supportHaptic = 0.5f;
                }
                break;
            case Phase::MagHeld:
                if (!gripHeld) { phase_ = Phase::MagOut; out.lost = true; break; }
                if (in.geometry.wellValid && Capture(in, c)) phase_ = Phase::Pushing;
                break;
            case Phase::Pushing:
            {
                const float along = Along(in);
                const Vec lateral = Lateral(in, along);
                if (!in.geometry.wellValid || Length(lateral) > c.seatKeep) { phase_ = Phase::MagHeld; break; }
                // Home, or let go on the way in: it goes in.
                if (along >= -c.seatDone || !gripHeld) Seat(in, out);
                break;
            }
            case Phase::Racking:
            {
                if (!gripHeld || !in.geometry.rackValid) { phase_ = Phase::Idle; break; }
                const float pull = weapon_interaction::Dot(in.support - rackFrom_, in.geometry.rackBack);
                rackPull_ = std::fmin(std::fmax(pull / std::fmax(c.rackTravel, 0.01f), 0.0f), 1.0f);
                if (rackArmed_ && rackPull_ >= kRackDone)
                {
                    rackArmed_ = false;
                    if (ammo_.Rack()) { out.racked = true; out.supportHaptic = 1.0f; out.primaryHaptic = 0.4f; }
                }
                else if (!rackArmed_ && rackPull_ < kRackRearm) rackArmed_ = true;
                break;
            }
            }
        }
        if (phase_ != Phase::Racking) rackPull_ = 0;

        // The game's side: check a press took, catch up a game left behind, never leave it behind for good.
        const reload_ammo::Sim& game = in.sim;
        if (pressPending_ && (game.reloading || game.loaded > pressLoaded_)) pressPending_ = false;
        if (pressPending_ && in.now - pressAt_ > kPressCheckMs)
        {
            pressPending_ = false;
            if (!pressRetried_ && ammo_.GameCanReload()) { Press(in.now, out); pressRetried_ = true; }
        }
        if (ammo_.MagIn() && ammo_.Behind() > 0 && !game.reloading && !pressPending_ && in.now >= catchUpAt_)
        {
            if (catchUp_ < kCatchUpPresses && ammo_.GameCanReload())
            {
                ++catchUp_;
                catchUpAt_ = in.now + kCatchUpMs;
                Press(in.now, out);
                pressRetried_ = false;
            }
            else
                ammo_.ClampToGame();
        }
        if (!ammo_.Behind()) catchUp_ = 0;

        out.phase = phase_;
        out.triggerHeld = !ammo_.TriggerOk();
        out.hideMagazine = !ammo_.MagIn();
        out.magazineInHand = phase_ == Phase::MagHeld || phase_ == Phase::Pushing;
        out.magazineOnAxis = phase_ == Phase::Pushing;
        if (out.magazineInHand)
        {
            const Vec m = Magazine(in);
            out.magazine = phase_ == Phase::Pushing ? OnAxis(in, Along(in)) : m;
        }
        out.rackPull = rackPull_;
        out.consumeSupport = phase_ == Phase::MagHeld || phase_ == Phase::Pushing || phase_ == Phase::Racking ||
                             (gripOwned_ && gripHeld);
        // The two-handed hold may not take a hand that is fetching, holding or pushing a magazine, working the
        // rack, or reaching into a rack zone with work to do (with no rack work the two-handed grip wins).
        out.blockTwoHand = out.consumeSupport || phase_ == Phase::MagOut ||
            (RackHasWork() && c.rack && in.weapon.rackChambers && in.geometry.rackValid &&
             weapon_interaction::Near(in.support, in.geometry.rack, c.rackRadius));
        if (in.now < pulseUntil_) { out.buttons = c.reloadButton; out.pulseUntil = pulseUntil_; }
        out.loaded = ammo_.Loaded();
        out.reserve = ammo_.ShownReserve();
        out.chamber = ammo_.Chamber();
        return out;
    }

private:
    bool RackHasWork() const noexcept
    {
        const auto* r = ammo_.HeldRecord();
        return r && r->magIn && r->mag > 0 && r->chamber < reload_ammo::Model::ChamberCap(r->traits);
    }
    void Press(uint64_t now, Output& out) noexcept
    {
        pulseUntil_ = now + kPulseMs;
        pressAt_ = now;
        pressPending_ = true;
        pressLoaded_ = ammo_.Game().loaded;
        pressedSinceDrop_ = true;
        out.pressed = true;
    }
    static Vec Magazine(const Input& in) noexcept
    { return weapon_interaction::HeldMagazineCenter({.support = in.support}, in.supportRotation); }
    float Along(const Input& in) const noexcept
    { return weapon_interaction::Dot(Magazine(in) - in.geometry.well, in.geometry.wellAxis); }
    Vec Lateral(const Input& in, float along) const noexcept
    { return (Magazine(in) - in.geometry.well) - in.geometry.wellAxis * along; }
    Vec OnAxis(const Input& in, float along) const noexcept
    { return in.geometry.well + in.geometry.wellAxis * std::fmin(along, 0.0f); }
    bool Capture(const Input& in, const Settings& c) const noexcept
    {
        const Vec m = Magazine(in);
        if (Length(m - grabbedAt_) < c.seatTravel) return false;
        const float along = Along(in);
        if (along < -(in.weapon.insertDepth + 0.10f) || along > 0.10f) return false;
        if (Length(Lateral(in, along)) > c.seatLateral) return false;
        const Vec top = weapon_interaction::Rotate(in.supportRotation,
            {kMagazineTopLocal[0], kMagazineTopLocal[1], kMagazineTopLocal[2]});
        const float cosine = weapon_interaction::Dot(Normalized(top), in.geometry.wellAxis);
        return cosine >= std::cos(c.seatAlignDeg * 3.14159265f / 180.0f);
    }
    void Seat(const Input& in, Output& out) noexcept
    {
        // The game has already loaded this magazine if a press went out since the drop and it has finished.
        const bool fromGame = pressedSinceDrop_ && !in.sim.reloading && !pressPending_;
        if (ammo_.SeatMag(fromGame))
        {
            out.seated = true;
            out.supportHaptic = 1.0f;
            out.primaryHaptic = 1.0f;
        }
        phase_ = Phase::Idle;
        gripOwned_ = true; // the still-closed grip stays the reload's until let go (no two-handed grab on it)
        catchUpAt_ = in.now + kPressCheckMs;
    }

    reload_ammo::Model ammo_{};
    Phase phase_ = Phase::Idle;
    uint32_t weaponId_ = 0;
    bool gripArmed_ = false, gripOwned_ = false;
    uint64_t gripClosedAt_ = 0;
    Vec grabbedAt_{}, rackFrom_{};
    bool rackArmed_ = false;
    float rackPull_ = 0;
    uint64_t pulseUntil_ = 0, pressAt_ = 0, catchUpAt_ = 0;
    bool pressPending_ = false, pressRetried_ = false, pressedSinceDrop_ = false;
    int pressLoaded_ = 0, catchUp_ = 0;
};

// THE RELOAD BUTTON: a tap drops the magazine, a hold is the game's (Halo 3's reload button is also pick up and
// use). A press is held back from the game; let go within kHoldMs it is a tap, held longer it is passed to the game
// from then on, as a hold. Runs on the thread that builds the game's pad.
class TapFilter
{
public:
    static constexpr uint64_t kHoldMs = 500;
    struct Result { bool swallow = false; bool tap = false; };
    void Reset() noexcept { *this = TapFilter{}; }
    Result Update(uint64_t now, bool down, bool owned) noexcept
    {
        Result r{};
        if (!down)
        {
            if (pending_ && owned) r.tap = true;
            pending_ = passing_ = false;
            return r;
        }
        if (!pending_ && !passing_)
        {
            // A new press: ours only while manual reload owns the held gun.
            if (!owned) { passing_ = true; return r; }
            pending_ = true;
            pressAt_ = now;
        }
        if (pending_ && now - pressAt_ >= kHoldMs) { pending_ = false; passing_ = true; }
        r.swallow = pending_;
        return r;
    }
private:
    bool pending_ = false, passing_ = false;
    uint64_t pressAt_ = 0;
};
} // namespace manual_reload
