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

enum class Phase : uint8_t { Idle, MagOut, MagHeld, Pushing, Racking, Pulling };
inline constexpr const char* kPhaseNames[]{"idle", "magazine out", "magazine in hand", "pushing in", "racking",
                                           "pulling out"};
inline constexpr int kPhaseCount = 6;

struct Settings
{
    bool rack = true;               // the player racks a gun that ran dry (else seating chambers the round)
    bool lockBack = true;           // a handle or slide waiting to be racked is drawn held back until it is
    float pouchRadius = 0.20f;      // the pouch's reach (the weapon gestures' body zone)
    // The magazine must have moved this far from where it was taken, or closed in on the well by as much (Campaign
    // Evolved's reloadlift). 0, its default since 14b7fb5: a magazine held at the well goes in, whatever brought it
    // there (a gun lowered to the pouch included). A magazine pulled out of the gun always needs kPulledTravel.
    float seatTravel = 0.0f;
    float seatLateral = 0.10f;      // ... come within this of the well's axis (the receiver's reach)
    float seatAlignDeg = 90.0f;     // ... turned no more than this from the right way up
    float seatKeep = 0.30f;         // a push is abandoned if the magazine strays this far off the axis, or below it
    float seatDone = 0.01f;         // seated within this of home
    float rackRadius = 0.10f;       // the rack part's grab reach
    float rackTravel = 0.06f;       // the rack's full pull
    float rackGrabOffset[3]{};      // metres back, up, side from the rack part: where it is taken (calibrated)
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
    bool gunSeen = false;            // some first-person gun is in the hands this tick (ours or not)
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
    bool supportInTwoHandZone = false; // the support hand is where the two-handed hold would take it
    float trigger = 0;               // the gun hand's trigger as the player pulls it (before the hold)
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
    bool rackLock = false;           // ... held back because a round waits to be chambered (not the hand's pull)
    bool rackZoneValid = false;      // where the hand takes the rack part, and how far from it a grip takes it
    Vec rackZone{};
    float rackReach = 0;
    bool rackDue = false;            // a round waits to be chambered by hand
    bool consumeSupport = false;     // the support grip is the reload's
    bool blockTwoHand = false;       // the two-handed hold may not take the support hand
    // A pulse for each hand: its strength, and how long it is felt (Campaign Evolved's lengths).
    float supportHaptic = 0, primaryHaptic = 0;
    uint32_t supportHapticMs = 0, primaryHapticMs = 0;
    // What happened this update (the log).
    bool dropped = false, grabbed = false, seated = false, racked = false, pressed = false, lost = false;
    bool rackedBack = false;         // the rack part reached the back of its stroke (its own click)
    bool pulled = false;             // the magazine was pulled out by hand (it is spent: it goes back with its rounds)
    bool retried = false;            // the game did not take a reload press: it was pressed again
    bool clamped = false;            // the game never caught up: the player's count took the game's
    bool pushAborted = false;        // a magazine on its way in strayed too far: it is back in the hand
    bool carryGrip = false;          // a grip still closed from a seat: hidden from the game, free for the two-handed hold
    float rackTravel = 0;            // metres: the rack stroke in use
    float pushDepth = 0;             // pushing in: metres still to go (the palette hook scrubs the animation by it)
    bool followValid = false;        // the support hand rides the magazine or the rack part:
    Vec follow{};                    // ... its drawn target moves by this (tracking space, m)
    uint64_t droppedAt = 0;          // the magazine left (the animation's way out plays from here)
    bool emptyAtDrop = false;        // the player's gun was dry when it left (the empty reload's animation is used)
    bool pressPending = false;       // a reload press is out, not yet taken by the game
    bool promptRack = false;         // the rack is due and is not being done: show "PULL" here
    Vec prompt{};
    bool promptMag = false;          // the trigger was pulled with nothing to fire and the pouch to go to: mark it
    bool promptShell = false;        // ... and it gives shells, not magazines
    Vec promptMagAt{};
    bool promptWait = false;         // the trigger was pulled on a loaded gun the game is still reloading: "WAIT" here
    Vec promptWaitAt{};
    bool gripMissed = false;         // `miss` is new this update
    GripMiss miss{};
    int loaded = 0, reserve = 0, chamber = 0;
    int shellsWaiting = 0;           // shells in the port the game has not loaded yet
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
    static constexpr int kShellPresses = 8;          // presses for a waiting shell the game never loads, then it is dropped
    static constexpr uint64_t kLateGripMs = 300;     // a grip closed this long before reaching a zone still takes
    static constexpr uint64_t kMissCheckMs = kLateGripMs + 50; // a grip that has taken nothing by now is a miss
    static constexpr float kMissNear = 1.5f;         // ... reported only if it closed this many reaches from a zone
    static constexpr float kGripOn = 0.55f, kGripOff = 0.35f;
    // THE RACK'S STROKE (Campaign Evolved's slide_update). A handle or slide with a round waiting is a short tug and
    // a release: a quarter of its travel back, and the round chambers as the hand lets go (ridden forward again
    // instead, nothing chambers). A pump is back all the way, then forward all the way.
    static constexpr float kRackDone = 0.92f, kPumpHome = 0.15f;
    static constexpr float kRackDue = 0.25f, kRackDueCancel = 0.12f;
    static constexpr float kPulledTravel = 0.08f;    // a magazine pulled out goes back only after moving this far
    static constexpr float kRackDueReach = 1.5f;     // a due handle takes a grip this many reaches away (CampE 30c7c1b)
    static constexpr uint64_t kRackSpringMs = 90;    // the part runs home over this once let go
    static constexpr uint64_t kRackStuckMs = 1500;   // a rack that cannot be worked this long chambers by itself
    // No gun in the hands at all for this long (a death and checkpoint revert, a cinematic, a level load): every
    // count is forgotten and each gun is taken as the game has it next time (Campaign Evolved forgets on death and
    // level loads). FA 2026-11-15: a Legendary death reverted the game to a full gun while the player's count
    // still had its magazine out. Holding a gun the reload does not cover forgets nothing: the rifle on the back
    // keeps its magazine where it was.
    static constexpr uint64_t kForgetMs = 2000;
    // A rack left undone this long gets its prompt (user 2026-11-16: "say pull, but only after 2 seconds of the
    // player not doing so"); a trigger pulled on it shows it at once.
    static constexpr uint64_t kRackPromptMs = 2000;
    // Pulling a magazine out: the hand's magazine within this of the well takes it, and it is out at 90% of the way
    // (Campaign Evolved: "the last centimetres of a full reach are where a player's arm runs out").
    static constexpr float kPullReach = 0.07f, kPullDone = 0.9f;
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
        if (gripValid && in.supportGrip < kGripOff) { gripArmed_ = true; gripOwned_ = false; carry_ = false; }
        bool freshGrip = false;
        if (gripHeld && gripArmed_) { gripArmed_ = false; gripClosedAt_ = in.now; freshGrip = true; }
        const bool lateGrip = gripHeld && !gripOwned_ && gripClosedAt_ && in.now >= gripClosedAt_ &&
                              in.now - gripClosedAt_ <= kLateGripMs;

