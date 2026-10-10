// The vehicle camera file (src/common/vehicle_camera_file.h): reading, writing, the built-in table, per-game
// sections, tethering modes, the leash cascade, vehicle matching, and every bound a hand-edited file can reach.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include "../src/common/vehicle_camera_file.h"

using namespace vehicle_camera_file;

static unsigned g_checks = 0;
static void Check(bool condition, const char* message)
{
    ++g_checks;
    if (!condition)
    {
        std::fprintf(stderr, "FAIL: %s\n", message);
        std::exit(1);
    }
}

static ParseResult Read(const std::string& text, Table& out)
{
    return FromJson(text.c_str(), text.size(), out);
}

static bool Has(const std::string& text, const char* part)
{
    return text.find(part) != std::string::npos;
}

// One game section holding one vehicle entry with the given body.
static std::string OneVehicle(const char* body)
{
    return std::string("{ \"games\": { \"halo3\": { \"vehicles\": { \"Warthog\": ") + body + " } } } }";
}

static void TestBuiltInTable()
{
    const Table built = DefaultTable();
    Check(built.games.size() == 1 && built.games[0].key == kDefaultKey, "the built-in table is one default section");
    const Game* game = built.FindGame("halo3");
    Check(game != nullptr && game == &built.games[0], "any game finds the default section");
    Check(game->vehicles.size() == 1 && game->vehicles[0].isDefault, "with one default entry");
    const Vehicle& any = game->vehicles[0];
    Check(any.cameras.size() == 2 && any.defaultCamera == 0, "two cameras, starting in the first");
    Check(any.cameras[0].type == CameraType::FirstPerson, "the first is the seat view the player already has");
    Check(any.cameras[1].type == CameraType::Chase && any.cameras[1].ModeCount() == 2, "the second has two modes");
    Check(!any.cameras[1].Mode(0).rotYaw && any.cameras[1].Mode(1).rotYaw, "untethered first, then tethered to yaw");

    // Written and read back it is the same table, and writing that again gives the same text.
    const std::string text = ToJson(built);
    Table back;
    const ParseResult result = Read(text, back);
    Check(result.ok && result.ignored.empty(), "the built-in table's own text reads with nothing ignored");
    Check(result.vehicles == 1 && result.cameras == 2, "and counts one entry and two cameras");
    Check(ToJson(back) == text, "writing what was read gives the same text");
    Check(Has(text, "\"_readme\"") && !Has(ToJson(built, false), "\"_readme\""), "the guide is optional");
    Check(Has(text, "{ \"name\": \"Seat\", \"type\": \"firstperson\" }"),
          "a firstperson camera names only what was set");
}

static void TestGames()
{
    Table table;
    const std::string text =
        "{ \"version\": 1, \"_note\": \"mine\", \"games\": {"
        "  \"Halo3\": { \"vehicles\": { \"default\": { \"cameras\": [ { \"name\": \"A\" } ] } } },"
        "  \"_skipped\": 5,"
        "  \"default\": { \"vehicles\": { \"default\": { \"cameras\": [ { \"name\": \"B\" } ] } } } } }";
    Check(Read(text, table).ok, "two sections read");
    Check(table.games.size() == 2 && table.games[0].key == "halo3", "a section's key is matched in lower case");
    Check(table.FindGame("halo3")->vehicles[0].cameras[0].name == "A", "a game gets its own section");
    Check(table.FindGame("reach")->vehicles[0].cameras[0].name == "B", "another game gets the default section");
    Check(table.FindGame(nullptr)->vehicles[0].cameras[0].name == "B", "no game gets the default section");

    Table only;
    Check(Read("{ \"games\": { \"odst\": { \"vehicles\": { } } } }", only).ok, "a section may list no vehicles");
    Check(only.FindGame("odst") != nullptr && only.FindGame("halo4") == nullptr,
          "with no default section another game has none");

    Table untouched = DefaultTable();
    ParseResult bad = Read("{ \"games\": { \"halo5\": { \"vehicles\": { } } } }", untouched);
    Check(!bad.ok && Has(bad.error, "games.halo5") && Has(bad.error, "line 1"), "an unknown game key is refused");
    Check(untouched.games.size() == 1 && untouched.games[0].key == kDefaultKey, "and the table is left as it was");
    bad = Read("{ \"games\": { \"reach\": { \"vehicles\": { } }, \"REACH\": { \"vehicles\": { } } } }", untouched);
    Check(!bad.ok && Has(bad.error, "listed once"), "a game listed twice is refused");
    bad = Read("{ \"version\": 1 }", untouched);
    Check(!bad.ok && Has(bad.error, "\"games\""), "a file without games is refused");
    bad = Read("{ \"games\": { \"halo3\": { } } }", untouched);
    Check(!bad.ok && Has(bad.error, "\"vehicles\""), "a section without vehicles is refused");
    bad = Read("[]", untouched);
    Check(!bad.ok && Has(bad.error, "the file"), "a file that is not an object is refused");

    const ParseResult extra = Read("{ \"gamez\": 1, \"games\": { \"halo3\": { \"vehicles\": { }, \"vehicle\": 2 } } }", only);
    Check(extra.ok && extra.ignored.size() == 2 && extra.ignored[0] == "gamez" &&
              extra.ignored[1] == "games.halo3.vehicle",
          "keys that match nothing are reported by their path");
}

