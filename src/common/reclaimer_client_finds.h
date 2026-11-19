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
// and in 0.10.5 (2026-11-16): VISR's camera exe+0x379D310, constants exe+0x37A2C40, navpoint draw exe+0x3A87350, and
// the widget draw's crosshair gate (widget draw exe+0x3C42430, gate exe+0x3AB7B40, crosshair check exe+0x3AB7970), and
// VISR's view user (set at exe+0x3CCF712 in exe+0x3CCF6F0; user exe+0x54B6A58, first-frame flags exe+0x6AE1FEC).
// 0.10.6 (2026-11-19) is built with MSVC and calls its versions of game functions directly from its own code, each
// behind a framed pass-through (halo3_player_tags::ThunkCallee); the mod hooks those versions themselves
// (lifted_engine.cpp). Found there: VISR's view user (wrapper exe+0x4AB8090, user exe+0x85C3488, flags exe+0x87ABD74,
// object pass exe+0x574BE10, which compares the user with 3 two stores before its ja) and the navpoint version, which
// is its draw (exe+0x4EC0D40, count exe+0x872B200). VISR's camera reads are inlined into the version of +0x279BEC
// (exe+0x568A9C0) and its constants' builder (exe+0x46C9E30) takes other arguments: both left unhooked, VISR as drawn.

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
    bool InCode(int64_t rva) const { return rva >= codeRva && rva - codeRva < static_cast<int64_t>(codeSize); }
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

// The start of movss [r+disp8],xmm (F3, an optional REX, 0F 11, a modrm with an 8-bit displacement and no SIB) that
// ends exactly at `end`, or 0.
inline uint32_t MovssStoreEndingAt(const Image& image, uint32_t end, uint8_t disp)
{
    for (const uint32_t length : {5u, 6u})
    {
        if (end < length + 1 || !image.Holds(end - length, length))
            continue;
        const uint8_t* p = image.base + end - length;
        const bool rex = length == 6;
        if (p[0] != 0xF3 || (rex && (p[1] & 0xF0) != 0x40))
            continue;
        const uint8_t* op = p + (rex ? 2 : 1);
        if (op[0] == 0x0F && op[1] == 0x11 && (op[2] & 0xC0) == 0x40 && (op[2] & 7) != 4 && op[3] == disp)
            return end - length;
    }
    return 0;
}

// VISR's constants' stores: movss [r+0x40],-tanX; movss [r+0x44],tanY; mov dword [r+0x48],1.0, each with or without a
// REX prefix. 0.10.0 to 0.10.5 (kVisrConstantsStores) had REX on the second movss and the mov; 0.10.6 and 0.10.7, an
// MSVC build, have none (exe+0x45BFC5E in 0.10.7). Each match's start (the first movss).
inline std::vector<uint32_t> FindVisrConstantsStores(const Image& image, size_t max = 2)
{
    std::vector<uint32_t> out;
    // "48 00 00 80 3F": the mov's displacement and 1.0; before it, its modrm ([r+disp8], reg 0, no SIB) and C7.
    for (const uint32_t imm : FindAll(image, image.codeRva, image.codeSize, ParsePattern("48 00 00 80 3F"), 4096))
    {
        if (imm < 3 || (image.base[imm - 1] & 0xF8) != 0x40 || image.base[imm - 1] == 0x44 ||
            image.base[imm - 2] != 0xC7)
            continue;
        // The mov without a REX prefix, then with one (a byte 0x4X before it).
        uint32_t first = 0;
        for (const uint32_t mov : {imm - 2, imm - 3})
        {
            if (mov == imm - 3 && (image.base[mov] & 0xF0) != 0x40)
                continue;
            const uint32_t second = MovssStoreEndingAt(image, mov, 0x44);
            first = second ? MovssStoreEndingAt(image, second, 0x40) : 0;
            if (first)
                break;
        }
        if (!first)
            continue;
        out.push_back(first);
        if (out.size() >= max)
            break;
    }
    return out;
}

