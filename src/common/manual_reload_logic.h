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
    // From the gun's own reload animation, once the palette hook has learned it (reload_clip_logic.h).
    bool learned = false;     // the magazine's way in is known (the well and axis above are the animation's)
    float insertDepth = 0;    // metres: where the animation's push begins (0: the weapon's built-in depth)
    float rackTravel = 0;     // metres: the animation's rack stroke (0: the setting)
    bool posedValid = false;  // pushing in: where the posed magazine was drawn
    Vec posedMagazine{};
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
    bool twoHandHeld = false;        // the two-handed hold has the support hand (a grip it took is not a miss)
};

// WHY A GRIP TOOK NOTHING (Campaign Evolved 418e7a5, user 2026-11-15: "sometimes I have trouble interacting with either
// the handle or the support grip"). A fresh support grip closed near the pouch or the rack part that, once the late-grip
// window has passed, holds neither (nor a magazine, nor the two-handed hold) is reported with every gate as it stood at
// the press. Distances are metres; a negative distance is a zone that was not there.
struct GripMiss
{
    Phase phase = Phase::Idle;       // the phase at the press
    float pouchDistance = -1, pouchReach = 0;
    float rackDistance = -1, rackReach = 0;
    bool magazineOut = false;        // a magazine is to be fetched (the pouch can give one)
    bool rackEnabled = false;        // this gun is racked by hand (the setting and the weapon)
    bool rackWork = false;           // the rack has a round to chamber
    bool blockTwoHand = false;       // the reload held the two-handed hold off the support hand
    bool trackingLost = false;       // the support hand's pose read 0,0,0
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
    float rackTravel = 0;            // metres: the rack stroke in use
    float pushDepth = 0;             // pushing in: metres still to go (the palette hook scrubs the animation by it)
    bool followValid = false;        // the support hand rides the magazine or the rack part:
    Vec follow{};                    // ... its drawn target moves by this (tracking space, m)
    uint64_t droppedAt = 0;          // the magazine left (the animation's way out plays from here)
    bool emptyAtDrop = false;        // the game's gun was empty when it left (the game plays its empty reload)
    bool pressPending = false;       // a reload press is out, not yet taken by the game
    bool gripMissed = false;         // `miss` is new this update
    GripMiss miss{};
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
    static constexpr uint64_t kMissCheckMs = kLateGripMs + 50; // a grip that has taken nothing by now is a miss
    static constexpr float kMissNear = 1.5f;         // ... reported only if it closed this many reaches from a zone
    static constexpr float kGripOn = 0.55f, kGripOff = 0.35f;
    static constexpr float kRackDone = 0.92f, kRackRearm = 0.50f;
    // No reading of the held gun for this long (a death and checkpoint revert, a cinematic, a vehicle, a level load):
    // every count is forgotten and each gun is taken as the game has it next time (Campaign Evolved forgets on death
    // and level loads). FA 2026-11-15: a Legendary death reverted the game to a full gun while the player's count
    // still had its magazine out.
    static constexpr uint64_t kForgetMs = 2000;
    static constexpr float kMagazineTopLocal[3]{0, 1, 0}; // the drawn magazine's top, in the controller's frame

    void Reset() noexcept { *this = Engine{}; }
    const reload_ammo::Model& Ammo() const noexcept { return ammo_; }
    Phase CurrentPhase() const noexcept { return phase_; }

