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

// ---- ODST's prompts --------------------------------------------------------------------------------------------------
// Reclaimer runs ODST's tutorial prompts (unit_confirm_message, unit_confirm_cancel_message) as natives of its own
// (docs/ODST-PROMPT-AND-MAP-EVIDENCE-2026-11-15.md). Both call one setter that ORs the prompt's bit (0 confirm, 1 cancel,
// 2 retrain) into a byte per controller: lea rdx,[rip+FLAGS]; mov r8b,1; mov ecx,ebx; shl r8b,cl; ...;
// lock or byte [r14+rdx],r8b. The client's control step clears the controller's byte once the prompt is answered:
// lea rcx,[rip+FLAGS]; mov byte [rax+rcx],0; then tests the answered bits 1, 2 and 4.
inline constexpr char kPromptFlagSet[] = "48 8D 15 ?? ?? ?? ?? 41 B0 01 89 D9 41 D2 E0 4C 89 74 24 ?? F0 45 08 04 16";
inline constexpr char kPromptFlagClear[] =
    "48 8D 0D ?? ?? ?? ?? C6 04 08 00 0F B6 44 24 ?? A8 01 75 ?? A8 02 75 ?? A8 04 75";
// 0.10.6 (an MSVC build) keeps the answered bits in a byte register: test r8b,1; jne; test r8b,2; jne; test r8b,4; jne.
inline constexpr char kPromptFlagClearReg[] =
    "48 8D 0D ?? ?? ?? ?? C6 04 08 00 40 F6 ?? 01 75 ?? 40 F6 ?? 02 75 ?? 40 F6 ?? 04 75";
// 0.10.7: the same tests with near jumps (jne rel32).
inline constexpr char kPromptFlagClearRegNear[] =
    "48 8D 0D ?? ?? ?? ?? C6 04 08 00 40 F6 ?? 01 0F 85 ?? ?? ?? ?? 40 F6 ?? 02 0F 85 ?? ?? ?? ?? 40 F6 ?? 04 0F 85";
// Any build: the clear itself (lea rcx,[rip+FLAGS]; mov byte [rax+rcx],0), taken only when it names the setter's bytes
// and tests of bits 1, 2 and 4 follow within kPromptClearTestWindow bytes, in that order. 0.10.9 reloads the answered
// bits from the stack (mov eax,[rbp+0x50]; test al,1; jne; test al,2; jne; test al,4; jne near), which none of the
// fixed forms above match; this keeps the next register or jump change from breaking the find.
inline constexpr char kPromptFlagClearAny[] = "48 8D 0D ?? ?? ?? ?? C6 04 08 00";
inline constexpr uint32_t kPromptClearTestWindow = 0x20;
// The two natives' own bodies: the unit's controller, then the setter with the prompt's kind (0, then 1).
inline constexpr char kConfirmNative[] = "48 83 EC 28 48 8B 01 8B 08 31 D2 E8";
inline constexpr char kCancelNative[] = "48 83 EC 28 48 8B 01 8B 08 B2 01 E8";
// The natives' entries in the client's script table: the name's address, then its length.
inline constexpr char kConfirmName[] = "unit_confirm_message";
inline constexpr char kCancelName[] = "unit_confirm_cancel_message";

struct OdstPrompt
{
    uint32_t flags = 0;    // the byte per controller (rva)
    uint32_t setter = 0;   // the function that sets it
    const char* missing = nullptr;
};