inline Find FindVisrConstants(const Image& image)
{
    Find out;
    const std::vector<uint32_t> hits = FindVisrConstantsStores(image);
    uint32_t b = 0, e = 0;
    if (hits.empty())
        out.missing = "its stores of -tanX, tanY and 1.0";
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

// ---- the HUD widget draw's crosshair gate ----------------------------------------------------------------------------
// The client's own widget draw (its version of halo3.dll's chud_draw_widget +0x2EDF24, which the game's callers reach
// through a pass-through) keeps the game's crosshair gate: for a widget of scripting class 2 it asks a gate (in the
// game, game_is_playback +0xF01A0) and, only when that says yes, whether the crosshair shows (+0x2EDE38). The mod's
// crosshair redirect opens the gate and answers the second question. Up to 0.10.4 both questions reached the game's
// functions, which the mod hooks. From 0.10.5 the widget draw asks them through two shims of the client's own, each
// called from there alone, and the game's functions are never reached: the crosshair stayed in the HUD.
// Shape: call GATE; ... test al,al; je; cmp r16,2; jne; call CROSSHAIR.
inline constexpr char kWidgetCrosshairGate[] = "84 C0 74 ?? 66 83 ?? 02 75 ?? E8";
inline constexpr uint32_t kWidgetCrosshairCall = 10;    // the call's offset in the pattern
inline constexpr uint32_t kWidgetGateCallWindow = 40;   // how far before the pattern the gate's call may be

struct WidgetGate
{
    uint32_t widgetDraw = 0;   // the widget draw holding the gate (its start)
    uint32_t gate = 0;         // the function the gate asks (its start)
    uint32_t crosshair = 0;    // the function asked whether the crosshair shows (its start)
    uint32_t at = 0;           // the match
    const char* missing = nullptr;
};

// Past up to two pass-throughs (sub rsp; call X; nop; add rsp; ret) from `rva`.
inline uint32_t PastPassThroughs(const Image& image, uint32_t rva)
{
    for (int hop = 0; hop < 2 && image.Holds(rva, halo3_player_tags::kThunkBytes); ++hop)
    {
        int64_t offset = 0;
        if (!halo3_player_tags::ThunkCallee(image.base + rva, halo3_player_tags::kThunkBytes, offset) ||
            !image.InCode(static_cast<int64_t>(rva) + offset))
            break;   // not a pass-through, or a jump out of the code (a hook's, MinHook writes one over a start)
        rva = static_cast<uint32_t>(static_cast<int64_t>(rva) + offset);
    }
    return rva;
}

// `widgetDraw` 0: anywhere in the code (offline); else only in that function, which must be the one holding it.
inline WidgetGate FindWidgetCrosshairGate(const Image& image, uint32_t widgetDraw = 0)
{
    WidgetGate out;
    uint32_t from = image.codeRva, length = image.codeSize;
    uint32_t b = 0, e = 0;
    if (widgetDraw)
    {
        if (!image.Extent(widgetDraw, b, e) || b != widgetDraw)
        {
            out.missing = "the widget draw's extent";
            return out;
        }
        from = b;
        length = e - b;
    }
    const std::vector<uint32_t> hits = FindAll(image, from, length, ParsePattern(kWidgetCrosshairGate), 2);
    if (hits.size() != 1)
    {
        out.missing = hits.empty() ? "the crosshair class test (test al,al; je; cmp r16,2; jne; call)"
                                   : "a single crosshair class test (more than one)";
        return out;
    }
    const uint32_t hit = hits[0];
    if (!image.Extent(hit, b, e))
    {
        out.missing = "the class test's function extent";
        return out;
    }
    const auto startOf = [&image](int64_t target) -> uint32_t {
        uint32_t fb = 0, fe = 0;
        if (target <= 0 || target > 0xFFFFFFFFll || !image.Extent(static_cast<uint32_t>(target), fb, fe) ||
            fb != static_cast<uint32_t>(target))
            return 0;
        return fb;
    };
    const uint32_t call = hit + kWidgetCrosshairCall;
    const uint32_t crosshair = startOf(static_cast<int64_t>(call) + 5 + Rel32(image, call + 1));
    if (!crosshair)
    {
        out.missing = "the crosshair check's start";
        return out;
    }
    // The gate: the nearest call before the class test that reaches a function's start.
    uint32_t gate = 0;
    for (uint32_t at = hit >= 5 ? hit - 5 : 0; at + kWidgetGateCallWindow >= hit && at >= b; --at)
    {
        if (image.base[at] == 0xE8)
            if (const uint32_t target = startOf(static_cast<int64_t>(at) + 5 + Rel32(image, at + 1)))
            {
                gate = target;
                break;
            }
        if (at == 0)
            break;
    }
    if (!gate || gate == crosshair)
    {
        out.missing = "the gate's call before the class test";
        return out;
    }
    // Each is asked from the widget draw alone, so hooking it changes nothing else the client does.
    const std::vector<uint32_t> gateCallers = CallingFunctions(image, gate, 2);
    const std::vector<uint32_t> crosshairCallers = CallingFunctions(image, crosshair, 2);
    if (gateCallers.size() != 1 || gateCallers[0] != b)
    {
        out.missing = "the gate asked from the widget draw alone";
        return out;
    }
    if (crosshairCallers.size() != 1 || crosshairCallers[0] != b)
    {
        out.missing = "the crosshair check asked from the widget draw alone";
        return out;
    }
    out.widgetDraw = b;
    out.gate = gate;
    out.crosshair = crosshair;
    out.at = hit;
    return out;
}

// ---- the navpoint draw ---------------------------------------------------------------------------------------------
struct NavpointDraw
{
    uint32_t version = 0;      // the version the search started from, past any pass-through
    uint32_t draw = 0;         // its own draw (the function to hook)
    uint32_t countDisp = 0;    // the rva of the count block's disp32 in the draw
    // 0.10.6: the version is the draw itself, in the recompiled engine's convention (its context in rcx), so it is
    // not hooked as a native draw: the game function's own hook reaches it (lifted_engine.cpp hooks the function a
    // version passes to) and only the count is taken from here.
    bool versionIsDraw = false;
    const char* missing = nullptr;
};

// From the client's version of chud_draw_navpoints (what the game's callers reach): through up to two pass-throughs,
// the call of its own draw in the version's first bytes, the draw's extent starting there, and its one count read.
inline NavpointDraw FindNavpointDrawFromVersion(const Image& image, uint32_t version)
{
    using namespace halo3_player_tags;
    NavpointDraw out;
    for (int hop = 0; hop < 2 && image.Holds(version, kThunkBytes); ++hop)
    {
        int64_t offset = 0;
        if (!ThunkCallee(image.base + version, kThunkBytes, offset) || !image.InCode(static_cast<int64_t>(version) + offset))
            break;   // not a pass-through, or a hook's jump out of the code
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
    if (calls.empty())
    {
        // 0.10.6 (2026-11-19): no draw of its own; the version reads the count itself as it walks the entries.
        uint32_t vb = 0, ve = 0;
        const long disp =
            image.Extent(version, vb, ve) && vb == version ? FindReclaimerCountRead(image.base + vb, ve - vb) : -1;
        if (disp < 0)
        {
            out.missing = "the call of its own draw in its version, or the version's own read of the entry count";
            return out;
        }
        out.draw = version;
        out.countDisp = version + static_cast<uint32_t>(disp);
        out.versionIsDraw = true;
        return out;
    }
    if (calls.size() != 1)
    {
        out.missing = "a single call of its own draw";
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
    if (out.draw)
        return out;
    // 0.10.6: the versions are the draws, reached through pass-throughs. Every function a pass-through leads to that
    // reads the count must read the same global (more than one does: the draw and the update).
    uint32_t global = 0;
    for (uint32_t at = image.codeRva; at + kThunkBytes <= image.codeRva + image.codeSize; ++at)
    {
        if (image.base[at] != 0x55 && image.base[at] != 0x48)
            continue;
        uint32_t b = 0, e = 0;
        int64_t offset = 0;
        if (!ThunkCallee(image.base + at, kThunkBytes, offset) || !image.Extent(at, b, e) || b != at)
            continue;
        const int64_t callee = static_cast<int64_t>(at) + offset;
        uint32_t cb = 0, ce = 0;
        if (callee <= 0 || !image.Extent(static_cast<uint32_t>(callee), cb, ce) || cb != callee)
            continue;
        const long disp = FindReclaimerCountRead(image.base + cb, ce - cb);
        if (disp < 0)
            continue;
        const uint32_t dispAt = cb + static_cast<uint32_t>(disp);
        const uint32_t target = static_cast<uint32_t>(static_cast<int64_t>(dispAt) + 4 + Rel32(image, dispAt));
        if (global && global != target)
        {
            out = NavpointDraw{};
            out.missing = "a single navpoint count (two versions read different ones)";
            return out;
        }
        global = target;
        if (!out.draw)
        {
            out.version = at;
            out.draw = cb;
            out.countDisp = dispAt;
            out.versionIsDraw = true;
        }
    }
    if (!out.draw)
        out.missing = "a version that calls a draw with the navpoint count read, or reads it itself";
    return out;
}
// ---- VISR's view user ------------------------------------------------------------------------------------------------
// VISR's outlines take their colours (enemy, ally, objective, item) from a category mask that the client's own object
// pass draws only while a global names the local player whose view is being drawn (0 to 3). The client's wrapper of
// a player's view sets it for the call and then -1, and with it raises that player's first-pass flag, which makes the
// first object pass of the view set the mask up again. The mod draws each eye outside that wrapper, so the eyes saw
// -1, drew no mask, and outlined everything with the categories of the game's own flat view.
// Shape, in the wrapper: mov [rip+USER],r32; cmp r32,3; ja; movzx r32,byte [rip+ENABLE]; test al,al; je;
// lea r64,[rip+FLAGS]; and later in the same function mov dword [rip+USER],-1.
inline constexpr char kVisrViewUserSet[] =
    "89 ?? ?? ?? ?? ?? 83 ?? 03 77 ?? 0F B6 ?? ?? ?? ?? ?? 84 C0 74 ?? ?? 8D ?? ?? ?? ?? ??";
// Read by the object pass: mov r32,[rip+USER], then cmp of that register with 3 within kVisrViewUserCmpWindow bytes
// (0.10.5 compared at once; 0.10.6 puts two stores between the compare and its ja, and may load into r8-r15).
inline constexpr uint32_t kVisrViewUserCmpWindow = 16;

struct VisrViewUser
{
    uint32_t wrapper = 0, at = 0;              // the wrapper and its store
    uint32_t user = 0, enable = 0, flags = 0;  // the globals (RVAs)
    uint32_t reader = 0;                       // an object pass that reads the user and the flags
    const char* missing = nullptr;
};

inline bool RipModrm(uint8_t modrm) { return (modrm & 0xC7) == 0x05; }
inline bool RipModrmAt(const Image& image, uint32_t at) { return image.Holds(at, 1) && RipModrm(image.base[at]); }
inline uint32_t RipTarget(const Image& image, uint32_t dispAt, uint32_t instructionEnd)
{
    return static_cast<uint32_t>(static_cast<int64_t>(instructionEnd) + Rel32(image, dispAt));
}

// Whether at `r` there is mov r32,[rip+disp32] (an optional REX before it) that reads `global`, and the same
// register is compared with 3 soon after.
inline bool ReadsAndTestsUser(const Image& image, uint32_t r, uint32_t global)
{
    if (!image.Holds(r, 6 + kVisrViewUserCmpWindow + 4) || image.base[r] != 0x8B || !RipModrmAt(image, r + 1))
        return false;
    const uint32_t end = r + 6;
    if (static_cast<uint32_t>(static_cast<int64_t>(end) + Rel32(image, r + 2)) != global)
        return false;
    const bool rexR = r > 0 && (image.base[r - 1] & 0xF4) == 0x44;   // 0x44/0x4C: the destination is r8-r15
    const uint8_t reg = (image.base[r + 1] >> 3) & 7;
    for (uint32_t at = end; at < end + kVisrViewUserCmpWindow; ++at)
    {
        const bool rex = image.base[at] == 0x41;
        const uint32_t op = at + (rex ? 1u : 0u);
        if (rex == rexR && image.base[op] == 0x83 && image.base[op + 1] == (0xF8 | reg) && image.base[op + 2] == 3)
            return true;
    }
    return false;
}

// 0.10.6 and later: the builder keeps its list of constant buffers ({capacity, items, count}, items of 0x28 bytes:
// name, its length, capacity, data, its length) in a global it stores whole: mov [rip+A],r64; mov [rip+A+8],r64;
// mov [rip+A+0x10],r64 (exe+0x45C071B in 0.10.7). The global's RVA, or 0 when the builder has no such stores, or names
// more than one global.
inline uint32_t FindVisrConstantsList(const Image& image, uint32_t function)
{
    uint32_t begin = 0, end = 0, found = 0;
    if (!image.Extent(function, begin, end) || begin != function)
        return 0;
    const auto store = [&](uint32_t at, uint32_t& target) {
        if (!image.Holds(at, 7) || (image.base[at] & 0xFB) != 0x48 || image.base[at + 1] != 0x89 ||
            !RipModrm(image.base[at + 2]))
            return false;
        target = RipTarget(image, at + 3, at + 7);
        return true;
    };
    for (uint32_t at = begin; at + 21 <= end; ++at)
    {
        uint32_t a = 0, b = 0, c = 0;
        if (!store(at, a) || !store(at + 7, b) || !store(at + 14, c) || b != a + 8 || c != a + 0x10 ||
            !image.Holds(a, 0x18) || image.InCode(a))
            continue;
        if (found && found != a)
            return 0;
        found = a;
    }
    return found;
}

inline VisrViewUser FindVisrViewUser(const Image& image)
{
    VisrViewUser out;
    const std::vector<uint32_t> hits =
        FindAll(image, image.codeRva, image.codeSize, ParsePattern(kVisrViewUserSet), 2);
    if (hits.size() != 1)
    {
        out.missing = hits.empty() ? "the store of the view's player" : "a single store of the view's player";
        return out;
    }
    const uint32_t at = hits[0];
    const uint8_t* b = image.base + at;
    uint32_t begin = 0, end = 0;
    if (!RipModrm(b[1]) || (b[7] & 0xF8) != 0xF8 || !RipModrm(b[13]) || !RipModrm(b[24]) ||
        (b[22] & 0xF8) != 0x48)
    {
        out.missing = "the store, test and flags of the view's player as rip-relative globals";
        return out;
    }
    if (!image.Extent(at, begin, end))
    {
        out.missing = "the wrapper's function extent";
        return out;
    }
    const uint32_t user = RipTarget(image, at + 2, at + 6);
    const uint32_t enable = RipTarget(image, at + 14, at + 18);
    const uint32_t flags = RipTarget(image, at + 25, at + 29);
    const bool inCode = [&] {
        for (const uint32_t g : {user, enable, flags})
            if (!image.Holds(g, 4) || (g >= image.codeRva && g - image.codeRva < image.codeSize))
                return true;
        return false;
    }();
    if (inCode)
    {
        out.missing = "the view's player, enable and flags in data";
        return out;
    }
    // The wrapper sets the user back to -1 after the view.
    bool reset = false;
    for (const uint32_t r : FindAll(image, at, end - at, ParsePattern("C7 05 ?? ?? ?? ?? FF FF FF FF"), 8))
        reset = reset || RipTarget(image, r + 2, r + 10) == user;
    if (!reset)
    {
        out.missing = "the wrapper's reset of the view's player to -1";
        return out;
    }
    // An object pass reads the user (and tests it against 3) and addresses the same flags. Every mov from the user's
    // global is looked at, however many other loads share the opcode (no cap: a cap could stop short of the reader).
    for (uint32_t r = image.codeRva; r + 6 <= image.codeRva + image.codeSize; ++r)
    {
        const uint8_t* next = static_cast<const uint8_t*>(
            std::memchr(image.base + r, 0x8B, image.codeRva + image.codeSize - r));
        if (!next)
            break;
        r = static_cast<uint32_t>(next - image.base);
        uint32_t rb = 0, re = 0;
        if (r == at || !ReadsAndTestsUser(image, r, user) || !image.Extent(r, rb, re))
            continue;
        for (const uint32_t l : FindAll(image, rb, re - rb, ParsePattern("8D ?? ?? ?? ?? ??"), 65536))
            if (l > rb && (image.base[l - 1] & 0xF8) == 0x48 && RipModrm(image.base[l + 1]) &&
                RipTarget(image, l + 2, l + 6) == flags)
            {
                out.reader = rb;
                break;
            }
        if (out.reader)
            break;
    }
    if (!out.reader)
    {
        out.missing = "an object pass reading the view's player and its flags";
        return out;
    }
    out.wrapper = begin;
    out.at = at;
    out.user = user;
    out.enable = enable;
    out.flags = flags;
    return out;
}

// ---- The input device its prompts show ----------------------------------------------------------------------------
// The client draws its button prompts (the game's training hints among them) for the device last used: a mouse click
// or key in its menus makes it "keyboard and mouse", and only a controller button or a stick past the deadzone in a
// menu makes it "controller" again (both logged as "[menus] input device: ..."). The mod's menu pointer clicks, so a
// pointer click left last in a menu kept keyboard pictures in the prompts of the game that followed. Its main loop
// keeps two globals: the device (1 keyboard and mouse, 2 controller) and a controller flag the prompts read.
// Shape (0.10.6, exe+0x4B4D78 and exe+0x4B4162 in one function):
//   controller: mov byte [rip+DEVICE],2; mov al,1; xchg [rip+CONTROLLER],al
//   keyboard:   mov byte [rip+DEVICE],1; xor eax,eax; xchg [rip+CONTROLLER],al
inline constexpr char kInputDeviceController[] = "C6 05 ?? ?? ?? ?? 02 B0 01 86 05 ?? ?? ?? ??";
inline constexpr char kInputDeviceKeyboard[] = "C6 05 ?? ?? ?? ?? 01 ?? C0 86 05 ?? ?? ?? ??";

struct InputDevice
{
    uint32_t controllerAt = 0, keyboardAt = 0;  // the two stores
    uint32_t device = 0, controller = 0;        // the globals (RVAs)
    const char* missing = nullptr;
};

inline InputDevice FindInputDevice(const Image& image)
{
    InputDevice out;
    const std::vector<uint32_t> hits =
        FindAll(image, image.codeRva, image.codeSize, ParsePattern(kInputDeviceController), 2);
    if (hits.size() != 1)
    {
        out.missing = hits.empty() ? "the store of the controller as the input device"
                                   : "a single store of the controller as the input device";
        return out;
    }
    const uint32_t at = hits[0];
    uint32_t begin = 0, end = 0;
    if (!image.Extent(at, begin, end))
    {
        out.missing = "the input device's function extent";
        return out;
    }
    const uint32_t device = RipTarget(image, at + 2, at + 7);
    const uint32_t controller = RipTarget(image, at + 11, at + 15);
    if (device == controller || !image.Holds(device, 1) || !image.Holds(controller, 1) || image.InCode(device) ||
        image.InCode(controller))
    {
        out.missing = "the input device and its controller flag in data";
        return out;
    }
    // The keyboard's store, in the same function, sets the same two globals.
    for (const uint32_t k : FindAll(image, begin, end - begin, ParsePattern(kInputDeviceKeyboard), 8))
        if ((image.base[k + 7] == 0x31 || image.base[k + 7] == 0x33) && RipTarget(image, k + 2, k + 7) == device &&
            RipTarget(image, k + 11, k + 15) == controller)
        {
            out.keyboardAt = k;
            break;
        }
    if (!out.keyboardAt)
    {
        out.missing = "the keyboard's store of the same input device";
        return out;
    }
    out.controllerAt = at;
    out.device = device;
    out.controller = controller;
    return out;
}
} // namespace reclaimer_client
