#include "vehicle_camera_file.h"

#include "json_lite.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <utility>

namespace vehicle_camera_file
{
namespace
{
std::string Lower(std::string text)
{
    for (char& c : text)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return text;
}

bool SameText(const std::string& a, const char* b)
{
    return Lower(a) == Lower(std::string(b));
}

bool IsNote(const std::string& key)
{
    return !key.empty() && key[0] == '_';
}

std::string Indexed(const std::string& where, std::size_t index)
{
    return where + "[" + std::to_string(index) + "]";
}

// ---- reading ------------------------------------------------------------------------------------------------
using json_lite::Value;

struct Reader
{
    const char* text;
    ParseResult& result;

    bool TypeError(const Value& value, const std::string& where, const char* want)
    {
        int line = 0, col = 0;
        json_lite::LineCol(text, value.at, line, col);
        char buffer[400];
        std::snprintf(buffer, sizeof(buffer), "%s must be %s (line %d col %d)", where.c_str(), want, line, col);
        result.error = buffer;
        return false;
    }

    // Offsets and margins, in cm. Bounded well inside float range: the JSON reader refuses only a non-finite
    // DOUBLE, and 1e39 would narrow to infinity here and then to a non-finite view position.
    bool Number(const Value& value, const std::string& where, float& out)
    {
        if (value.IsNull())
            return true;
        if (!value.IsNumber() || !(std::fabs(value.number) <= 1.0e6))
            return TypeError(value, where, "a number from -1000000 to 1000000");
        out = static_cast<float>(value.number);
        return true;
    }

    bool Boolean(const Value& value, const std::string& where, bool& out)
    {
        if (value.IsNull())
            return true;
        if (!value.IsBool())
            return TypeError(value, where, "true or false");
        out = value.boolean;
        return true;
    }

    // true / false (1 / 0), or null = unset (-1).
    bool TriState(const Value& value, const std::string& where, int& out, const char* want)
    {
        if (value.IsNull())
        {
            out = -1;
            return true;
        }
        if (!value.IsBool())
            return TypeError(value, where, want);
        out = value.boolean ? 1 : 0;
        return true;
    }

    bool Index(const Value& value, const std::string& where, int& out, const char* want)
    {
        // Bounded before the cast: a huge number would otherwise wrap to a negative index.
        if (!value.IsNumber() || value.number < 0.0 || value.number > 1000.0 || value.number != std::floor(value.number))
            return TypeError(value, where, want);
        out = static_cast<int>(value.number);
        return true;
    }

    bool Offset(const Value& value, const std::string& where, float out[3])
    {
        if (!value.IsArray() || value.items.size() > 3)
            return TypeError(value, where, "[forward, right, up] in cm");
        for (std::size_t i = 0; i < value.items.size(); ++i)
            if (!Number(value.items[i], Indexed(where, i), out[i]))
                return false;
        return true;
    }

    // A list of any of "yaw", "pitch", "roll" ([] = none).
    bool Axes(const Value& value, const std::string& where, bool& yaw, bool& pitch, bool& roll)
    {
        if (!value.IsArray())
            return TypeError(value, where, "a list of any of \"yaw\", \"pitch\", \"roll\" ([] = none)");
        yaw = pitch = roll = false;
        for (std::size_t i = 0; i < value.items.size(); ++i)
        {
            const Value& item = value.items[i];
            const std::string at = Indexed(where, i);
            if (!item.IsString())
                return TypeError(item, at, "\"yaw\", \"pitch\" or \"roll\"");
            if (SameText(item.text, "yaw"))
                yaw = true;
            else if (SameText(item.text, "pitch"))
                pitch = true;
            else if (SameText(item.text, "roll"))
                roll = true;
            else
                return TypeError(item, at, "\"yaw\", \"pitch\" or \"roll\"");
        }
        return true;
    }

    // locationTracking: a list of axes, or "view".
    bool LocationTracking(const Value& value, const std::string& where, bool& view, bool& yaw, bool& pitch, bool& roll)
    {
        if (value.IsString())
        {
            if (!SameText(value.text, "view"))
                return TypeError(value, where, "a list of any of \"yaw\", \"pitch\", \"roll\", or \"view\"");
            view = true;
            yaw = pitch = roll = false;
            return true;
        }
        view = false;
        return Axes(value, where, yaw, pitch, roll);
    }

    // "leashMin" / "leashMax" as the file wrote them: per axis a limit in cm, null (no limit that way) or
    // "inherit" (the level above's value on that axis: the seat's for a camera, the camera's for a mode). The
    // whole key may be null (no limits) or "inherit" (the same as leaving it out); a shorter list leaves the
    // axes it does not reach unlimited. Kept as written until the level above is known, then resolved: a mode's
    // list may come before its camera's own leash in the file.
    struct LeashSide
    {
        float value[3];
        bool inherit[3];
    };
    static LeashSide Inherited() { return LeashSide{{0.0f, 0.0f, 0.0f}, {true, true, true}}; }
    static void Resolve(const LeashSide& side, const float parent[3], float out[3])
    {
        for (int axis = 0; axis < 3; ++axis)
            out[axis] = side.inherit[axis] ? parent[axis] : side.value[axis];
    }
    bool Leash(const Value& value, const std::string& where, bool isMin, LeashSide& out)
    {
        const float none = isMin ? -kUnleashed : kUnleashed;
        for (int axis = 0; axis < 3; ++axis)
        {
            out.value[axis] = none;
            out.inherit[axis] = false;
        }
        const char* want =
            "[forward, right, up] in cm from the origin, each a number, null (no limit) or \"inherit\"";
        if (value.IsNull())
            return true;
        if (value.IsString())
        {
            if (!SameText(value.text, "inherit"))
                return TypeError(value, where, want);
            for (int axis = 0; axis < 3; ++axis)
                out.inherit[axis] = true;
            return true;
        }
        if (!value.IsArray() || value.items.size() > 3)
            return TypeError(value, where, want);
        for (std::size_t i = 0; i < value.items.size(); ++i)
        {
            const Value& item = value.items[i];
            const std::string at = Indexed(where, i);
            if (item.IsNull())
                continue;
            if (item.IsString())
            {
                if (!SameText(item.text, "inherit"))
                    return TypeError(item, at, want);
                out.inherit[i] = true;
                continue;
            }
            float limit = none;
            if (!Number(item, at, limit))
                return false;
            out.value[i] = limit;
        }
        return true;
    }

