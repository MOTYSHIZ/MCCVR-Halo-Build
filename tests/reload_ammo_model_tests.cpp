// Ported from the Halo: Campaign Evolved VR mod (Scripts/tests/ammomodel_standalone_test.cpp) for reload_ammo_model.h;
// the original: out-of-tree test for core/reload/AmmoModel (the player's own ammunition count). Compiled by
// Scripts\Verify-AmmoModelStandalone.ps1 with no include path but a copy of the model itself, /W4 /WX.
//
// Each case drives the model the way the reload engine will: one sample() per tick with the game's
// GetMagazine numbers, and the hand events (drop, seat, rack, insert) in between. A small "game" stands
// in for Halo's own rules where a case needs them: a shot takes one loaded round, a reload moves
// min(capacity - loaded, reserve) from reserve to loaded, and it reloads by itself when it is empty.

#include "reload_ammo_model.h"

#include <cstdint>
#include <cstdio>

using namespace reload_ammo;

static int g_fail = 0, g_checks = 0;
#define CHECK(cond, ...) do { ++g_checks; if (!(cond)) { ++g_fail; std::printf("FAIL line %d: ", __LINE__); std::printf(__VA_ARGS__); std::printf("\n"); } } while (0)
#define EQ(a, b) CHECK((a) == (b), "%s == %d, expected %d", #a, (int)(a), (int)(b))

// ---- a stand-in for the game's magazine
struct Game {
    int loaded, reserve, capacity;
    bool reloading = false;
    Sim sim() const { Sim s; s.loaded = loaded; s.reserve = reserve; s.capacity = capacity; s.reloading = reloading; return s; }
    void shoot(int n) { loaded -= n; if (loaded < 0) loaded = 0; }
    void reload() { int take = capacity - loaded; if (take > reserve) take = reserve; if (take < 0) take = 0; loaded += take; reserve -= take; }
    void reload_one() { if (loaded < capacity && reserve > 0) { ++loaded; --reserve; } }
};

static const Traits kRifle{};   // magazine, self-loading, the seat chambers
static Traits rack_pistol() { Traits t; t.chamberOnSeat = false; return t; }
static Traits shotgun() { Traits t; t.feed = Feed::PerRound; t.cycle = Cycle::Manual; t.chamberOnSeat = false; return t; }
static Traits burst_rifle() { Traits t; t.chamberRounds = 3; return t; }   // halo_vr_reloads.json "chamberRounds": 3
static Traits no_chamber() { Traits t; t.chamber = false; return t; }      // "chamber": false (Fuel Rod, Spike, Needle)

constexpr uint32_t BR = 0xE27C000Du, MAGNUM = 0xE27D000Eu, SHOTGUN = 0xE27E000Fu;

// The shown counts always add up to the game's total (or to ours, if ours is ahead of the game).
static void check_conservation(const Model& m, const Game& g, int line) {
    const int total = g.loaded + g.reserve;
    const int want = total > m.Loaded() ? total : m.Loaded();
    ++g_checks;
    if (m.Loaded() + m.ShownReserve() != want) { ++g_fail; std::printf("FAIL line %d: loaded %d + reserve %d != %d\n", line, m.Loaded(), m.ShownReserve(), want); }
}
#define CONSERVED(m, g) check_conservation(m, g, __LINE__)

static void first_sight_and_shots() {
    Model m; Game g{36, 72, 36};
    m.Sample(BR, g.sim(), kRifle);
    EQ(m.Loaded(), 36); EQ(m.Chamber(), 1); EQ(m.Mag(), 35); EQ(m.ShownReserve(), 72); CHECK(m.TriggerOk(), "fresh BR fires");
    g.shoot(3); m.Sample(BR, g.sim(), kRifle);   // one burst
    EQ(m.Loaded(), 33); EQ(m.LastShots(), 3); EQ(m.LastUnbacked(), 0); EQ(m.ShownReserve(), 72); CONSERVED(m, g);
    g.shoot(1); m.Sample(BR, g.sim(), kRifle);
    g.shoot(1); m.Sample(BR, g.sim(), kRifle);
    EQ(m.Loaded(), 31); CONSERVED(m, g);
}

