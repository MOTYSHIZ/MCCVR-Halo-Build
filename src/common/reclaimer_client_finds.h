#pragma once

// PROJECT RECLAIMER'S OWN CODE THE MOD HOOKS, FOUND BY SHAPE (pure; no Windows, engine or OpenXR includes).
//
// The same functions run in the DLL, over the client executable's image in memory (reclaimer_visr_camera.inl,
// halo3_player_tags.inl), and offline, over the executable on disk laid out by its sections
// (tests/reclaimer_client_file_check.cpp), so a new Reclaimer release can be checked before anyone plays it.
//
// Each find names what it did not find, and every hook it serves stays off when it fails: nothing is ever written
// to a place that was not found exactly once with the shape it is expected to have. Where two finds are independent
// they must agree (VisrAgree), so one that drifts onto another function is refused rather than hooked.
//
// Found in Project Reclaimer 0.10.0 and 0.10.4 (2026-11-15), unique in each:
//   VISR's camera      0.10.0 exe+0x2B1FB40   0.10.4 exe+0x3775E80
//   VISR's constants   0.10.0 exe+0x2B24250   0.10.4 exe+0x377B7B0
//   navpoint draw      0.10.0 exe+0x2D839B0   0.10.4 exe+0x3A5EAC0 (count read exe+0x2D83C69 / exe+0x3A5EEF0)

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

#include "halo3_player_tags_logic.h"

namespace reclaimer_client
{
// An image laid out by RVA (the module in memory, or the file mapped by its sections).
struct Image
{
    const uint8_t* base = nullptr;
    size_t size = 0;
    uint32_t codeRva = 0, codeSize = 0;   // the code section
    // The function (from .pdata) containing `rva`: its first and one-past-last RVA. False when none.
    bool (*extent)(const void* context, uint32_t rva, uint32_t& begin, uint32_t& end) = nullptr;
    const void* context = nullptr;

