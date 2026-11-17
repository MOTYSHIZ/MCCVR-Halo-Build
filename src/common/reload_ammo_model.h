#pragma once
// THE PLAYER'S OWN AMMUNITION: what the VR player's gun holds, kept apart from what the game holds.
// Ported from the Halo: Campaign Evolved VR mod's core/reload/AmmoModel (2026-11-15), which is meant to carry
// to every Halo (vr-game-conversion skill, references/manual-reload.md §1).
//
// The game is let run AHEAD: it is sent its ordinary reload button and reloads by itself, unseen, while the
// player's gun keeps its own count (a magazine, in or out, and a chamber). The trigger is held back on this
// count. Nothing here writes the game: the game only ever sees the trigger and the reload button, so co-op
// stays in step (the host never corrects a count the mod wrote).
//
// Rules:
//   * Ours = magazine + chamber. Total ammunition is the game's (loaded + reserve); rounds the game has loaded
//     and the player has not are shown as still in reserve. Nothing is created or lost.
//   * A shot is the game's loaded count falling, never the trigger. Each round fired takes the chambered round,
//     and a self-loader brings the next one up. A round the game fires that ours does not have floors at 0.
//   * A seated magazine is counted at once even if the game has not finished its own reload; the game is then
//     BEHIND and the trigger waits for it (TriggerOk).
//   * A weapon seen for the first time is taken as the game has it: magazine in, chambered.
//
// Pure: no game or plugin includes, no heap, no clock. Records are keyed by the weapon's identity (its object
// handle) so each gun keeps its own state across swaps. One owner thread.
#include <cstdint>

namespace reload_ammo
{
// One read of the game's magazine.
struct Sim
{
    int loaded = 0;          // rounds loaded
    int reserve = 0;         // rounds inventory
    int capacity = 0;        // rounds loaded maximum (the weapon tag's)
    bool reloading = false;  // the magazine is in a reload state
};

enum class Feed : uint8_t { Magazine, PerRound };
enum class Cycle : uint8_t { Auto, Manual };
struct Traits
{
    Feed feed = Feed::Magazine;
    Cycle cycle = Cycle::Auto;
    // Seating a magazine also chambers a round when the chamber is empty: true wherever the player has no rack.
    bool chamberOnSeat = true;
    // The weapon has a chamber at all (false: it fires straight from the magazine; nothing fires with it out).
    bool chamber = true;
    // How many rounds the chamber holds (a burst weapon keeps its burst: the Battle Rifle's 3).
    int chamberRounds = 1;
};

enum class Insert : uint8_t { Accepted, Wait, Full, NoReserve, NotPerRound, NoWeapon };

struct Record
{
    uint32_t id = 0;
    bool used = false;
    bool magIn = true;
    int mag = 0;        // rounds in the magazine
    int dropped = 0;    // rounds in the magazine that last came out
    int chamber = 0;
    int capacity = 0;
    Traits traits{};
    int lastLoaded = -1;
    uint32_t touched = 0;
};

class Model
{
public:
    static constexpr int kRecords = 16;

    // The held weapon, once per tick. Selects or creates its record and charges the shots since its last sample.
    // A weapon coming back into the hands is not charged for anything that changed while it was away.
    void Sample(uint32_t id, const Sim& sim, const Traits& traits) noexcept
    {
        lastShots_ = lastUnbacked_ = 0;
        sim_ = sim;
        ++clock_;
        bool made = false;
        Record* r = FindOrMake(id, made);
        const bool returning = r != held_;
        r->touched = clock_;
        r->traits = traits;
        r->capacity = sim.capacity;
        if (r->traits.feed == Feed::PerRound) r->magIn = true;
        const int cap = ChamberCap(r->traits);
        // A chamber holding fewer than before: what no longer fits goes back to the magazine -- the one that is out,
        // while it is out, so those rounds come back with it rather than vanishing when its seat recounts the gun
        // (Campaign Evolved 70c5c1b).
        if (r->chamber > cap)
        {
            const int extra = r->chamber - cap;
            if (r->magIn) r->mag += extra; else r->dropped += extra;
            r->chamber = cap;
        }
        if (made)
        {
            r->chamber = Min(Clamp0(sim.loaded), cap);
            r->mag = Clamp0(sim.loaded - r->chamber);
            r->magIn = true;
        }
        else if (!returning && r->lastLoaded >= 0 && sim.loaded < r->lastLoaded)
            Fire(*r, r->lastLoaded - sim.loaded);
        r->lastLoaded = sim.loaded;
        held_ = r;
    }
    void ReleaseHeld() noexcept { held_ = nullptr; lastShots_ = lastUnbacked_ = 0; }