        if (!owned)
        {
            // Not ours: the game's reload. Each gun's record is kept while any gun is in the hands (a gun put away
            // comes back with its magazine where it was); no gun at all for kForgetMs forgets them. Either way the
            // phase is worked out again from the held gun's record when one is ours again.
            if (phase_ != Phase::Idle) { phase_ = Phase::Idle; out.lost = true; }
            if (in.gunSeen) unseenSince_ = 0;
            else if (!unseenSince_) unseenSince_ = in.now ? in.now : 1;
            else if (in.now >= unseenSince_ && in.now - unseenSince_ > kForgetMs) ammo_.ForgetAll();
            ammo_.ReleaseHeld();
            weaponId_ = 0;
            pulseUntil_ = 0;
            pressPending_ = false;
            missPending_ = false;
            shellsWaiting_ = 0;
            rackedBack_ = false;
            magPrompt_ = rackPromptNow_ = waitPrompt_ = inPouch_ = false;
            return out;
        }
        unseenSince_ = 0;
        const bool pump = in.weapon.supportGripRacks;
        // The rack is the player's to work: the setting, and a gun that has one. Without it a round chambers by
        // itself (the shotgun too: Campaign Evolved switches its cycle when there is no rack to work).
        const bool rackWanted = c.rack && in.weapon.rackChambers;
        reload_ammo::Traits traits = in.weapon.traits;
        traits.chamberOnSeat = !rackWanted;
        if (!rackWanted) traits.cycle = reload_ammo::Cycle::Auto;
        ammo_.Sample(in.weaponId, in.sim, traits);
        out.owned = true;
        const uint64_t now = in.now;