    // "vehicle" | "seat" | "playerhead" ("head" too) | "thirdperson"; any other name is a marker or node of the
    // vehicle.
    bool OriginValue(const Value& value, const std::string& where, Origin& origin, std::string& marker, const char* want)
    {
        if (!value.IsString() || value.text.empty())
            return TypeError(value, where, want);
        marker.clear();
        if (SameText(value.text, "vehicle"))
            origin = Origin::Vehicle;
        else if (SameText(value.text, "seat"))
            origin = Origin::Seat;
        else if (SameText(value.text, "playerhead") || SameText(value.text, "head"))
            origin = Origin::Head;
        else if (SameText(value.text, "thirdperson"))
            origin = Origin::ThirdPerson;
        else
        {
            origin = Origin::Marker;
            marker = value.text;
        }
        return true;
    }

    // One tethering mode with what it named; the rest is filled from its camera once the whole camera is read
    // (the file may list "tethering" before the camera's own tracking).
    struct RawTether
    {
        Tether mode;
        bool hasLocation = false, hasRotation = false, hasOffset = false;
        LeashSide leashMin = Inherited(), leashMax = Inherited();
    };
    bool ReadTether(const Value& value, const std::string& where, RawTether& raw)
    {
        if (!value.IsObject())
            return TypeError(value, where, "an object { \"name\": ..., \"rotationTracking\": [...] }");
        for (std::size_t k = 0; k < value.keys.size(); ++k)
        {
            const std::string& key = value.keys[k];
            const Value& item = value.items[k];
            const std::string at = where + "." + key;
            if (IsNote(key))
                continue;
            bool ok = true;
            if (key == "name")
            {
                if (item.IsNull())
                {
                    raw.mode.name.clear();
                    continue;
                }
                if (!item.IsString())
                    return TypeError(item, at, "text");
                raw.mode.name = item.text;
            }
            else if (key == "locationTracking")
            {
                raw.hasLocation = true;
                ok = LocationTracking(item, at, raw.mode.locView, raw.mode.locYaw, raw.mode.locPitch, raw.mode.locRoll);
            }
            else if (key == "rotationTracking")
            {
                raw.hasRotation = true;
                ok = Axes(item, at, raw.mode.rotYaw, raw.mode.rotPitch, raw.mode.rotRoll);
            }
            else if (key == "offset")
            {
                raw.hasOffset = true;
                ok = Offset(item, at, raw.mode.offset);
            }
            else if (key == "leashMin")
            {
                ok = Leash(item, at, true, raw.leashMin);
            }
            else if (key == "leashMax")
            {
                ok = Leash(item, at, false, raw.leashMax);
            }
            else if (key == "aimMarker")
            {
                ok = TriState(item, at, raw.mode.aimMarker, "true, false or null (null = the camera's)");
            }
            else if (key == "origin")
            {
                if (item.IsNull())
                {
                    raw.mode.origin = -1;
                    raw.mode.originMarker.clear();
                    continue;
                }
                Origin origin = Origin::Vehicle;
                if (!OriginValue(item, at, origin, raw.mode.originMarker,
                                 "\"vehicle\", \"seat\", \"playerhead\", \"thirdperson\", a marker name, or null (null = the camera's)"))
                    return false;
                raw.mode.origin = static_cast<int>(origin);
            }
            else
            {
                result.ignored.push_back(at);
            }
            if (!ok)
                return false;
        }
        return true;
    }

    // `camera` arrives holding its seat's leash: that is what "inherit" takes here.
    bool ReadCamera(const Value& value, const std::string& where, Camera& camera)
    {
        if (!value.IsObject())
            return TypeError(value, where, "an object { \"name\": ..., \"offset\": [...] }");
        const float seatMin[3] = {camera.leashMin[0], camera.leashMin[1], camera.leashMin[2]};
        const float seatMax[3] = {camera.leashMax[0], camera.leashMax[1], camera.leashMax[2]};
        std::vector<RawTether> raw;
        for (std::size_t k = 0; k < value.keys.size(); ++k)
        {
            const std::string& key = value.keys[k];
            const Value& item = value.items[k];
            const std::string at = where + "." + key;
            if (IsNote(key))
                continue;
            bool ok = true;
            if (key == "name")
            {
                if (item.IsNull())
                {
                    camera.name.clear();
                    continue;
                }
                if (!item.IsString())
                    return TypeError(item, at, "text");
                camera.name = item.text;
            }
            else if (key == "type")
            {
                if (!item.IsString())
                    return TypeError(item, at, "\"chase\" or \"firstperson\"");
                if (SameText(item.text, "chase"))
                    camera.type = CameraType::Chase;
                else if (SameText(item.text, "firstperson"))
                    camera.type = CameraType::FirstPerson;
                else
                    return TypeError(item, at, "\"chase\" or \"firstperson\"");
            }
            else if (key == "origin")
            {
                if (!OriginValue(item, at, camera.origin, camera.originMarker,
                                 "\"vehicle\", \"seat\", \"playerhead\", \"thirdperson\", or a marker name on the vehicle"))
                    return false;
            }
            else if (key == "offset")
            {
                ok = Offset(item, at, camera.offset);
            }
            else if (key == "leashMin")
            {
                LeashSide side;
                ok = Leash(item, at, true, side);
                if (ok)
                    Resolve(side, seatMin, camera.leashMin);
            }
            else if (key == "leashMax")
            {
                LeashSide side;
                ok = Leash(item, at, false, side);
                if (ok)
                    Resolve(side, seatMax, camera.leashMax);
            }
            else if (key == "locationTracking")
            {
                ok = LocationTracking(item, at, camera.locView, camera.locYaw, camera.locPitch, camera.locRoll);
            }
            else if (key == "rotationTracking")
            {
                ok = Axes(item, at, camera.rotYaw, camera.rotPitch, camera.rotRoll);
            }
            else if (key == "collide")
            {
                ok = Boolean(item, at, camera.collide);
            }
            else if (key == "collideMargin")
            {
                ok = Number(item, at, camera.collideMargin);
            }
            else if (key == "lag")
            {
                ok = Number(item, at, camera.lag);
                if (ok && !(camera.lag >= 0.0f && camera.lag <= kMaxLagSeconds))
                    return TypeError(item, at, "seconds from 0 (no lag) to 5");
            }
            else if (key == "lagDamping")
            {
                ok = Number(item, at, camera.lagDamping);
                if (ok && !(camera.lagDamping >= kMinLagDamping && camera.lagDamping <= kMaxLagDamping))
                    return TypeError(item, at, "a damping from 0.1 to 2 (1 = no overshoot)");
            }
            else if (key == "aimMarker")
            {
                ok = TriState(item, at, camera.aimMarker, "true, false or null (null = the seat's aimMarker)");
            }
            else if (key == "tethering")
            {
                if (!item.IsArray() || item.items.empty())
                    return TypeError(item, at, "a list of at least one tethering mode");
                if (static_cast<int>(item.items.size()) > kMaxTethers)
                    return TypeError(item, at, "at most 8 tethering modes");
                for (std::size_t i = 0; i < item.items.size(); ++i)
                {
                    RawTether one;
                    if (!ReadTether(item.items[i], Indexed(at, i), one))
                        return false;
                    raw.push_back(one);
                }
            }
            else
            {
                result.ignored.push_back(at);
            }
            if (!ok)
                return false;
        }
        // Each mode takes the camera's own values for what it left out.
        camera.tethering.clear();
        for (RawTether& one : raw)
        {
            if (!one.hasLocation)
            {
                one.mode.locView = camera.locView;
                one.mode.locYaw = camera.locYaw;
                one.mode.locPitch = camera.locPitch;
                one.mode.locRoll = camera.locRoll;
            }
            if (!one.hasRotation)
            {
                one.mode.rotYaw = camera.rotYaw;
                one.mode.rotPitch = camera.rotPitch;
                one.mode.rotRoll = camera.rotRoll;
            }
            if (!one.hasOffset)
                for (int axis = 0; axis < 3; ++axis)
                    one.mode.offset[axis] = camera.offset[axis];
            Resolve(one.leashMin, camera.leashMin, one.mode.leashMin);
            Resolve(one.leashMax, camera.leashMax, one.mode.leashMax);
            camera.tethering.push_back(one.mode);
        }
        return true;
    }