namespace detail
{
// The rip-relative target of a 7-byte lea at `rva` (its disp32 at +3).
inline uint64_t LeaTarget(const Image& image, uint32_t rva)
{
    return static_cast<uint64_t>(static_cast<int64_t>(rva) + 7 + Rel32(image, rva + 3));
}
// The preferred image base in the image's own headers (0 when they do not read).
inline uint64_t HeaderImageBase(const Image& image)
{
    if (!image.Holds(0, 0x40))
        return 0;
    uint32_t nt = 0;
    std::memcpy(&nt, image.base + 0x3C, 4);
    uint64_t base = 0;
    if (!image.Holds(static_cast<uint64_t>(nt) + 0x30, 8))
        return 0;
    std::memcpy(&base, image.base + nt + 0x30, 8);
    return base;
}
// The function the client's script table names `name` for: the table entry holds the name's address and length, and
// its function 0x30 further on. Pointers are the image's own (in memory) or the headers' preferred base (on disk).
inline uint32_t ScriptNative(const Image& image, const char* name)
{
    const size_t length = std::strlen(name);
    std::vector<int> text;
    for (size_t i = 0; i < length; ++i)
        text.push_back(static_cast<unsigned char>(name[i]));
    const std::vector<uint32_t> names = FindAll(image, 0, static_cast<uint32_t>(image.size), text, 4);
    const uint64_t bases[2]{reinterpret_cast<uint64_t>(image.base), HeaderImageBase(image)};
    uint32_t found = 0;
    for (const uint32_t at : names)
        for (const uint64_t base : bases)
        {
            if (!base)
                continue;
            std::vector<int> entry;
            const uint64_t pointer = base + at, size = length;
            for (int i = 0; i < 8; ++i)
                entry.push_back(static_cast<int>((pointer >> (8 * i)) & 0xFF));
            for (int i = 0; i < 8; ++i)
                entry.push_back(static_cast<int>((size >> (8 * i)) & 0xFF));
            for (const uint32_t e : FindAll(image, 0, static_cast<uint32_t>(image.size), entry, 8))
            {
                uint64_t function = 0;
                if (!image.Holds(static_cast<uint64_t>(e) + 0x30, 8))
                    continue;
                std::memcpy(&function, image.base + e + 0x30, 8);
                if (function < base || function - base < image.codeRva ||
                    function - base >= static_cast<uint64_t>(image.codeRva) + image.codeSize)
                    continue;
                const uint32_t rva = static_cast<uint32_t>(function - base);
                if (found && found != rva)
                    return 0;   // two tables name it differently: not one answer
                found = rva;
            }
        }
    return found;
}
// True when tests of the answered bits 1, 2 and 4 follow `from` in that order within `window` bytes: test al,imm8
// (A8 ib) or test r8,imm8 with a register operand ([REX] F6 C0+r ib).
inline bool PromptAnswerTestsFollow(const Image& image, uint32_t from, uint32_t window)
{
    if (!image.Holds(from, window))
        return false;
    const uint8_t* p = image.base + from;
    const uint8_t want[3]{1, 2, 4};
    int next = 0;
    for (uint32_t i = 0; i + 1 < window && next < 3; ++i)
    {
        if (p[i] == 0xA8 && p[i + 1] == want[next])
            ++next, ++i;
        else if (p[i] == 0xF6 && i + 2 < window && (p[i + 1] & 0xF8) == 0xC0 && p[i + 2] == want[next])
            ++next, i += 2;
    }
    return next == 3;
}
} // namespace detail