static void TestCamerasAndTethering()
{
    Table table;
    const std::string text = OneVehicle(
        "{ \"defaultCamera\": 1, \"defaultMode\": 1, \"cameras\": ["
        "  { \"name\": \"Seat\", \"type\": \"FirstPerson\" },"
        "  { \"name\": \"Behind\", \"origin\": \"vehicle\", \"offset\": [-450, 10.5, 180],"
        "    \"locationTracking\": [\"yaw\"], \"rotationTracking\": [], \"collide\": false, \"collideMargin\": 12,"
        "    \"tethering\": ["
        "      { \"name\": \"Untethered\" },"
        "      { \"name\": \"Tethered\", \"rotationTracking\": [\"yaw\", \"roll\"], \"offset\": [-300, 0, 150],"
        "        \"origin\": \"turret_base\", \"locationTracking\": \"view\" } ] },"
        "  { \"origin\": \"PlayerHead\", \"locationTracking\": \"VIEW\" } ] }");
    const ParseResult result = Read(text, table);
    Check(result.ok && result.vehicles == 1 && result.cameras == 3 && result.ignored.empty(), "three cameras read");
    const Vehicle& warthog = table.games[0].vehicles[0];
    Check(warthog.name == "Warthog" && warthog.match.size() == 1 && warthog.match[0] == "warthog",
          "an entry without match is matched by its own key");
    Check(warthog.defaultCamera == 1 && warthog.defaultMode == 1, "where the seat starts");
    Check(warthog.cameras[0].type == CameraType::FirstPerson && warthog.cameras[0].ModeCount() == 1,
          "the type is read in any case");

    const Camera& behind = warthog.cameras[1];
    Check(behind.offset[0] == -450.0f && behind.offset[1] == 10.5f && behind.offset[2] == 180.0f, "the offset");
    Check(behind.locYaw && !behind.locPitch && !behind.locRoll && !behind.locView, "location tracking by axis");
    Check(!behind.collide && behind.collideMargin == 12.0f, "collision settings");
    Check(behind.ModeCount() == 2, "two modes");
    const Tether still = behind.Mode(0);
    Check(still.name == "Untethered" && still.offset[0] == -450.0f && still.locYaw && !still.locPitch &&
              !still.rotYaw && still.origin == -1,
          "a mode takes the camera's own values for what it leaves out");
    Check(behind.OriginOf(still) == Origin::Vehicle && behind.MarkerOf(still).empty(), "and the camera's origin");
    const Tether tethered = behind.Mode(1);
    Check(tethered.rotYaw && !tethered.rotPitch && tethered.rotRoll, "a mode's own rotation tracking");
    Check(tethered.offset[0] == -300.0f && tethered.offset[2] == 150.0f, "a mode's own offset");
    Check(tethered.locView && !tethered.locYaw, "a mode's own location tracking");
    Check(behind.OriginOf(tethered) == Origin::Marker && behind.MarkerOf(tethered) == "turret_base",
          "a mode's own origin: any other name is a marker");
    Check(behind.Mode(2).name == "Untethered" && behind.Mode(-1).name == "Tethered", "a mode index wraps");

    const Camera& head = warthog.cameras[2];
    Check(head.origin == Origin::Head && head.locView && !head.locYaw && head.name.empty(),
          "playerhead and view, in any case");
    const Tether own = head.Mode(5);
    Check(own.locView && own.name.empty() && head.ModeCount() == 1, "a camera without tethering is its one mode");

    // The file as written reads back to the same text.
    const std::string written = ToJson(table, false);
    Table back;
    Check(Read(written, back).ok && ToJson(back, false) == written, "the table round-trips");
    Check(Has(written, "{ \"name\": \"Untethered\" }"), "a mode names only what differs from its camera");
    Check(Has(written, "\"origin\": \"turret_base\"") && Has(written, "\"locationTracking\": \"view\""),
          "and writes what does");

    // "head" is read as playerhead; a marker on the camera itself is kept by name.
    Check(Read(OneVehicle("{ \"cameras\": [ { \"origin\": \"head\" }, { \"origin\": \"driver_seat\" } ] }"), table).ok,
          "origins by name");
    Check(table.games[0].vehicles[0].cameras[0].origin == Origin::Head, "head is the player's head");
    Check(table.games[0].vehicles[0].cameras[1].origin == Origin::Marker &&
              table.games[0].vehicles[0].cameras[1].originMarker == "driver_seat",
          "a marker keeps its name as written");
    Check(OriginText(Origin::Marker, "driver_seat") == "driver_seat" && OriginText(Origin::Head, "") == "playerhead" &&
              OriginText(Origin::Seat, "") == "seat" && OriginText(Origin::Vehicle, "x") == "vehicle",
          "origins as the file writes them");
}