    bool ReadSeats(const Value& value, const std::string& where, uint8_t& seats)
    {
        // "passenger", or a list of them; [] or null = any seat.
        seats = 0;
        if (value.IsNull())
            return true;
        const char* want = "\"driver\", \"gunner\" or \"passenger\" (or a list of them)";
        const auto roleBit = [](const std::string& role) -> uint8_t {
            if (SameText(role, "driver"))
                return kSeatDriver;
            if (SameText(role, "gunner"))
                return kSeatGunner;
            if (SameText(role, "passenger"))
                return kSeatPassenger;
            return 0;
        };
        if (value.IsString())
        {
            seats = roleBit(value.text);
            return seats != 0 || TypeError(value, where, want);
        }
        if (!value.IsArray())
            return TypeError(value, where, want);
        for (std::size_t i = 0; i < value.items.size(); ++i)
        {
            const Value& item = value.items[i];
            const uint8_t bit = item.IsString() ? roleBit(item.text) : uint8_t{0};
            if (bit == 0)
                return TypeError(item, Indexed(where, i), "\"driver\", \"gunner\" or \"passenger\"");
            seats |= bit;
        }
        return true;
    }

    bool ReadVehicle(const std::string& key, const Value& value, const std::string& where, Vehicle& out)
    {
        if (!value.IsObject())
            return TypeError(value, where, "an object { \"cameras\": [...] }");
        out = Vehicle{};
        out.name = key;
        out.isDefault = SameText(key, kDefaultKey);
        bool haveMatch = false, haveCameras = false;
        // The seat's leash FIRST, wherever the file puts it: every camera starts from it, and "cameras" may come
        // before it in the file. Nothing sits above a seat, so "inherit" here is no limit.
        for (std::size_t k = 0; k < value.keys.size(); ++k)
        {
            const std::string& field = value.keys[k];
            if (field != "leashMin" && field != "leashMax")
                continue;
            const bool isMin = field == "leashMin";
            const float none[3] = {isMin ? -kUnleashed : kUnleashed, isMin ? -kUnleashed : kUnleashed,
                                   isMin ? -kUnleashed : kUnleashed};
            LeashSide side;
            if (!Leash(value.items[k], where + "." + field, isMin, side))
                return false;
            Resolve(side, none, isMin ? out.leashMin : out.leashMax);
        }
        for (std::size_t k = 0; k < value.keys.size(); ++k)
        {
            const std::string& field = value.keys[k];
            const Value& item = value.items[k];
            const std::string at = where + "." + field;
            if (IsNote(field) || field == "leashMin" || field == "leashMax")
                continue;
            if (field == "match")
            {
                haveMatch = true;
                if (item.IsString())
                {
                    if (!item.text.empty())
                        out.match.push_back(Lower(item.text));
                    continue;
                }
                if (!item.IsArray())
                    return TypeError(item, at, "text or a list of text");
                for (std::size_t i = 0; i < item.items.size(); ++i)
                {
                    if (!item.items[i].IsString())
                        return TypeError(item.items[i], Indexed(at, i), "text");
                    if (!item.items[i].text.empty())
                        out.match.push_back(Lower(item.items[i].text));
                }
            }
            else if (field == "defaultCamera")
            {
                if (!Index(item, at, out.defaultCamera, "a camera index from 0"))
                    return false;
            }
            else if (field == "defaultMode")
            {
                if (!Index(item, at, out.defaultMode, "a tethering mode index from 0"))
                    return false;
            }
            else if (field == "enabled")
            {
                if (!item.IsBool())
                    return TypeError(item, at, "true or false (false = the seat view as it is without this file)");
                out.enabled = item.boolean;
            }
            else if (field == "aimMarker")
            {
                if (!TriState(item, at, out.aimMarker,
                              "true, false or null (null = shown where the view turns with the vehicle)"))
                    return false;
            }
            else if (field == "seat")
            {
                if (!ReadSeats(item, at, out.seats))
                    return false;
            }
            else if (field == "cameras")
            {
                haveCameras = true;
                if (!item.IsArray() || item.items.empty())
                    return TypeError(item, at, "a list of at least one camera");
                if (static_cast<int>(item.items.size()) > kMaxCameras)
                    return TypeError(item, at, "at most 16 cameras");
                for (std::size_t i = 0; i < item.items.size(); ++i)
                {
                    Camera camera;
                    for (int axis = 0; axis < 3; ++axis)
                    {
                        camera.leashMin[axis] = out.leashMin[axis];
                        camera.leashMax[axis] = out.leashMax[axis];
                    }
                    if (!ReadCamera(item.items[i], Indexed(at, i), camera))
                        return false;
                    out.cameras.push_back(std::move(camera));
                }
            }
            else
            {
                result.ignored.push_back(at);
            }
        }
        // An entry set "enabled": false keeps the seat view as it is, so it needs no cameras of its own.
        if (!haveCameras && out.enabled)
            return TypeError(value, where, "an object with a \"cameras\" list (or \"enabled\": false)");
        if (!haveMatch && !out.isDefault)
            out.match.push_back(Lower(key));
        if (out.cameras.empty())
        {
            out.defaultCamera = 0;
            out.defaultMode = 0;
            return true;
        }
        if (out.defaultCamera >= static_cast<int>(out.cameras.size()))
            out.defaultCamera = 0;
        if (out.defaultMode >= out.cameras[static_cast<std::size_t>(out.defaultCamera)].ModeCount())
            out.defaultMode = 0;
        return true;
    }