inline OdstPrompt FindOdstPrompt(const Image& image)
{
    OdstPrompt out;
    const std::vector<uint32_t> set = FindAll(image, image.codeRva, image.codeSize, ParsePattern(kPromptFlagSet), 2);
    // The clear in either known form; exactly one place in the code between them.
    std::vector<uint32_t> clear = FindAll(image, image.codeRva, image.codeSize, ParsePattern(kPromptFlagClear), 2);
    for (const char* form : {kPromptFlagClearReg, kPromptFlagClearRegNear})
        for (const uint32_t hit : FindAll(image, image.codeRva, image.codeSize, ParsePattern(form), 2))
            clear.push_back(hit);
    // No known form: any clear naming the setter's own bytes, followed by the three answer tests.
    if (clear.empty() && set.size() == 1)
        for (const uint32_t hit : FindAll(image, image.codeRva, image.codeSize, ParsePattern(kPromptFlagClearAny)))
            if (detail::LeaTarget(image, hit) == detail::LeaTarget(image, set[0]) &&
                detail::PromptAnswerTestsFollow(image, hit + 11, kPromptClearTestWindow))
                clear.push_back(hit);
    uint32_t b = 0, e = 0;
    if (set.size() != 1)
        out.missing = set.empty() ? "the prompt setter's store" : "a single prompt setter's store (more than one)";
    else if (clear.size() != 1)
        out.missing = clear.empty() ? "the control step's clear of the prompt" : "a single clear of the prompt";
    else if (!image.Extent(set[0], b, e))
        out.missing = "the prompt setter's extent";
    else if (detail::LeaTarget(image, set[0]) != detail::LeaTarget(image, clear[0]))
        out.missing = "the setter and the clear naming the same prompt bytes";
    else
    {
        const uint64_t flags = detail::LeaTarget(image, set[0]);
        // Both natives, found by their table entries and by their own bodies, call this setter.
        const uint32_t confirm = detail::ScriptNative(image, kConfirmName);
        const uint32_t cancel = detail::ScriptNative(image, kCancelName);
        if (!image.Holds(flags, 4) || flags < static_cast<uint64_t>(image.codeRva) + image.codeSize)
            out.missing = "prompt bytes in the client's data";
        else if (!confirm || !cancel)
            out.missing = "unit_confirm_message and unit_confirm_cancel_message in the script table";
        else if (!MatchAt(image, confirm, ParsePattern(kConfirmNative)) ||
                 !MatchAt(image, cancel, ParsePattern(kCancelNative)))
            out.missing = "the two natives' bodies (the prompt's kind, then the setter)";
        else if (!Calls(image, confirm, b) || !Calls(image, cancel, b))
            out.missing = "both natives calling the prompt setter";
        else
        {
            out.flags = static_cast<uint32_t>(flags);
            out.setter = b;
        }
    }
    return out;
}

// ---- ODST's VISR map (the PDA): a LOG-ONLY probe ---------------------------------------------------------------------
// Reclaimer's pda_is_active_deterministic native asks a reader (`lea rcx,[rsp+0x28]; call READER`) for the player's
// PDA record and answers "active" from its flag bits 5 or 11. The reader takes the record from one of two places: a
// record in the game's state memory, reached through the guest image's global +OFFSET and a layout struct of the
// client's ((lo32(s[0]) + hi32(s[0]) + ADD) + [guest+OFFSET] + dword s[8] + SUB) & ~0xF, + RECORD + player * 20), used
// while a snapshot struct's byte +0x44 is 1; else a static record in the client's data (STATIC + OFFSET2 + player * 20).
// Which of them tracks the map being open is not proven: the DLL only reads and logs both (docs/ODST-PROMPT-AND-MAP-
// EVIDENCE-2026-11-15.md), and nothing acts on them.
inline constexpr char kPdaNative[] = "48 83 EC 48 48 8B 01 8B 08 BA 03 10 00 00 E8";
inline constexpr char kPdaNativeCall[] =
    "48 8D 4C 24 28 E8 ?? ?? ?? ?? 0F B6 4C 24 45 83 E1 01 F6 44 24 44 01 B8 01 00 00 00 0F 44 C1";