// THE BUG THIS MODEL EXISTS FOR: the game reloads by itself and the counter went full while the VR
// gun had nothing in it.
static void auto_reload_is_not_ours() {
    Model m; Game g{36, 72, 36};
    m.Sample(BR, g.sim(), kRifle);
    while (g.loaded > 0) { g.shoot(3); m.Sample(BR, g.sim(), kRifle); }
    EQ(m.Loaded(), 0); EQ(m.Chamber(), 0); CHECK(!m.TriggerOk(), "empty: trigger held");
    g.reload(); m.Sample(BR, g.sim(), kRifle);   // the game's own reload, nobody asked
    EQ(g.loaded, 36);
    EQ(m.Loaded(), 0);            // still empty in the player's hands
    EQ(m.ShownReserve(), 72);    // the refill is shown as reserve, not as loaded
    CHECK(!m.TriggerOk(), "auto-reloaded game, empty VR gun: trigger held");
    EQ(m.Behind(), 0);
    CONSERVED(m, g);
    // The player's reload: drop, seat. The game is already full, so nothing is waited for.
    CHECK(m.DropMag(), "drop");
    CHECK(!m.DropMag(), "a second drop does nothing");
    CHECK(m.SeatMag(), "seat");
    CHECK(!m.SeatMag(), "a second seat does nothing");
    EQ(m.Loaded(), 36); EQ(m.Chamber(), 1); EQ(m.ShownReserve(), 36); EQ(m.Behind(), 0);
    CHECK(m.TriggerOk(), "reloaded: fires");
    CONSERVED(m, g);
}

// Seated before the game has refilled: counted at once, the trigger waits for the game.
static void seat_before_the_game() {
    Model m; Game g{2, 40, 12};
    m.Sample(MAGNUM, g.sim(), kRifle);
    g.shoot(1); m.Sample(MAGNUM, g.sim(), kRifle);
    g.shoot(1); m.Sample(MAGNUM, g.sim(), kRifle);
    EQ(m.Loaded(), 0);
    CHECK(m.DropMag(), "drop"); CHECK(m.SeatMag(), "seat");
    EQ(m.Loaded(), 12); EQ(m.Behind(), 12); CHECK(!m.TriggerOk(), "game behind: trigger waits");
    EQ(m.ShownReserve(), 28); CONSERVED(m, g);
    CHECK(m.GameCanReload(), "the game would reload");
    g.reloading = true; m.Sample(MAGNUM, g.sim(), kRifle);
    CHECK(!m.TriggerOk(), "still waiting while the game reloads");
    g.reloading = false; g.reload(); m.Sample(MAGNUM, g.sim(), kRifle);
    EQ(m.Behind(), 0); CHECK(m.TriggerOk(), "game caught up: fires"); EQ(m.Loaded(), 12); EQ(m.ShownReserve(), 28);
    CHECK(!m.GameCanReload(), "full game has nothing to reload");
    CONSERVED(m, g);
}

// A tactical reload: rounds left in the dropped magazine go back to the reserve, the chambered round
// can still fire with the magazine out, and the fresh magazine tops the gun up.
static void tactical_reload() {
    Model m; Game g{20, 50, 36};
    m.Sample(BR, g.sim(), kRifle);
    EQ(m.Loaded(), 20);
    CHECK(m.DropMag(), "drop");
    EQ(m.Loaded(), 1); EQ(m.Chamber(), 1); EQ(m.ShownReserve(), 69); CONSERVED(m, g);
    CHECK(m.TriggerOk(), "the chambered round fires with the magazine out");
    g.shoot(1); m.Sample(BR, g.sim(), kRifle);
    EQ(m.Loaded(), 0); EQ(m.ShownReserve(), 69); CHECK(!m.TriggerOk(), "chamber spent");
    CHECK(m.SeatMag(), "seat");
    EQ(m.Loaded(), 36); EQ(m.Behind(), 17); CONSERVED(m, g);
    g.reload(); m.Sample(BR, g.sim(), kRifle);
    EQ(m.Behind(), 0); EQ(m.Loaded(), 36); EQ(m.ShownReserve(), 33); CONSERVED(m, g);
    // The same with the chambered round kept: the fresh magazine fills to capacity, never one past it
    // (the game counts no chamber, so magazine + chamber is the capacity).
    Model k; Game h{20, 50, 36};
    k.Sample(BR, h.sim(), kRifle);
    CHECK(k.DropMag(), "drop"); CHECK(k.SeatMag(), "seat");
    EQ(k.Chamber(), 1); EQ(k.Mag(), 35); EQ(k.Loaded(), 36); EQ(k.Behind(), 16); CONSERVED(k, h);
}