    bool Holds(uint64_t rva, size_t length) const { return rva <= size && length <= size - rva; }
    bool Extent(uint32_t rva, uint32_t& begin, uint32_t& end) const
    {
        return extent && extent(context, rva, begin, end) && begin <= rva && rva < end && Holds(begin, end - begin);
    }
};

// "8B ?? 28 ..." as bytes, -1 for a wildcard.
inline std::vector<int> ParsePattern(const char* text)
{
    std::vector<int> out;
    for (const char* p = text; *p;)
    {
        while (*p == ' ')
            ++p;
        if (!*p)
            break;
        if (p[0] == '?' && p[1] == '?')
            out.push_back(-1);
        else
        {
            const auto hex = [](char c) { return c >= '0' && c <= '9' ? c - '0' : (c | 0x20) - 'a' + 10; };
            out.push_back(hex(p[0]) * 16 + hex(p[1]));
        }
        p += 2;
    }
    return out;
}

inline bool MatchAt(const Image& image, uint64_t rva, const std::vector<int>& pattern)
{
    if (!image.Holds(rva, pattern.size()))
        return false;
    const uint8_t* at = image.base + rva;
    for (size_t i = 0; i < pattern.size(); ++i)
        if (pattern[i] >= 0 && at[i] != pattern[i])
            return false;
    return true;
}

// Every match in [from, from + length), up to `max` of them.
inline std::vector<uint32_t> FindAll(const Image& image, uint32_t from, uint32_t length, const std::vector<int>& pattern,
                                     size_t max = 16)
{
    std::vector<uint32_t> out;
    size_t anchor = 0;
    while (anchor < pattern.size() && pattern[anchor] < 0)
        ++anchor;
    if (anchor == pattern.size() || !image.Holds(from, length) || length < pattern.size())
        return out;
    const uint8_t* begin = image.base + from + anchor;
    const uint8_t* end = image.base + from + length - pattern.size() + anchor + 1;
    for (const uint8_t* p = begin; p < end && out.size() < max;)
    {
        p = static_cast<const uint8_t*>(std::memchr(p, pattern[anchor], static_cast<size_t>(end - p)));
        if (!p)
            break;
        const uint32_t rva = static_cast<uint32_t>(p - image.base - anchor);
        if (MatchAt(image, rva, pattern))
            out.push_back(rva);
        ++p;
    }
    return out;
}

inline int32_t Rel32(const Image& image, uint64_t rva)
{
    int32_t value = 0;
    std::memcpy(&value, image.base + rva, sizeof(value));
    return value;
}

// The functions (their starts) that call `target` directly (call rel32) anywhere in the code section, up to `max`.
inline std::vector<uint32_t> CallingFunctions(const Image& image, uint32_t target, size_t max = 8)
{
    std::vector<uint32_t> out;
    if (!image.Holds(image.codeRva, image.codeSize) || image.codeSize < 5)
        return out;
    const uint8_t* begin = image.base + image.codeRva;
    const uint8_t* end = begin + image.codeSize - 4;
    for (const uint8_t* p = begin; p < end;)
    {
        p = static_cast<const uint8_t*>(std::memchr(p, 0xE8, static_cast<size_t>(end - p)));
        if (!p)
            break;
        const uint32_t rva = static_cast<uint32_t>(p - image.base);
        if (static_cast<int64_t>(rva) + 5 + Rel32(image, rva + 1) == static_cast<int64_t>(target))
        {
            uint32_t b = 0, e = 0;
            if (image.Extent(rva, b, e))
            {
                bool seen = false;
                for (const uint32_t f : out)
                    seen = seen || f == b;
                if (!seen)
                {
                    if (out.size() >= max)
                        break;
                    out.push_back(b);
                }
            }
        }
        ++p;
    }
    return out;
}

// Whether the function starting at `function` calls `target` directly.
inline bool Calls(const Image& image, uint32_t function, uint32_t target)
{
    uint32_t b = 0, e = 0;
    if (!image.Extent(function, b, e) || b != function)
        return false;
    for (uint32_t at = b; at + 5 <= e; ++at)
        if (image.base[at] == 0xE8 && static_cast<int64_t>(at) + 5 + Rel32(image, at + 1) == target)
            return true;
    return false;
}

struct Find
{
    uint32_t function = 0;          // the function to hook (its start)
    uint32_t at = 0;                // the match that proved it
    const char* missing = nullptr;  // what was not found (null: found)
};

// ---- VISR ------------------------------------------------------------------------------------------------------------
// The camera VISR asks for: its reads of field of view, near and far (+0x28, +0x64, +0x68; the first tested), and,
// later in the same function, its stores of them into the answer (+4, +8, +0xC).
inline constexpr char kVisrCameraReads[] = "8B ?? 28 8B ?? 64 8B ?? 68 85";
inline constexpr char kVisrCameraStores[] = "89 ?? 04 89 ?? 08 89 ?? 0C";
// VISR's constants: movss [r+0x40],-tanX; movss [r+0x44],tanY; mov dword [r+0x48],1.0.
inline constexpr char kVisrConstantsStores[] = "0F 11 ?? 40 F3 ?? 0F 11 ?? 44 ?? C7 ?? 48 00 00 80 3F";

inline Find FindUniqueFunction(const Image& image, const char* patternText, const char* what)
{
    Find out;
    const std::vector<int> pattern = ParsePattern(patternText);
    const std::vector<uint32_t> hits = FindAll(image, image.codeRva, image.codeSize, pattern, 2);
    uint32_t b = 0, e = 0;
    if (hits.empty())
        out.missing = what;
    else if (hits.size() > 1)
        out.missing = "a single match (more than one)";
    else if (!image.Extent(hits[0], b, e))
        out.missing = "the match's function extent";
    else
    {
        out.function = b;
        out.at = hits[0];
    }
    return out;
}

inline Find FindVisrCamera(const Image& image)
{
    Find out = FindUniqueFunction(image, kVisrCameraReads, "its reads of field of view, near and far");
    uint32_t b = 0, e = 0;
    if (!out.missing && (!image.Extent(out.at, b, e) ||
                         FindAll(image, out.at, e - out.at, ParsePattern(kVisrCameraStores), 1).empty()))
    {
        out = Find{};
        out.missing = "its stores of them into the answer";
    }
    return out;
}

inline Find FindVisrConstants(const Image& image)
{
    return FindUniqueFunction(image, kVisrConstantsStores, "its stores of -tanX, tanY and 1.0");
}

// The two VISR finds agree: the camera has exactly one calling function (VISR's state builder), and that function
// also calls the constants' builder. Null when they agree, else what did not.
inline const char* VisrAgree(const Image& image, uint32_t camera, uint32_t constants)
{
    const std::vector<uint32_t> callers = CallingFunctions(image, camera, 2);
    if (callers.empty())
        return "a caller of VISR's camera";
    if (callers.size() > 1)
        return "a single caller of VISR's camera (more than one)";
    if (!Calls(image, callers[0], constants))
        return "VISR's camera and constants called from the same function";
    return nullptr;
}

// ---- the navpoint draw ---------------------------------------------------------------------------------------------
struct NavpointDraw
{
    uint32_t version = 0;      // the version the search started from, past any pass-through
    uint32_t draw = 0;         // its own draw (the function to hook)
    uint32_t countDisp = 0;    // the rva of the count block's disp32 in the draw
    const char* missing = nullptr;
};

// From the client's version of chud_draw_navpoints (what the game's callers reach): through up to two pass-throughs,
// the call of its own draw in the version's first bytes, the draw's extent starting there, and its one count read.
inline NavpointDraw FindNavpointDrawFromVersion(const Image& image, uint32_t version)
{
    using namespace halo3_player_tags;
    NavpointDraw out;
    for (int hop = 0; hop < 2 && image.Holds(version, 16); ++hop)
    {
        int64_t offset = 0;
        if (!ThunkCallee(image.base + version, 16, offset))
            break;
        version = static_cast<uint32_t>(static_cast<int64_t>(version) + offset);
    }
    out.version = version;
    if (!image.Holds(version, kReclaimerVersionBytes))
    {
        out.missing = "its version";
        return out;
    }
    const std::vector<uint32_t> calls =
        FindAll(image, version, static_cast<uint32_t>(kReclaimerVersionBytes), ParsePattern(kReclaimerVersionPattern), 2);
    if (calls.size() != 1)
    {
        out.missing = calls.empty() ? "the call of its own draw in its version" : "a single call of its own draw";
        return out;
    }
    const uint64_t call = calls[0] + static_cast<uint32_t>(kReclaimerVersionCallRel);
    const int64_t draw = static_cast<int64_t>(call) + 4 + Rel32(image, call);
    uint32_t b = 0, e = 0;
    if (draw <= 0 || !image.Extent(static_cast<uint32_t>(draw), b, e) || b != static_cast<uint32_t>(draw))
    {
        out.missing = "its own draw's extent";
        return out;
    }
    const long disp = FindReclaimerCountRead(image.base + b, e - b);
    if (disp < 0)
    {
        out.missing = "its own draw's read of the entry count";
        return out;
    }
    out.draw = b;
    out.countDisp = b + static_cast<uint32_t>(disp);
    return out;
}

// Offline (no running client to ask for its version): every place in the code that calls a function the way the
// version calls its draw, kept where that function proves as the draw; exactly one draw must remain.
inline NavpointDraw FindNavpointDraw(const Image& image)
{
    using namespace halo3_player_tags;
    NavpointDraw out;
    const std::vector<uint32_t> hits =
        FindAll(image, image.codeRva, image.codeSize, ParsePattern(kReclaimerVersionPattern), 256);
    for (const uint32_t hit : hits)
    {
        uint32_t b = 0, e = 0;
        if (!image.Extent(hit, b, e) || hit - b >= kReclaimerVersionBytes)
            continue;
        const NavpointDraw found = FindNavpointDrawFromVersion(image, b);
        if (found.missing)
            continue;
        if (out.draw && out.draw != found.draw)
        {
            out = NavpointDraw{};
            out.missing = "a single navpoint draw (two versions call different ones)";
            return out;
        }
        out = found;
    }
    if (!out.draw)
        out.missing = "a version that calls a draw with the navpoint count read";
    return out;
}
} // namespace reclaimer_client