// The reader then copies the snapshot to the stack and tests its selecting byte there (`cmp byte [rsp+COPY],1`). Up to
// 0.10.9 the byte is the snapshot's +0x44, read alone (`mov eax,[rcx+0x44]`); 0.10.10 grew the struct and copies its tail
// with one 16-byte move (`movups xmm0,[rcx+SRC]; movups [rsp+DST],xmm0`), and the byte is +SRC + (COPY - DST) = +0x4C.
// 0.10.11 copies the whole struct, the tail first (`movups xmm0,[rcx+0x44]; movaps [rsp+0x70],xmm0`), and tests
// [rsp+0x7C] on both of its paths: +0x50.
inline constexpr char kPdaReady[] = "48 8B 05 ?? ?? ?? ?? 48 85 C0 74 ?? 48 8B 0D ?? ?? ?? ?? 48 85 C9 74 ??";
inline constexpr char kPdaSnapshotDword[] = "8B 41 44";
// movups [rsp+DST] (0F 11, 0.10.10) or movaps [rsp+DST] (0F 29, 0.10.11, which copies the whole struct and tests the
// byte twice, once on each path that reaches the record).
inline constexpr char kPdaSnapshotTail[] = "0F 10 41 ?? 0F ?? 44 24";
inline constexpr char kPdaSnapshotTest[] = "80 7C 24 ?? 01";
inline constexpr size_t kPdaSnapshotTestsMax = 2;
// The guest image: `mov rax,[rip+G]` (0.10.0-0.10.5: a pointer to it) or `lea rax,[rip+G]` (0.10.6: the image is part
// of the executable); the second opcode byte says which. The flags load is movd (66 0F 6E) or vmovd (C5 F9 6E).
inline constexpr char kPdaStateRecord[] =
    "48 ?? 05 ?? ?? ?? ?? 48 8B 98 ?? ?? ?? ?? 48 85 DB 74 ?? 48 8B 05 ?? ?? ?? ?? 48 85 C0 0F 84 ?? ?? ?? ?? "
    "48 83 FF 10 0F 83 ?? ?? ?? ?? 8B 48 08 48 8B 00 48 01 CB 89 C1 48 C1 E8 20 01 C8 05 ?? ?? ?? ?? 48 01 D8 "
    "48 05 ?? ?? ?? ?? 48 83 E0 F0 48 8D 0C BF ?? ?? 6E B4 88";
// 0.10.11 adds the layout's dword before loading its first qword (add rbx,rcx; mov rax,[rax]): same length, same
// offsets of every field read.
inline constexpr char kPdaStateRecordAddFirst[] =
    "48 ?? 05 ?? ?? ?? ?? 48 8B 98 ?? ?? ?? ?? 48 85 DB 74 ?? 48 8B 05 ?? ?? ?? ?? 48 85 C0 0F 84 ?? ?? ?? ?? "
    "48 83 FF 10 0F 83 ?? ?? ?? ?? 8B 48 08 48 01 CB 48 8B 00 89 C1 48 C1 E8 20 01 C8 05 ?? ?? ?? ?? 48 01 D8 "
    "48 05 ?? ?? ?? ?? 48 83 E0 F0 48 8D 0C BF ?? ?? 6E B4 88";
inline constexpr char kPdaStaticRecord[] =
    "48 8D 05 ?? ?? ?? ?? 48 83 FF 10 48 8D 0C BF 48 8D 84 88 ?? ?? ?? ?? 48 8D 0D ?? ?? ?? ?? 48 0F 42 C8 ?? ?? 6E 31";

struct OdstPdaProbe
{
    uint32_t reader = 0;                         // the record reader
    uint32_t readyGlobal = 0, snapshotGlobal = 0; // client globals (rva): the ready pointer, the snapshot pointer
    uint32_t snapshotField = 0x44;               // the snapshot's byte that selects the game-state record
    uint32_t guestGlobal = 0, guestOffset = 0;   // the client global holding the guest image; the guest global's offset
    bool guestDirect = false;                    // guestGlobal IS the guest image (0.10.6), not a pointer to it
    uint32_t layoutGlobal = 0;                   // the client global pointing at the layout struct
    uint32_t add = 0;                            // added to lo32 + hi32 (32-bit)
    int32_t sub = 0;                             // added after the guest pointer (sign-extended)
    uint32_t record = 0;                         // the game-state record's offset (+ player * 20)
    uint32_t staticBase = 0, staticOffset = 0;   // the static record (rva, + offset + player * 20)
    const char* missing = nullptr;
};

