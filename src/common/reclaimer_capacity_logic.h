#pragma once

// PROJECT RECLAIMER'S OWN LIST OF WHAT IT ENLARGED (src/dll/reclaimer_capacity.cpp). Every launch of the 0.10 client
// writes it near the top of its log, one "[capacity]" line each:
//
//   [capacity] reclaimer capacity: cinematics=150 coop=true ... fp_nodes=128 ... players=128 ...   (a summary)
//   [capacity] players.count: 0x10 -> 0x80                    (retail Halo 3's value -> the client's)
//   [capacity] simulation_world.address: 0x7ff66cb08698 -> 0x23127ae3000    (where it moved a table: not a size)
//   [capacity] Forge objects: 9999 placements, ...            (prose: not read)
//
// From 0.10.3 each line starts with the time ("12:16:34.616 "). The mod takes the sizes from this list as the host's
// own statement, never as proof: a value read here is only ever a candidate that the mod's own check in memory must
// still pass, so a list that is missing, renamed or wrong leaves the mod exactly as it was without one. And the list
// of one version against the previous one's says at once which of the sizes the mod depends on a new release changed.
//
// Pure logic; unit-tested in tests/reclaimer_capacity_tests.cpp.

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <string_view>
#include <vector>

namespace reclaimer_capacity
{
struct Entry
{
    std::string name;
    uint64_t retail = 0;   // retail Halo 3's value (0 for a summary item)
    uint64_t host = 0;     // the client's
    bool summary = false;  // from the "reclaimer capacity:" line (decimal, no retail value)
};

struct Manifest
{
    std::vector<Entry> entries;