// D6: a Battle Rifle with one chambered round fires its whole burst; ours floors at 0.
static void burst_from_one_round() {
    Model m; Game g{10, 30, 36};
    m.Sample(BR, g.sim(), kRifle);
    CHECK(m.DropMag(), "drop");
    g.shoot(3); m.Sample(BR, g.sim(), kRifle);
    EQ(m.Loaded(), 0); EQ(m.LastShots(), 1); EQ(m.LastUnbacked(), 2);
    EQ(m.ShownReserve(), 37);    // total 37: the burst's two extra rounds came out of the shown reserve
    CONSERVED(m, g);
}

static void pickup_and_partial_reserve() {
    Model m; Game g{5, 0, 36};
    m.Sample(BR, g.sim(), kRifle);
    EQ(m.ShownReserve(), 0);
    g.reserve += 12; m.Sample(BR, g.sim(), kRifle);   // ammo picked up
    EQ(m.Loaded(), 5); EQ(m.ShownReserve(), 12); CONSERVED(m, g);
    while (g.loaded > 0) { g.shoot(1); m.Sample(BR, g.sim(), kRifle); }
    CHECK(m.DropMag(), "drop"); CHECK(m.SeatMag(), "seat");
    EQ(m.Loaded(), 12); EQ(m.ShownReserve(), 0); CONSERVED(m, g);   // only 12 to load: a short magazine
    g.reload(); m.Sample(BR, g.sim(), kRifle);
    EQ(g.loaded, 12); EQ(m.Behind(), 0); CHECK(m.TriggerOk(), "short magazine fires");
    // No reserve at all: an empty seat chambers nothing.
    while (g.loaded > 0) { g.shoot(1); m.Sample(BR, g.sim(), kRifle); }
    CHECK(m.DropMag(), "drop"); CHECK(m.SeatMag(), "seat with nothing to load");
    EQ(m.Loaded(), 0); CHECK(!m.TriggerOk(), "empty magazine");
    CHECK(!m.GameCanReload(), "no reserve: the game cannot reload");
}

// Each weapon keeps its own state across swaps, and coming back charges nothing for the time away.
static void swaps() {
    Model m; Game br{36, 72, 36}, mag{12, 24, 12};
    m.Sample(BR, br.sim(), kRifle);
    br.shoot(6); m.Sample(BR, br.sim(), kRifle);
    CHECK(m.DropMag(), "BR magazine out");
    m.Sample(MAGNUM, mag.sim(), kRifle);   // swap
    EQ(m.Loaded(), 12); CHECK(m.MagIn(), "magnum fresh");
    mag.shoot(2); m.Sample(MAGNUM, mag.sim(), kRifle);
    EQ(m.Loaded(), 10);
    br.reload();                            // the game reloads the BR while it is away (an auto-reload on draw)
    m.Sample(BR, br.sim(), kRifle);         // swap back
    CHECK(!m.MagIn(), "the BR's magazine is still out");
    EQ(m.Loaded(), 1); EQ(m.LastShots(), 0); EQ(m.ShownReserve(), 101); CONSERVED(m, br);
    m.Sample(MAGNUM, mag.sim(), kRifle);
    EQ(m.Loaded(), 10);
    // Unarmed for a while (a vehicle): nothing held, and the return is not charged.
    m.ReleaseHeld();
    CHECK(!m.HasHeld(), "released"); EQ(m.Loaded(), 0); CHECK(!m.TriggerOk(), "no weapon");
    CHECK(!m.DropMag() && !m.SeatMag() && !m.Rack(), "no weapon: hands do nothing");
    EQ((int)m.InsertRound(), (int)Insert::NoWeapon);
    m.Sample(MAGNUM, mag.sim(), kRifle);
    EQ(m.Loaded(), 10); EQ(m.LastShots(), 0);
    // A gun whose count FELL while it was out of the hands (dropped and fired by someone else, a
    // reload cancelled by the swap) is not charged as our shots: ours stays, the game is behind, and
    // the reload press catches it up. Charging it would throw away a magazine the player seated.
    mag.loaded = 3;
    m.Sample(BR, br.sim(), kRifle);
    m.Sample(MAGNUM, mag.sim(), kRifle);
    EQ(m.Loaded(), 10); EQ(m.LastShots(), 0); EQ(m.Behind(), 7);
}