inline OdstPdaProbe FindOdstPdaProbe(const Image& image)
{
    OdstPdaProbe out;
    const uint32_t native = detail::ScriptNative(image, "pda_is_active_deterministic");
    uint32_t nb = 0, ne = 0, rb = 0, re = 0;
    if (!native || !MatchAt(image, native, ParsePattern(kPdaNative)) || !image.Extent(native, nb, ne))
    {
        out.missing = "pda_is_active_deterministic in the script table, with its body";
        return out;
    }
    const std::vector<uint32_t> call = FindAll(image, nb, ne - nb, ParsePattern(kPdaNativeCall), 2);
    if (call.size() != 1)
    {
        out.missing = "the native's call of its record reader";
        return out;
    }
    const int64_t reader = static_cast<int64_t>(call[0]) + 10 + Rel32(image, call[0] + 6);
    if (reader <= 0 || !image.Extent(static_cast<uint32_t>(reader), rb, re) || rb != static_cast<uint32_t>(reader))
    {
        out.missing = "the record reader's start";
        return out;
    }
    const auto unique = [&](const char* text) {
        const std::vector<uint32_t> hits = FindAll(image, rb, re - rb, ParsePattern(text), 2);
        return hits.size() == 1 ? hits[0] : 0u;
    };
    const uint32_t stateLoadFirst = unique(kPdaStateRecord), stateAddFirst = unique(kPdaStateRecordAddFirst);
    const uint32_t ready = unique(kPdaReady), fallback = unique(kPdaStaticRecord);
    const uint32_t state = stateLoadFirst && stateAddFirst ? 0u : stateLoadFirst ? stateLoadFirst : stateAddFirst;
    if (!ready || !state || !fallback)
    {
        out.missing = !ready ? "the reader's ready and snapshot checks" : !state ? "the reader's game-state record"
                                                                             : "the reader's static record";
        return out;
    }
    const auto u32 = [&image](uint32_t rva) {
        uint32_t v = 0;
        std::memcpy(&v, image.base + rva, 4);
        return v;
    };
    // The snapshot's selecting byte: read alone at +0x44 (0.10.0-0.10.9), or inside a copied 16-byte tail (0.10.10).
    const uint32_t copy = ready + 24;
    if (MatchAt(image, copy, ParsePattern(kPdaSnapshotDword)))
        out.snapshotField = 0x44;
    else
    {
        const std::vector<uint32_t> test = FindAll(image, copy, re - copy < 0x80 ? re - copy : 0x80,
                                                   ParsePattern(kPdaSnapshotTest), kPdaSnapshotTestsMax + 1);
        const uint32_t src = image.base[copy + 3], dst = image.base[copy + 8], at = test.empty() ? 0 : image.base[test[0] + 3];
        // Every test reads the same byte of the copy.
        bool sameByte = !test.empty() && test.size() <= kPdaSnapshotTestsMax;
        for (const uint32_t t : test)
            sameByte = sameByte && image.base[t + 3] == at;
        const uint8_t store = image.base[copy + 5];
        if (!MatchAt(image, copy, ParsePattern(kPdaSnapshotTail)) || (store != 0x11 && store != 0x29) || !sameByte ||
            at < dst || at >= dst + 16)
        {
            out.missing = "the reader's snapshot byte (read alone, or in a copied 16-byte tail)";
            return out;
        }
        out.snapshotField = src + (at - dst);
    }
    out.reader = rb;
    out.readyGlobal = static_cast<uint32_t>(detail::LeaTarget(image, ready));
    out.snapshotGlobal = static_cast<uint32_t>(detail::LeaTarget(image, ready + 12));
    const uint8_t guestOpcode = image.base[state + 1];
    if (guestOpcode != 0x8B && guestOpcode != 0x8D)
    {
        out.missing = "the reader's guest image (a load or an address)";
        out.reader = 0;
        return out;
    }
    const auto isVecLoad = [&image](uint32_t at) {
        return (image.base[at] == 0x66 && image.base[at + 1] == 0x0F) ||
               (image.base[at] == 0xC5 && image.base[at + 1] == 0xF9);
    };
    if (!isVecLoad(state + 84) || !isVecLoad(fallback + 34))
    {
        out.missing = "the reader's flags loads (movd or vmovd)";
        out.reader = 0;
        return out;
    }
    out.guestDirect = guestOpcode == 0x8D;
    out.guestGlobal = static_cast<uint32_t>(detail::LeaTarget(image, state));
    out.guestOffset = u32(state + 10);
    out.layoutGlobal = static_cast<uint32_t>(detail::LeaTarget(image, state + 19));
    out.add = u32(state + 63);
    out.sub = static_cast<int32_t>(u32(state + 72));
    out.record = u32(state + 89);
    out.staticBase = static_cast<uint32_t>(detail::LeaTarget(image, fallback));
    out.staticOffset = u32(fallback + 19);
    // Every global the probe reads lies in the client's data, past its code.
    const uint64_t codeEnd = static_cast<uint64_t>(image.codeRva) + image.codeSize;
    for (const uint32_t global : {out.readyGlobal, out.snapshotGlobal, out.guestGlobal, out.layoutGlobal, out.staticBase})
        if (global < codeEnd || !image.Holds(global, 8))
        {
            out = OdstPdaProbe{};
            out.missing = "the reader's globals in the client's data";
            return out;
        }
    if (out.record > 0x100000 || out.staticOffset > 0x100000)
    {
        out = OdstPdaProbe{};
        out.missing = "record offsets of a plausible size";
    }
    return out;
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
// 0.10.11 (exe+0x52FF822): the same with near jumps (ja rel32; je rel32), and player 0 set on a path of its own.
inline constexpr char kVisrViewUserSetNear[] =
    "89 ?? ?? ?? ?? ?? 83 ?? 03 0F 87 ?? ?? ?? ?? 0F B6 ?? ?? ?? ?? ?? 84 C0 0F 84 ?? ?? ?? ?? ?? 8D ?? ?? ?? ?? ??";
// Where each form keeps the compare's modrm, the enable load's modrm, the flags lea's REX and modrm.
struct VisrViewUserLayout
{
    const char* pattern;
    uint32_t cmp, enable, rex, flags;
};
inline constexpr VisrViewUserLayout kVisrViewUserLayouts[] = {{kVisrViewUserSet, 7, 13, 22, 24},
                                                              {kVisrViewUserSetNear, 7, 17, 30, 32}};
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
    uint32_t at = 0, found = 0;
    const VisrViewUserLayout* layout = nullptr;
    for (const VisrViewUserLayout& form : kVisrViewUserLayouts)
        for (const uint32_t hit : FindAll(image, image.codeRva, image.codeSize, ParsePattern(form.pattern), 2))
        {
            ++found;
            at = hit;
            layout = &form;
        }
    if (found != 1)
    {
        out.missing = !found ? "the store of the view's player" : "a single store of the view's player";
        return out;
    }
    const uint8_t* b = image.base + at;
    uint32_t begin = 0, end = 0;
    if (!RipModrm(b[1]) || (b[layout->cmp] & 0xF8) != 0xF8 || !RipModrm(b[layout->enable]) ||
        !RipModrm(b[layout->flags]) || (b[layout->rex] & 0xF8) != 0x48)
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
    const uint32_t enable = RipTarget(image, at + layout->enable + 1, at + layout->enable + 5);
    const uint32_t flags = RipTarget(image, at + layout->flags + 1, at + layout->flags + 5);
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

// 0.10.11 (2026-11-26) inlines the controller's store into two callers (exe+0x5606F3 and exe+0x58C493) and moves the
// keyboard's into a function of its own (exe+0x10117D7): every controller store must name the same two globals, and
// the keyboard's store of them may be anywhere in the code (its own function's first, as up to 0.10.10).
inline constexpr size_t kInputDeviceMaxControllerStores = 4;

inline InputDevice FindInputDevice(const Image& image)
{
    InputDevice out;
    const std::vector<uint32_t> hits = FindAll(image, image.codeRva, image.codeSize,
                                               ParsePattern(kInputDeviceController), kInputDeviceMaxControllerStores + 1);
    if (hits.empty() || hits.size() > kInputDeviceMaxControllerStores)
    {
        out.missing = hits.empty() ? "the store of the controller as the input device"
                                   : "a few stores of the controller as the input device (too many)";
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
    for (const uint32_t other : hits)
        if (RipTarget(image, other + 2, other + 7) != device || RipTarget(image, other + 11, other + 15) != controller)
        {
            out.missing = "a single store of the controller as the input device (two name different globals)";
            return out;
        }
    // The keyboard's store sets the same two globals: in the controller's function (up to 0.10.10), else anywhere.
    const auto keyboardIn = [&](uint32_t from, uint32_t length) -> uint32_t {
        for (const uint32_t k : FindAll(image, from, length, ParsePattern(kInputDeviceKeyboard), 64))
            if ((image.base[k + 7] == 0x31 || image.base[k + 7] == 0x33) && RipTarget(image, k + 2, k + 7) == device &&
                RipTarget(image, k + 11, k + 15) == controller)
                return k;
        return 0;
    };
    out.keyboardAt = keyboardIn(begin, end - begin);
    if (!out.keyboardAt)
        out.keyboardAt = keyboardIn(image.codeRva, image.codeSize);
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

// ---- ODST's tutorial text (odst_prompt_text.h) ---------------------------------------------------------------------
// Reclaimer runs chud_show_cinematic_title as a native of its own (exe+0x432A180 in 0.10.7). It clears a 0x800-byte
// buffer on its stack (1024 characters), has the title's string looked up into it through the client's version of
// Halo 3's string lookup (the guest context passed in rcx, the buffer at +0x40), measures it (at most 255 characters:
// cmp rdx,0xFE) and copies it into each player's chud. The mod rewrites the buffer as the lookup returns to this native,
// before it is measured: presentation only, as in MCC.
inline constexpr char kTitleNative[] = "chud_show_cinematic_title";
// lea rcx,[rbp+BUF]; mov r8d,0x800; xor edx,edx; call (the clear)
inline constexpr char kTitleBufferClear[] = "48 8D 4D ?? 41 B8 00 08 00 00 31 D2 E8";
// lea rax,[rbp+BUF]; mov rdi,rax; mov [rbx+40h],rax (the buffer into the guest context)
inline constexpr char kTitleBufferArg[] = "48 8D 45 ?? 48 89 C7 48 89 43 40";
// mov rcx,rbx; call LOOKUP; nop (each of the native's lookups)
inline constexpr char kTitleLookupCall[] = "48 89 D9 E8 ?? ?? ?? ?? 90";
// The measure: at most 0xFF characters.
inline constexpr char kTitleMeasure[] = "48 81 FA FE 00 00 00";

struct OdstTitleText
{
    uint32_t native = 0;       // chud_show_cinematic_title's native
    uint32_t lookup = 0;       // the string lookup it calls (to hook)
    uint32_t returns[2]{};     // where each of its lookup calls returns (the hook rewrites only for these)
    const char* missing = nullptr;
    // ODST's HUD prompts ("Press <Back> to open VISR NAV data", "Hold <use> to swap for <weapon>"): the client's fetch of
    // a HUD message for the chud (FindOdstHudText). Zero, with hudMissing, when not found: the title text still works.
    uint32_t hud = 0;          // the fetch
    uint32_t hudReturns[2]{};  // where its lookup calls return (the call and the client's resume of it)
    const char* hudMissing = nullptr;
};

// THE HUD MESSAGE FETCH. After its lookup it hands the line to the chud's post-pass with the size 0x15E
// (mov qword [reg+0x40],0x15E) and then cuts a line of 95 characters or more at the 95th (cmp rax,0x5F; a zero
// written at +0xBE): its line holds at least 96 characters, and the chud shows 95. Found by those two, after a call of
// the lookup, in one function; then every call of the lookup in it.
inline constexpr char kHudPostPassSize[] = "C7 ?? 40 5E 01 00 00";
inline constexpr char kHudLineCut[] = "48 83 F8 5F";
inline constexpr unsigned kHudLineChars = 95;
namespace detail
{
inline bool CallsAt(const Image& image, uint32_t at, uint32_t target)
{
    return image.Holds(at, 5) && image.base[at] == 0xE8 &&
           static_cast<int64_t>(at) + 5 + Rel32(image, at + 1) == static_cast<int64_t>(target);
}
}
inline void FindOdstHudText(const Image& image, OdstTitleText& out)
{
    const std::vector<int> size = ParsePattern(kHudPostPassSize), cut = ParsePattern(kHudLineCut);
    uint32_t fetch = 0, end = 0;
    for (const uint32_t at : FindAll(image, image.codeRva, image.codeSize, size, 64))
    {
        // A store of a qword (REX.W) to [reg+0x40].
        if (at < 0x200 || (image.base[at - 1] & 0xF8) != 0x48 || (image.base[at + 1] & 0xF8) != 0x40)
            continue;
        bool called = false;
        for (uint32_t back = at - 0x200; back < at && !called; ++back)
            called = detail::CallsAt(image, back, out.lookup);
        if (!called || FindAll(image, at, 0x100, cut, 1).empty())
            continue;
        uint32_t b = 0, e = 0;
        if (!image.Extent(at, b, e) || (fetch && fetch != b))
        {
            out.hudMissing = "one HUD message fetch (a lookup, the 0x15E post-pass and the 95-character cut)";
            return;
        }
        fetch = b;
        end = e;
    }
    if (!fetch)
    {
        out.hudMissing = "the HUD message fetch (a lookup, the 0x15E post-pass and the 95-character cut)";
        return;
    }
    unsigned n = 0;
    for (uint32_t at = fetch; at + 5 <= end; ++at)
        if (detail::CallsAt(image, at, out.lookup))
        {
            if (n == 2)
            {
                out.hudMissing = "the HUD message fetch's lookup calls (more than two)";
                return;
            }
            out.hudReturns[n++] = at + 5;
        }
    if (!n)
    {
        out.hudMissing = "the HUD message fetch's lookup calls";
        return;
    }
    out.hud = fetch;
}

inline OdstTitleText FindOdstTitleText(const Image& image)
{
    OdstTitleText out;
    const uint32_t native = detail::ScriptNative(image, kTitleNative);
    uint32_t b = 0, e = 0;
    if (!native || !image.Extent(native, b, e) || b != native)
    {
        out.missing = "chud_show_cinematic_title in the script table, with its body";
        return out;
    }
    const std::vector<uint32_t> clear = FindAll(image, b, e - b, ParsePattern(kTitleBufferClear), 2);
    const std::vector<uint32_t> arg = FindAll(image, b, e - b, ParsePattern(kTitleBufferArg), 2);
    if (clear.size() != 1 || arg.size() != 1 || image.base[clear[0] + 3] != image.base[arg[0] + 3])
    {
        out.missing = "the native's 1024-character buffer, cleared and handed to the lookup";
        return out;
    }
    if (FindAll(image, b, e - b, ParsePattern(kTitleMeasure), 2).size() != 1)
    {
        out.missing = "the native's measure of at most 255 characters";
        return out;
    }
    const std::vector<uint32_t> calls = FindAll(image, arg[0], e - arg[0], ParsePattern(kTitleLookupCall), 3);
    if (calls.empty() || calls.size() > 2)
    {
        out.missing = "the native's lookup calls after the buffer is handed over";
        return out;
    }
    for (size_t i = 0; i < calls.size(); ++i)
    {
        const int64_t target = static_cast<int64_t>(calls[i]) + 8 + Rel32(image, calls[i] + 4);
        uint32_t lb = 0, le = 0;
        if (!image.InCode(target) || !image.Extent(static_cast<uint32_t>(target), lb, le) ||
            lb != static_cast<uint32_t>(target) || (out.lookup && out.lookup != lb))
        {
            out.missing = "one lookup function, called by each of the native's lookup calls";
            return out;
        }
        out.lookup = lb;
        out.returns[i] = calls[i] + 8;
    }
    // The lookup takes the buffer from the context it is given: a load of [rcx+0x40] near its start.
    bool readsBuffer = false;
    for (uint32_t at = out.lookup; at + 4 <= out.lookup + 0x40 && image.Holds(at, 4); ++at)
        readsBuffer = readsBuffer || ((image.base[at] & 0xFB) == 0x48 && image.base[at + 1] == 0x8B &&
                                      (image.base[at + 2] & 0xC7) == 0x41 && image.base[at + 3] == 0x40);
    if (!readsBuffer)
    {
        out.missing = "the lookup's read of the buffer from its context (+0x40)";
        out.lookup = 0;
        return out;
    }
    out.native = native;
    FindOdstHudText(image, out);
    return out;
}
} // namespace reclaimer_client