    const Entry* Find(std::string_view name) const
    {
        for (const Entry& entry : entries)
            if (entry.name == name)
                return &entry;
        return nullptr;
    }
    bool Empty() const { return entries.empty(); }
};

namespace detail
{
inline bool ParseNumber(std::string_view text, uint64_t& out)
{
    while (!text.empty() && text.front() == ' ')
        text.remove_prefix(1);
    while (!text.empty() && (text.back() == ' ' || text.back() == '\r'))
        text.remove_suffix(1);
    int base = 10;
    if (text.size() > 2 && text[0] == '0' && (text[1] == 'x' || text[1] == 'X'))
    {
        text.remove_prefix(2);
        base = 16;
    }
    if (text.empty() || text.size() > 16)
        return false;
    uint64_t value = 0;
    for (const char c : text)
    {
        int digit = -1;
        if (c >= '0' && c <= '9')
            digit = c - '0';
        else if (base == 16 && c >= 'a' && c <= 'f')
            digit = c - 'a' + 10;
        else if (base == 16 && c >= 'A' && c <= 'F')
            digit = c - 'A' + 10;
        if (digit < 0)
            return false;
        value = value * base + static_cast<uint64_t>(digit);
    }
    out = value;
    return true;
}
} // namespace detail

// Reads every "[capacity]" line of a client log's text. Anything else is skipped.
inline Manifest Parse(std::string_view text)
{
    Manifest manifest;
    constexpr std::string_view kTag = "[capacity] ";
    constexpr std::string_view kSummary = "reclaimer capacity:";
    size_t at = 0;
    while (at < text.size())
    {
        size_t end = text.find('\n', at);
        if (end == std::string_view::npos)
            end = text.size();
        std::string_view line = text.substr(at, end - at);
        at = end + 1;
        const size_t tag = line.find(kTag);
        if (tag == std::string_view::npos || tag > 32)
            continue;
        line.remove_prefix(tag + kTag.size());
        if (line.substr(0, kSummary.size()) == kSummary)
        {
            line.remove_prefix(kSummary.size());
            while (!line.empty())
            {
                const size_t space = line.find(' ');
                const std::string_view item = line.substr(0, space);
                line.remove_prefix(space == std::string_view::npos ? line.size() : space + 1);
                const size_t equals = item.find('=');
                uint64_t value = 0;
                if (equals == std::string_view::npos || equals == 0 ||
                    !detail::ParseNumber(item.substr(equals + 1), value))
                    continue;
                Entry entry;
                entry.name = std::string(item.substr(0, equals));
                entry.host = value;
                entry.summary = true;
                manifest.entries.push_back(std::move(entry));
            }
            continue;
        }
        const size_t colon = line.find(": 0x");
        const size_t arrow = line.find(" -> ");
        if (colon == std::string_view::npos || colon == 0 || arrow == std::string_view::npos || arrow < colon)
            continue;
        const std::string_view name = line.substr(0, colon);
        if (name.find(' ') != std::string_view::npos)
            continue;
        Entry entry;
        if (!detail::ParseNumber(line.substr(colon + 2, arrow - colon - 2), entry.retail) ||
            !detail::ParseNumber(line.substr(arrow + 4), entry.host))
            continue;
        entry.name = std::string(name);
        manifest.entries.push_back(std::move(entry));
    }
    return manifest;
}

// A size, count or offset, as opposed to an address the client moved a table to (those differ on every launch).
inline bool IsSize(const Entry& entry)
{
    return entry.host < (uint64_t{1} << 32) && entry.retail < (uint64_t{1} << 32);
}

// The client's version from its log's name: "client-2026-11-15_121625-v0.10.4.log" -> "0.10.4". Empty for the names
// before 0.10.3 (client.log, client.previous.log), which do not say.
inline std::string VersionFromLogName(std::string_view name)
{
    const size_t slash = name.find_last_of("\\/");
    if (slash != std::string_view::npos)
        name.remove_prefix(slash + 1);
    const size_t v = name.rfind("-v");
    const size_t dot = name.rfind(".log");
    if (v == std::string_view::npos || dot == std::string_view::npos || dot <= v + 2)
        return {};
    return std::string(name.substr(v + 2, dot - v - 2));
}

// The sizes the mod's own reads depend on (audited 2026-11-15): the first-person weapon block (fp_nodes), the session
// reader's players array, machine mask and the block beside them (players.*, machines.*, players_globals.*), and the
// player tags' navpoints (hud.markers).
inline bool ModDependsOn(std::string_view name)
{
    for (const std::string_view watched : {std::string_view("fp_nodes"), std::string_view("players"),
                                           std::string_view("hud.markers")})
        if (name == watched)
            return true;
    for (const std::string_view prefix : {std::string_view("players."), std::string_view("machines."),
                                          std::string_view("players_globals."), std::string_view("mask.")})
        if (name.substr(0, prefix.size()) == prefix)
            return true;
    return false;
}

struct Change
{
    std::string name;
    bool before = false, after = false;   // listed in the older / newer list
    uint64_t was = 0, now = 0;            // the client's values
    bool depended = false;                // ModDependsOn
};

// The sizes that differ between two lists (added, removed or another value), in the newer list's order, then the
// ones only the older list has. Addresses are left out: they change on every launch.
inline std::vector<Change> Changes(const Manifest& older, const Manifest& newer)
{
    std::vector<Change> changes;
    for (const Entry& entry : newer.entries)
    {
        if (!IsSize(entry))
            continue;
        const Entry* old = older.Find(entry.name);
        if (old && (!IsSize(*old) || old->host == entry.host))
            continue;
        Change change;
        change.name = entry.name;
        change.after = true;
        change.now = entry.host;
        change.before = old != nullptr;
        change.was = old ? old->host : 0;
        change.depended = ModDependsOn(entry.name);
        changes.push_back(std::move(change));
    }
    for (const Entry& entry : older.entries)
    {
        if (!IsSize(entry) || newer.Find(entry.name))
            continue;
        Change change;
        change.name = entry.name;
        change.before = true;
        change.was = entry.host;
        change.depended = ModDependsOn(entry.name);
        changes.push_back(std::move(change));
    }
    return changes;
}

// THE FIRST-PERSON NODE CAPACITIES TO TRY, in order (game.cpp Halo3ProveFpLayout; each must still pass its check in
// memory): retail's 64 first, then the client's own fp_nodes when it states one in range, then 128 (Reclaimer
// 0.10.0-0.10.4) when the client states none. A stated value is tried before 128, never in place of retail's.
struct NodeCandidates
{
    uint32_t values[3]{};
    size_t count = 0;
};

// `stated`: the client's fp_nodes, 0 when it states none. Allocation-free (the game thread asks).
inline NodeCandidates FpNodeCandidates(uint64_t stated)
{
    NodeCandidates out;
    out.values[out.count++] = 64;
    if (stated > 64 && stated <= 1024)
        out.values[out.count++] = static_cast<uint32_t>(stated);
    if (out.values[out.count - 1] != 128)
        out.values[out.count++] = 128;
    return out;
}

inline NodeCandidates FpNodeCandidates(const Manifest& manifest)
{
    const Entry* stated = manifest.Find("fp_nodes");
    return FpNodeCandidates(stated ? stated->host : 0);
}
} // namespace reclaimer_capacity