    bool DropMag() noexcept
    {
        if (!held_ || held_->traits.feed != Feed::Magazine || !held_->magIn) return false;
        held_->magIn = false;
        held_->dropped = held_->mag;
        held_->mag = 0;
        return true;
    }
    // A fresh magazine goes in. fromGame: the game has already loaded it (its reload ran while the magazine was
    // out), so it holds what the game counts, less a chambered round fired since.
    bool SeatMag(bool fromGame = false) noexcept
    {
        if (!held_ || held_->traits.feed != Feed::Magazine || held_->magIn) return false;
        Record& r = *held_;
        r.magIn = true;
        r.dropped = 0;
        r.mag = Clamp0(Min(r.capacity - r.chamber, Total() - r.chamber));
        if (fromGame) r.mag = Clamp0(Min(r.mag, sim_.loaded - r.chamber));
        if (r.traits.chamber && r.traits.chamberOnSeat && r.chamber == 0) TopUp(r);
        return true;
    }
    // The magazine that came out goes back in as it was (a cancelled reload).
    bool ReseatDropped() noexcept
    {
        if (!held_ || held_->traits.feed != Feed::Magazine || held_->magIn) return false;
        Record& r = *held_;
        r.magIn = true;
        r.mag = Clamp0(Min(r.dropped, Min(r.capacity - r.chamber, Total() - r.chamber)));
        r.dropped = 0;
        if (r.traits.chamber && r.traits.chamberOnSeat && r.chamber == 0) TopUp(r);
        return true;
    }
    // An empty chamber with a round behind it: chamber it.
    bool Rack() noexcept
    {
        if (!held_) return false;
        Record& r = *held_;
        if (r.chamber >= ChamberCap(r.traits) || !r.magIn || r.mag <= 0) return false;
        TopUp(r);
        return true;
    }
    Insert InsertRound() noexcept
    {
        if (!held_) return Insert::NoWeapon;
        Record& r = *held_;
        if (r.traits.feed != Feed::PerRound) return Insert::NotPerRound;
        const int ours = r.mag + r.chamber;
        if (ours >= r.capacity) return Insert::Full;
        if (ShownReserve() <= 0) return Insert::NoReserve;
        if (ours >= sim_.loaded) return Insert::Wait;
        ++r.mag;
        return Insert::Accepted;
    }
    // The game is behind and will not catch up (a host correction, a reload that never ran): take its count.
    void ClampToGame() noexcept
    {
        if (!held_) return;
        Record& r = *held_;
        int over = (r.mag + r.chamber) - Clamp0(sim_.loaded);
        while (over > 0 && r.mag > 0) { --r.mag; --over; }
        while (over > 0 && r.chamber > 0) { --r.chamber; --over; }
    }
    void ForgetAll() noexcept
    {
        for (auto& r : records_) r = Record{};
        held_ = nullptr;
        lastShots_ = lastUnbacked_ = 0;
    }

    bool HasHeld() const noexcept { return held_ != nullptr; }
    int Loaded() const noexcept { return held_ ? held_->mag + held_->chamber : 0; }
    int ShownReserve() const noexcept { return held_ ? Clamp0(Total() - Loaded()) : 0; }
    int Behind() const noexcept { return held_ ? Clamp0(Loaded() - sim_.loaded) : 0; }
    // A round to fire (chambered, or in the magazine of a weapon without a chamber), and the game has every
    // round we count.
    bool TriggerOk() const noexcept
    {
        if (!held_ || sim_.loaded < Loaded()) return false;
        return held_->traits.chamber ? held_->chamber > 0 : (held_->magIn && held_->mag > 0);
    }
    bool MagIn() const noexcept { return held_ && held_->magIn; }
    int Mag() const noexcept { return held_ ? held_->mag : 0; }
    int Chamber() const noexcept { return held_ ? held_->chamber : 0; }
    int Dropped() const noexcept { return held_ ? held_->dropped : 0; }
    // The game would reload if asked: short of its capacity, with reserve to load from.
    bool GameCanReload() const noexcept
    { return held_ && sim_.loaded < sim_.capacity && sim_.reserve > 0; }
    int LastShots() const noexcept { return lastShots_; }
    int LastUnbacked() const noexcept { return lastUnbacked_; }
    const Record* HeldRecord() const noexcept { return held_; }
    const Sim& Game() const noexcept { return sim_; }

    static int ChamberCap(const Traits& t) noexcept { return t.chamber ? (t.chamberRounds > 1 ? t.chamberRounds : 1) : 0; }

private:
    static int Clamp0(int v) noexcept { return v < 0 ? 0 : v; }
    static int Min(int a, int b) noexcept { return a < b ? a : b; }
    static void TopUp(Record& r) noexcept
    {
        if (!r.magIn) return;
        const int n = Min(ChamberCap(r.traits) - r.chamber, r.mag);
        if (n > 0) { r.mag -= n; r.chamber += n; }
    }
    Record* FindOrMake(uint32_t id, bool& made) noexcept
    {
        made = false;
        for (auto& r : records_) if (r.used && r.id == id) return &r;
        Record* oldest = nullptr;
        for (auto& r : records_)
        {
            if (!r.used) { oldest = &r; break; }
            if (!oldest || r.touched < oldest->touched) oldest = &r;
        }
        if (oldest == held_) held_ = nullptr;
        *oldest = Record{};
        oldest->used = true;
        oldest->id = id;
        made = true;
        return oldest;
    }
    void Fire(Record& r, int shots) noexcept
    {
        for (int i = 0; i < shots; ++i)
        {
            if (!r.traits.chamber)
            {
                if (r.magIn && r.mag > 0) { --r.mag; ++lastShots_; }
                else ++lastUnbacked_;
            }
            else if (r.chamber > 0)
            {
                // One round per shot either way: a manual chamber holding a burst gives one per shot until it is
                // racked (Campaign Evolved 70c5c1b).
                if (r.traits.cycle == Cycle::Manual) --r.chamber;
                else { --r.chamber; TopUp(r); }
                ++lastShots_;
            }
            else if (r.magIn && r.mag > 0) { --r.mag; ++lastShots_; }
            else ++lastUnbacked_;
        }
    }
    int Total() const noexcept { return sim_.loaded + sim_.reserve; }

    Record records_[kRecords]{};
    Record* held_ = nullptr;
    Sim sim_{};
    uint32_t clock_ = 0;
    int lastShots_ = 0, lastUnbacked_ = 0;
};
} // namespace reload_ammo