// A cancelled reload (or one the watchdog gave up on): the same magazine goes back in, no fresher.
static void cancelled_reload() {
    Model m; Game g{20, 50, 36};
    m.Sample(BR, g.sim(), kRifle);
    CHECK(m.DropMag(), "drop");
    EQ(m.Loaded(), 1);
    CHECK(m.ReseatDropped(), "re-seat the same magazine");
    CHECK(!m.ReseatDropped(), "a second re-seat does nothing");
    EQ(m.Loaded(), 20); EQ(m.Chamber(), 1); EQ(m.Mag(), 19); EQ(m.Behind(), 0); CONSERVED(m, g);
    // The chambered round fired while it was out: the magazine comes back with what it had.
    CHECK(m.DropMag(), "drop");
    g.shoot(1); m.Sample(BR, g.sim(), kRifle);
    EQ(m.Loaded(), 0);
    CHECK(m.ReseatDropped(), "re-seat");
    EQ(m.Loaded(), 19); EQ(m.Chamber(), 1); EQ(m.Mag(), 18); CONSERVED(m, g);   // self-loading: the seat chambers
    // The game reloaded underneath meanwhile: the old magazine still holds only its own rounds.
    CHECK(m.DropMag(), "drop");
    g.reload(); m.Sample(BR, g.sim(), kRifle);
    CHECK(m.ReseatDropped(), "re-seat");
    EQ(m.Loaded(), 19); CONSERVED(m, g);
    // A seat after a drop never re-seats the old one as well.
    CHECK(m.DropMag(), "drop"); CHECK(m.SeatMag(), "fresh magazine");
    CHECK(!m.ReseatDropped(), "nothing left to re-seat");
    EQ(m.Loaded(), 36);
}

static void death_forgets() {
    Model m; Game g{36, 72, 36};
    m.Sample(BR, g.sim(), kRifle);
    CHECK(m.DropMag(), "drop");
    m.ForgetAll();
    CHECK(!m.HasHeld(), "forgotten");
    m.Sample(BR, g.sim(), kRifle);
    CHECK(m.MagIn(), "a forgotten weapon comes back as the game has it"); EQ(m.Loaded(), 36);
}

// A rack weapon: the seat does not chamber, the rack does.
static void rack_weapon() {
    Model m; Game g{1, 24, 12};
    m.Sample(MAGNUM, g.sim(), rack_pistol());
    g.shoot(1); m.Sample(MAGNUM, g.sim(), rack_pistol());
    EQ(m.Loaded(), 0);
    CHECK(!m.Rack(), "nothing to rack with an empty magazine");
    CHECK(m.DropMag(), "drop"); CHECK(m.SeatMag(), "seat");
    EQ(m.Chamber(), 0); EQ(m.Mag(), 12); EQ(m.Loaded(), 12);
    g.reload(); m.Sample(MAGNUM, g.sim(), rack_pistol());
    CHECK(!m.TriggerOk(), "seated but not racked");
    CHECK(m.Rack(), "rack"); EQ(m.Chamber(), 1); EQ(m.Mag(), 11);
    CHECK(!m.Rack(), "a second rack does nothing (no round is thrown away)");
    CHECK(m.TriggerOk(), "racked: fires");
    g.shoot(1); m.Sample(MAGNUM, g.sim(), rack_pistol());
    EQ(m.Chamber(), 1); EQ(m.Loaded(), 11);   // self-loading: the next round came up
    CONSERVED(m, g);
}

