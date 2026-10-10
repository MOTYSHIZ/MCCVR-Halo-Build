#include "vehicle_camera_select.h"

#include <cctype>
#include <cstdio>
#include <cstring>
#include <utility>

namespace vehicle_camera_select
{
namespace
{
using vehicle_camera_file::Camera;
using vehicle_camera_file::CameraType;
using vehicle_camera_file::Origin;
using vehicle_camera_file::Tether;
using vehicle_camera_file::Vehicle;

bool SameName(const std::string& a, const std::string& b) noexcept
{
    if (a.size() != b.size())
        return false;
    for (std::size_t i = 0; i < a.size(); ++i)
        if (std::tolower(static_cast<unsigned char>(a[i])) != std::tolower(static_cast<unsigned char>(b[i])))
            return false;
    return true;
}

int Wrap(int index, int count) noexcept
{
    return count > 0 ? ((index % count) + count) % count : 0;
}

// The camera an earlier choice points at in `entry` as it is now: the one still at that position under that
// name, else the first of that name, else whatever is at that position, else -1. Position is tried first so
// that two cameras of one name are not confused, and last so that a renamed camera keeps its player.
int FindCamera(const Vehicle& entry, const std::string& name, int index) noexcept
{
    const int count = static_cast<int>(entry.cameras.size());
    const bool inRange = index >= 0 && index < count;
    if (inRange && SameName(entry.cameras[static_cast<std::size_t>(index)].name, name))
        return index;
    if (!name.empty())
        for (int i = 0; i < count; ++i)
            if (SameName(entry.cameras[static_cast<std::size_t>(i)].name, name))
                return i;
    return inRange ? index : -1;
}

int FindMode(const Camera& camera, const std::string& name, int index) noexcept
{
    const int count = camera.ModeCount();
    const bool inRange = index >= 0 && index < count;
    if (camera.tethering.empty())
        return inRange ? index : -1;
    if (inRange && SameName(camera.tethering[static_cast<std::size_t>(index)].name, name))
        return index;
    if (!name.empty())
        for (int i = 0; i < count; ++i)
            if (SameName(camera.tethering[static_cast<std::size_t>(i)].name, name))
                return i;
    return inRange ? index : -1;
}

std::string Number(float value)
{
    char text[32];
    std::snprintf(text, sizeof(text), "%g", static_cast<double>(value));
    return text;
}
} // namespace

const Vehicle* Selector::Entry() const noexcept
{
    if (!game_ || entry_ < 0 || entry_ >= static_cast<int>(game_->vehicles.size()))
        return nullptr;
    return &game_->vehicles[static_cast<std::size_t>(entry_)];
}

std::string Selector::MemoryKey(const Vehicle& entry) const
{
    // Per game, not per section: a choice made in one game's seat is not another game's, even when both read
    // their cameras from the "default" section.
    return gameKey_ + '\n' + entry.name;
}

void Selector::Clear(std::string reason)
{
    const uint32_t changes = active_.changes + (active_.valid ? 1u : 0u);
    active_ = ActiveCamera{};
    active_.changes = changes;
    entry_ = -1;
    entryName_.clear();
    cameraName_.clear();
    modeName_.clear();
    haveChoice_ = false;
    standDown_ = std::move(reason);
}

void Selector::Select(int camera, int mode, bool countsAsChange)
{
    const Vehicle* entry = Entry();
    if (!entry || entry->cameras.empty())
        return;
    const int count = static_cast<int>(entry->cameras.size());
    if (camera < 0 || camera >= count)
        camera = 0;
    const Camera& chosen = entry->cameras[static_cast<std::size_t>(camera)];
    const int modes = chosen.ModeCount();
    mode = Wrap(mode, modes);
    const Tether tether = chosen.Mode(mode);

    ActiveCamera next{};
    next.valid = true;
    next.chase = chosen.type == CameraType::Chase;
    const Origin origin = chosen.OriginOf(tether);
    next.origin = static_cast<uint8_t>(origin);
    if (origin == Origin::Marker)
    {
        const std::string& marker = chosen.MarkerOf(tether);
        const std::size_t room = static_cast<std::size_t>(kMarkerCapacity - 1);
        std::memcpy(next.marker, marker.data(), marker.size() < room ? marker.size() : room);
    }
    for (int axis = 0; axis < 3; ++axis)
    {
        next.offsetCm[axis] = tether.offset[axis];
        next.leashMinCm[axis] = tether.leashMin[axis];
        next.leashMaxCm[axis] = tether.leashMax[axis];
    }
    next.locYaw = tether.locYaw;
    next.locPitch = tether.locPitch;
    next.locRoll = tether.locRoll;
    next.locView = tether.locView;
    next.rotYaw = tether.rotYaw;
    next.rotPitch = tether.rotPitch;
    next.rotRoll = tether.rotRoll;
    next.collide = chosen.collide;
    next.collideMarginCm = chosen.collideMargin;
    next.camera = camera;
    next.cameraCount = count;
    next.mode = mode;
    next.modeCount = modes;
    next.changes = active_.changes + (countsAsChange ? 1u : 0u);

    active_ = next;
    entryName_ = entry->name;
    cameraName_ = chosen.name;
    modeName_ = tether.name;
    if (modes > 1)
    {
        choice_ = tether;
        haveChoice_ = true;
    }
    standDown_.clear();
}

void Selector::Remember()
{
    const Vehicle* entry = Entry();
    if (!entry || !active_.valid)
        return;
    Remembered& kept = memory_[MemoryKey(*entry)];
    kept.camera = active_.camera;
    kept.cameraName = cameraName_;
    kept.mode = active_.mode;
    kept.modeName = modeName_;
}

bool Selector::Match()
{
    const ActiveCamera before = active_;
    const std::string previousEntry = entryName_;
    const std::string previousCamera = cameraName_;
    const std::string previousMode = modeName_;

    game_ = table_.FindGame(gameKey_.c_str());
    entry_ = game_ ? vehicle_camera_file::MatchVehicle(*game_, vehicleName_, seat_) : -1;
    const Vehicle* entry = Entry();
    if (!entry)
    {
        Clear(game_ ? "the camera file has no entry for this vehicle and no \"default\" entry"
                    : "the camera file has no section for this game and no \"default\" section");
        return !(before == active_);
    }
    if (!entry->enabled)
    {
        Clear("the camera file's entry \"" + entry->name + "\" is set \"enabled\": false");
        return !(before == active_);
    }
    if (entry->cameras.empty())
    {
        Clear("the camera file's entry \"" + entry->name + "\" lists no cameras");
        return !(before == active_);
    }

    if (before.valid && SameName(previousEntry, entry->name))
    {
        // The same seat's entry as before (the file was reloaded, or the game has only now named the seat):
        // the player stays in the camera and mode he is in.
        const int camera = FindCamera(*entry, previousCamera, before.camera);
        if (camera >= 0)
        {
            const Camera& kept = entry->cameras[static_cast<std::size_t>(camera)];
            const int mode = FindMode(kept, previousMode, before.mode);
            const bool otherKind = (kept.type == CameraType::Chase) != before.chase;
            if (mode >= 0)
            {
                Select(camera, mode, otherKind);
            }
            else
            {
                // His mode is gone from the file: the nearest to it the camera still has.
                Tether was;
                was.name = previousMode;
                was.rotYaw = before.rotYaw;
                was.rotPitch = before.rotPitch;
                was.rotRoll = before.rotRoll;
                Select(camera, vehicle_camera_file::CarryMode(kept, was), true);
            }
            return !(before == active_);
        }
    }

    // A seat the player was not in a moment ago: his last choice there, else the entry's defaults.
    int camera = entry->defaultCamera;
    int mode = entry->defaultMode;
    const auto found = memory_.find(MemoryKey(*entry));
    if (found != memory_.end())
    {
        const int remembered = FindCamera(*entry, found->second.cameraName, found->second.camera);
        if (remembered >= 0)
        {
            camera = remembered;
            const int rememberedMode =
                FindMode(entry->cameras[static_cast<std::size_t>(remembered)], found->second.modeName, found->second.mode);
            mode = rememberedMode >= 0 ? rememberedMode : 0;
        }
    }
    Select(camera, mode, true);
    return !(before == active_);
}

void Selector::SetTable(vehicle_camera_file::Table table)
{
    table_ = std::move(table);
    game_ = nullptr;
    if (seated_)
        Match();
}

bool Selector::Update(const char* gameKey, bool seated, const char* vehicleName, uint8_t seat)
{
    if (!seated)
    {
        if (!seated_)
            return false;
        seated_ = false;
        const bool was = active_.valid;
        Clear(kOnFoot);
        return was;
    }
    const char* key = gameKey ? gameKey : "";
    const char* name = vehicleName ? vehicleName : "";
    if (seated_ && seat_ == seat && gameKey_ == key && vehicleName_ == name)
        return false;
    seated_ = true;
    seat_ = seat;
    gameKey_ = key;
    vehicleName_ = name;
    return Match();
}

bool Selector::StepCamera(int direction)
{
    const Vehicle* entry = Entry();
    if (!entry || !active_.valid || active_.cameraCount < 2 || direction == 0)
        return false;
    const int count = static_cast<int>(entry->cameras.size());
    const int to = Wrap(active_.camera + (direction > 0 ? 1 : -1), count);
    if (to == active_.camera)
        return false;
    const Tether from = active_.modeCount > 1 || !haveChoice_
                            ? entry->cameras[static_cast<std::size_t>(active_.camera)].Mode(active_.mode)
                            : choice_;
    Select(to, vehicle_camera_file::CarryMode(entry->cameras[static_cast<std::size_t>(to)], from), true);
    Remember();
    return true;
}

bool Selector::StepMode(int direction)
{
    const Vehicle* entry = Entry();
    if (!entry || !active_.valid || active_.modeCount < 2 || direction == 0)
        return false;
    Select(active_.camera, Wrap(active_.mode + (direction > 0 ? 1 : -1), active_.modeCount), true);
    Remember();
    return true;
}

std::string Selector::Describe() const
{
    const Vehicle* entry = Entry();
    if (!entry || !active_.valid)
        return {};
    std::string line = entry->name;
    const std::string seat = vehicle_camera_file::SeatText(seat_);
    if (!seat.empty())
        line += " (" + seat + ")";
    line += ": camera " + std::to_string(active_.camera + 1) + " of " + std::to_string(active_.cameraCount);
    if (!cameraName_.empty())
        line += " \"" + cameraName_ + "\"";
    if (!active_.chase)
        return line + ", the seat view";
    if (active_.modeCount > 1)
    {
        line += ", mode " + std::to_string(active_.mode + 1) + " of " + std::to_string(active_.modeCount);
        if (!modeName_.empty())
            line += " \"" + modeName_ + "\"";
    }
    Tether mode;
    mode.locYaw = active_.locYaw;
    mode.locPitch = active_.locPitch;
    mode.locRoll = active_.locRoll;
    mode.locView = active_.locView;
    mode.rotYaw = active_.rotYaw;
    mode.rotPitch = active_.rotPitch;
    mode.rotRoll = active_.rotRoll;
    line += ", chase from " + vehicle_camera_file::OriginText(static_cast<Origin>(active_.origin), active_.marker);
    line += ", offset " + Number(active_.offsetCm[0]) + " " + Number(active_.offsetCm[1]) + " " +
            Number(active_.offsetCm[2]) + " cm";
    line += ", location " + vehicle_camera_file::LocationTrackingText(mode);
    line += ", rotation " + vehicle_camera_file::RotationTrackingText(mode);
    line += ", leash " + vehicle_camera_file::LeashText(active_.leashMinCm, active_.leashMaxCm);
    return line;
}
} // namespace vehicle_camera_select