    Output Update(const Input& in, const Settings& c) noexcept
    {
        Output out{};
        // A weapon whose magazine moves by its own animation (the Covenant guns) is the player's only once the
        // palette hook has learned that animation: its first reload is the game's own, watched.
        const bool owned = in.weapon.enabled && in.simValid && in.weaponId && in.sim.capacity > 0 &&
                           (!in.weapon.needsLearning || in.geometry.learned);
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
            // Not ours: the game's reload. A short gap keeps each gun's record (a gun put away comes back with its
            // magazine where it was); a long one forgets them all. Either way the phase is worked out again from
            // the held gun's record when one is ours again.
            if (phase_ != Phase::Idle) { phase_ = Phase::Idle; out.lost = true; }
            if (!unownedSince_) unownedSince_ = in.now ? in.now : 1;
            else if (in.now >= unownedSince_ && in.now - unownedSince_ > kForgetMs) ammo_.ForgetAll();
            ammo_.ReleaseHeld();
            weaponId_ = 0;
            pulseUntil_ = 0;
            pressPending_ = false;
            missPending_ = false;
            return out;
        }
        unownedSince_ = 0;
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
                    droppedAt_ = in.now;
                    emptyAtDrop_ = in.sim.loaded == 0;
                    out.primaryHaptic = 0.6f;
                    pressedSinceDrop_ = false;
                    // The game reloads now, unseen, while the hands fetch a magazine.
                    if (ammo_.GameCanReload()) Press(in.now, out);
                    pressRetried_ = false;
                    break;
                }
                if (PerRound(in) && in.pouchValid && (freshGrip || lateGrip) &&
                    weapon_interaction::Near(in.support, in.pouch, c.pouchRadius) &&
                    ammo_.Loaded() < in.sim.capacity && ammo_.ShownReserve() > 0)
                {
                    // A SHELL from the pouch (Campaign Evolved's shell feed). The game loads its shells ahead, unseen:
                    // it is asked as the first shell leaves the pouch, and again whenever it has stopped.
                    phase_ = Phase::MagHeld;
                    grabbedAt_ = Magazine(in);
                    gripOwned_ = true;
                    shellHome_ = false;
                    out.grabbed = true;
                    out.supportHaptic = 0.7f;
                    if (ammo_.GameCanReload() && !in.sim.reloading && !pressPending_) { Press(in.now, out); pressRetried_ = false; }
                    break;
                }
                if (in.weapon.supportGripRacks && in.twoHandHeld && in.weapon.rackChambers && c.rack && RackHasWork() &&
                    in.geometry.rackValid)
                {
                    // THE PUMP UNDER THE SUPPORT HAND: the two-handed grip IS on it; drawing that hand back along the
                    // gun works it, and the hold stays (Campaign Evolved's supportGripRacks).
                    phase_ = Phase::Racking;
                    rackByHold_ = true;
                    rackFrom_ = in.support;
                    rackArmed_ = true;
                    break;
                }
                if (in.weapon.rackChambers && c.rack && RackHasWork() && in.geometry.rackValid &&
                    (freshGrip || lateGrip) &&
                    weapon_interaction::Near(in.support, in.geometry.rack, c.rackRadius))
                {
                    phase_ = Phase::Racking;
                    rackByHold_ = false;
                    rackFrom_ = in.support;
                    rackArmed_ = true;
                    {
                        // Where the hand took the part, in the part's own frame, so the drawn hand rides it.
                        const Vec off = in.support - in.geometry.rack;
                        Vec back{}, up{}, side{};
                        RackFrame(in, back, up, side);
                        rackGrab_[0] = weapon_interaction::Dot(off, back);
                        rackGrab_[1] = weapon_interaction::Dot(off, up);
                        rackGrab_[2] = weapon_interaction::Dot(off, side);
                    }
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
                    out.supportHaptic = 0.7f; // the hand knows it has one without looking (Campaign Evolved 718d716)
                }
                break;
            case Phase::MagHeld:
                if (!gripHeld) { phase_ = PerRound(in) ? Phase::Idle : Phase::MagOut; out.lost = true; break; }
                if (in.geometry.wellValid && Capture(in, c)) phase_ = Phase::Pushing;
                break;
            case Phase::Pushing:
            {
                if (shellHome_) { InsertShell(in, out); break; }   // a shell in the port, waiting on the game
                const float along = Along(in);
                const Vec lateral = Lateral(in, along);
                if (!in.geometry.wellValid || Length(lateral) > c.seatKeep) { phase_ = Phase::MagHeld; break; }
                // Home, or let go on the way in: it goes in.
                if (along >= -c.seatDone || !gripHeld)
                {
                    if (PerRound(in)) { shellHome_ = true; shellHomeAt_ = in.now; InsertShell(in, out); }
                    else Seat(in, out);
                }
                break;
            }
            case Phase::Racking:
            {
                if (!in.geometry.rackValid || (rackByHold_ ? !in.twoHandHeld : !gripHeld))
                {
                    phase_ = Phase::Idle;
                    break;
                }
                if (rackByHold_)
                {
                    // Pumped from the hold: the stroke counts from the furthest-forward place the hand reached, and
                    // the hold goes back to being a hold once the pump is home with nothing left to chamber.
                    if (weapon_interaction::Dot(in.support - rackFrom_, in.geometry.rackBack) < 0) rackFrom_ = in.support;
                    if (!RackHasWork() && rackPull_ < kRackRearm) { phase_ = Phase::Idle; break; }
                }
                const float pull = weapon_interaction::Dot(in.support - rackFrom_, in.geometry.rackBack);
                rackPull_ = std::fmin(std::fmax(pull / std::fmax(RackTravel(in, c), 0.01f), 0.0f), 1.0f);
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
        if (phase_ != Phase::Racking) { rackPull_ = 0; rackByHold_ = false; }
        if (phase_ != Phase::Pushing) shellHome_ = false;
        NoteGrip(in, c, freshGrip, gripHeld, out);

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
        // Pushing in along a learned animation, the gun's own magazine is drawn (posed by the palette hook) and the
        // mod's drawn one goes.
        out.magazineInHand = phase_ == Phase::MagHeld || (phase_ == Phase::Pushing && !in.geometry.learned);
        out.magazineOnAxis = phase_ == Phase::Pushing;
        if (out.magazineInHand)
        {
            const Vec m = Magazine(in);
            out.magazine = phase_ == Phase::Pushing ? OnAxis(in, Along(in)) : m;
        }
        out.rackPull = rackPull_;
        out.rackTravel = RackTravel(in, c);
        out.droppedAt = droppedAt_;
        out.emptyAtDrop = emptyAtDrop_;
        out.pressPending = pressPending_;
        if (phase_ == Phase::Pushing && in.geometry.wellValid)
        {
            // The hand rides the magazine as it goes in: shifted by how far the drawn magazine sits from where the
            // controller carries it (on the axis, or where the gun's animation put it last frame).
            out.pushDepth = std::fmax(-Along(in), 0.f);
            const Vec drawn = in.geometry.posedValid ? in.geometry.posedMagazine : OnAxis(in, Along(in));
            out.follow = drawn - Magazine(in);
            out.followValid = weapon_interaction::Finite(out.follow) && Length(out.follow) < 0.3f;
        }
        else if (phase_ == Phase::Racking && !rackByHold_ && in.geometry.rackValid)
        {
            Vec back{}, up{}, side{};
            RackFrame(in, back, up, side);
            const Vec handle = in.geometry.rack + back * (rackPull_ * RackTravel(in, c));
            const Vec target = handle + back * rackGrab_[0] + up * rackGrab_[1] + side * rackGrab_[2];
            out.follow = target - in.support;
            out.followValid = weapon_interaction::Finite(out.follow) && Length(out.follow) < 0.3f;
        }
        out.consumeSupport = phase_ == Phase::MagHeld || (phase_ == Phase::Pushing && !shellHome_) ||
                             (phase_ == Phase::Racking && !rackByHold_) || (gripOwned_ && gripHeld && !rackByHold_);
        // The two-handed hold may not take a hand that is fetching, holding or pushing a magazine, working the
        // rack, or reaching into a rack zone with work to do (with no rack work the two-handed grip wins).
        out.blockTwoHand = out.consumeSupport || phase_ == Phase::MagOut ||
            // A pump under the support hand is worked from the hold: the hold may keep the hand then.
            (in.weapon.supportGripRacks ? false :
            (RackHasWork() && c.rack && in.weapon.rackChambers && in.geometry.rackValid &&
             weapon_interaction::Near(in.support, in.geometry.rack, c.rackRadius)));
        if (in.now < pulseUntil_) { out.buttons = c.reloadButton; out.pulseUntil = pulseUntil_; }
        out.loaded = ammo_.Loaded();
        out.reserve = ammo_.ShownReserve();
        out.chamber = ammo_.Chamber();
        return out;
    }

