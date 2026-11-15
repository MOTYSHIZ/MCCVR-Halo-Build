// Project Reclaimer's own list of what it enlarged (src/common/reclaimer_capacity_logic.h), read from lines of its
// real logs (0.10.3 and 0.10.4, 2026-11-15).

#include "../src/common/reclaimer_capacity_logic.h"

#include <cstdio>
#include <cstdlib>

static int g_failures = 0;
#define CHECK(x)                                                                                                       \
    do                                                                                                                 \
    {                                                                                                                  \
        if (!(x))                                                                                                      \
        {                                                                                                              \
            std::printf("FAILED line %d: %s\n", __LINE__, #x);                                                         \
            ++g_failures;                                                                                              \
        }                                                                                                              \
    } while (0)

using namespace reclaimer_capacity;

static const char kV0104[] =
    "12:16:33.901 [engine] halo3.dll: lifted\n"
    "12:16:34.356 [capacity] reclaimer capacity: cinematics=150 coop=true effects=4 entities=16384 fp_nodes=128 "
    "hud=512 odst=true players=128 teams=8\r\n"
    "12:16:34.600 [capacity] Forge objects: 9999 placements, 4096 budget entries (0xcb300 bytes a map variant's "
    "extension)\n"
    "12:16:34.616 [capacity] players.count: 0x10 -> 0x80\n"
    "12:16:34.616 [capacity] machines.count: 0x11 -> 0x80\n"
    "12:16:34.616 [capacity] hud.markers: 0x14 -> 0xd4\n"
    "12:16:34.616 [capacity] ai.task_known_bytes: 0x105c -> 0x2000\n"
    "12:16:34.616 [capacity] simulation_world.address: 0x7ff66cb08698 -> 0x23127ae3000\n"
    "12:16:34.616 [capacity] havok.batch_switch: 0x7ff66b38cd63 -> 0x2311ac513d0\n"
    "12:16:35.000 [hud] motion sensor: user 0 holds 132 contacts\n";

// 0.10.3: another task size, an address elsewhere, and a line 0.10.4 no longer has. Before 0.10.3 lines had no time.
static const char kV0103[] =
    "[capacity] reclaimer capacity: fp_nodes=128 players=128\n"
    "[capacity] players.count: 0x10 -> 0x80\n"
    "[capacity] machines.count: 0x11 -> 0x80\n"
    "[capacity] hud.markers: 0x14 -> 0xd4\n"
    "[capacity] ai.task_known_bytes: 0x105c -> 0x1900\n"
    "[capacity] simulation_world.address: 0x7ff75da79698 -> 0x21563347000\n"
    "[capacity] old.table: 0x10 -> 0x20\n";

int main()
{
    const Manifest now = Parse(kV0104);
    CHECK(now.Find("fp_nodes") && now.Find("fp_nodes")->host == 128 && now.Find("fp_nodes")->summary);
    CHECK(now.Find("players") && now.Find("players")->host == 128);
    CHECK(!now.Find("coop") && !now.Find("odst"));   // not numbers
    CHECK(now.Find("players.count") && now.Find("players.count")->retail == 0x10 &&
          now.Find("players.count")->host == 0x80 && !now.Find("players.count")->summary);
    CHECK(now.Find("hud.markers")->host == 0xd4);
    CHECK(!now.Find("Forge objects") && !now.Find("Forge"));   // prose
    CHECK(now.Find("simulation_world.address") && !IsSize(*now.Find("simulation_world.address")));
    CHECK(IsSize(*now.Find("ai.task_known_bytes")));
    CHECK(Parse("").Empty() && Parse("[hud] players.count: 0x10 -> 0x80\n").Empty());
    CHECK(Parse("12:00:00.000 [capacity] bad: 0xzz -> 0x10\n").Empty());

    // What changed between the versions: sizes only, the ones the mod depends on marked.
    const Manifest before = Parse(kV0103);
    const std::vector<Change> changes = Changes(before, now);
    bool task = false, old = false, address = false, players = false;
    for (const Change& change : changes)
    {
        task = task || (change.name == "ai.task_known_bytes" && change.was == 0x1900 && change.now == 0x2000 &&
                         !change.depended);
        old = old || (change.name == "old.table" && change.before && !change.after);
        address = address || change.name == "simulation_world.address" || change.name == "havok.batch_switch";
        players = players || change.name == "players.count";
    }
    CHECK(task && old && !address && !players);
    CHECK(Changes(now, now).empty());
    // A size the mod depends on, changed.
    const Manifest grown = Parse("[capacity] reclaimer capacity: fp_nodes=256\n[capacity] players.count: 0x10 -> 0x100\n");
    bool fp = false, count = false;
    for (const Change& change : Changes(now, grown))
    {
        fp = fp || (change.name == "fp_nodes" && change.depended && change.was == 128 && change.now == 256);
        count = count || (change.name == "players.count" && change.depended);
    }
    CHECK(fp && count);
    CHECK(ModDependsOn("machines.count") && ModDependsOn("players_globals.bytes") && !ModDependsOn("playersx") &&
          !ModDependsOn("ai.task_known_bytes"));

    CHECK(VersionFromLogName("client-2026-11-15_121625-v0.10.4.log") == "0.10.4");
    CHECK(VersionFromLogName("C:\\x\\Logs\\client-2026-11-13_052800-v0.10.3.log") == "0.10.3");
    CHECK(VersionFromLogName("client.log").empty() && VersionFromLogName("client.previous.log").empty());

    // The node capacities: retail's first, the stated value before 128, never in place of retail's.
    NodeCandidates c = FpNodeCandidates(now);
    CHECK(c.count == 2 && c.values[0] == 64 && c.values[1] == 128);
    c = FpNodeCandidates(grown);
    CHECK(c.count == 3 && c.values[0] == 64 && c.values[1] == 256 && c.values[2] == 128);
    c = FpNodeCandidates(Manifest{});
    CHECK(c.count == 2 && c.values[0] == 64 && c.values[1] == 128);
    c = FpNodeCandidates(uint64_t{64});
    CHECK(c.count == 2 && c.values[1] == 128);
    c = FpNodeCandidates(uint64_t{5000});   // out of range: not taken
    CHECK(c.count == 2 && c.values[1] == 128);

    if (g_failures)
        return 1;
    std::printf("reclaimer capacity: all passed\n");
    return 0;
}