static void TestLeash()
{
    Table table;
    const std::string text = OneVehicle(
        "{ \"cameras\": ["
        "    { \"name\": \"Inherits\" },"
        "    { \"name\": \"Own\", \"leashMax\": [\"inherit\", null, 70], \"leashMin\": null,"
        "      \"tethering\": [ { \"name\": \"A\" }, { \"name\": \"B\", \"leashMax\": [5] },"
        "                       { \"name\": \"C\", \"leashMin\": [-1, \"inherit\"], \"leashMax\": \"inherit\" } ] } ],"
        "  \"leashMin\": [-10, -15, 40], \"leashMax\": [8, 12, 64] }");
    Check(Read(text, table).ok, "a leash at every level reads");
    const Vehicle& seat = table.games[0].vehicles[0];
    Check(seat.leashMin[0] == -10.0f && seat.leashMax[2] == 64.0f, "the seat's leash, wherever the file puts it");
    const Camera& inherits = seat.cameras[0];
    Check(inherits.leashMin[1] == -15.0f && inherits.leashMax[1] == 12.0f, "a camera starts from its seat's leash");
    const Camera& own = seat.cameras[1];
    Check(own.leashMax[0] == 8.0f && own.leashMax[1] == kUnleashed && own.leashMax[2] == 70.0f,
          "per axis: inherit, no limit, a limit of its own");
    Check(!LeashSet(own.leashMin), "null drops the seat's leash on that side");
    Check(own.Mode(0).leashMax[2] == 70.0f && !LeashSet(own.Mode(0).leashMin), "a mode starts from its camera's");
    Check(own.Mode(1).leashMax[0] == 5.0f && own.Mode(1).leashMax[1] == kUnleashed &&
              own.Mode(1).leashMax[2] == kUnleashed,
          "a shorter list leaves the other axes unlimited");
    Check(own.Mode(2).leashMin[0] == -1.0f && own.Mode(2).leashMin[1] == -kUnleashed &&
              own.Mode(2).leashMax[2] == 70.0f,
          "inherit in a mode is the camera's value");

    const std::string written = ToJson(table, false);
    Table back;
    Check(Read(written, back).ok && ToJson(back, false) == written, "leashes round-trip");
    Check(LeashText(seat.leashMin, seat.leashMax) == "forward -10 to 8, right -15 to 12, up 40 to 64 cm",
          "the leash as places from the origin");
    Check(LeashText(own.leashMin, own.Mode(1).leashMax) == "forward at most 5 cm", "one limit");
    const float noFloor[3] = {-kUnleashed, -kUnleashed, -kUnleashed};
    const float noCeiling[3] = {kUnleashed, kUnleashed, kUnleashed};
    const float floorOnly[3] = {-kUnleashed, -kUnleashed, 20.0f};
    Check(LeashText(floorOnly, own.Mode(1).leashMax) == "forward at most 5, up at least 20 cm", "a floor");
    Check(LeashText(noFloor, noCeiling) == "None" && !LeashSet(noFloor) && !LeashSet(noCeiling) && LeashSet(floorOnly),
          "no limit at all");

    ParseResult bad = Read(OneVehicle("{ \"leashMin\": \"tight\", \"cameras\": [ { } ] }"), table);
    Check(!bad.ok && Has(bad.error, "vehicles.Warthog.leashMin"), "a leash that is neither a list nor inherit");
    bad = Read(OneVehicle("{ \"cameras\": [ { \"leashMax\": [1, 2, 3, 4] } ] }"), table);
    Check(!bad.ok && Has(bad.error, "cameras[0].leashMax"), "a leash of four axes");
}