private:
    static float RackTravel(const Input& in, const Settings& c) noexcept
    {
        return in.geometry.rackTravel > 0 ? in.geometry.rackTravel : c.rackTravel;
    }
    static void RackFrame(const Input& in, Vec& back, Vec& up, Vec& side) noexcept
    {
        back = Normalized(in.geometry.rackBack);
        Vec u = in.geometry.wellValid ? in.geometry.wellAxis : Vec{0, 1, 0};
        u = Normalized(u - back * weapon_interaction::Dot(u, back));
        if (Length(u) < 0.5f) u = Normalized(Vec{0, 1, 0} - back * back.y);
        up = u;
        side = Vec{back.y * up.z - back.z * up.y, back.z * up.x - back.x * up.z, back.x * up.y - back.y * up.x};
    }
    bool HandHasSomething(const Input& in) const noexcept
    {
        return phase_ == Phase::MagHeld || phase_ == Phase::Pushing || phase_ == Phase::Racking || in.twoHandHeld;
    }
    void NoteGrip(const Input& in, const Settings& c, bool freshGrip, bool gripHeld, Output& out) noexcept
    {
        if (freshGrip && in.ready && !HandHasSomething(in))
        {
            GripMiss m{};
            m.phase = phase_;
            m.magazineOut = !ammo_.MagIn();
            m.rackEnabled = c.rack && in.weapon.rackChambers;
            m.rackWork = RackHasWork();
            m.trackingLost = std::fabs(in.support.x) < 1e-6f && std::fabs(in.support.y) < 1e-6f &&
                             std::fabs(in.support.z) < 1e-6f;
            m.pouchReach = c.pouchRadius;
            m.rackReach = c.rackRadius;
            if (in.pouchValid) m.pouchDistance = Length(in.support - in.pouch);
            if (in.geometry.rackValid) m.rackDistance = Length(in.support - in.geometry.rack);
            const bool nearPouch = m.pouchDistance >= 0 && m.pouchDistance <= c.pouchRadius * kMissNear;
            const bool nearRack = m.rackDistance >= 0 && m.rackDistance <= c.rackRadius * kMissNear;
            // The block as it will stand this update (worked out again below for the output).
            m.blockTwoHand = phase_ == Phase::MagOut ||
                (m.rackWork && m.rackEnabled && in.geometry.rackValid &&
                 weapon_interaction::Near(in.support, in.geometry.rack, c.rackRadius));
            if (nearPouch || nearRack || m.trackingLost)
            {
                miss_ = m;
                missAt_ = in.now;
                missPending_ = true;
            }
        }
        if (!missPending_) return;
        if (!gripHeld || HandHasSomething(in) || !in.ready) { missPending_ = false; return; }
        if (in.now >= missAt_ && in.now - missAt_ >= kMissCheckMs)
        {
            missPending_ = false;
            out.gripMissed = true;
            out.miss = miss_;
        }
    }
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
        const float depth = in.geometry.insertDepth > 0 ? in.geometry.insertDepth : in.weapon.insertDepth;
        if (along < -(depth + 0.10f) || along > 0.10f) return false;
        if (Length(Lateral(in, along)) > c.seatLateral) return false;
        const Vec top = weapon_interaction::Rotate(in.supportRotation,
            {kMagazineTopLocal[0], kMagazineTopLocal[1], kMagazineTopLocal[2]});
        const float cosine = weapon_interaction::Dot(Normalized(top), in.geometry.wellAxis);
        return cosine >= std::cos(c.seatAlignDeg * 3.14159265f / 180.0f);
    }
    static bool PerRound(const Input& in) noexcept { return in.weapon.traits.feed == reload_ammo::Feed::PerRound; }
    // A shell at home in the port: counted once the game has loaded a round for it. A game that has stopped loading
    // is asked again; a shell the game cannot take (full, nothing left) is dropped.
    static constexpr uint64_t kShellWaitMs = 3000;
    void InsertShell(const Input& in, Output& out) noexcept
    {
        switch (ammo_.InsertRound())
        {
        case reload_ammo::Insert::Accepted:
            out.seated = true;
            out.supportHaptic = 1.0f;
            out.primaryHaptic = 0.5f;
            phase_ = Phase::Idle;
            gripOwned_ = true;
            return;
        case reload_ammo::Insert::Wait:
            if (ammo_.GameCanReload() && !in.sim.reloading && !pressPending_) { Press(in.now, out); pressRetried_ = false; }
            if (in.now >= shellHomeAt_ && in.now - shellHomeAt_ <= kShellWaitMs) return;
            [[fallthrough]];
        default:
            out.lost = true;
            phase_ = Phase::Idle;
            return;
        }
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
    uint64_t gripClosedAt_ = 0, unownedSince_ = 0;
    Vec grabbedAt_{}, rackFrom_{};
    bool rackArmed_ = false;
    float rackPull_ = 0;
    uint64_t pulseUntil_ = 0, pressAt_ = 0, catchUpAt_ = 0;
    bool pressPending_ = false, pressRetried_ = false, pressedSinceDrop_ = false;
    int pressLoaded_ = 0, catchUp_ = 0;
    GripMiss miss_{};
    uint64_t missAt_ = 0, droppedAt_ = 0;
    bool emptyAtDrop_ = false;
    float rackGrab_[3]{};
    bool rackByHold_ = false, shellHome_ = false;
    uint64_t shellHomeAt_ = 0;
    bool missPending_ = false;
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