        if (in.weaponId != weaponId_)
        {
            // Another gun in the hand: whatever the hand held is gone; that gun's own record says where its
            // magazine is.
            if (phase_ == Phase::MagHeld || phase_ == Phase::Pushing || phase_ == Phase::Racking ||
               phase_ == Phase::Pulling) out.lost = true;
            phase_ = ammo_.MagIn() ? Phase::Idle : Phase::MagOut;
            weaponId_ = in.weaponId;
            pressPending_ = false;
            catchUp_ = 0;
            pressedSinceDrop_ = false;
            lowSinceDrop_ = in.sim.loaded;
            shellsWaiting_ = 0;
            rackedBack_ = false;
            springAt_ = 0;
        }
        if (!ammo_.MagIn() && in.sim.loaded < lowSinceDrop_) lowSinceDrop_ = in.sim.loaded;
        if (!in.ready)
        {
            // A menu, a cutscene, a seat, lost tracking: a magazine in the hand is let go, nothing else changes.
            if (phase_ == Phase::MagHeld || phase_ == Phase::Pushing || phase_ == Phase::Racking ||
               phase_ == Phase::Pulling)
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
                if (in.reloadTap)
                {
                    // Dry is the PLAYER's gun (Campaign Evolved's review fix): the game has usually refilled its own
                    // already, so its count would always pick the full reload's animation.
                    const bool dry = in.weapon.traits.chamber ? ammo_.Chamber() == 0 : ammo_.Mag() == 0;
                    if (ammo_.DropMag())
                    {
                        phase_ = Phase::MagOut;
                        out.dropped = true;
                        droppedAt_ = now;
                        emptyAtDrop_ = dry;
                        lowSinceDrop_ = in.sim.loaded;
                        Pulse(out.primaryHaptic, out.primaryHapticMs, 0.6f, 50);
                        pressedSinceDrop_ = false;
                        // The game reloads now, unseen, while the hands fetch a magazine.
                        if (ammo_.GameCanReload()) Press(now, out);
                        pressRetried_ = false;
                        break;
                    }
                }
                if (PerRound(in) && in.pouchValid && (freshGrip || lateGrip) &&
                    weapon_interaction::Near(in.support, in.pouch, c.pouchRadius) && PouchHasShell(in))
                {
                    // A SHELL from the pouch (Campaign Evolved's shell feed). The game loads its shells ahead, unseen:
                    // it is asked as the first shell leaves the pouch, and again whenever it has stopped.
                    phase_ = Phase::MagHeld;
                    NoteGrab(in);
                    gripOwned_ = true;
                    out.grabbed = true;
                    Pulse(out.supportHaptic, out.supportHapticMs, 0.7f, 50);
                    if (ammo_.GameCanReload() && !in.sim.reloading && !pressPending_) { Press(now, out); pressRetried_ = false; }
                    break;
                }
                if (pump && in.twoHandHeld && rackWanted && RackHasWork() && in.geometry.rackValid)
                {
                    // THE PUMP UNDER THE SUPPORT HAND: the two-handed grip IS on it; drawing that hand back along the
                    // gun works it, and the hold stays (Campaign Evolved's supportGripRacks).
                    phase_ = Phase::Racking;
                    rackByHold_ = true;
                    rackFrom_ = RackAlong(in);
                    rackedBack_ = false;
                    break;
                }
                if (!PerRound(in) && in.weapon.pullByHand && ammo_.MagIn() && in.geometry.wellValid &&
                    !in.twoHandHeld && (freshGrip || lateGrip) && OnMagazine(in))
                {
                    // PULLING THE MAGAZINE OUT BY HAND (Campaign Evolved's pull): the support grip closed on the
                    // magazine in the gun. A hand on the magazine never takes the rack. Measured along the well, so
                    // moving the gun is not a pull.
                    phase_ = Phase::Pulling;
                    pullAlong_ = SupportAlong(in);
                    pullDepth_ = 0;
                    gripOwned_ = true;
                    carry_ = false;
                    Pulse(out.supportHaptic, out.supportHapticMs, 0.4f, 40);
                    break;
                }
                if (rackWanted && RackHasWork() && in.geometry.rackValid && (freshGrip || lateGrip) &&
                    weapon_interaction::Near(in.support, RackZone(in, c), RackReach(in, c)))
                {
                    phase_ = Phase::Racking;
                    rackByHold_ = false;
                    rackFrom_ = RackAlong(in);
                    rackedBack_ = false;
                    {
                        // Where the hand took the part, in the part's own frame, so the drawn hand rides it.
                        const Vec off = in.support - RackPart(in, c, Locked(in, c) ? 1.0f : 0.0f);
                        Vec back{}, up{}, side{};
                        RackFrame(in, back, up, side);
                        rackGrab_[0] = weapon_interaction::Dot(off, back);
                        rackGrab_[1] = weapon_interaction::Dot(off, up);
                        rackGrab_[2] = weapon_interaction::Dot(off, side);
                    }
                    gripOwned_ = true;
                    Pulse(out.supportHaptic, out.supportHapticMs, 0.5f, 40);
                }
                break;
            case Phase::MagOut:
                // A magazine is taken by a grip HELD at the pouch, however long ago it closed (Campaign Evolved); not
                // by a hand the two-handed hold has, nor by a grip that is already the reload's.
                if (in.pouchValid && gripHeld && !gripOwned_ && !in.twoHandHeld &&
                    weapon_interaction::Near(in.support, in.pouch, c.pouchRadius))
                {
                    phase_ = Phase::MagHeld;
                    NoteGrab(in);
                    gripOwned_ = true;
                    spent_ = false;
                    out.grabbed = true;
                    // The hand knows it has one without looking (Campaign Evolved 718d716).
                    Pulse(out.supportHaptic, out.supportHapticMs, 0.7f, 50);
                }
                break;
            case Phase::MagHeld:
                if (!gripHeld)
                {
                    // Let go: a fresh magazine or a pulled (spent) one falls; the pouch gives the next.
                    phase_ = PerRound(in) ? Phase::Idle : Phase::MagOut;
                    spent_ = false;
                    out.lost = true;
                    break;
                }
                if (in.geometry.wellValid && Capture(in, c))
                {
                    phase_ = Phase::Pushing;
                    // The well's mouth has it (Campaign Evolved's pulse there).
                    Pulse(out.supportHaptic, out.supportHapticMs, 0.4f, 30);
                }
                break;
            case Phase::Pushing:
            {
                const float along = Along(in);
                const Vec lateral = Lateral(in, along);
                // Strayed off the line, or drawn far back down it: it is back in the hand.
                if (!in.geometry.wellValid || Length(lateral) > c.seatKeep || along < -(InsertDepth(in) + c.seatKeep))
                {
                    phase_ = Phase::MagHeld;
                    out.pushAborted = true;
                    break;
                }
                // Home, or let go on the way in: it goes in.
                if (along >= -c.seatDone || !gripHeld)
                {
                    if (PerRound(in)) ShellHome(out);
                    else Seat(in, out);
                }
                break;
            }
            case Phase::Pulling:
            {
                // Let go part way: it goes back in, locked; nothing changes in the count.
                if (!gripHeld || !in.geometry.wellValid) { phase_ = Phase::Idle; pullDepth_ = 0; break; }
                pullDepth_ = std::fmax(pullAlong_ - SupportAlong(in), 0.0f);
                if (pullDepth_ >= InsertDepth(in) * kPullDone)
                {
                    // Out: a spent magazine in the hand. The game reloads now, unseen, as for a drop.
                    const bool dry = in.weapon.traits.chamber ? ammo_.Chamber() == 0 : ammo_.Mag() == 0;
                    if (ammo_.DropMag())
                    {
                        out.dropped = true;
                        out.pulled = true;
                        droppedAt_ = 0;   // no way out to play: the hand took it out
                        emptyAtDrop_ = dry;
                        lowSinceDrop_ = in.sim.loaded;
                        pressedSinceDrop_ = false;
                        if (ammo_.GameCanReload()) Press(now, out);
                        pressRetried_ = false;
                    }
                    phase_ = Phase::MagHeld;
                    spent_ = true;
                    NoteGrab(in);
                    pullDepth_ = 0;
                    Pulse(out.supportHaptic, out.supportHapticMs, 0.7f, 50);
                    Pulse(out.primaryHaptic, out.primaryHapticMs, 0.6f, 50);
                }
                break;
            }
            case Phase::Racking:
            {
                const bool released = rackByHold_ ? !in.twoHandHeld : !gripHeld;
                if (!in.geometry.rackValid || released)
                {
                    // Let go from the back: the part runs home (from where it was drawn) and the round chambers.
                    const float drawn = Locked(in, c) ? 1.0f : rackPull_;
                    if (released && rackedBack_) Chamber(out, pump);
                    EndRack(in, c, drawn);
                    break;
                }
                if (rackByHold_ && !rackedBack_ && !RackHasWork()) { EndRack(in, c, rackPull_); break; }
                const float at = RackAlong(in);
                // Pumped from the hold, the stroke counts from the furthest-forward place the hand reached.
                if (rackByHold_ && at < rackFrom_) rackFrom_ = at;
                rackPull_ = std::fmin(std::fmax((at - rackFrom_) / std::fmax(RackTravel(in, c), 0.01f), 0.0f), 1.0f);
                if (!rackedBack_ && rackPull_ >= (pump ? kRackDone : kRackDue))
                {
                    rackedBack_ = true;
                    out.rackedBack = true;
                    Pulse(out.supportHaptic, out.supportHapticMs, 1.0f, pump ? 150 : 80);
                }
                else if (rackedBack_ && rackPull_ < (pump ? kPumpHome : kRackDueCancel))
                {
                    if (pump)
                    {
                        // Back all the way, then forward all the way: the cycle. The hand is still closed on the
                        // pump: it stays there as the two-handed hold (Campaign Evolved 7793231).
                        Chamber(out, true);
                        if (!rackByHold_) carry_ = true;
                        EndRack(in, c, rackPull_);
                    }
                    else rackedBack_ = false;   // ridden forward again: nothing chambers
                }
                break;
            }
            }
        }
        if (phase_ != Phase::Racking) { rackPull_ = 0; rackByHold_ = false; rackedBack_ = false; }

        // Shells in the port the game had not loaded yet are counted as it loads them.
        while (shellsWaiting_ > 0)
        {
            const reload_ammo::Insert r = ammo_.InsertRound();
            if (r == reload_ammo::Insert::Accepted) { --shellsWaiting_; shellPresses_ = 0; continue; }
            if (r != reload_ammo::Insert::Wait) shellsWaiting_ = 0;
            break;
        }
        // A rack the player cannot work chambers by itself: the setting off, or its part not found for a while.
        if (!rackWanted)
        {
            if (RackHasWork()) (void)ammo_.Rack();
            rackStuckSince_ = 0;
        }
        else if (in.ready && !in.geometry.rackValid && RackHasWork())
        {
            if (!rackStuckSince_) rackStuckSince_ = now ? now : 1;
            else if (now >= rackStuckSince_ && now - rackStuckSince_ > kRackStuckMs) (void)ammo_.Rack();
        }
        else rackStuckSince_ = 0;
        NoteGrip(in, c, freshGrip, gripHeld, out);

        // The game's side: check a press took, catch up a game left behind, never leave it behind for good.
        const reload_ammo::Sim& game = in.sim;
        if (pressPending_ && (game.reloading || game.loaded > pressLoaded_)) pressPending_ = false;
        if (!in.ready)
        {
            // A menu, a cutscene, a seat: the pad drops the press there, so none is made and no time counts
            // against the game.
            if (pressPending_) pressAt_ = now;
            if (catchUpAt_ < now + kPressCheckMs) catchUpAt_ = now + kPressCheckMs;
        }
        else
        {
            if (pressPending_ && now - pressAt_ > kPressCheckMs)
            {
                pressPending_ = false;
                if (!pressRetried_ && ammo_.GameCanReload()) { Press(now, out); pressRetried_ = true; out.retried = true; }
            }
            if (shellsWaiting_ > 0 && !game.reloading && !pressPending_)
            {
                // A waiting shell and a game that has stopped loading: asked again, a few times; a game that cannot
                // take it (full, nothing left) or never does loses the shell.
                if (ammo_.GameCanReload() && shellPresses_ < kShellPresses)
                {
                    ++shellPresses_;
                    Press(now, out);
                    pressRetried_ = true;
                }
                else { shellsWaiting_ = 0; out.lost = true; }
            }
            if (ammo_.MagIn() && ammo_.Behind() > 0 && !game.reloading && !pressPending_ && now >= catchUpAt_)
            {
                if (catchUp_ < kCatchUpPresses && ammo_.GameCanReload())
                {
                    ++catchUp_;
                    catchUpAt_ = now + kCatchUpMs;
                    Press(now, out);
                    pressRetried_ = false;
                }
                else
                {
                    ammo_.ClampToGame();
                    out.clamped = true;
                }
            }
        }
        if (!ammo_.Behind()) catchUp_ = 0;

        out.phase = phase_;
        out.triggerHeld = !ammo_.TriggerOk();
        // Pulled out along a learned animation the gun's own magazine is drawn on it; without one the mod's drawn
        // magazine rides the axis and the gun's is hidden.
        out.hideMagazine = !ammo_.MagIn() || (phase_ == Phase::Pulling && !in.geometry.learned);
        // Pushing in along a learned animation, the gun's own magazine is drawn (posed by the palette hook) and the
        // mod's drawn one goes.
        out.magazineInHand = phase_ == Phase::MagHeld ||
            ((phase_ == Phase::Pushing || phase_ == Phase::Pulling) && !in.geometry.learned);
        out.magazineOnAxis = phase_ == Phase::Pushing || phase_ == Phase::Pulling;
        if (phase_ == Phase::MagHeld || out.magazineOnAxis)
            out.magazine = phase_ == Phase::Pushing ? OnAxis(in, Along(in)) :
                           phase_ == Phase::Pulling ? OnAxis(in, -pullDepth_) : Magazine(in);
        // THE RACK PART AS DRAWN: held back while a round waits on a handle or slide (the hand's tug does not move it
        // further), drawn by the hand's pull otherwise, and run home over kRackSpringMs once let go.
        const bool lock = Locked(in, c);
        float spring = 0;
        if (springAt_ && now >= springAt_ && now - springAt_ < kRackSpringMs)
            spring = springFrom_ * (1.0f - float(now - springAt_) / float(kRackSpringMs));
        else springAt_ = 0;
        const float shownPull = lock ? 1.0f : phase_ == Phase::Racking ? rackPull_ : spring;
        if (lock) { springFrom_ = 1.0f; springAt_ = now ? now : 1; }   // it runs home from the back once chambered
        out.rackPull = shownPull;
        out.rackLock = lock;
        out.rackTravel = RackTravel(in, c);
        if (in.geometry.rackValid && rackWanted)
        {
            out.rackZoneValid = true;
            out.rackZone = RackZone(in, c);
            out.rackDue = RackHasWork();
            out.rackReach = out.rackDue ? RackReach(in, c) : c.rackRadius;
        }
        out.droppedAt = droppedAt_;
        out.emptyAtDrop = emptyAtDrop_;
        out.pressPending = pressPending_;
        out.shellsWaiting = shellsWaiting_;
        if ((phase_ == Phase::Pushing || phase_ == Phase::Pulling) && in.geometry.wellValid)
        {
            // The hand rides the magazine as it goes in (or comes out): shifted by how far the drawn magazine sits
            // from where the controller carries it (on the axis, or where the gun's animation put it last frame).
            const float depth = phase_ == Phase::Pulling ? pullDepth_ : std::fmax(-Along(in), 0.f);
            out.pushDepth = depth;
            const Vec drawn = in.geometry.posedValid ? in.geometry.posedMagazine : OnAxis(in, -depth);
            out.follow = drawn - Magazine(in);
            out.followValid = weapon_interaction::Finite(out.follow) && Length(out.follow) < 0.3f;
        }
        else if (phase_ == Phase::Racking && !rackByHold_ && in.geometry.rackValid)
        {
            Vec back{}, up{}, side{};
            RackFrame(in, back, up, side);
            const Vec handle = RackPart(in, c, shownPull);
            const Vec target = handle + back * rackGrab_[0] + up * rackGrab_[1] + side * rackGrab_[2];
            out.follow = target - in.support;
            out.followValid = weapon_interaction::Finite(out.follow) && Length(out.follow) < 0.3f;
        }
        const bool pulled = std::isfinite(in.trigger) && in.trigger > 0.5f;
        {
            // The rack is due: a round to chamber, the hand not on it (a pump under the two-handed hold counts as
            // not working it until it moves).
            const bool due = in.ready && rackWanted && in.geometry.rackValid && RackHasWork() &&
                             (phase_ == Phase::Idle || (phase_ == Phase::Racking && rackByHold_ && rackPull_ < 0.1f));
            if (!due) { rackDueSince_ = 0; rackPromptNow_ = false; }
            else
            {
                if (!rackDueSince_) rackDueSince_ = now ? now : 1;
                if (pulled) rackPromptNow_ = true;   // a pull that fires nothing says why at once
            }
            if (due && (rackPromptNow_ || (now >= rackDueSince_ && now - rackDueSince_ >= kRackPromptMs)))
            {
                out.promptRack = true;
                out.prompt = RackZone(in, c);
            }
        }
        {
            // MAG: the trigger pulled with the magazine out and nothing left to fire marks the pouch, until a magazine
            // is taken (user 2026-11-16). An empty shotgun's pouch is marked the same way, for a shell.
            const bool shells = PerRound(in) && phase_ == Phase::Idle && ammo_.Loaded() == 0 && shellsWaiting_ == 0 &&
                                ammo_.ShownReserve() > 0;
            if ((phase_ != Phase::MagOut && !shells) || !in.ready) magPrompt_ = false;
            else if (out.triggerHeld && pulled) magPrompt_ = true;
            if (magPrompt_ && in.pouchValid)
            {
                out.promptMag = true;
                out.promptShell = shells;
                out.promptMagAt = in.pouch;
            }
        }
        {
            // WAIT: the trigger pulled on a gun that has its round while the game is still reloading its own, or has
            // not caught up: nothing fires, and this says why (Campaign Evolved shows a ring there).
            const bool round = in.weapon.traits.chamber ? ammo_.Chamber() > 0 : (ammo_.MagIn() && ammo_.Mag() > 0);
            const bool busy = in.ready && round && (game.reloading || game.loaded < ammo_.Loaded());
            if (!busy) waitPrompt_ = false;
            else if (pulled) waitPrompt_ = true;
            if (waitPrompt_ && (in.geometry.rackValid || in.geometry.wellValid))
            {
                out.promptWait = true;
                out.promptWaitAt = in.geometry.rackValid ? RackZone(in, c) : in.geometry.well;
            }
        }
        {
            // The pouch says "here" as a free hand reaches it with something to take (Campaign Evolved's zone buzz,
            // which stays quiet for a hand that is busy).
            const bool gives = phase_ == Phase::MagOut || (PerRound(in) && phase_ == Phase::Idle && PouchHasShell(in));
            const bool inside = in.ready && gives && in.pouchValid && !in.twoHandHeld && !gripOwned_ &&
                                weapon_interaction::Near(in.support, in.pouch, c.pouchRadius);
            if (inside && !inPouch_) Pulse(out.supportHaptic, out.supportHapticMs, 0.5f, 80);
            inPouch_ = inside;
        }
        out.consumeSupport = phase_ == Phase::MagHeld || phase_ == Phase::Pushing ||
                             phase_ == Phase::Pulling || (phase_ == Phase::Racking && !rackByHold_) ||
                             (gripOwned_ && gripHeld && !rackByHold_ && !carry_);
        // A GRIP CARRIED FROM A SEAT (Campaign Evolved 70c5c1b) or from a pump stroke: still closed, it is the
        // two-handed hold's to take as the hand reaches the foregrip (not while a rack is due), and stays hidden
        // from the game until it opens.
        out.carryGrip = carry_ && gripHeld && phase_ == Phase::Idle;
        // The two-handed hold may not take a hand that is fetching, holding or pushing a magazine, working the
        // rack, or reaching into a rack zone with work to do (with no rack work the two-handed grip wins).
        out.blockTwoHand = out.consumeSupport ||
            // The spiker's magazine IS its support grip: no two-handed hold while it is out (twoHandNeedsMagazine).
            (in.weapon.twoHandNeedsMagazine && !ammo_.MagIn()) ||
            // A pump under the support hand is worked from the hold: the hold may keep the hand then.
            (pump ? false :
            (RackHasWork() && rackWanted && in.geometry.rackValid &&
             weapon_interaction::Near(in.support, RackZone(in, c), c.rackRadius)));
        if (now < pulseUntil_) { out.buttons = c.reloadButton; out.pulseUntil = pulseUntil_; }
        out.loaded = ammo_.Loaded();
        out.reserve = ammo_.ShownReserve();
        out.chamber = ammo_.Chamber();
        return out;
    }