static void TestMatching()
{
    Table table;
    const std::string text =
        "{ \"games\": { \"halo3\": { \"vehicles\": {"
        "  \"Warthog\": { \"match\": [\"warthog\"], \"cameras\": [ { \"name\": \"driver\" } ] },"
        "  \"Warthog passenger\": { \"match\": \"warthog\", \"seat\": \"passenger\", \"cameras\": [ { } ] },"
        "  \"Warthog gunner\": { \"match\": [\"warthog\"], \"seat\": [\"gunner\"], \"cameras\": [ { } ] },"
        "  \"Wraith\": { \"cameras\": [ { } ] },"
        "  \"Wraith turret\": { \"match\": [\"wraith_turret\", \"anti_infantry\"], \"cameras\": [ { } ] },"
        "  \"Pelican\": { \"enabled\": false },"
        "  \"default\": { \"cameras\": [ { } ] },"
        "  \"Unseated twin\": { \"match\": [\"WARTHOG\"], \"cameras\": [ { } ] } } } } }";
    Check(Read(text, table).ok, "a section of entries reads");
    const Game& game = table.games[0];
    const auto name = [&game](const char* vehicle, uint8_t seat) -> std::string {
        const int index = MatchVehicle(game, vehicle, seat);
        return index < 0 ? std::string("(none)") : game.vehicles[static_cast<std::size_t>(index)].name;
    };
    const char* hog = "objects\\vehicles\\warthog\\warthog";
    Check(name(hog, 0) == "Warthog", "an unknown seat never picks an entry that names a seat");
    Check(name(hog, kSeatDriver) == "Warthog", "the driver uses the entry without a seat list");
    Check(name(hog, kSeatPassenger) == "Warthog passenger", "a passenger uses the entry naming that seat");
    Check(name(hog, kSeatGunner) == "Warthog gunner", "and a gunner the gunner's");
    Check(name(hog, SeatBits(true, true)) == "Warthog gunner", "a seat with two flags passes a list naming either");
    Check(name("Objects\\Vehicles\\WARTHOG\\Warthog", kSeatDriver) == "Warthog", "names match in any case");
    Check(name("objects\\vehicles\\wraith\\wraith", kSeatDriver) == "Wraith", "an entry's own key is its match");
    Check(name("objects\\vehicles\\wraith\\turrets\\wraith_turret", kSeatGunner) == "Wraith turret",
          "the longest match wins");
    Check(name("objects\\weapons\\turret\\anti_infantry", 0) == "Wraith turret", "any of an entry's match texts");
    Check(name("objects\\vehicles\\ghost\\ghost", kSeatDriver) == "default", "an unlisted vehicle uses default");
    Check(name("objects\\vehicles\\pelican\\pelican", 0) == "Pelican", "a disabled entry still matches");
    const Vehicle& pelican = game.vehicles[static_cast<std::size_t>(MatchVehicle(game, "pelican", 0))];
    Check(!pelican.enabled && pelican.cameras.empty(), "and may list no cameras");
    Check(game.vehicles[1].seats == kSeatPassenger && game.vehicles[2].seats == kSeatGunner, "seat lists as bits");

    Table bare;
    Check(Read("{ \"games\": { \"reach\": { \"vehicles\": { \"Ghost\": { \"cameras\": [ { } ] } } } } }", bare).ok,
          "a section without a default entry");
    Check(MatchVehicle(bare.games[0], "banshee", 0) == -1, "gives an unlisted vehicle nothing");

    Check(SeatBits(true, false) == kSeatDriver && SeatBits(false, true) == kSeatGunner &&
              SeatBits(false, false) == kSeatPassenger && SeatBits(true, true) == (kSeatDriver | kSeatGunner),
          "a seat's flags as bits");
    Check(SeatText(kSeatDriver | kSeatGunner) == "Driver, Gunner" && SeatText(kSeatPassenger) == "Passenger" &&
              SeatText(0).empty(),
          "a seat as text");

    const std::string written = ToJson(table, false);
    Table back;
    Check(Read(written, back).ok && ToJson(back, false) == written, "entries round-trip");
    Check(Has(written, "\"seat\": [\"passenger\"]") && Has(written, "\"enabled\": false"), "seat and enabled written");

    ParseResult bad = Read(OneVehicle("{ \"seat\": \"pilot\", \"cameras\": [ { } ] }"), table);
    Check(!bad.ok && Has(bad.error, "Warthog.seat"), "an unknown seat is refused");
    bad = Read(OneVehicle("{ \"seat\": [\"driver\", 3], \"cameras\": [ { } ] }"), table);
    Check(!bad.ok && Has(bad.error, "seat[1]"), "and one in a list, by its place");
    Check(Read(OneVehicle("{ \"seat\": [], \"cameras\": [ { } ] }"), table).ok && table.games[0].vehicles[0].seats == 0,
          "an empty seat list is any seat");
}