// THE MAGAZINE THE GAME ALREADY LOADED (seat_mag(true), 2026-11-12): the grab sent the game's reload with a
// round still chambered, and the player fired that round once the reload had finished. The seated magazine
// holds what the game counts, so the rack leaves ours level with the game -- no second reload for one round.
static void seat_the_games_magazine() {
    Model m; Game g{18, 72, 36};
    const Traits t = rack_pistol();
    m.Sample(BR, g.sim(), t);
    CHECK(m.DropMag(), "drop"); EQ(m.Chamber(), 1);
    g.reload(); m.Sample(BR, g.sim(), t);   // the grab's press: the game loads the magazine in the hand
    EQ(g.loaded, 36); EQ(m.Loaded(), 1);
    g.shoot(1); m.Sample(BR, g.sim(), t);   // the chambered round, after the game's reload
    EQ(m.Chamber(), 0); EQ(m.Loaded(), 0); EQ(g.loaded, 35);
    CHECK(m.SeatMag(true), "seat the game's magazine");
    EQ(m.Mag(), 35); EQ(m.Chamber(), 0); CHECK(!m.TriggerOk(), "seated, not racked");
    CHECK(m.Rack(), "rack"); EQ(m.Loaded(), 35); EQ(m.Behind(), 0);
    CHECK(m.TriggerOk(), "racked and level with the game: fires");
    CONSERVED(m, g);
    // The round not fired: the whole magazine, level with the game.
    Model k; Game h{18, 72, 36};
    k.Sample(BR, h.sim(), kRifle);
    CHECK(k.DropMag(), "drop"); h.reload(); k.Sample(BR, h.sim(), kRifle);
    CHECK(k.SeatMag(true), "seat"); EQ(k.Loaded(), 36); EQ(k.Behind(), 0); CHECK(k.TriggerOk(), "fires");
    CONSERVED(k, h);
    // Without the flag (the game still to load it) the magazine is a full one and the game is behind.
    Model f; Game e{18, 72, 36};
    f.Sample(BR, e.sim(), kRifle);
    CHECK(f.DropMag(), "drop"); CHECK(f.SeatMag(), "seat"); EQ(f.Loaded(), 36); EQ(f.Behind(), 18);
}

// The shotgun: a tube and a chamber, one round at a time, a pump after every shot.
static void shotgun_per_round() {
    Model m; Game g{12, 36, 12};
    const Traits t = shotgun();
    m.Sample(SHOTGUN, g.sim(), t);
    EQ(m.Loaded(), 12); EQ(m.Chamber(), 1); EQ(m.Mag(), 11);
    EQ((int)m.InsertRound(), (int)Insert::Full);
    CHECK(!m.DropMag() && !m.SeatMag(), "no magazine to drop or seat");
    g.shoot(1); m.Sample(SHOTGUN, g.sim(), t);
    EQ(m.Loaded(), 11); EQ(m.Chamber(), 0); CHECK(!m.TriggerOk(), "pump first");
    CHECK(m.Rack(), "pump"); EQ(m.Chamber(), 1); EQ(m.Mag(), 10); CHECK(m.TriggerOk(), "pumped: fires");
    // Fire three more.
    for (int i = 0; i < 3; ++i) { g.shoot(1); m.Sample(SHOTGUN, g.sim(), t); CHECK(m.Rack(), "pump"); }
    EQ(m.Loaded(), 8); EQ(g.loaded, 8);
    // A shell ahead of the game waits at the port.
    EQ((int)m.InsertRound(), (int)Insert::Wait); EQ(m.Loaded(), 8);
    // The game loads underneath (its own reload, one shell per cycle).
    g.reloading = true;
    g.reload_one(); m.Sample(SHOTGUN, g.sim(), t);
    EQ((int)m.InsertRound(), (int)Insert::Accepted); EQ(m.Loaded(), 9);
    EQ((int)m.InsertRound(), (int)Insert::Wait);
    // The plan's phase-4 exit: load 3, fire 2, load 1, pump.
    g.reload_one(); m.Sample(SHOTGUN, g.sim(), t); EQ((int)m.InsertRound(), (int)Insert::Accepted);
    g.reload_one(); m.Sample(SHOTGUN, g.sim(), t); EQ((int)m.InsertRound(), (int)Insert::Accepted);
    EQ(m.Loaded(), 11); g.reloading = false;
    g.shoot(1); m.Sample(SHOTGUN, g.sim(), t); CHECK(m.Rack(), "pump");
    g.shoot(1); m.Sample(SHOTGUN, g.sim(), t);
    EQ(m.Loaded(), 9); EQ(m.Chamber(), 0);
    g.reloading = true; g.reload_one(); m.Sample(SHOTGUN, g.sim(), t);
    EQ((int)m.InsertRound(), (int)Insert::Accepted); EQ(m.Loaded(), 10);
    CHECK(m.Rack(), "pump"); EQ(m.Chamber(), 1); EQ(m.Mag(), 9); CHECK(m.TriggerOk(), "fires");
    CONSERVED(m, g);
    // Nothing left to load.
    Game dry{2, 0, 12};
    Model m2; m2.Sample(SHOTGUN + 1, dry.sim(), t);
    dry.shoot(1); m2.Sample(SHOTGUN + 1, dry.sim(), t);
    EQ((int)m2.InsertRound(), (int)Insert::NoReserve);
    // A magazine weapon is not loaded a round at a time.
    Model m3; Game br{10, 10, 36}; m3.Sample(BR, br.sim(), kRifle);
    EQ((int)m3.InsertRound(), (int)Insert::NotPerRound);
}