private:
    // The stronger of two pulses asked for in one update, with its own length.
    static void Pulse(float& strength, uint32_t& ms, float want, uint32_t forMs) noexcept
    {
        if (want >= strength) { strength = want; ms = forMs; }
    }
    static float RackTravel(const Input& in, const Settings& c) noexcept
    {
        return in.geometry.rackTravel > 0 ? in.geometry.rackTravel : c.rackTravel;
    }
    // A handle or slide with nothing chambered is drawn held back while a rack is due, and while the magazine of a
    // dry gun is out (Campaign Evolved shows the slide locked). Not a pump, which rests forward.
    bool Locked(const Input& in, const Settings& c) const noexcept
    {
        return c.lockBack && c.rack && in.weapon.rackChambers && !in.weapon.supportGripRacks &&
               in.weapon.traits.chamber && in.geometry.rackValid && ammo_.Chamber() == 0 &&
               (RackHasWork() || !ammo_.MagIn());
    }
    // The rack part where it is drawn: its rest place, moved back by its pull.
    static Vec RackPart(const Input& in, const Settings& c, float pull) noexcept
    {
        return in.geometry.rack + Normalized(in.geometry.rackBack) * (pull * RackTravel(in, c));
    }
    // Where the hand takes the rack part: the part as drawn, moved by the gun's calibrated grab spot.
    Vec RackZone(const Input& in, const Settings& c) const noexcept
    {
        Vec back{}, up{}, side{};
        RackFrame(in, back, up, side);
        return RackPart(in, c, Locked(in, c) ? 1.0f : 0.0f) + back * c.rackGrabOffset[0] + up * c.rackGrabOffset[1] +
               side * c.rackGrabOffset[2];
    }
    // A handle with a round waiting takes a grip from further away, unless the hand is where the two-handed hold
    // would take it (Campaign Evolved 30c7c1b: the sniper's handle refused twenty grips at 7-10 cm against 6.5).
    static float RackReach(const Input& in, const Settings& c) noexcept
    {
        return c.rackRadius * (in.supportInTwoHandZone ? 1.0f : kRackDueReach);
    }
    // The support hand along the rack's line, from the part's rest place: gun-relative, so moving the gun is no pull.
    static float RackAlong(const Input& in) noexcept
    {
        return weapon_interaction::Dot(in.support - in.geometry.rack, Normalized(in.geometry.rackBack));
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
        return phase_ == Phase::MagHeld || phase_ == Phase::Pushing || phase_ == Phase::Racking ||
               phase_ == Phase::Pulling || in.twoHandHeld;
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
            m.rackReach = m.rackWork ? RackReach(in, c) : c.rackRadius;
            if (in.pouchValid) m.pouchDistance = Length(in.support - in.pouch);
            if (in.geometry.rackValid) m.rackDistance = Length(in.support - RackZone(in, c));
            const bool nearPouch = m.pouchDistance >= 0 && m.pouchDistance <= c.pouchRadius * kMissNear;
            const bool nearRack = m.rackDistance >= 0 && m.rackDistance <= m.rackReach * kMissNear;
            // The block as it will stand this update (worked out again below for the output).
            m.blockTwoHand = (in.weapon.twoHandNeedsMagazine && !ammo_.MagIn()) ||
                (!in.weapon.supportGripRacks && m.rackWork && m.rackEnabled && in.geometry.rackValid &&
                 weapon_interaction::Near(in.support, RackZone(in, c), c.rackRadius));
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
        // A press while one is still held down is the same press: never a longer one (a hold is the game's pick up).
        if (now >= pulseUntil_) pulseUntil_ = now + kPulseMs;
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
    // The hand itself along the well's line (a turn of the wrist is not a pull).
    static float SupportAlong(const Input& in) noexcept
    { return weapon_interaction::Dot(in.support - in.geometry.well, in.geometry.wellAxis); }
    Vec Lateral(const Input& in, float along) const noexcept
    { return (Magazine(in) - in.geometry.well) - in.geometry.wellAxis * along; }
    Vec OnAxis(const Input& in, float along) const noexcept
    { return in.geometry.well + in.geometry.wellAxis * std::fmin(along, 0.0f); }
    // The hand's magazine where the gun's is: just under the well, close to its axis. A hand on the foregrip is not
    // on the magazine. (A gun whose magazine is where the support hand holds it -- the spiker's foregrip, a pistol's
    // grip -- never pulls by hand: its button drops it, as Campaign Evolved's pullNeedsRelease.)
    bool OnMagazine(const Input& in) const noexcept
    {
        const float along = Along(in);
        return along >= -kPullReach && along <= 0.03f && Length(Lateral(in, along)) <= 0.05f;
    }
    // Where a magazine (or shell) was taken, and how far from the well: the seat gate's measures.
    void NoteGrab(const Input& in) noexcept
    {
        grabbedAt_ = Magazine(in);
        grabbedWell_ = in.geometry.wellValid ? Length(grabbedAt_ - in.geometry.well) : 0.0f;
    }
    static float InsertDepth(const Input& in) noexcept
    {
        return in.geometry.insertDepth > 0 ? in.geometry.insertDepth : in.weapon.insertDepth;
    }
    bool Capture(const Input& in, const Settings& c) const noexcept
    {
        const Vec m = Magazine(in);
        const float travel = spent_ ? std::fmax(c.seatTravel, kPulledTravel) : c.seatTravel;
        if (travel > 0 && Length(m - grabbedAt_) < travel &&
            !(in.geometry.wellValid && grabbedWell_ - Length(m - in.geometry.well) >= travel))
            return false;
        const float along = Along(in);
        // Within the receiver's reach of the line, and along it from that far below the push's start to that far
        // past home (Campaign Evolved's mouth test, one reach for all three).
        if (along < -(InsertDepth(in) + c.seatLateral) || along > c.seatLateral) return false;
        if (Length(Lateral(in, along)) > c.seatLateral) return false;
        const Vec top = weapon_interaction::Rotate(in.supportRotation,
            {kMagazineTopLocal[0], kMagazineTopLocal[1], kMagazineTopLocal[2]});
        const float cosine = weapon_interaction::Dot(Normalized(top), in.geometry.wellAxis);
        return cosine >= std::cos(c.seatAlignDeg * 3.14159265f / 180.0f);
    }
    static bool PerRound(const Input& in) noexcept { return in.weapon.traits.feed == reload_ammo::Feed::PerRound; }
    // The pouch has a shell to give: room in the tube and rounds in reserve, each less the shells already waiting.
    bool PouchHasShell(const Input& in) const noexcept
    {
        return ammo_.Loaded() + shellsWaiting_ < in.sim.capacity && ammo_.ShownReserve() - shellsWaiting_ > 0;
    }
    // A SHELL AT HOME IN THE PORT. The hand is free at once and the next shell can follow: one the game has already
    // loaded a round for is counted now, one ahead of the game WAITS and is counted as the game loads it (Campaign
    // Evolved's waiting shells). One the gun cannot take (full, nothing left) is dropped.
    void ShellHome(Output& out) noexcept
    {
        phase_ = Phase::Idle;
        gripOwned_ = true;
        const reload_ammo::Insert r = ammo_.InsertRound();
        if (r == reload_ammo::Insert::Wait) { ++shellsWaiting_; shellPresses_ = 0; }
        else if (r != reload_ammo::Insert::Accepted) { out.lost = true; return; }
        out.seated = true;
        Pulse(out.supportHaptic, out.supportHapticMs, 0.8f, 60);
        Pulse(out.primaryHaptic, out.primaryHapticMs, 0.8f, 60);
    }
    void Seat(const Input& in, Output& out) noexcept
    {
        // The game has already loaded this magazine only on PROOF (Campaign Evolved): a press went out since the
        // drop, the game is at rest, and it is full, or has nothing left to load, or its count has risen since the
        // drop. A game that ignored the press, or whose reload a melee cut short, has not: the magazine then counts
        // as fresh and full, and the catch-up below has the game load it.
        const bool proof = in.sim.loaded >= in.sim.capacity || in.sim.reserve <= 0 || in.sim.loaded > lowSinceDrop_;
        const bool fromGame = pressedSinceDrop_ && !in.sim.reloading && !pressPending_ && proof;
        // A pulled magazine goes back with the rounds it held; a fresh one is counted from the game.
        const bool seatedOne = spent_ ? ammo_.ReseatDropped() : ammo_.SeatMag(fromGame);
        spent_ = false;
        if (seatedOne)
        {
            out.seated = true;
            Pulse(out.supportHaptic, out.supportHapticMs, 1.0f, 80);
            Pulse(out.primaryHaptic, out.primaryHapticMs, 1.0f, 80);
        }
        phase_ = Phase::Idle;
        // The still-closed grip stays the reload's until let go, but the two-handed hold may take it on the foregrip
        // -- unless the rack is due, which that hand should do next, or the gun loads round by round (the shotgun:
        // its grip opens before it holds the gun).
        gripOwned_ = true;
        carry_ = in.weapon.loadGripHolds && !(in.weapon.rackChambers && RackHasWork());
        catchUpAt_ = in.now + kPressCheckMs;
    }
    // The round goes into the chamber: the slide let go from the back, or the pump brought forward.
    void Chamber(Output& out, bool pump) noexcept
    {
        if (!ammo_.Rack()) return;
        out.racked = true;
        Pulse(out.supportHaptic, out.supportHapticMs, pump ? 1.0f : 0.9f, pump ? 150 : 60);
        Pulse(out.primaryHaptic, out.primaryHapticMs, 0.4f, 40);
    }
    void EndRack(const Input& in, const Settings& c, float drawn) noexcept
    {
        // The part runs home from where it was drawn (still held back if no round went in).
        springFrom_ = Locked(in, c) ? 1.0f : drawn;
        springAt_ = in.now ? in.now : 1;
        phase_ = Phase::Idle;
        rackedBack_ = false;
    }

    reload_ammo::Model ammo_{};
    Phase phase_ = Phase::Idle;
    uint32_t weaponId_ = 0;
    bool gripArmed_ = false, gripOwned_ = false;
    uint64_t gripClosedAt_ = 0, unseenSince_ = 0;
    Vec grabbedAt_{};
    float rackFrom_ = 0;
    bool rackedBack_ = false;
    float rackPull_ = 0, springFrom_ = 0;
    uint64_t springAt_ = 0, rackStuckSince_ = 0;
    uint64_t pulseUntil_ = 0, pressAt_ = 0, catchUpAt_ = 0;
    bool pressPending_ = false, pressRetried_ = false, pressedSinceDrop_ = false;
    int pressLoaded_ = 0, catchUp_ = 0, lowSinceDrop_ = 0;
    int shellsWaiting_ = 0, shellPresses_ = 0;
    GripMiss miss_{};
    uint64_t missAt_ = 0, droppedAt_ = 0;
    bool emptyAtDrop_ = false;
    float rackGrab_[3]{};
    bool rackByHold_ = false, magPrompt_ = false, rackPromptNow_ = false, waitPrompt_ = false, inPouch_ = false;
    bool spent_ = false, carry_ = false;
    float pullAlong_ = 0, pullDepth_ = 0, grabbedWell_ = 0;
    uint64_t rackDueSince_ = 0;
    bool missPending_ = false;
};