    bool ReadGame(const std::string& key, const Value& value, const std::string& where, Game& out)
    {
        if (!value.IsObject())
            return TypeError(value, where, "an object { \"vehicles\": { ... } }");
        out = Game{};
        out.key = Lower(key);
        bool haveVehicles = false;
        for (std::size_t k = 0; k < value.keys.size(); ++k)
        {
            const std::string& field = value.keys[k];
            const Value& item = value.items[k];
            const std::string at = where + "." + field;
            if (IsNote(field))
                continue;
            if (field != "vehicles")
            {
                result.ignored.push_back(at);
                continue;
            }
            haveVehicles = true;
            if (!item.IsObject())
                return TypeError(item, at, "an object { \"Warthog\": { ... }, ... }");
            int count = 0;
            for (std::size_t i = 0; i < item.keys.size(); ++i)
            {
                if (IsNote(item.keys[i]))
                    continue;
                if (++count > kMaxVehicles)
                    return TypeError(item, at, "at most 64 entries");
                Vehicle vehicle;
                if (!ReadVehicle(item.keys[i], item.items[i], at + "." + item.keys[i], vehicle))
                    return false;
                result.cameras += static_cast<int>(vehicle.cameras.size());
                out.vehicles.push_back(std::move(vehicle));
            }
        }
        if (!haveVehicles)
            return TypeError(value, where, "an object with a \"vehicles\" section");
        result.vehicles += static_cast<int>(out.vehicles.size());
        return true;
    }
};

bool KnownGameKey(const std::string& key)
{
    if (key == kDefaultKey)
        return true;
    for (const char* known : kGameKeys)
        if (key == known)
            return true;
    return false;
}

// ---- writing ------------------------------------------------------------------------------------------------
std::string NumberText(float value)
{
    char buffer[32];
    if (std::fabs(value - std::round(value)) < 1e-4f)
        std::snprintf(buffer, sizeof(buffer), "%d", static_cast<int>(std::lround(value)));
    else
        std::snprintf(buffer, sizeof(buffer), "%g", static_cast<double>(value));
    return buffer;
}

std::string Quoted(const std::string& text)
{
    std::string out = "\"";
    for (const char c : text)
    {
        if (c == '"' || c == '\\')
        {
            out += '\\';
            out += c;
        }
        else if (static_cast<unsigned char>(c) < 0x20)
            out += ' ';
        else
            out += c;
    }
    return out + "\"";
}

std::string AxesJson(bool yaw, bool pitch, bool roll)
{
    std::string out = "[";
    if (yaw)
        out += "\"yaw\"";
    if (pitch)
        out += std::string(out.size() > 1 ? ", " : "") + "\"pitch\"";
    if (roll)
        out += std::string(out.size() > 1 ? ", " : "") + "\"roll\"";
    return out + "]";
}

std::string OffsetJson(const float offset[3])
{
    return "[" + NumberText(offset[0]) + ", " + NumberText(offset[1]) + ", " + NumberText(offset[2]) + "]";
}

// [forward, right, up] with null on an axis that has no limit; "null" when no axis has one.
std::string LeashJson(const float side[3])
{
    if (!LeashSet(side))
        return "null";
    std::string out = "[";
    for (int axis = 0; axis < 3; ++axis)
        out += std::string(axis ? ", " : "") +
               ((side[axis] > -kUnleashed && side[axis] < kUnleashed) ? NumberText(side[axis]) : std::string("null"));
    return out + "]";
}

bool Same3(const float a[3], const float b[3])
{
    return a[0] == b[0] && a[1] == b[1] && a[2] == b[2];
}

std::string AxesText(bool yaw, bool pitch, bool roll)
{
    std::string out;
    if (yaw)
        out += "Yaw";
    if (pitch)
        out += std::string(out.empty() ? "" : ", ") + "Pitch";
    if (roll)
        out += std::string(out.empty() ? "" : ", ") + "Roll";
    return out.empty() ? "None" : out;
}

const char* const kGuide[] = {
    "VEHICLE CAMERAS. While you sit in a vehicle seat or a turret, the cameras listed for that seat are yours to",
    "step through, and each camera's tethering modes with them.",
    "Each game has its own section under 'games' (halo3, odst, reach, halo4, halo2, halo1); 'default' covers a",
    "game without one. Inside a section each vehicle seat has its own list, used in order. The entry whose",
    "'match' text appears in the vehicle's name is used -- the longest such text when several do (case does not",
    "matter); the 'default' entry covers any vehicle not listed.",
    "Distances are centimetres. An offset or a leash limit is [forward, right, up].",
    "Camera fields -- all optional; anything left out takes its default:",
    "  name              shown when you switch to it (left out = just its number)",
    "  type              chase (a camera placed by the fields below) | firstperson (the seat view you have",
    "                    without this file; the fields below are then not used)",
    "  origin            vehicle | seat | playerhead | thirdperson : where 'offset' is measured from (the",
    "                    vehicle's own origin, your seat, your character's head, or where the game's own",
    "                    third-person camera rests for that seat -- with a zero offset, the view the game gives",
    "                    that vehicle); or any other name: a marker on the vehicle, for the camera to ride (markers",
    "                    are not looked up yet: the vehicle's origin is used, and the log says so)",
    "  offset            [forward, right, up] in cm from the origin (negative forward = behind);",
    "                    locationTracking decides whether those directions turn with the vehicle",
    "  locationTracking  which vehicle rotations carry the camera's position round: any of yaw, pitch, roll",
    "                    ([] = a fixed world direction), or \"view\" to orbit with YOUR view",
    "  rotationTracking  which vehicle rotations turn your VIEW: yaw, and pitch with it ([] = the world holds",
    "                    still). Your horizon is never rolled, so roll, and pitch without yaw, are not applied.",
    "  leashMin, leashMax  the LEASH: a box your HEAD stays inside, so a lean in a tight seat cannot take you",
    "                    through the canopy. Both are [forward, right, up] in cm FROM THE ORIGIN, the same space",
    "                    as offset, so each limit is a place: leashMin = the furthest back, left and down your",
    "                    head may go, leashMax = the furthest forward, right and up. Past a limit the view stops",
    "                    following your head that way; coming back is free. Per axis: a number = that limit,",
    "                    null = no limit that way, \"inherit\" = the level above's limit on that axis. Set it for",
    "                    a whole seat (in the vehicle entry), for a camera, or for a tethering mode; each",
    "                    overrides the one above it.",
    "  tethering         the camera's MODES: a list of { name, origin, offset, leashMin, leashMax,",
    "                    locationTracking, rotationTracking, aimMarker }, each taking the camera's own for what",
    "                    it leaves out -- e.g. the same camera held still, and turning with the vehicle",
    "  aimMarker         true | false: the purple ring where the VEHICLE is aiming, beside the crosshair, in",
    "                    this camera (left out = the seat's). A tethering mode may say otherwise",
    "  collide           pull the camera in when a wall is in the way; collideMargin = cm to stop short",
    "  lag               seconds the camera takes to catch up when the vehicle turns or bumps: it follows on a",
    "                    spring instead of a rigid boom (0 = rigid). A steady speed does not lag. Left out: 0.5",
    "                    for an origin of \"thirdperson\", 0 for any other. lagDamping: 1 = no overshoot, lower =",
    "                    a softer spring that swings past its place and back, higher = slower to settle (left",
    "                    out: 1.5 for \"thirdperson\", 1 for any other)",
    "Per vehicle entry: defaultCamera / defaultMode = where you start (from 0);",
    "  seat = \"driver\" | \"gunner\" | \"passenger\" (or a list): this entry is only for that seat. Left out: the",
    "  driver's entry, and the one any other seat of the vehicle uses when it has none of its own;",
    "  enabled = false: none of this in this seat; its cameras may then be left out;",
    "  aimMarker = true | false: a purple ring where the VEHICLE is aiming, beside the crosshair, in this seat's",
    "  chase cameras (vehicle_aim_marker = 0 in halomccvr.cfg turns it off everywhere). The built-in cameras",
    "  turn it off in their Untethered modes, so it shows while your view turns with the vehicle: aim at the",
    "  ring to keep going straight without turning. Left out here and in a camera and its mode: shown in",
    "  exactly the modes whose rotationTracking turns the view with the vehicle's yaw;",
    "  leashMin / leashMax = the leash for every camera of this seat.",
    "Keys starting with _ are notes and are ignored: add your own anywhere (\"_why\": \"...\").",
    "This file is yours: it is written once and never overwritten, and deleting it brings the built-in cameras",
    "back.",
};

void WriteCamera(std::string& out, const Vehicle& vehicle, const Camera& camera)
{
    // A chase camera names every placement field, so the file shows what there is to tune. A firstperson camera
    // uses none of them, so it names only those the player set.
    const Camera unset{};
    const bool all = camera.type != CameraType::FirstPerson;
    out += "            { ";
    if (!camera.name.empty())
        out += "\"name\": " + Quoted(camera.name) + ", ";
    out += std::string("\"type\": \"") + TypeName(camera.type) + "\"";
    if (all || camera.origin != unset.origin || camera.originMarker != unset.originMarker)
        out += ", \"origin\": " + Quoted(OriginText(camera.origin, camera.originMarker));
    if (all || !Same3(camera.offset, unset.offset))
        out += ", \"offset\": " + OffsetJson(camera.offset);
    // Only where the camera differs from its seat's (null = it drops the seat's leash).
    if (!Same3(camera.leashMin, vehicle.leashMin))
        out += ", \"leashMin\": " + LeashJson(camera.leashMin);
    if (!Same3(camera.leashMax, vehicle.leashMax))
        out += ", \"leashMax\": " + LeashJson(camera.leashMax);
    if (all || camera.locView != unset.locView || camera.locYaw != unset.locYaw || camera.locPitch != unset.locPitch ||
        camera.locRoll != unset.locRoll)
        out += ", \"locationTracking\": " +
               (camera.locView ? std::string("\"view\"") : AxesJson(camera.locYaw, camera.locPitch, camera.locRoll));
    if (all || camera.rotYaw != unset.rotYaw || camera.rotPitch != unset.rotPitch || camera.rotRoll != unset.rotRoll)
        out += ", \"rotationTracking\": " + AxesJson(camera.rotYaw, camera.rotPitch, camera.rotRoll);
    if (all || camera.collide != unset.collide)
        out += std::string(", \"collide\": ") + (camera.collide ? "true" : "false");
    if (all || camera.collideMargin != unset.collideMargin)
        out += ", \"collideMargin\": " + NumberText(camera.collideMargin);
    if (camera.lag >= 0.0f)
        out += ", \"lag\": " + NumberText(camera.lag);
    if (camera.lag >= 0.0f || camera.lagDamping >= 0.0f)
        out += ", \"lagDamping\": " + NumberText(LagDampingOf(camera));
    if (camera.aimMarker >= 0)
        out += std::string(", \"aimMarker\": ") + (camera.aimMarker ? "true" : "false");
    if (camera.tethering.empty())
    {
        out += " }";
        return;
    }
    // Each mode on its own line, naming only what differs from the camera's own values: the reader fills the
    // rest back in, so the file round-trips.
    out += ",\n              \"tethering\": [\n";
    for (std::size_t m = 0; m < camera.tethering.size(); ++m)
    {
        const Tether& mode = camera.tethering[m];
        std::string fields;
        const auto add = [&fields](const std::string& pair) { fields += (fields.empty() ? "" : ", ") + pair; };
        if (!mode.name.empty())
            add("\"name\": " + Quoted(mode.name));
        if (mode.origin >= 0 &&
            (mode.origin != static_cast<int>(camera.origin) ||
             (mode.origin == static_cast<int>(Origin::Marker) && mode.originMarker != camera.originMarker)))
            add("\"origin\": " + Quoted(OriginText(static_cast<Origin>(mode.origin), mode.originMarker)));
        if (!Same3(mode.offset, camera.offset))
            add("\"offset\": " + OffsetJson(mode.offset));
        if (!Same3(mode.leashMin, camera.leashMin))
            add("\"leashMin\": " + LeashJson(mode.leashMin));
        if (!Same3(mode.leashMax, camera.leashMax))
            add("\"leashMax\": " + LeashJson(mode.leashMax));
        if (mode.locView != camera.locView || mode.locYaw != camera.locYaw || mode.locPitch != camera.locPitch ||
            mode.locRoll != camera.locRoll)
            add("\"locationTracking\": " +
                (mode.locView ? std::string("\"view\"") : AxesJson(mode.locYaw, mode.locPitch, mode.locRoll)));
        if (mode.rotYaw != camera.rotYaw || mode.rotPitch != camera.rotPitch || mode.rotRoll != camera.rotRoll)
            add("\"rotationTracking\": " + AxesJson(mode.rotYaw, mode.rotPitch, mode.rotRoll));
        if (mode.aimMarker >= 0)
            add(std::string("\"aimMarker\": ") + (mode.aimMarker ? "true" : "false"));
        out += "                { " + fields + (fields.empty() ? "}" : " }") +
               (m + 1 < camera.tethering.size() ? ",\n" : "\n");
    }
    out += "              ] }";
}

void WriteVehicle(std::string& out, const Vehicle& vehicle, bool last)
{
    out += "        " + Quoted(vehicle.name) + ": {\n";
    if (!vehicle.isDefault && !vehicle.match.empty())
    {
        out += "          \"match\": [";
        for (std::size_t m = 0; m < vehicle.match.size(); ++m)
            out += (m ? ", " : "") + Quoted(vehicle.match[m]);
        out += "],\n";
    }
    if (vehicle.seats != 0)
    {
        std::string roles;
        const auto add = [&roles](const char* role) { roles += std::string(roles.empty() ? "" : ", ") + "\"" + role + "\""; };
        if (vehicle.seats & kSeatDriver)
            add("driver");
        if (vehicle.seats & kSeatGunner)
            add("gunner");
        if (vehicle.seats & kSeatPassenger)
            add("passenger");
        out += "          \"seat\": [" + roles + "],\n";
    }
    if (!vehicle.enabled)
        out += "          \"enabled\": false,\n";
    out += "          \"defaultCamera\": " + std::to_string(vehicle.defaultCamera) + ",\n";
    if (vehicle.defaultMode != 0)
        out += "          \"defaultMode\": " + std::to_string(vehicle.defaultMode) + ",\n";
    if (vehicle.aimMarker >= 0)
        out += std::string("          \"aimMarker\": ") + (vehicle.aimMarker ? "true" : "false") + ",\n";
    if (LeashSet(vehicle.leashMin))
        out += "          \"leashMin\": " + LeashJson(vehicle.leashMin) + ",\n";
    if (LeashSet(vehicle.leashMax))
        out += "          \"leashMax\": " + LeashJson(vehicle.leashMax) + ",\n";
    if (vehicle.cameras.empty())
    {
        // An entry set "enabled": false may list none: it ends on the key before, without that key's comma.
        if (out.size() >= 2 && out.compare(out.size() - 2, 2, ",\n") == 0)
            out.erase(out.size() - 2, 1);
        out += std::string("        }") + (last ? "\n" : ",\n");
        return;
    }
    out += "          \"cameras\": [\n";
    for (std::size_t c = 0; c < vehicle.cameras.size(); ++c)
    {
        WriteCamera(out, vehicle, vehicle.cameras[c]);
        out += (c + 1 < vehicle.cameras.size() ? ",\n" : "\n");
    }
    out += "          ]\n";
    out += std::string("        }") + (last ? "\n" : ",\n");
}
} // namespace

Tether Camera::Mode(int index) const
{
    if (tethering.empty())
    {
        Tether own;
        for (int axis = 0; axis < 3; ++axis)
        {
            own.offset[axis] = offset[axis];
            own.leashMin[axis] = leashMin[axis];
            own.leashMax[axis] = leashMax[axis];
        }
        own.locYaw = locYaw;
        own.locPitch = locPitch;
        own.locRoll = locRoll;
        own.locView = locView;
        own.rotYaw = rotYaw;
        own.rotPitch = rotPitch;
        own.rotRoll = rotRoll;
        return own;
    }
    const int count = static_cast<int>(tethering.size());
    return tethering[static_cast<std::size_t>(((index % count) + count) % count)];
}

const Game* Table::FindGame(const char* gameKey) const noexcept
{
    const Game* fallback = nullptr;
    for (const Game& game : games)
    {
        if (gameKey != nullptr && game.key == gameKey)
            return &game;
        if (game.key == kDefaultKey)
            fallback = &game;
    }
    return fallback;
}

namespace
{
// The built-in cameras' parts. Every seat starts in its seat view, so the file changes nothing until another
// camera is chosen; the second camera is where the game's own third-person camera rests, which suits a vehicle of
// any size. A vehicle the mod knows adds one or two of its own, after Halo Campaign Evolved's.
Camera SeatView()
{
    Camera seat;
    seat.name = "Seat";
    seat.type = CameraType::FirstPerson;
    return seat;
}

// Two modes: the world held still, and the view turning with the vehicle's heading. The aim ring only in the second,
// as Halo Campaign Evolved's built-in cameras have it: there it is where the view comes to rest.
void UntetheredAndTethered(Camera& camera)
{
    Tether still = camera.Mode(0);
    still.name = "Untethered";
    Tether turning = still;
    turning.name = "Tethered";
    turning.rotYaw = true;
    still.aimMarker = 0;
    camera.tethering = {still, turning};
}

Camera ThirdPerson()
{
    Camera behind;
    behind.name = "Third Person";
    behind.origin = Origin::ThirdPerson;
    // Behind the way the vehicle faces and level, as the game's own camera is: it does not dip and bank with
    // the hull. On a spring: it stands a long way back, where a rigid boom swings it through every turn and bump.
    behind.locPitch = false;
    behind.locRoll = false;
    behind.lag = kThirdPersonLagSeconds;
    behind.lagDamping = kThirdPersonLagDamping;
    UntetheredAndTethered(behind);
    return behind;
}

// A chase camera at `offset` cm ([forward, right, up]) from `origin`. `level`: it turns with the vehicle's heading
// only (a ground vehicle's: the horizon stays put over bumps); otherwise it is rigid to the vehicle (a flyer's).
Camera Chase(const char* name, Origin origin, float forward, float right, float up, bool level, float lag)
{
    Camera camera;
    camera.name = name;
    camera.origin = origin;
    camera.offset[0] = forward;
    camera.offset[1] = right;
    camera.offset[2] = up;
    camera.locPitch = camera.locRoll = !level;
    camera.lag = lag;
    UntetheredAndTethered(camera);
    return camera;
}

Vehicle Entry(const char* key, const char* match, uint8_t seats, std::vector<Camera> extra)
{
    Vehicle vehicle;
    vehicle.name = key;
    vehicle.match = {match};
    vehicle.seats = seats;
    vehicle.aimMarker = 1;
    vehicle.cameras = {SeatView(), ThirdPerson()};
    for (Camera& camera : extra)
        vehicle.cameras.push_back(std::move(camera));
    return vehicle;
}

Vehicle AnyVehicle()
{
    Vehicle any;
    any.name = kDefaultKey;
    any.isDefault = true;
    any.aimMarker = 1;
    any.cameras = {SeatView(), ThirdPerson()};
    return any;
}
} // namespace

Table DefaultTable()
{
    // Halo 3: the vehicles the mod knows by name (Halo3VehicleIdName), each with the seats Halo Campaign Evolved
    // gives its own cameras. A gunner on a vehicle's gun is named by the vehicle; a passenger is any seat that
    // neither drives nor fires the vehicle's gun.
    constexpr uint8_t driver = kSeatDriver, gunner = kSeatGunner, passenger = kSeatPassenger;
    Game halo3;
    halo3.key = kGameKeys[0];
    halo3.vehicles = {
        Entry("Warthog", "warthog", driver,
              {Chase("Over the Shoulder", Origin::Seat, -200, 90, 70, true, 0.15f),
               Chase("Left Side", Origin::Seat, 0, -140, 40, true, 0.0f)}),
        Entry("Warthog passenger", "warthog", passenger,
              {Chase("Over the Shoulder", Origin::Seat, -200, -90, 70, true, 0.15f),
               Chase("Right Side", Origin::Seat, 0, 140, 40, true, 0.0f)}),
        Entry("Warthog gunner", "warthog", gunner, {Chase("Behind the Gun", Origin::Seat, -150, 0, 50, true, 0.1f)}),
        Entry("Mongoose", "mongoose", driver, {Chase("Over the Shoulder", Origin::Seat, -170, 80, 70, true, 0.15f)}),
        Entry("Mongoose passenger", "mongoose", passenger,
              {Chase("Over the Shoulder", Origin::Seat, -170, -80, 70, true, 0.15f)}),
        Entry("Ghost", "ghost", driver,
              {Chase("Follow", Origin::Vehicle, -450, 0, 220, true, 0.25f),
               Chase("Over the Shoulder", Origin::Vehicle, -10, 60, 135, true, 0.1f)}),
        Entry("Chopper", "chopper", driver, {Chase("Follow", Origin::Vehicle, -500, 0, 250, true, 0.25f)}),
        Entry("Prowler", "prowler", driver, {Chase("Follow", Origin::Vehicle, -550, 0, 300, true, 0.25f)}),
        Entry("Prowler gunner", "prowler", gunner, {Chase("Behind the Gun", Origin::Seat, -150, 0, 50, true, 0.1f)}),
        Entry("Banshee", "banshee", driver,
              {Chase("Over the Shoulder", Origin::Vehicle, -350, 150, 200, false, 0.2f),
               Chase("Chase", Origin::Vehicle, -600, 0, 150, false, 0.3f)}),
        Entry("Hornet", "hornet", driver,
              {Chase("Over the Shoulder", Origin::Vehicle, -350, 150, 250, false, 0.2f),
               Chase("Chase", Origin::Vehicle, -700, 0, 250, false, 0.3f)}),
        Entry("Scorpion", "scorpion", driver, {Chase("High", Origin::Vehicle, -700, 0, 700, true, 0.3f)}),
        Entry("Scorpion gunner", "scorpion", gunner,
              {Chase("Behind the Gun", Origin::Seat, -150, 0, 60, true, 0.1f)}),
        Entry("Wraith", "wraith", driver, {Chase("High", Origin::Vehicle, -700, 100, 700, true, 0.3f)}),
        Entry("Wraith gunner", "wraith", gunner, {Chase("Behind the Gun", Origin::Seat, -150, 0, 60, true, 0.1f)}),
        Entry("Turret", "turret", 0, {Chase("Behind the Gun", Origin::Seat, -150, 0, 50, true, 0.1f)}),
        AnyVehicle(),
    };

    // Every other game, until it has cameras of its own.
    Game game;
    game.key = kDefaultKey;
    game.vehicles.push_back(AnyVehicle());

    Table table;
    table.games.push_back(std::move(halo3));
    table.games.push_back(std::move(game));
    return table;
}

ParseResult FromJson(const char* text, std::size_t len, Table& out)
{
    ParseResult result;
    if (text == nullptr || len > kMaxFileBytes)
    {
        result.error = "the file is larger than 512 KB";
        return result;
    }
    Value root;
    std::string error;
    if (!json_lite::Parse(text, len, root, error))
    {
        result.error = error;
        return result;
    }
    Reader reader{text, result};
    if (!root.IsObject())
    {
        reader.TypeError(root, "the file", "an object { \"games\": { ... } }");
        return result;
    }
    Table table;
    bool haveGames = false;
    for (std::size_t k = 0; k < root.keys.size(); ++k)
    {
        const std::string& key = root.keys[k];
        const Value& item = root.items[k];
        if (IsNote(key) || key == "version")
            continue;
        if (key != "games")
        {
            result.ignored.push_back(key);
            continue;
        }
        haveGames = true;
        if (!item.IsObject())
        {
            reader.TypeError(item, "games", "an object { \"halo3\": { ... }, \"default\": { ... } }");
            return result;
        }
        for (std::size_t i = 0; i < item.keys.size(); ++i)
        {
            if (IsNote(item.keys[i]))
                continue;
            const std::string gameKey = Lower(item.keys[i]);
            if (!KnownGameKey(gameKey))
            {
                reader.TypeError(item.items[i], "games." + item.keys[i],
                                 "under one of halo3, odst, reach, halo4, halo2, halo1 or default");
                return result;
            }
            for (const Game& seen : table.games)
                if (seen.key == gameKey)
                {
                    reader.TypeError(item.items[i], "games." + item.keys[i], "listed once");
                    return result;
                }
            Game game;
            if (!reader.ReadGame(item.keys[i], item.items[i], "games." + item.keys[i], game))
                return result;
            table.games.push_back(std::move(game));
        }
    }
    if (!haveGames)
    {
        reader.TypeError(root, "the file", "an object with a \"games\" section");
        return result;
    }
    result.ok = true;
    out = std::move(table);
    return result;
}

std::string ToJson(const Table& table, bool guide)
{
    std::string out = "{\n";
    if (guide)
    {
        out += "  \"_readme\": [\n";
        const std::size_t count = sizeof(kGuide) / sizeof(kGuide[0]);
        for (std::size_t i = 0; i < count; ++i)
            out += "    " + Quoted(kGuide[i]) + (i + 1 < count ? ",\n" : "\n");
        out += "  ],\n";
    }
    out += "  \"version\": 1,\n";
    out += "  \"games\": {\n";
    for (std::size_t g = 0; g < table.games.size(); ++g)
    {
        const Game& game = table.games[g];
        out += "    " + Quoted(game.key) + ": {\n";
        out += "      \"vehicles\": {\n";
        for (std::size_t v = 0; v < game.vehicles.size(); ++v)
            WriteVehicle(out, game.vehicles[v], v + 1 == game.vehicles.size());
        out += "      }\n";
        out += std::string("    }") + (g + 1 < table.games.size() ? ",\n" : "\n");
    }
    out += "  }\n";
    out += "}\n";
    return out;
}

// THE MOST SPECIFIC MATCH WINS: the longest "match" text found in the name, then an entry naming the seat over
// one that does not, then the earlier entry. So a turret with a name of its own gets its own entry wherever it
// sits in the file, and a seat sharing its vehicle's name gets its own by naming the seat.
int MatchVehicle(const Game& game, const std::string& vehicleName, uint8_t seat)
{
    const std::string name = Lower(vehicleName);
    int fallback = -1, best = -1;
    std::size_t bestLength = 0;
    bool bestSeated = false;
    for (std::size_t i = 0; i < game.vehicles.size(); ++i)
    {
        const Vehicle& vehicle = game.vehicles[i];
        if (vehicle.isDefault)
        {
            if (fallback < 0)
                fallback = static_cast<int>(i);
            continue;
        }
        const bool seated = vehicle.seats != 0;
        if (seated && (vehicle.seats & seat) == 0)
            continue;
        for (const std::string& match : vehicle.match)
        {
            if (match.empty() || name.find(match) == std::string::npos)
                continue;
            if (match.size() > bestLength || (match.size() == bestLength && seated && !bestSeated))
            {
                best = static_cast<int>(i);
                bestLength = match.size();
                bestSeated = seated;
            }
        }
    }
    return best >= 0 ? best : fallback;
}

int CarryMode(const Camera& to, const Tether& from)
{
    const int count = to.ModeCount();
    if (count <= 1)
        return 0;
    for (int i = 0; i < count; ++i)
        if (!from.name.empty() && Lower(to.tethering[static_cast<std::size_t>(i)].name) == Lower(from.name))
            return i;
    for (int i = 0; i < count; ++i)
    {
        const Tether& mode = to.tethering[static_cast<std::size_t>(i)];
        if (mode.rotYaw == from.rotYaw && mode.rotPitch == from.rotPitch && mode.rotRoll == from.rotRoll)
            return i;
    }
    return 0;
}

bool EffectiveAimMarker(const Vehicle& vehicle, int camera, int mode)
{
    if (camera < 0 || camera >= static_cast<int>(vehicle.cameras.size()))
        return vehicle.aimMarker > 0;
    const Camera& chosen = vehicle.cameras[static_cast<std::size_t>(camera)];
    const Tether tether = chosen.Mode(mode);
    if (!chosen.tethering.empty() && tether.aimMarker >= 0)
        return tether.aimMarker != 0;
    if (chosen.aimMarker >= 0)
        return chosen.aimMarker != 0;
    if (vehicle.aimMarker >= 0)
        return vehicle.aimMarker != 0;
    return tether.rotYaw;
}

const char* TypeName(CameraType type) noexcept
{
    return type == CameraType::FirstPerson ? "firstperson" : "chase";
}

std::string OriginText(Origin origin, const std::string& marker)
{
    switch (origin)
    {
    case Origin::Seat:
        return "seat";
    case Origin::Head:
        return "playerhead";
    case Origin::Marker:
        return marker;
    case Origin::ThirdPerson:
        return "thirdperson";
    default:
        return "vehicle";
    }
}

std::string SeatText(uint8_t seat)
{
    std::string out;
    if (seat & kSeatDriver)
        out += "Driver";
    if (seat & kSeatGunner)
        out += std::string(out.empty() ? "" : ", ") + "Gunner";
    if (seat & kSeatPassenger)
        out += std::string(out.empty() ? "" : ", ") + "Passenger";
    return out;
}

std::string RotationTrackingText(const Tether& mode)
{
    return AxesText(mode.rotYaw, mode.rotPitch, mode.rotRoll);
}

std::string LocationTrackingText(const Tether& mode)
{
    return mode.locView ? std::string("Your view (orbit)") : AxesText(mode.locYaw, mode.locPitch, mode.locRoll);
}

std::string LeashText(const float leashMin[3], const float leashMax[3])
{
    static const char* const kAxis[3] = {"forward", "right", "up"};
    std::string out;
    for (int axis = 0; axis < 3; ++axis)
    {
        const bool low = leashMin[axis] > -kUnleashed, high = leashMax[axis] < kUnleashed;
        if (!low && !high)
            continue;
        out += std::string(out.empty() ? "" : ", ") + kAxis[axis] + " ";
        if (low && high)
            out += NumberText(leashMin[axis]) + " to " + NumberText(leashMax[axis]);
        else if (high)
            out += "at most " + NumberText(leashMax[axis]);
        else
            out += "at least " + NumberText(leashMin[axis]);
    }
    return out.empty() ? std::string("None") : out + " cm";
}
} // namespace vehicle_camera_file