static void TestCarryMode()
{
    Table table;
    const std::string text = OneVehicle(
        "{ \"cameras\": ["
        "  { \"name\": \"Cockpit\", \"tethering\": [ { \"name\": \"Untethered\" },"
        "      { \"name\": \"Tethered\", \"rotationTracking\": [\"yaw\", \"pitch\", \"roll\"] } ] },"
        "  { \"name\": \"Behind\", \"tethering\": [ { \"name\": \"Free\" },"
        "      { \"name\": \"Level\", \"rotationTracking\": [\"yaw\"] },"
        "      { \"name\": \"tethered\", \"rotationTracking\": [\"yaw\", \"roll\"] } ] },"
        "  { \"name\": \"Plain\" } ] }");
    Check(Read(text, table).ok, "three cameras with modes read");
    const Vehicle& seat = table.games[0].vehicles[0];
    Check(CarryMode(seat.cameras[1], seat.cameras[0].Mode(1)) == 2, "a mode of the same name is kept, in any case");
    Check(CarryMode(seat.cameras[1], seat.cameras[0].Mode(0)) == 0, "else one turning the view on the same axes");
    Check(CarryMode(seat.cameras[0], seat.cameras[1].Mode(1)) == 0, "else the first");
    Check(CarryMode(seat.cameras[2], seat.cameras[0].Mode(1)) == 0, "a camera with one mode has only that");
    Check(RotationTrackingText(seat.cameras[0].Mode(1)) == "Yaw, Pitch, Roll" &&
              RotationTrackingText(seat.cameras[0].Mode(0)) == "None" &&
              RotationTrackingText(seat.cameras[1].Mode(2)) == "Yaw, Roll",
          "rotation tracking as text");
    Check(LocationTrackingText(seat.cameras[0].Mode(0)) == "Yaw, Pitch, Roll", "location tracking as text");
}