static void clamp_when_the_game_will_not_catch_up() {
    Model m; Game g{0, 30, 36};
    m.Sample(BR, g.sim(), kRifle);
    CHECK(m.DropMag(), "drop"); CHECK(m.SeatMag(), "seat");
    EQ(m.Behind(), 30);
    m.ClampToGame();
    EQ(m.Loaded(), 0); EQ(m.Behind(), 0); CONSERVED(m, g);
    // A host correction that leaves the game short of ours mid-magazine.
    Model m2; Game h{20, 10, 36};
    m2.Sample(BR, h.sim(), kRifle);
    h.loaded = 15; h.reserve = 15;   // corrected, not fired: the sample charges it as shots ...
    m2.Sample(BR, h.sim(), kRifle);
    EQ(m2.Loaded(), 15);             // ... which is the right count either way
    CONSERVED(m2, h);
}

static void eviction() {
    Model m; Game g{5, 5, 12};
    for (uint32_t id = 1; id <= (uint32_t)Model::kRecords; ++id) { m.Sample(id, g.sim(), kRifle); }
    m.Sample(1, g.sim(), kRifle); CHECK(m.DropMag(), "weapon 1 magazine out");   // 1 is now the most recent
    m.Sample(1000, g.sim(), kRifle);   // a 17th weapon: weapon 2 (least recent) gives its slot up
    m.Sample(1, g.sim(), kRifle);
    CHECK(!m.MagIn(), "weapon 1 kept its record");
    m.Sample(2, g.sim(), kRifle);
    CHECK(m.MagIn(), "weapon 2 was evicted and comes back as the game has it");
}

// Thousands of random ticks of a player and a game obeying Halo's rules. The player only shoots when
// the model allows it, so no round is ever fired that ours does not have, and after the game has
// finished a reload it is never behind.
static void fuzz() {
    uint32_t s = 12345u;
    auto rnd = [&](int n) { s = s * 1664525u + 1013904223u; return (int)((s >> 8) % (uint32_t)n); };
    for (int run = 0; run < 200; ++run) {
        const int cap = 4 + rnd(57);
        Game g{cap, rnd(3 * cap + 1), cap};
        const Traits t = (run % 3 == 0) ? rack_pistol() : kRifle;
        Model m; const uint32_t id = 100u + (uint32_t)run;
        m.Sample(id, g.sim(), t);
        int reload_due = -1;   // ticks until a pressed reload completes
        for (int tick = 0; tick < 400; ++tick) {
            const int act = rnd(10);
            if (act < 5 && m.TriggerOk() && g.loaded > 0 && !g.reloading) g.shoot(1);
            else if (act == 5) m.DropMag();
            else if (act == 6 && !m.MagIn()) { m.SeatMag(); if (m.Behind() > 0 && reload_due < 0) { g.reloading = true; reload_due = 1 + rnd(5); } }
            else if (act == 7) m.Rack();
            else if (act == 8 && rnd(20) == 0) g.reserve += rnd(cap);   // a pickup
            else if (act == 9 && rnd(3) == 0) m.ReseatDropped();       // a cancelled reload
            if (reload_due == 0) { g.reload(); g.reloading = false; reload_due = -1; }
            else if (reload_due > 0) --reload_due;
            if (g.loaded == 0 && g.reserve > 0 && !g.reloading && rnd(4) == 0) g.reload();   // the game's own auto-reload
            m.Sample(id, g.sim(), t);
            CHECK(m.LastUnbacked() == 0, "run %d tick %d: a round fired that ours did not have", run, tick);
            CHECK(m.Chamber() == 0 || m.Chamber() == 1, "chamber %d", m.Chamber());
            CHECK(m.Mag() >= 0, "mag %d", m.Mag());
            CHECK(m.Loaded() <= cap, "run %d: loaded %d over capacity %d", run, m.Loaded(), cap);
            CHECK(m.MagIn() || m.Mag() == 0, "a magazine that is out holds nothing");
            if (!g.reloading && reload_due < 0 && m.Behind() > 0) {
                // Only a seat can put ours ahead of the game, and every such seat pressed a reload.
                CHECK(false, "run %d tick %d: game behind by %d with no reload running", run, tick, m.Behind());
            }
            check_conservation(m, g, __LINE__);
            if (g_fail > 20) return;
        }
    }
}

