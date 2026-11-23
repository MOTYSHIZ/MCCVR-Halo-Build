#pragma once
// ODST'S TUTORIALS AGAIN (developer setting "Reset ODST training", docs/ODST-PROMPT-TEXT-EVIDENCE-2026-11-20.md).
//
// Mombasa Streets runs its tutorials (look, health, VISR, VISR database, NAV beacons) only while the campaign's
// progress marks say they are not done (gp_boolean_get gp_tr_look_complete ..., H3ODSTEK h100_mission_l100.hsc), and
// asks to repeat them only once all five are done. With the setting on, the game's read of those marks answers "not
// done", so the tutorials play again. Reads only: the saved marks are not changed, and the game sets them as usual.
//
// WHERE (halo3odst.dll). gp_boolean_get's implementation (+0x1C81D8) takes the mark's string id: it finds the mark's
// index with the progression lookup (+0x1C87D0, id and the list's type, 1 = booleans), then tests that bit of the
// campaign's progress. The lookup walks the game_progression tag's list for that type (tag +12*type: count, then the
// list's place); the tag comes from its getter (+0x1C8904). Three shapes, each checked where the one before calls it.
//
// WHICH MARKS. The kit's tags\globals\game_progression lists the boolean marks in order; the tutorials' are 72-76
// (gp_tr_look/health/vision/pda/beacon_complete) and each player's VISR database mark 96-99
// (gp_player0..3_pda_complete), of 176. The hook acts only while the game's list also holds 176 marks.
#include "drive_resolve.h"

#include <cstdint>
#include <cstdio>
#include <cstring>

namespace odst_training
{
// gp_boolean_get's implementation: mov edx,1 (booleans); call LOOKUP; ...; the progress bit test (bt; setb).
inline constexpr char kGet[] =
    "40 53 48 83 EC 20 BA 01 00 00 00 32 DB E8 ?? ?? ?? ?? 4C 63 C8 41 83 F9 FF 74 ?? E8 ?? ?? ?? ?? 8B 0D ?? ?? ?? ?? "
    "4D 8B C1 49 C1 F8 05 41 83 E1 1F BA 40 00 00 00 48 63 00 4C 03 C0 65 48 8B 04 25 58 00 00 00 48 8B 04 C8 48 8B 14 "
    "10 42 8B 84 82 ?? ?? ?? ?? 44 0F A3 C8 0F 92 C3";
inline constexpr uint32_t kGetLookupCall = 0x0E, kGetLookupNext = 0x12;
// The lookup (id, type): the tag getter, then the type's list (lea r9,[r9+r9*2]: 12 bytes a type) and its count.
inline constexpr char kLookup[] =
    "40 53 48 83 EC 20 4C 63 CA 8B D9 E8 ?? ?? ?? ?? 41 83 C8 FF 48 85 C0 74 ?? 4F 8D 0C 49 33 D2 42 39 14 88";
inline constexpr uint32_t kLookupTagCall = 0x0C, kLookupTagNext = 0x10;
// The game_progression tag's getter: the globals' reference to it, then its data.
inline constexpr char kTag[] =
    "48 8B 05 ?? ?? ?? ?? 33 D2 48 85 C0 74 ?? 8B 88 ?? ?? ?? ?? 83 F9 FF 74 ?? 48 8B 05 ?? ?? ?? ?? 0F B7 C9 39 54 C8 04";

inline constexpr int kBooleans = 1;                  // the lookup's type for gp_boolean_*
inline constexpr uint32_t kBooleanCountOffset = 12;  // tag + 12 * type: the list's count
inline constexpr int kBooleanCount = 176;
inline bool TrainingMark(int index) noexcept
{
    return (index >= 72 && index <= 76) || (index >= 96 && index <= 99);
}

struct Where
{
    bool ok = false;
    uint32_t getRva = 0, lookupRva = 0, tagRva = 0;
    char missing[160]{};
};

namespace detail
{
inline void Note(Where& w, const char* what)
{
    const size_t used = std::strlen(w.missing);
    std::snprintf(w.missing + used, sizeof(w.missing) - used, "%s%s", used ? "; " : "", what);
}
inline uint32_t Target(const drive_resolve::Image& im, uint32_t operand, uint32_t next)
{
    uint8_t rel[4]{};
    if (!drive_resolve::ReadOriginal(im, operand, rel, 4))
        return 0;
    int32_t disp = 0;
    std::memcpy(&disp, rel, 4);
    return next + static_cast<uint32_t>(disp);
}
// A leaf (the tag's getter: no stack frame) has no .pdata entry, so only the others must start one.
inline bool Shape(const drive_resolve::Image& im, const char* text, uint32_t rva, bool leaf = false)
{
    drive_resolve::Pattern p{};
    return rva && drive_resolve::Compile(text, p) && drive_resolve::MatchesAt(im, p, rva) &&
           (leaf || drive_resolve::FunctionStartOf(im, rva) == rva);
}
} // namespace detail

inline Where Resolve(const drive_resolve::Image& im)
{
    using namespace detail;
    Where w{};
    drive_resolve::Pattern p{};
    if (!drive_resolve::Compile(kGet, p))
    {
        Note(w, "gp_boolean_get (bad signature)");
        return w;
    }
    const auto hits = drive_resolve::CountExecutable(im, p);
    if (hits.count != 1)
    {
        Note(w, hits.count ? "gp_boolean_get (several matches)" : "gp_boolean_get (not found)");
        return w;
    }
    const uint32_t get = hits.first;
    if (drive_resolve::FunctionStartOf(im, get) != get)
    {
        Note(w, "gp_boolean_get is not a function's start (.pdata)");
        return w;
    }
    const uint32_t lookup = Target(im, get + kGetLookupCall, get + kGetLookupNext);
    if (!Shape(im, kLookup, lookup))
    {
        Note(w, "the progression lookup gp_boolean_get calls");
        return w;
    }
    const uint32_t tag = Target(im, lookup + kLookupTagCall, lookup + kLookupTagNext);
    if (!Shape(im, kTag, tag, true))
    {
        Note(w, "the game_progression tag's getter the lookup calls");
        return w;
    }
    w.getRva = get;
    w.lookupRva = lookup;
    w.tagRva = tag;
    w.ok = true;
    return w;
}
} // namespace odst_training
