#pragma once

// PROJECT RECLAIMER'S OWN COPY OF ODST'S HUD LAYOUT RECORD (game.cpp, reclaimer_odst_hud_record.inl). Reclaimer 0.10
// draws an ODST level's HUD from its own parse of ODST's HUD globals, where each of the block's five layout records is
// a byte buffer of its own, listed as {capacity, pointer, length}. The heap also holds hundreds of freed buffers with
// the 1280x720 record's bytes and two dozen lone entries naming them (FA, 2026-11-07); only one list named all five
// records in ODST's order, and its 1280x720 buffer was the one the client draws from.
// Pure logic; unit-tested in tests/core_tests.cpp.

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <string>

namespace reclaimer_odst_hud
{
// ODST's s_chud_curvature_info is 0x110 bytes (halo3odst/maps/sc100.map keeps its five 0x110 apart); its virtual width
// sits 0x94 in, after the resolution flags, the curvature points and the screen basis, as in Reach's record.
inline constexpr uint64_t kRecordLength = 0x110;
inline constexpr uintptr_t kWidthOffset = 0x94;
// The longest capacity a list entry for one record is taken to have.
inline constexpr uint64_t kMaxCapacity = 0x10000;

// Whether three words {capacity, pointer, length} list `record` as a whole ODST record.
inline constexpr bool ListsRecord(const uint64_t* words, uint64_t record) noexcept
{
    return record != 0 && words[1] == record && words[2] == kRecordLength && words[0] >= kRecordLength &&
        words[0] <= kMaxCapacity;
}

struct RecordShape
{
    uint32_t width;
    uint32_t height;
};

// ODST's layout block in its own order (halo3odst/maps/sc100.map 0xE778A88, 0x110 apart).
inline constexpr int kBlockRecords = 5;
inline constexpr RecordShape kBlockShapes[kBlockRecords] = {
    {1280, 720}, {1600, 580}, {920, 690}, {1180, 470}, {1138, 640}};

// Whether five {capacity, pointer, length} entries in a row list ODST's whole block: `shapes` holds the virtual width
// and height each entry's buffer holds.
inline constexpr bool ListsBlock(const uint64_t* words, const RecordShape* shapes) noexcept
{
    for (int i = 0; i < kBlockRecords; ++i)
    {
        if (!ListsRecord(words + 3 * i, words[3 * i + 1]) || shapes[i].width != kBlockShapes[i].width ||
            shapes[i].height != kBlockShapes[i].height)
            return false;
    }
    return true;
}

// Whether a loaded scenario's path is one of ODST's: Reclaimer runs ODST's levels as Halo 3 maps, and the header of
// the map it serves names ODST's scenario (levels\atlas\sc100\sc100, 2026-11-07).
inline bool IsOdstScenarioPath(const char* path) noexcept
{
    constexpr char kPrefix[] = "levels\\atlas\\";
    if (!path)
        return false;
    for (size_t i = 0; i + 1 < sizeof(kPrefix); ++i)
    {
        const char c = path[i] >= 'A' && path[i] <= 'Z' ? static_cast<char>(path[i] - 'A' + 'a') : path[i];
        if (c != kPrefix[i])
            return false;
    }
    return true;
}

// Whether `value` is one of the `count` candidate records, sorted ascending.
inline bool IsCandidate(const uintptr_t* sorted, size_t count, uint64_t value) noexcept
{
    return count && std::binary_search(sorted, sorted + count, static_cast<uintptr_t>(value));
}

// ---- the client's own reference to the list (reclaimer_odst_hud_record.inl: the anchor) -----------------------------
// The shape search finds the list by what it holds. Once found, the mod also looks for what in the client points at
// it, starting from a word in the client executable's own data: that word points at the list, or at a heap block
// holding a pointer to the list, or at a heap block the list sits in -- and on later levels follows that first. What it reaches must pass the very check the shape search
// makes before anything is written, and the shape search still runs after it and wins when the two disagree. The
// anchor is learned again by itself whenever a client update moves it: nothing about it is written in this source.

// The list's entries are {capacity, pointer, length}, 0x18 bytes each; a list holding ODST's block is taken to hold
// it within its first kMaxListIndex entries.
inline constexpr uint64_t kListStride = 0x18;
inline constexpr uint32_t kMaxListIndex = 64;
// The furthest into a heap block a reference to the list is looked for (two steps).
inline constexpr uint64_t kMaxBlockOffset = 0x1000;

// Whether `value` can be the start of a list whose entry `index` is `listEntry`.
inline constexpr bool CanStartListAt(uint64_t value, uint64_t listEntry, uint32_t& index) noexcept
{
    if (!value || value > listEntry || (listEntry - value) % kListStride)
        return false;
    const uint64_t at = (listEntry - value) / kListStride;
    if (at > kMaxListIndex)
        return false;
    index = static_cast<uint32_t>(at);
    return true;
}

enum class AnchorKind : uint8_t
{
    Pointer,          // the executable's word points at the list
    PointerInBlock,   // ... at a heap block whose word at `offset` points at the list
    InBlock,          // ... at a heap block the list sits in, `offset` bytes in
};

struct Anchor
{
    uint32_t exeStamp = 0, exeSize = 0;   // the client executable it was learned in (PE TimeDateStamp, SizeOfImage)
    uint32_t rva = 0;                     // the word in the executable's data
    AnchorKind kind = AnchorKind::Pointer;
    uint32_t offset = 0;
};

// Where the list starts, given the executable's word and (for PointerInBlock) the block's word: 0 when unknown.
inline constexpr uint64_t ListStart(const Anchor& anchor, uint64_t exeWord, uint64_t blockWord) noexcept
{
    switch (anchor.kind)
    {
    case AnchorKind::Pointer: return exeWord;
    case AnchorKind::PointerInBlock: return exeWord ? blockWord : 0;
    case AnchorKind::InBlock: return exeWord ? exeWord + anchor.offset : 0;
    }
    return 0;
}

// One line of the mod's cache file: "odst-hud <stamp> <size> <rva> <how>", hexadecimal, <how> "-" (Pointer),
// "<offset>" (PointerInBlock) or "+<offset>" (InBlock).
inline std::string FormatAnchor(const Anchor& anchor)
{
    char line[96];
    if (anchor.kind == AnchorKind::Pointer)
        std::snprintf(line, sizeof(line), "odst-hud %08x %08x %08x -", anchor.exeStamp, anchor.exeSize, anchor.rva);
    else
        std::snprintf(line, sizeof(line), "odst-hud %08x %08x %08x %s%x", anchor.exeStamp, anchor.exeSize, anchor.rva,
                      anchor.kind == AnchorKind::InBlock ? "+" : "", anchor.offset);
    return line;
}

inline bool ParseAnchor(const std::string& text, Anchor& out)
{
    const size_t at = text.find("odst-hud ");
    if (at == std::string::npos)
        return false;
    unsigned stamp = 0, size = 0, rva = 0;
    char last[16]{};
    if (std::sscanf(text.c_str() + at, "odst-hud %x %x %x %15s", &stamp, &size, &rva, last) != 4 || !size || !rva ||
        rva >= size)
        return false;
    Anchor anchor;
    anchor.exeStamp = stamp;
    anchor.exeSize = size;
    anchor.rva = rva;
    if (last[0] != '-' || last[1])
    {
        const bool inBlock = last[0] == '+';
        unsigned offset = 0;
        char extra = 0;
        if ((inBlock && last[1] == '-') ||
            std::sscanf(last + (inBlock ? 1 : 0), "%x%c", &offset, &extra) != 1 || offset >= kMaxBlockOffset ||
            offset % 8)
            return false;
        anchor.kind = inBlock ? AnchorKind::InBlock : AnchorKind::PointerInBlock;
        anchor.offset = offset;
    }
    out = anchor;
    return true;
}
} // namespace reclaimer_odst_hud