static void TestBoundsAndErrors()
{
    Table table = DefaultTable();
    const std::string before = ToJson(table, false);

    ParseResult bad = Read("{ \"games\": { \"halo3\": { \"vehicles\": {\n  \"Ghost\": { \"cameras\": [ { \"offset\": \"far\" } ] } } } } }", table);
    Check(!bad.ok && Has(bad.error, "games.halo3.vehicles.Ghost.cameras[0].offset") && Has(bad.error, "line 2"),
          "a type error names the key and the line");
    Check(ToJson(table, false) == before, "and the table is left as it was");
    bad = Read(OneVehicle("{ \"cameras\": [ { \"offset\": [0, 0, 1e39] } ] }"), table);
    Check(!bad.ok && Has(bad.error, "offset[2]"), "a number past the bound is refused");
    bad = Read(OneVehicle("{ \"cameras\": [ { \"offset\": [1, 2, 3, 4] } ] }"), table);
    Check(!bad.ok, "an offset of four numbers");
    Check(Read(OneVehicle("{ \"cameras\": [ { \"offset\": [5] } ] }"), table).ok &&
              table.games[0].vehicles[0].cameras[0].offset[0] == 5.0f &&
              table.games[0].vehicles[0].cameras[0].offset[1] == 0.0f,
          "a shorter offset leaves the rest at zero");
    bad = Read(OneVehicle("{ \"cameras\": [ { \"type\": \"orbit\" } ] }"), table);
    Check(!bad.ok && Has(bad.error, "chase"), "an unknown camera type");
    bad = Read(OneVehicle("{ \"cameras\": [ { \"rotationTracking\": [\"yaw\", \"spin\"] } ] }"), table);
    Check(!bad.ok && Has(bad.error, "rotationTracking[1]"), "an unknown axis, by its place");
    bad = Read(OneVehicle("{ \"cameras\": [ { \"locationTracking\": \"orbit\" } ] }"), table);
    Check(!bad.ok && Has(bad.error, "view"), "a location tracking word that is not view");
    bad = Read(OneVehicle("{ \"cameras\": [ { \"collide\": 1 } ] }"), table);
    Check(!bad.ok && Has(bad.error, "true or false"), "a number where true or false belongs");
    bad = Read(OneVehicle("{ \"cameras\": [ { \"origin\": \"\" } ] }"), table);
    Check(!bad.ok && Has(bad.error, "origin"), "an empty origin");
    bad = Read(OneVehicle("{ \"cameras\": [ ] }"), table);
    Check(!bad.ok && Has(bad.error, "at least one camera"), "an empty camera list");
    bad = Read(OneVehicle("{ }"), table);
    Check(!bad.ok && Has(bad.error, "\"cameras\""), "an enabled entry without cameras");
    bad = Read(OneVehicle("{ \"cameras\": [ { \"tethering\": [ ] } ] }"), table);
    Check(!bad.ok && Has(bad.error, "at least one tethering mode"), "an empty tethering list");
    bad = Read(OneVehicle("{ \"cameras\": [ { \"tethering\": [ 3 ] } ] }"), table);
    Check(!bad.ok && Has(bad.error, "tethering[0]"), "a tethering mode that is not an object");
    bad = Read(OneVehicle("{ \"defaultCamera\": 3000000000, \"cameras\": [ { } ] }"), table);
    Check(!bad.ok && Has(bad.error, "defaultCamera"), "a camera index past any list is refused");
    bad = Read(OneVehicle("{ \"defaultCamera\": -1, \"cameras\": [ { } ] }"), table);
    Check(!bad.ok, "and a negative one");
    bad = Read(OneVehicle("{ \"defaultMode\": 1.5, \"cameras\": [ { } ] }"), table);
    Check(!bad.ok && Has(bad.error, "defaultMode"), "and a fraction");
    Check(Read(OneVehicle("{ \"defaultCamera\": 7, \"defaultMode\": 7, \"cameras\": [ { } ] }"), table).ok &&
              table.games[0].vehicles[0].defaultCamera == 0 && table.games[0].vehicles[0].defaultMode == 0,
          "an index past this entry's list starts at the first");
    bad = Read(OneVehicle("{ \"enabled\": \"no\", \"cameras\": [ { } ] }"), table);
    Check(!bad.ok && Has(bad.error, "enabled"), "enabled that is not true or false");
    bad = Read(OneVehicle("{ \"match\": 5, \"cameras\": [ { } ] }"), table);
    Check(!bad.ok && Has(bad.error, "match"), "match that is not text");

    // The counts a file may reach, and one more.
    const auto cameras = [](int count) {
        std::string list;
        for (int i = 0; i < count; ++i)
            list += std::string(i ? ", " : "") + "{ }";
        return OneVehicle(("{ \"cameras\": [ " + list + " ] }").c_str());
    };
    Check(Read(cameras(kMaxCameras), table).ok, "sixteen cameras read");
    bad = Read(cameras(kMaxCameras + 1), table);
    Check(!bad.ok && Has(bad.error, "at most 16 cameras"), "seventeen do not");
    const auto modes = [](int count) {
        std::string list;
        for (int i = 0; i < count; ++i)
            list += std::string(i ? ", " : "") + "{ }";
        return OneVehicle(("{ \"cameras\": [ { \"tethering\": [ " + list + " ] } ] }").c_str());
    };
    Check(Read(modes(kMaxTethers), table).ok, "eight tethering modes read");
    bad = Read(modes(kMaxTethers + 1), table);
    Check(!bad.ok && Has(bad.error, "at most 8"), "nine do not");
    const auto entries = [](int count) {
        std::string list;
        for (int i = 0; i < count; ++i)
            list += std::string(i ? ", " : "") + "\"v" + std::to_string(i) + "\": { \"cameras\": [ { } ] }";
        return "{ \"games\": { \"halo3\": { \"vehicles\": { \"_note\": 1, " + list + " } } } }";
    };
    Check(Read(entries(kMaxVehicles), table).ok && table.games[0].vehicles.size() == kMaxVehicles,
          "sixty-four entries read, notes aside");
    bad = Read(entries(kMaxVehicles + 1), table);
    Check(!bad.ok && Has(bad.error, "at most 64"), "sixty-five do not");

    // A file past the size bound is refused before it is parsed.
    std::string huge = "{ \"_pad\": \"";
    huge.append(kMaxFileBytes, 'x');
    huge += "\", \"games\": { } }";
    bad = Read(huge, table);
    Check(!bad.ok && Has(bad.error, "512 KB"), "a file past the size bound");
    bad = FromJson(nullptr, 0, table);
    Check(!bad.ok, "no text at all");

    // Notes anywhere, and keys that match nothing.
    const ParseResult noted = Read(
        OneVehicle("{ \"_why\": \"mine\", \"cameras\": [ { \"_note\": 1, \"ofset\": [1], \"name\": null,"
                   " \"tethering\": [ { \"_n\": 2, \"speed\": 3 } ] } ], \"colour\": \"red\" }"),
        table);
    Check(noted.ok && noted.ignored.size() == 3, "notes are skipped and unknown keys reported");
    Check(noted.ignored[0] == "games.halo3.vehicles.Warthog.cameras[0].ofset" &&
              noted.ignored[1] == "games.halo3.vehicles.Warthog.cameras[0].tethering[0].speed" &&
              noted.ignored[2] == "games.halo3.vehicles.Warthog.colour",
          "each by its path");

    // Text that needs escaping survives the round trip.
    Check(Read(OneVehicle("{ \"cameras\": [ { \"name\": \"Say \\\"hi\\\" \\\\ there\" } ] }"), table).ok,
          "a name with quotes and a backslash reads");
    const std::string written = ToJson(table, false);
    Table back;
    Check(Read(written, back).ok && back.games[0].vehicles[0].cameras[0].name == "Say \"hi\" \\ there",
          "and is written so that it reads back");
    Check(std::string(TypeName(CameraType::Chase)) == "chase" &&
              std::string(TypeName(CameraType::FirstPerson)) == "firstperson",
          "camera types as the file writes them");
}

int main()
{
    TestBuiltInTable();
    TestGames();
    TestCamerasAndTethering();
    TestLeash();
    TestMatching();
    TestCarryMode();
    TestBoundsAndErrors();
    std::printf("PASS: %u vehicle camera file checks\n", g_checks);
    return 0;
}