// THE RELOAD BUTTON: a tap drops the magazine; a hold is the game's (Halo 3's reload button is also pick up, board
// and use). The press reaches the game as pressed (Campaign Evolved 5c52fd7), so a pick up or a board is never made
// to wait; the reload that press starts in the game is hidden like any other the manual reload did not send. Let go
// within kTapMs it is a tap too -- unless the held gun changed under it (the press picked one up). `withhold`
// (manual_reload_hold_button) is the old way: the press is held back from the game until let go (a tap) or held
// for kHoldMs (then it is the game's from then on). Runs on the thread that builds the game's pad.
class TapFilter
{
public:
    static constexpr uint64_t kTapMs = 250, kHoldMs = 500;
    struct Result { bool swallow = false; bool tap = false; bool pressed = false; };
    void Reset() noexcept { *this = TapFilter{}; }
    // gun: the held gun's identity (any value that changes when it does).
    Result Update(uint64_t now, bool down, bool owned, uint64_t gun = 0, bool withhold = true) noexcept
    {
        Result r{};
        if (!down)
        {
            if (pending_ && owned && gun == gunAtPress_) r.tap = true;
            pending_ = passing_ = false;
            return r;
        }
        if (!pending_ && !passing_)
        {
            // A new press: ours only while manual reload owns the held gun.
            if (!owned) { passing_ = true; return r; }
            pending_ = true;
            pressAt_ = now;
            gunAtPress_ = gun;
            withhold_ = withhold;
            r.pressed = true;
        }
        if (pending_ && now - pressAt_ >= (withhold_ ? kHoldMs : kTapMs)) { pending_ = false; passing_ = true; }
        r.swallow = pending_ && withhold_;
        return r;
    }
private:
    bool pending_ = false, passing_ = false, withhold_ = true;
    uint64_t pressAt_ = 0, gunAtPress_ = 0;
};
} // namespace manual_reload