// "chamberRounds": 3 (the Battle Rifle): the burst stays in the gun with the magazine out, on the counter too.
static void burst_chamber() {
    const Traits t = burst_rifle();
    Model m; Game g{20, 50, 36};
    m.Sample(BR, g.sim(), t);
    EQ(m.Chamber(), 3); EQ(m.Mag(), 17); EQ(m.Loaded(), 20); CONSERVED(m, g);
    g.shoot(3); m.Sample(BR, g.sim(), t);   // a burst with the magazine in: the chamber stays full
    EQ(m.Chamber(), 3); EQ(m.Mag(), 14); EQ(m.Loaded(), 17); CONSERVED(m, g);
    CHECK(m.DropMag(), "drop");
    EQ(m.Loaded(), 3); EQ(m.Chamber(), 3); CHECK(m.TriggerOk(), "the burst fires with the magazine out"); CONSERVED(m, g);
    g.shoot(3); m.Sample(BR, g.sim(), t);
    EQ(m.Loaded(), 0); EQ(m.Chamber(), 0); EQ(m.LastShots(), 3); EQ(m.LastUnbacked(), 0);
    CHECK(!m.TriggerOk(), "chamber spent: nothing fires"); CONSERVED(m, g);
    CHECK(m.SeatMag(), "seat");
    EQ(m.Chamber(), 3); EQ(m.Loaded(), 36); CONSERVED(m, g);
    // A magazine with fewer rounds than the burst fills what it can.
    Model k; Game h{2, 0, 36};
    k.Sample(BR, h.sim(), t);
    EQ(k.Chamber(), 2); EQ(k.Mag(), 0); EQ(k.Loaded(), 2);
    // A rack tops the chamber up to the burst, never past it.
    Model r; Game rg{20, 50, 36};
    Traits rt = burst_rifle(); rt.chamberOnSeat = false;
    r.Sample(BR, rg.sim(), rt);
    CHECK(r.DropMag(), "drop"); rg.shoot(3); r.Sample(BR, rg.sim(), rt);
    EQ(r.Chamber(), 0);
    CHECK(r.SeatMag(), "seat"); EQ(r.Chamber(), 0);
    CHECK(r.Rack(), "rack"); EQ(r.Chamber(), 3);
    CHECK(!r.Rack(), "a full chamber does not rack again");
}

// "chamber": false: shots come from the magazine, and with it out nothing fires.
static void no_chamber_weapon() {
    const Traits t = no_chamber();
    Model m; Game g{5, 20, 5};
    m.Sample(MAGNUM, g.sim(), t);
    EQ(m.Chamber(), 0); EQ(m.Mag(), 5); EQ(m.Loaded(), 5); CHECK(m.TriggerOk(), "fires from the magazine");
    g.shoot(1); m.Sample(MAGNUM, g.sim(), t);
    EQ(m.Mag(), 4); EQ(m.Chamber(), 0); CONSERVED(m, g);
    CHECK(m.DropMag(), "drop");
    EQ(m.Loaded(), 0); CHECK(!m.TriggerOk(), "no magazine, no shot");
    CHECK(!m.Rack(), "nothing to rack");
    CHECK(m.SeatMag(), "seat");
    EQ(m.Chamber(), 0); EQ(m.Loaded(), 5); CONSERVED(m, g);
    // A record that had a chambered round when the table said otherwise gives it back to the magazine.
    Model k; Game h{5, 20, 5};
    k.Sample(MAGNUM, h.sim(), kRifle);
    EQ(k.Chamber(), 1);
    k.Sample(MAGNUM, h.sim(), t);
    EQ(k.Chamber(), 0); EQ(k.Mag(), 5); EQ(k.Loaded(), 5);
}

int main() {
    first_sight_and_shots();
    auto_reload_is_not_ours();
    seat_before_the_game();
    tactical_reload();
    burst_from_one_round();
    pickup_and_partial_reserve();
    swaps();
    cancelled_reload();
    death_forgets();
    rack_weapon();
    seat_the_games_magazine();
    shotgun_per_round();
    clamp_when_the_game_will_not_catch_up();
    eviction();
    burst_chamber();
    no_chamber_weapon();
    fuzz();
    std::printf("ammomodel: %d checks, %d failed\n", g_checks, g_fail);
    return g_fail == 0 ? 0 : 1;
}
