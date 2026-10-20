#pragma once
// THE VEHICLE CAMERA FILE: per game and per vehicle seat, an ordered list of cameras the player steps through
// while seated, each with its own placement, its own motion settings and its TETHERING MODES.
//
//   { "version": 1,
//     "games": {
//       "halo3": {
//         "vehicles": {
//           "Warthog gunner": { "match": ["warthog"], "seat": ["gunner"], "defaultCamera": 0,
//                               "cameras": [ { "name": "Seat", "type": "firstperson" },
//                                            { "name": "Behind", "origin": "vehicle", "offset": [-450, 0, 180],
//                                              "tethering": [ { "name": "Untethered" },
//                                                             { "name": "Tethered", "rotationTracking": ["yaw"] } ] } ] },
//           "default": { "cameras": [ ... ] } } },
//       "default": { "vehicles": { "default": { "cameras": [ ... ] } } } } }
//
// A game's section is the one under its key (kGameKeys), else "default". Inside it a vehicle is picked by the
// entry with the LONGEST "match" text (case-insensitive substrings; the entry's own key when absent) found in the
// vehicle's name, then an entry naming the seat over one that does not, then the earlier entry; the "default"
// entry catches the rest. A field the file leaves out takes its built-in default. Keys starting with '_' are the
// player's notes and are skipped.
//
// Distances in the file are centimetres. An offset or a leash limit is [forward, right, up].
//
// Pure: no engine and no file I/O. The caller reads the file and hands the text in.

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace vehicle_camera_file
{
// The game keys a file may use, in the order of halomccvr.cfg's per-title prefixes, plus kDefaultKey.
inline constexpr const char* kGameKeys[] = {"halo3", "odst", "reach", "halo4", "halo2", "halo1"};
inline constexpr int kGameKeyCount = static_cast<int>(sizeof(kGameKeys) / sizeof(kGameKeys[0]));
inline constexpr const char* kDefaultKey = "default";

// chase: a camera this mod places. firstperson: the seat view the game and the mod already give that seat; its
// placement fields are not used.
enum class CameraType : uint8_t { Chase = 0, FirstPerson = 1 };

// Where a camera's offset is measured FROM: the vehicle's own origin, the seat the player sits in, the seated
// character's head, or -- any other name in the file -- a marker or node of the vehicle, so a camera can ride a
// part that turns on its own (a turret). A marker that is not found falls back to the vehicle's origin.
// ThirdPerson ("thirdperson") is where the GAME'S OWN third-person camera rests for the seat: the point it
// turns about and its own distance behind, so a zero offset is the view the game gives that vehicle, whatever
// its size. It is what a vehicle nobody wrote an entry for gets.
enum class Origin : uint8_t { Vehicle = 0, Seat = 1, Head = 2, Marker = 3, ThirdPerson = 4 };

// THE LEASH ("leashMin" / "leashMax"): a box the player's HEAD stays inside, in the same space as "offset" --
// [forward, right, up] in cm FROM THE ORIGIN -- so a limit is a place and the two are tuned together. Past a limit
// the view stops following the head that way, so a lean in a tight seat cannot put the eyes through the canopy.
// kUnleashed on an axis = no limit that way (null in the file, or left out).
inline constexpr float kUnleashed = 1.0e9f;
inline bool LeashSet(const float side[3]) noexcept
{
    for (int axis = 0; axis < 3; ++axis)
        if (side[axis] > -kUnleashed && side[axis] < kUnleashed)
            return true;
    return false;
}

// ONE TETHERING MODE of a camera: what carries the camera's position round (locationTracking), what turns the
// player's view (rotationTracking), where it sits (offset), what that is measured from (origin) and the leash.
// Whatever a mode leaves out in the file is its camera's own; the reader fills those in, so a Tether is complete.
struct Tether
{
    std::string name;
    float offset[3] = {0.0f, 0.0f, 0.0f};
    float leashMin[3] = {-kUnleashed, -kUnleashed, -kUnleashed};
    float leashMax[3] = {kUnleashed, kUnleashed, kUnleashed};
    bool locYaw = true, locPitch = true, locRoll = true, locView = false;
    bool rotYaw = false, rotPitch = false, rotRoll = false;
    int origin = -1;          // -1 = the camera's; else an Origin
    std::string originMarker; // origin Marker: the marker's or node's name
};

// A camera that does not set "lag": one where the game's own third-person camera rests lags this much (that camera
// stands a long way back, where every turn and bump of the vehicle swings it through a wide arc); any other none.
// Its damping, unset: past critical, so the far-back camera settles without swinging past its place.
inline constexpr float kThirdPersonLagSeconds = 0.5f;
inline constexpr float kThirdPersonLagDamping = 1.5f;
inline constexpr float kMaxLagSeconds = 5.0f;
inline constexpr float kMinLagDamping = 0.1f;
inline constexpr float kMaxLagDamping = 2.0f;

struct Camera
{
    std::string name; // optional; an unnamed camera is shown by its number
    CameraType type = CameraType::Chase;
    Origin origin = Origin::Vehicle;
    std::string originMarker;
    float offset[3] = {0.0f, 0.0f, 0.0f};
    float leashMin[3] = {-kUnleashed, -kUnleashed, -kUnleashed};
    float leashMax[3] = {kUnleashed, kUnleashed, kUnleashed};
    // LOCATION TRACKING: which of the vehicle's rotations carry the offset round. All three = rigid to the
    // vehicle; yaw only = a level offset that turns with it; none = a fixed world direction. locView instead
    // makes the offset ride the PLAYER'S view: an orbiting camera.
    bool locYaw = true, locPitch = true, locRoll = true;
    bool locView = false;
    // ROTATION TRACKING: which of the vehicle's rotations turn the player's VIEW. Independent of each other:
    // pitch and roll without yaw tilt the view with the vehicle's deck while the player keeps his own heading.
    bool rotYaw = false, rotPitch = false, rotRoll = false;
    bool collide = true;
    float collideMargin = 30.0f; // cm short of whatever blocks the offset
    // LAG ("lag", "lagDamping"): the camera follows its place on a spring and damper rather than a rigid boom.
    // Seconds to catch up with a turn or a bump; < 0 = not set (LagSecondsOf). lagDamping 1 = no overshoot, more =
    // slower to settle; < 0 = not set (LagDampingOf).
    float lag = -1.0f;
    float lagDamping = -1.0f;
    // "tethering": the modes this camera steps through. Empty = one mode, the camera's own fields above.
    std::vector<Tether> tethering;

    int ModeCount() const noexcept { return tethering.empty() ? 1 : static_cast<int>(tethering.size()); }
    // Mode `index`, wrapped into range; the camera's own fields when it lists no tethering.
    Tether Mode(int index) const;
    Origin OriginOf(const Tether& mode) const noexcept
    {
        return mode.origin >= 0 ? static_cast<Origin>(mode.origin) : origin;
    }
    const std::string& MarkerOf(const Tether& mode) const noexcept
    {
        return mode.origin >= 0 ? mode.originMarker : originMarker;
    }
};

// The lag a camera has: its own "lag", else kThirdPersonLagSeconds where it rests where the game's camera does.
inline float LagSecondsOf(const Camera& camera) noexcept
{
    if (camera.lag >= 0.0f)
        return camera.lag;
    return camera.origin == Origin::ThirdPerson ? kThirdPersonLagSeconds : 0.0f;
}

// Its damping: its own "lagDamping", else kThirdPersonLagDamping where it rests where the game's camera does, else 1.
inline float LagDampingOf(const Camera& camera) noexcept
{
    if (camera.lagDamping >= 0.0f)
        return camera.lagDamping;
    return camera.origin == Origin::ThirdPerson ? kThirdPersonLagDamping : 1.0f;
}

// THE SEAT, as the game reports it. A seat can be both a driver's and a gunner's (a turret's one seat drives and
// fires it), so an entry's "seat" list is tested against the seat's FLAGS: "driver", "gunner", and "passenger"
// for a seat that is neither. Zero bits = the game has not said.
enum class SeatRole : int8_t { Unknown = -1, Driver = 0, Gunner = 1, Passenger = 2 };
inline constexpr uint8_t kSeatDriver = 1u << static_cast<int>(SeatRole::Driver);
inline constexpr uint8_t kSeatGunner = 1u << static_cast<int>(SeatRole::Gunner);
inline constexpr uint8_t kSeatPassenger = 1u << static_cast<int>(SeatRole::Passenger);
constexpr uint8_t SeatBits(bool driver, bool gunner) noexcept
{
    return static_cast<uint8_t>((driver ? kSeatDriver : 0) | (gunner ? kSeatGunner : 0) |
                                ((!driver && !gunner) ? kSeatPassenger : 0));
}

struct Vehicle
{
    std::string name;               // the entry's key
    std::vector<std::string> match; // lower-case substrings of the vehicle's name
    bool isDefault = false;         // the "default" entry: never matched by name
    // "enabled": false -- none of this in the seats the entry matches: the seat view as it is without the file.
    // The entry still MATCHES, so it beats "default". It may then list no cameras.
    bool enabled = true;
    int defaultCamera = 0;
    int defaultMode = 0;
    // The seat's own leash: every camera starts from it, and a camera and then a mode may override it. The
    // reader resolves that, so each Camera and Tether already holds its effective limits.
    float leashMin[3] = {-kUnleashed, -kUnleashed, -kUnleashed};
    float leashMax[3] = {kUnleashed, kUnleashed, kUnleashed};
    // "seat": the seats this entry is for, as bits; 0 = any seat. A passenger rides the same vehicle as its
    // driver, so this is what tells their entries apart. Not used on "default".
    uint8_t seats = 0;
    std::vector<Camera> cameras;
};

struct Game
{
    std::string key; // one of kGameKeys, or kDefaultKey
    std::vector<Vehicle> vehicles;
};

struct Table
{
    std::vector<Game> games;
    // The section for `gameKey`, else the "default" section, else null.
    const Game* FindGame(const char* gameKey) const noexcept;
};

inline constexpr int kMaxVehicles = 64; // per game
inline constexpr int kMaxCameras = 16;  // per vehicle entry
inline constexpr int kMaxTethers = 8;   // per camera
// A larger file is refused before it is parsed.
inline constexpr std::size_t kMaxFileBytes = 512u * 1024u;

// The built-in table: what a missing file gives, and what the first run writes out. One "default" section whose
// "default" entry lists the existing seat view first and, second, a camera where the game's own third-person
// camera rests, so the feature changes nothing until the player steps to another camera and suits a vehicle of
// any size.
Table DefaultTable();

struct ParseResult
{
    bool ok{false};
    std::string error;                // a syntax or type error with "line L col C"; the table is untouched
    std::vector<std::string> ignored; // keys that matched nothing ("games.halo3.vehicles.Ghost.cameras[1].ofset")
    int vehicles{0}, cameras{0};
};

// Parse `text` into `out`, REPLACING it. On any error `out` is left exactly as it was.
ParseResult FromJson(const char* text, std::size_t len, Table& out);

// The table as the file format, with a guide to every key at the top (`guide` = false leaves it out).
// FromJson(ToJson(t)) gives t back.
std::string ToJson(const Table& table, bool guide = true);

// Index in `game.vehicles` of the entry for this vehicle name and seat, else the "default" entry, else -1.
// An entry with a "seat" list is considered only when the game has named the seat (`seat` != 0) and one of its
// flags is listed: guessing would hand a driver the passenger's cameras. An entry with no "seat" list is
// therefore the driver's, and the one any other seat of that vehicle uses when it has none of its own.
int MatchVehicle(const Game& game, const std::string& vehicleName, uint8_t seat = 0);

// Stepping onto camera `to` from a view in mode `from`: the mode that keeps what the player chose -- one of the
// same name, else one turning the view on the same axes, else the first. A comfort choice is not undone by
// changing camera.
int CarryMode(const Camera& to, const Tether& from);

const char* TypeName(CameraType type) noexcept;
// As the file writes it: the marker's name for a Marker origin.
std::string OriginText(Origin origin, const std::string& marker);
// For logs and readouts: "Driver", "Driver, Gunner", "Passenger"; "" = unknown.
std::string SeatText(uint8_t seat);
// "Yaw, Pitch, Roll" / "Pitch, Roll" / "None"; location adds "Your view (orbit)".
std::string RotationTrackingText(const Tether& mode);
std::string LocationTrackingText(const Tether& mode);
// The limits as places from the origin: "up at most 360 cm", "forward -30 to 15, up 40 to 65 cm"; "None".
std::string LeashText(const float leashMin[3], const float leashMax[3]);
} // namespace vehicle_camera_file
