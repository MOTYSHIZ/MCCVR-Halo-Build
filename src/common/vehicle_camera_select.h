#pragma once
// WHICH VEHICLE CAMERA IS IN USE: the camera file's table (vehicle_camera_file.h) applied to the seat the player
// is in. Picks the entry for the vehicle and seat, starts in the seat's remembered or default camera, steps
// through cameras and their tethering modes, and follows a reloaded file without moving the player.
//
// The result is ActiveCamera: a plain record with no pointers and no strings to follow, so a camera hook can
// copy it without allocating. Selector itself allocates and is for one thread at a time; whoever owns it
// publishes its ActiveCamera to the hooks.
//
// Pure: no engine, no clock, no file I/O.

#include <cstdint>
#include <map>
#include <string>

#include "vehicle_camera_file.h"

namespace vehicle_camera_select
{
inline constexpr int kMarkerCapacity = 48;
// StandDownReason() while the player is not in a seat at all.
inline constexpr char kOnFoot[] = "on foot";

struct ActiveCamera
{
    bool valid = false; // false: no camera of ours for this seat (on foot, no entry, or an entry set "enabled": false)
    bool chase = false; // false: a firstperson camera -- the seat view as it is without the file
    uint8_t origin = 0; // vehicle_camera_file::Origin
    char marker[kMarkerCapacity]{}; // origin Marker: its name, cut to fit
    float offsetCm[3]{};
    float leashMinCm[3]{};
    float leashMaxCm[3]{};
    bool locYaw = false, locPitch = false, locRoll = false, locView = false;
    bool rotYaw = false, rotPitch = false, rotRoll = false;
    bool collide = false;
    float collideMarginCm = 0.0f;
    float lagSeconds = 0.0f; // vehicle_camera_file::LagSecondsOf: 0 = rigid
    float lagDamping = 1.0f;
    int camera = 0, cameraCount = 0;
    int mode = 0, modeCount = 0;
    // Counts the times the camera or mode in use became a different one by the player's doing or the seat's:
    // getting in or out, changing seat, stepping. A reloaded file that keeps the same camera does not count, so
    // a consumer that re-aligns the view on a change leaves it alone when only a number was edited. It never
    // goes back, so two records with the same count describe the same stretch in one camera.
    uint32_t changes = 0;

    friend bool operator==(const ActiveCamera&, const ActiveCamera&) = default;
};

class Selector
{
public:
    // The file's contents. The seat in use is matched again; its camera and mode are kept by name where the new
    // table still has them (else by position), and `changes` moves only if a different camera or mode results.
    void SetTable(vehicle_camera_file::Table table);

    // The seat this moment. `seated` false = on foot: nothing selected. `gameKey` is one of
    // vehicle_camera_file::kGameKeys; `vehicleName` is what the entries' "match" texts are looked for in; `seat`
    // is SeatBits, 0 while the game has not said. `seatView`: the game has a first-person seat view to give this
    // seat; a seat with none (a vehicle the mod does not know) starts in the entry's first chase camera where its
    // default is the seat view. Cheap while nothing changed: no allocation, a few comparisons.
    // Returns true when the published camera changed in any way.
    bool Update(const char* gameKey, bool seated, const char* vehicleName, uint8_t seat, bool seatView = true);

    // The next (+1) or previous (-1) camera of the seat, in the mode that keeps the player's tethering choice
    // (vehicle_camera_file::CarryMode). False when there is no other camera to step to.
    bool StepCamera(int direction);
    // The current camera's next or previous tethering mode. False when it has only one.
    bool StepMode(int direction);

    // THE PLAYER'S CONTROL CHOICE FOR THIS SEAT (vehicle_control's modes, as numbers). `base` is the setting the
    // seat would have without one. A choice is kept WITH the base it was made against: when the base has changed
    // since, the base is the newer statement of what is wanted and the choice is forgotten. -1: none (or not
    // seated).
    int ControlChoice(int base);
    // Keeps `choice` for this seat, made against `base`. A choice equal to its base forgets itself.
    void SetControlChoice(int choice, int base);

    // WHAT EACH SEAT WAS LAST LEFT IN -- camera, tethering mode and control choice -- per game, vehicle and seat,
    // and kept across sessions by the caller: MemoryText() is the file's text, LoadMemory() reads it back (lines
    // it does not understand are skipped) and returns how many seats it read. MemoryDirty() is true from a
    // change until MemorySaved().
    std::string MemoryText() const;
    int LoadMemory(const std::string& text);
    bool MemoryDirty() const noexcept { return memoryDirty_; }
    void MemorySaved() noexcept { memoryDirty_ = false; }

    const ActiveCamera& Active() const noexcept { return active_; }
    // One line for the log, empty when nothing is selected:
    //   Warthog gunner (Gunner): camera 2 of 3 "Behind", mode 1 of 2 "Untethered", chase from vehicle,
    //   offset -450 0 180 cm, location Yaw, Pitch, Roll, rotation None, leash None
    //   default (Driver): camera 1 of 2 "Seat", the seat view
    std::string Describe() const;
    // The same, short, for the headset: "<vehicle> \u00B7 <camera>[, <mode>]" -- the mode only where the camera
    // has more than one; the vehicle by its entry's name, or by its own where the default entry has it. Empty
    // when nothing is selected.
    std::wstring Note() const;
    // Why nothing is selected, for the log; empty when a camera is.
    const std::string& StandDownReason() const noexcept { return standDown_; }

private:
    // The player's last choice in a seat, kept for the next time he sits in it. The camera and mode are the
    // entry's: a seat matched to another entry since (the file was edited) starts in that entry's defaults.
    struct Remembered
    {
        std::string entry;
        int camera = -1;
        std::string cameraName;
        int mode = -1;
        std::string modeName;
        int control = -1;
        int controlBase = -1;
    };

    const vehicle_camera_file::Vehicle* Entry() const noexcept;
    bool Match();
    void Select(int camera, int mode, bool countsAsChange);
    void Clear(std::string reason);
    void Remember();
    // Per game, vehicle and seat: not per entry, so one "default" entry still keeps each vehicle's own choice.
    std::string MemoryKey() const;

    vehicle_camera_file::Table table_ = vehicle_camera_file::DefaultTable();
    std::map<std::string, Remembered> memory_;
    bool memoryDirty_ = false;
    std::string gameKey_;
    std::string vehicleName_;
    std::string standDown_ = kOnFoot;
    uint8_t seat_ = 0;
    bool seatView_ = true;
    bool seated_ = false;
    const vehicle_camera_file::Game* game_ = nullptr; // into table_
    int entry_ = -1;
    std::string entryName_;
    // The ride the camera in use was chosen for: a match for the same game, vehicle and seat (or a seat the
    // game has only now named) keeps it; any other is a new seat.
    std::string rideGame_, rideVehicle_;
    uint8_t rideSeat_ = 0;
    std::string cameraName_;
    std::string modeName_;
    // The tethering mode the player last had in a camera that offers a choice, while he stays seated: stepping
    // through a camera with one mode (the seat view) must not forget it.
    vehicle_camera_file::Tether choice_;
    bool haveChoice_ = false;
    ActiveCamera active_{};
};
} // namespace vehicle_camera_select
