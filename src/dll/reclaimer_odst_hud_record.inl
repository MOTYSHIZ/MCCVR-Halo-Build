// PROJECT RECLAIMER: ODST'S HUD DRAWS FROM THE CLIENT'S OWN COPY OF ODST'S LAYOUT RECORD (included by game.cpp, in its
// namespace, after the safe-frame helpers).
//
// Reclaimer 0.10 converts an ODST level's tags into the engine's, but draws their HUD as ODST's engine does, from its
// own parse of ODST's HUD globals: each of ODST's five layout records a 0x110-byte buffer, listed as {capacity,
// pointer, length}. Writing the HUD size to the engine's converted copy, to Halo 3's records or to the client's
// converted copy moved nothing; writing it to the 1280x720 buffer the list names shrank the HUD at once, and putting
// the title's value back there brought the full-size HUD back (FA, 2026-11-07). The heap also holds hundreds of freed
// buffers with the same bytes and two dozen lone entries naming them, and writing such copies closed the game, so only
// the buffers of the one list that names ODST's whole block are written, and the list is read again before every write.
//
// ALL FIVE OF THEM. Through 0.10.1 the client drew one player's HUD from the block's first record (1280x720). 0.10.2
// reworked ODST's split-screen HUD (each of up to four players gets the whole HUD), and on its first run the HUD was
// the game's size again with the list found and the 1280x720 buffer holding the written layout (2026-11-11): the
// client now draws from another of the block's records, plausibly the one nearest its screen's shape (the mod's
// 2912x2100 is nearer 920x690's than 1280x720's). Every record of that one list gets the layout, as Halo 3's records
// do, each keeping its own depth.
//
// AND 0.10.2'S OWN TABLE. That alone left the HUD wide: 0.10.2 keeps a table of the five records' leading fields of its
// own (reclaimer_odst_hud::TableAt), with the game's safe frames, two of them zero. The one such table is found in the
// same pass and its five pairs get the layout too, the table read whole and checked before every write.
//
// ON ITS OWN, FROM THE LEVEL'S START. The first build searched only after Halo 3's layout scan (6 s) and read all of
// memory twice (13 s): ODST's HUD kept the game's size for about 20 s of every level (2026-11-07 16:55). The search now
// starts as the level does, on a thread of its own, reads the client's memory once for the list alone, and the record
// is written whether or not Halo 3's records have been found yet.

#include "../common/reclaimer_odst_hud_logic.h"

// halo3_tag_names.inl: whether the loaded map's scenario is one of ODST's.
bool Halo3LoadedScenarioIsOdst();

struct ReclaimerOdstHudRecord
{
    std::atomic<uintptr_t> listEntry{0};
    std::atomic<uintptr_t> record{0};
    std::atomic<uint32_t> generation{0};
    std::atomic<uint32_t> baseDepthBits[reclaimer_odst_hud::kBlockRecords]{};
    std::atomic<uintptr_t> table{0};   // 0.10.2's table (0: none found)
};
ReclaimerOdstHudRecord g_reclaimerOdstHud{};
// The depth field the last record found held when first found: the same buffer found again keeps it, so the curvature
// setting never compounds.
std::atomic<uintptr_t> g_reclaimerOdstHudRememberedRecord{0};
std::atomic<uint32_t> g_reclaimerOdstHudRememberedDepthBits[reclaimer_odst_hud::kBlockRecords]{};
// The search: one at a time, a few tries a level (the client's parse may not be listed yet at the very start).
std::atomic<bool> g_reclaimerOdstHudSearchInFlight{false};
std::atomic<uint32_t> g_reclaimerOdstHudSearchGeneration{0};
std::atomic<uint32_t> g_reclaimerOdstHudSearchTries{0};
std::atomic<uint64_t> g_reclaimerOdstHudSearchLastMs{0};
// The layout scan saw ODST's converted record in the engine's tag data in this generation.
std::atomic<uint32_t> g_reclaimerOdstHudMarkerGeneration{0};

static bool ReadOwnMemory(uintptr_t address, void* out, size_t size)
{
    SIZE_T got = 0;
    return ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<const void*>(address), out, size, &got) &&
        got == size;
}

// Whether the five entries at `listEntry` list ODST's whole layout block.
static bool ReclaimerOdstHudListsBlock(uintptr_t listEntry)
{
    uint64_t words[3 * reclaimer_odst_hud::kBlockRecords]{};
    if (!listEntry || !ReadOwnMemory(listEntry, words, sizeof(words)))
        return false;
    reclaimer_odst_hud::RecordShape shapes[reclaimer_odst_hud::kBlockRecords]{};
    for (int i = 0; i < reclaimer_odst_hud::kBlockRecords; ++i)
    {
        if (!reclaimer_odst_hud::ListsRecord(words + 3 * i, words[3 * i + 1]) ||
            !ReadOwnMemory(static_cast<uintptr_t>(words[3 * i + 1]) + reclaimer_odst_hud::kWidthOffset, &shapes[i],
                           sizeof(shapes[i])))
            return false;
    }
    return reclaimer_odst_hud::ListsBlock(words, shapes);
}

// Still the first of ODST's block in its list, and still ODST's 1280x720 record.
static bool ReclaimerOdstHudRecordHolds(uintptr_t listEntry, uintptr_t record)
{
    uint64_t words[3]{};
    if (!listEntry || !ReadOwnMemory(listEntry, words, sizeof(words)) ||
        !reclaimer_odst_hud::ListsRecord(words, record) || !ReclaimerOdstHudListsBlock(listEntry))
        return false;
    const HudLayoutAnchorView view = HudLayoutAnchorAt(kOdstHudLayoutAdapter, 0);
    unsigned char anchor[kHudLayoutMaxAnchor]{};
    return ReadOwnMemory(record + reclaimer_odst_hud::kWidthOffset, anchor, static_cast<size_t>(view.anchorLength)) &&
        memcmp(anchor, view.anchor, 8) == 0 && SafeFrameAnchorEquals(anchor, view);
}

// The five records the list at `listEntry` names, in the block's order.
static bool ReclaimerOdstHudBlockRecords(uintptr_t listEntry, uintptr_t (&records)[reclaimer_odst_hud::kBlockRecords])
{
    uint64_t words[3 * reclaimer_odst_hud::kBlockRecords]{};
    if (!listEntry || !ReadOwnMemory(listEntry, words, sizeof(words)))
        return false;
    for (int i = 0; i < reclaimer_odst_hud::kBlockRecords; ++i)
        records[i] = static_cast<uintptr_t>(words[3 * i + 1]);
    return true;
}

// Where a record's safe-frame pair sits.
static uintptr_t ReclaimerOdstHudSlot(uintptr_t record)
{
    return record + reclaimer_odst_hud::kWidthOffset + static_cast<uintptr_t>(kOdstHudLayoutAdapter.safeFrameOffset);
}

// Calls `visit(base, size)` for each committed private read-write region outside this module. False: the level
// changed on the way.
template <class Visit>
static bool ForEachPrivateReadWriteRegion(uint32_t generation, Visit&& visit)
{
    uintptr_t selfBase = 0;
    size_t selfSize = 0;
    sig::SelfModuleRange(selfBase, selfSize);
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    uintptr_t addr = reinterpret_cast<uintptr_t>(si.lpMinimumApplicationAddress);
    const uintptr_t end = reinterpret_cast<uintptr_t>(si.lpMaximumApplicationAddress);
    MEMORY_BASIC_INFORMATION mbi{};
    while (addr < end && VirtualQuery(reinterpret_cast<void*>(addr), &mbi, sizeof(mbi)) == sizeof(mbi))
    {
        if (!HudLayoutContextMatches(HudLayoutProfile::Halo3, generation))
            return false;
        const uintptr_t base = reinterpret_cast<uintptr_t>(mbi.BaseAddress);
        const bool self = selfBase && base >= selfBase && base < selfBase + selfSize;
        if (!self && mbi.State == MEM_COMMIT && mbi.Type == MEM_PRIVATE && mbi.Protect == PAGE_READWRITE)
            visit(base, static_cast<size_t>(mbi.RegionSize));
        addr = base + mbi.RegionSize;
    }
    return true;
}

// ---- the client's own reference to the list (reclaimer_odst_hud_logic.h: Anchor) ------------------------------------
// Learned from a list the shape search found, saved beside the mod's settings for the client executable it was learned
// in, and followed first on later levels. Whatever it leads to must pass ReclaimerOdstHudRecordHolds, as a shape match
// must, and the shape search still runs after it: when the two disagree the shape search's list is written and the
// reference is learned again without the word that misled. A client update changes the executable, which drops the
// saved reference; the next list the shape search finds teaches the new one. All of it runs on the search thread.

struct ReclaimerHostExe
{
    uintptr_t base = 0;
    uint32_t stamp = 0, size = 0;
    std::vector<std::pair<uintptr_t, size_t>> writable;   // the image's writable sections
};

static bool ReadReclaimerHostExe(ReclaimerHostExe& out)
{
    out = ReclaimerHostExe{};
    out.base = reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
    IMAGE_DOS_HEADER dos{};
    IMAGE_NT_HEADERS64 nt{};
    if (!out.base || !ReadOwnMemory(out.base, &dos, sizeof(dos)) || dos.e_magic != IMAGE_DOS_SIGNATURE ||
        !ReadOwnMemory(out.base + dos.e_lfanew, &nt, sizeof(nt)) || nt.Signature != IMAGE_NT_SIGNATURE ||
        nt.OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC)
        return false;
    out.stamp = nt.FileHeader.TimeDateStamp;
    out.size = nt.OptionalHeader.SizeOfImage;
    const uintptr_t sections = out.base + dos.e_lfanew + offsetof(IMAGE_NT_HEADERS64, OptionalHeader) +
        nt.FileHeader.SizeOfOptionalHeader;
    for (unsigned i = 0; i < nt.FileHeader.NumberOfSections && i < 96; ++i)
    {
        IMAGE_SECTION_HEADER section{};
        if (!ReadOwnMemory(sections + i * sizeof(section), &section, sizeof(section)))
            return false;
        if ((section.Characteristics & IMAGE_SCN_MEM_WRITE) && section.Misc.VirtualSize &&
            section.VirtualAddress < out.size)
            out.writable.push_back({out.base + section.VirtualAddress,
                                    (std::min)(static_cast<size_t>(section.Misc.VirtualSize),
                                               static_cast<size_t>(out.size - section.VirtualAddress))});
    }
    return true;
}

static std::wstring ReclaimerAnchorFile()
{
    HMODULE self = nullptr;
    wchar_t path[MAX_PATH]{};
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                            reinterpret_cast<LPCWSTR>(&ReclaimerAnchorFile), &self) ||
        !GetModuleFileNameW(self, path, MAX_PATH))
        return {};
    std::wstring file = path;
    const size_t slash = file.find_last_of(L"\\/");
    if (slash == std::wstring::npos)
        return {};
    file.resize(slash + 1);
    return file + L"HaloMCCVR-reclaimer-anchors.txt";
}

static bool LoadReclaimerOdstAnchor(const ReclaimerHostExe& exe, reclaimer_odst_hud::Anchor& out)
{
    const std::wstring path = ReclaimerAnchorFile();
    if (path.empty())
        return false;
    const HANDLE file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                                    FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE)
        return false;
    char text[1024]{};
    DWORD got = 0;
    const BOOL read = ReadFile(file, text, sizeof(text) - 1, &got, nullptr);
    CloseHandle(file);
    reclaimer_odst_hud::Anchor anchor;
    if (!read || !reclaimer_odst_hud::ParseAnchor(std::string(text, got), anchor) || anchor.exeStamp != exe.stamp ||
        anchor.exeSize != exe.size)
        return false;
    out = anchor;
    return true;
}

static void SaveReclaimerOdstAnchor(const reclaimer_odst_hud::Anchor& anchor)
{
    const std::wstring path = ReclaimerAnchorFile();
    if (path.empty())
        return;
    const std::string text = "# Project Reclaimer: where the client keeps its list of ODST's HUD layout records, learned "
                             "by HaloMCCVR for one client executable. A cache: safe to delete.\r\n" +
        reclaimer_odst_hud::FormatAnchor(anchor) + "\r\n";
    const HANDLE file = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL,
                                    nullptr);
    if (file == INVALID_HANDLE_VALUE)
        return;
    DWORD written = 0;
    WriteFile(file, text.data(), static_cast<DWORD>(text.size()), &written, nullptr);
    CloseHandle(file);
}

// The one entry starting ODST's block in the list the anchor leads to, or 0.
static uintptr_t ReclaimerOdstListEntryFromAnchor(const ReclaimerHostExe& exe, const reclaimer_odst_hud::Anchor& anchor)
{
    uint64_t exeWord = 0, blockWord = 0;
    if (!ReadOwnMemory(exe.base + anchor.rva, &exeWord, sizeof(exeWord)) || !exeWord)
        return 0;
    if (anchor.kind == reclaimer_odst_hud::AnchorKind::PointerInBlock &&
        !ReadOwnMemory(static_cast<uintptr_t>(exeWord) + anchor.offset, &blockWord, sizeof(blockWord)))
        return 0;
    const uint64_t list = reclaimer_odst_hud::ListStart(anchor, exeWord, blockWord);
    if (!list)
        return 0;
    uintptr_t found = 0;
    int count = 0;
    for (uint32_t k = 0; k <= reclaimer_odst_hud::kMaxListIndex; ++k)
    {
        const uintptr_t entry = static_cast<uintptr_t>(list + k * reclaimer_odst_hud::kListStride);
        uint64_t words[3]{};
        if (!ReadOwnMemory(entry, words, sizeof(words)))
            break;
        if (words[2] == reclaimer_odst_hud::kRecordLength &&
            ReclaimerOdstHudRecordHolds(entry, static_cast<uintptr_t>(words[1])))
        {
            found = entry;
            ++count;
        }
    }
    return count == 1 ? found : 0;
}

// Calls `visit(address, word)` for every aligned word of [base, base + size) that can be read.
template <class Visit>
static void ForEachWordIn(uintptr_t base, size_t size, uint64_t* window, size_t windowWords, Visit&& visit)
{
    size_t pos = 0;
    while (pos + 8 <= size)
    {
        const size_t want = (std::min)(size - pos, windowWords * 8) & ~size_t{7};
        SIZE_T got = 0;
        if (!ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<const void*>(base + pos), window, want, &got) &&
            got == 0)
        {
            pos = ((((base + pos) >> 12) + 1) << 12) - base;   // the next page
            continue;
        }
        for (size_t w = 0; w < got / 8; ++w)
            visit(base + pos + w * 8, window[w]);
        pos += got ? (got & ~SIZE_T{7}) : 8;
    }
}

// What in the client points at the list whose entry `listEntry` starts ODST's block. `avoid`: executable words that
// led elsewhere before.
static bool LearnReclaimerOdstAnchor(uint32_t generation, uintptr_t listEntry, const ReclaimerHostExe& exe,
                                     const std::vector<uint32_t>& avoid, reclaimer_odst_hud::Anchor& out,
                                     size_t& considered)
{
    using reclaimer_odst_hud::AnchorKind;
    constexpr size_t kWindowWords = size_t{1} << 16;
    static uint64_t s_learnWindow[kWindowWords];
    constexpr size_t kMaxCandidates = 1024;
    struct Candidate
    {
        reclaimer_odst_hud::Anchor anchor;
        int rank;
    };
    std::vector<Candidate> candidates;
    const auto avoided = [&](uint32_t rva) { return std::find(avoid.begin(), avoid.end(), rva) != avoid.end(); };
    const auto add = [&](uint32_t rva, AnchorKind kind, uint32_t offset, int rank) {
        if (candidates.size() >= kMaxCandidates || avoided(rva))
            return;
        Candidate c;
        c.anchor.exeStamp = exe.stamp;
        c.anchor.exeSize = exe.size;
        c.anchor.rva = rva;
        c.anchor.kind = kind;
        c.anchor.offset = offset;
        c.rank = rank;
        candidates.push_back(c);
    };
    // One step: the executable's own words that point at the list, or into the 4 KiB before ODST's block.
    for (const auto& span : exe.writable)
        ForEachWordIn(span.first, span.second, s_learnWindow, kWindowWords, [&](uintptr_t at, uint64_t value) {
            uint32_t index = 0;
            if (reclaimer_odst_hud::CanStartListAt(value, listEntry, index))
                add(static_cast<uint32_t>(at - exe.base), AnchorKind::Pointer, 0, 0);
            else if (value && value <= listEntry && listEntry - value < reclaimer_odst_hud::kMaxBlockOffset &&
                     !((listEntry - value) % 8))
                add(static_cast<uint32_t>(at - exe.base), AnchorKind::InBlock,
                    static_cast<uint32_t>(listEntry - value), 2);
        });
    // Two steps: heap words that point at the list, then the executable's words that point at the blocks holding them.
    std::vector<uintptr_t> heapRefs;
    constexpr uintptr_t kTagDataSpan = uintptr_t{1} << 34;
    const uintptr_t tagBase = Halo3TagDataBaseValue();
    if (!ForEachPrivateReadWriteRegion(generation, [&](uintptr_t base, size_t size) {
            if (tagBase && base >= tagBase && base - tagBase < kTagDataSpan)
                return;
            ForEachWordIn(base, size, s_learnWindow, kWindowWords, [&](uintptr_t at, uint64_t value) {
                uint32_t index = 0;
                if (heapRefs.size() < 256 && reclaimer_odst_hud::CanStartListAt(value, listEntry, index))
                    heapRefs.push_back(at);
            });
        }))
    {
        considered = SIZE_MAX;   // the level changed on the way: no verdict
        return false;
    }
    std::sort(heapRefs.begin(), heapRefs.end());
    if (!heapRefs.empty())
        for (const auto& span : exe.writable)
            ForEachWordIn(span.first, span.second, s_learnWindow, kWindowWords, [&](uintptr_t at, uint64_t value) {
                const auto ref = std::lower_bound(heapRefs.begin(), heapRefs.end(), static_cast<uintptr_t>(value));
                if (!value || ref == heapRefs.end() || *ref - value >= reclaimer_odst_hud::kMaxBlockOffset ||
                    (*ref - value) % 8)
                    return;
                add(static_cast<uint32_t>(at - exe.base), AnchorKind::PointerInBlock,
                    static_cast<uint32_t>(*ref - value), 1);
            });
    // The plainest that leads back to this very list.
    std::stable_sort(candidates.begin(), candidates.end(), [](const Candidate& a, const Candidate& b) {
        return a.rank != b.rank ? a.rank < b.rank : a.anchor.offset < b.anchor.offset;
    });
    considered = candidates.size();
    for (const Candidate& c : candidates)
        if (ReclaimerOdstListEntryFromAnchor(exe, c.anchor) == listEntry)
        {
            out = c.anchor;
            return true;
        }
    return false;
}

static const char* ReclaimerOdstAnchorText(const reclaimer_odst_hud::Anchor& anchor, char* text, size_t size)
{
    switch (anchor.kind)
    {
    case reclaimer_odst_hud::AnchorKind::Pointer:
        std::snprintf(text, size, "the client's word at exe+0x%X points at the list", anchor.rva);
        break;
    case reclaimer_odst_hud::AnchorKind::PointerInBlock:
        std::snprintf(text, size, "the client's word at exe+0x%X points at a block whose word at +0x%X points at the "
                                  "list", anchor.rva, anchor.offset);
        break;
    default:
        std::snprintf(text, size, "the client's word at exe+0x%X points at a block holding the list 0x%X in",
                      anchor.rva, anchor.offset);
        break;
    }
    return text;
}

// The layout goes to `record` from now on. False: not a record the layout can be written to (or the level changed).
static bool PublishReclaimerOdstHudRecord(uint32_t generation, uintptr_t listEntry, uintptr_t record)
{
    // All five of ODST's layout records the list names, each with the depth it held when first found.
    uintptr_t records[reclaimer_odst_hud::kBlockRecords]{};
    uint32_t depths[reclaimer_odst_hud::kBlockRecords]{};
    if (!ReclaimerOdstHudBlockRecords(listEntry, records) || records[0] != record)
        return false;
    for (int i = 0; i < reclaimer_odst_hud::kBlockRecords; ++i)
        if (!ReadOwnMemory(ReclaimerOdstHudSlot(records[i]) + kOdstHudLayoutAdapter.depthFromSlot, &depths[i],
                           sizeof(depths[i])) || !HudDestinationPlausible(depths[i]))
            return false;
    if (!HudLayoutContextMatches(HudLayoutProfile::Halo3, generation))
        return false;
    const bool remembered = g_reclaimerOdstHudRememberedRecord.exchange(record, std::memory_order_relaxed) == record;
    for (int i = 0; i < reclaimer_odst_hud::kBlockRecords; ++i)
    {
        if (remembered)
            depths[i] = g_reclaimerOdstHudRememberedDepthBits[i].load(std::memory_order_relaxed);
        else
            g_reclaimerOdstHudRememberedDepthBits[i].store(depths[i], std::memory_order_relaxed);
        g_reclaimerOdstHud.baseDepthBits[i].store(depths[i], std::memory_order_relaxed);
    }
    g_reclaimerOdstHud.listEntry.store(listEntry, std::memory_order_relaxed);
    g_reclaimerOdstHud.record.store(record, std::memory_order_relaxed);
    g_reclaimerOdstHud.generation.store(generation, std::memory_order_release);
    return true;
}

// The client's reference, the shape search and what to keep. Search thread only.
static void FindReclaimerOdstHudRecord(uint32_t generation, int attempt)
{
    const uint64_t t0 = GetTickCount64();
    static bool s_exeRead = false, s_haveAnchor = false;
    static ReclaimerHostExe s_exe;
    static reclaimer_odst_hud::Anchor s_anchor;
    static std::vector<uint32_t> s_misleading;   // executable words whose list the shape search did not confirm
    static uintptr_t s_confirmedEntry = 0;        // the list entry the shape search last agreed the reference leads to
    static unsigned s_learnFailures = 0;
    if (!s_exeRead)
    {
        s_exeRead = true;
        if (ReadReclaimerHostExe(s_exe))
        {
            s_haveAnchor = LoadReclaimerOdstAnchor(s_exe, s_anchor);
            char how[160];
            LOG("SAFEFRAME [Halo 3]: Project Reclaimer's ODST HUD list: %s", s_haveAnchor
                ? ReclaimerOdstAnchorText(s_anchor, how, sizeof(how))
                : "no reference to it learned for this client executable yet; the shape search finds it");
        }
        else
            LOG("SAFEFRAME [Halo 3]: Project Reclaimer's executable headers could not be read; ODST's HUD list is "
                "found by its shape alone");
    }

    // 1. The client's own reference, when one is known. It is written to at once only when it is known to be live:
    // the shape search confirmed it earlier in this launch, and the client has since pointed it at another list (a
    // reference the client re-points is one it maintains, not one left behind at a freed list). Otherwise it waits for
    // the shape search below, as a list found by its shape alone would.
    uintptr_t anchorEntry = 0, anchorRecord = 0;
    bool anchorWritten = false;
    if (s_haveAnchor && (anchorEntry = ReclaimerOdstListEntryFromAnchor(s_exe, s_anchor)) != 0)
    {
        uint64_t words[3]{};
        if (ReadOwnMemory(anchorEntry, words, sizeof(words)))
            anchorRecord = static_cast<uintptr_t>(words[1]);
        if (!anchorRecord)
            anchorEntry = 0;
        else if (s_confirmedEntry && anchorEntry != s_confirmedEntry &&
                 PublishReclaimerOdstHudRecord(generation, anchorEntry, anchorRecord))
        {
            anchorWritten = true;
            LOG("SAFEFRAME [Halo 3]: ODST's level on Project Reclaimer, try %d: the client's own reference, re-pointed "
                "since the shape search last confirmed it, leads to ODST's layout record at %p; the layout is written "
                "there (%llu ms); the shape search checks it next", attempt, reinterpret_cast<void*>(anchorRecord),
                static_cast<unsigned long long>(GetTickCount64() - t0));
        }
    }

    // 2. The shape search: one pass over the client's memory for a list of ODST's whole block, five {capacity,
    // pointer, 0x110} entries in a row, each checked against the record it names. The engine's tag data (16 GiB from
    // its base) is the game's, not the client's parse, and is skipped.
    constexpr uintptr_t kTagDataSpan = uintptr_t{1} << 34;
    const uintptr_t tagBase = Halo3TagDataBaseValue();
    constexpr size_t kEntryWords = 3 * reclaimer_odst_hud::kBlockRecords;
    constexpr size_t kWindowWords = (size_t{1} << 17) + kEntryWords;
    static uint64_t s_window[kWindowWords];
    uintptr_t listEntry = 0, record = 0, table = 0;
    int listed = 0, tabled = 0;
    uint64_t scanned = 0;
    // Windows overlap by a whole table, so one across a window's end is seen in the next; anything seen twice there is
    // counted once (addresses only rise).
    constexpr size_t kOverlap = (std::max)((kEntryWords - 1) * 8, (reclaimer_odst_hud::kTableBytes + 7) / 8 * 8);
    if (!ForEachPrivateReadWriteRegion(generation, [&](uintptr_t base, size_t size) {
            if (tagBase && base >= tagBase && base - tagBase < kTagDataSpan)
                return;
            scanned += size;
            size_t pos = 0;
            while (pos + kEntryWords * 8 <= size)
            {
                if (!HudLayoutContextMatches(HudLayoutProfile::Halo3, generation))
                    return;
                const size_t want = (std::min)(size - pos, kWindowWords * 8);
                SIZE_T got = 0;
                ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<const void*>(base + pos), s_window, want, &got);
                if (got < kEntryWords * 8)
                {
                    pos = ((pos >> 12) + 1) << 12;
                    continue;
                }
                const size_t words = got / 8;
                for (size_t w = 0; w + kEntryWords <= words; ++w)
                {
                    if (s_window[w + 2] != reclaimer_odst_hud::kRecordLength ||
                        s_window[w + 5] != reclaimer_odst_hud::kRecordLength ||
                        s_window[w + 8] != reclaimer_odst_hud::kRecordLength ||
                        s_window[w + 11] != reclaimer_odst_hud::kRecordLength ||
                        s_window[w + 14] != reclaimer_odst_hud::kRecordLength)
                        continue;
                    const uintptr_t entry = base + pos + w * 8;
                    if (entry <= listEntry ||
                        !ReclaimerOdstHudRecordHolds(entry, static_cast<uintptr_t>(s_window[w + 1])))
                        continue;
                    ++listed;
                    listEntry = entry;
                    record = static_cast<uintptr_t>(s_window[w + 1]);
                }
                const auto* bytes = reinterpret_cast<const uint8_t*>(s_window);
                for (size_t b = 0; b + reclaimer_odst_hud::kTableBytes <= got; b += 4)
                {
                    uint32_t first[2]{};
                    memcpy(first, bytes + b, sizeof(first));
                    if (first[0] != reclaimer_odst_hud::kBlockShapes[0].width ||
                        first[1] != reclaimer_odst_hud::kBlockShapes[0].height || base + pos + b <= table ||
                        !reclaimer_odst_hud::TableAt(bytes + b))
                        continue;
                    ++tabled;
                    table = base + pos + b;
                }
                // The next window starts where an entry or a table not yet seen whole could begin.
                pos += got == want && pos + got < size ? got - kOverlap : (((pos + got) >> 12) + 1) << 12;
            }
        }))
        return;

    // Only what this search found: a table from an earlier level may be freed memory now.
    g_reclaimerOdstHud.table.store(tabled == 1 ? table : 0, std::memory_order_relaxed);
    if (tabled == 1)
    {
        LOG("SAFEFRAME [Halo 3]: ODST's level on Project Reclaimer: the client's own table of ODST's five layouts at %p "
            "gets the layout too (Project Reclaimer 0.10.2 and later)", reinterpret_cast<void*>(table));
    }
    else if (tabled)
        LOG("SAFEFRAME [Halo 3]: ODST's level on Project Reclaimer, try %d: %d tables of ODST's five layouts, not told "
            "apart; nothing written to them", attempt, tabled);
    if (listed != 1)
    {
        LOG("SAFEFRAME [Halo 3]: ODST's level on Project Reclaimer, try %d: %d list(s) name ODST's whole layout block "
            "in %llu MB of the client's memory; %s (%llu ms)", attempt, listed,
            static_cast<unsigned long long>(scanned >> 20),
            anchorWritten ? "the record the client's own reference leads to is kept"
            : listed      ? "not told apart, nothing written there"
                          : "ODST's HUD keeps the game's size for now",
            static_cast<unsigned long long>(GetTickCount64() - t0));
        if (tabled == 1 && HudLayoutContextMatches(HudLayoutProfile::Halo3, generation))
            g_reclaimerOdstHud.generation.store(generation, std::memory_order_release);
        return;
    }
    const bool agrees = anchorEntry && anchorEntry == listEntry;
    if (agrees)
    {
        s_confirmedEntry = listEntry;
        LOG("SAFEFRAME [Halo 3]: ODST's level on Project Reclaimer: the shape search agrees with the client's own "
            "reference (%llu MB read, %llu ms)", static_cast<unsigned long long>(scanned >> 20),
            static_cast<unsigned long long>(GetTickCount64() - t0));
        if (anchorWritten)
            return;
    }
    else if (s_haveAnchor)
    {
        // It led elsewhere, or nowhere, while the client lists ODST's block: learned again without that word.
        s_misleading.push_back(s_anchor.rva);
        s_haveAnchor = false;
        s_confirmedEntry = 0;
        LOG("SAFEFRAME [Halo 3]: ODST's level on Project Reclaimer: the client's own reference led to %p, the shape "
            "search to %p; the shape search's is written, and the reference is learned again",
            reinterpret_cast<void*>(anchorEntry ? anchorRecord : 0), reinterpret_cast<void*>(record));
    }
    if (!PublishReclaimerOdstHudRecord(generation, listEntry, record))
        return;
    LOG("SAFEFRAME [Halo 3]: ODST's level on Project Reclaimer: the client draws its HUD from its own copy of ODST's "
        "layout records, the first at %p (ODST's block listed at %p; try %d, %llu MB of the client's memory read); the "
        "layout is written to all five (%llu ms)", reinterpret_cast<void*>(record), reinterpret_cast<void*>(listEntry),
        attempt, static_cast<unsigned long long>(scanned >> 20),
        static_cast<unsigned long long>(GetTickCount64() - t0));

    // 3. Learn the client's reference to this list for the next levels (and launches of this executable).
    if (agrees || s_haveAnchor || !s_exe.base || s_misleading.size() > 16 || s_learnFailures >= 2)
        return;
    const uint64_t t1 = GetTickCount64();
    reclaimer_odst_hud::Anchor learned;
    size_t considered = 0;
    if (LearnReclaimerOdstAnchor(generation, listEntry, s_exe, s_misleading, learned, considered))
    {
        s_anchor = learned;
        s_haveAnchor = true;
        s_confirmedEntry = listEntry;   // learned from the list the shape search just found
        SaveReclaimerOdstAnchor(learned);
        char how[160];
        LOG("SAFEFRAME [Halo 3]: Project Reclaimer's ODST HUD list, learned: %s (%zu candidate(s) weighed, %llu ms); "
            "later levels follow it first", ReclaimerOdstAnchorText(learned, how, sizeof(how)), considered,
            static_cast<unsigned long long>(GetTickCount64() - t1));
    }
    else if (considered == SIZE_MAX)
        LOG("SAFEFRAME [Halo 3]: Project Reclaimer's ODST HUD list: the level changed while its reference was looked "
            "for; looked for again on the next level found by its shape");
    else if (++s_learnFailures, true)
        LOG("SAFEFRAME [Halo 3]: Project Reclaimer's ODST HUD list: no reference to it from the client's executable "
            "within two steps (%zu candidate(s) weighed, %llu ms); later levels find it by its shape",
            considered, static_cast<unsigned long long>(GetTickCount64() - t1));
}

static DWORD WINAPI ReclaimerOdstHudSearchThread(LPVOID parameter)
{
    const uintptr_t token = reinterpret_cast<uintptr_t>(parameter);
    FindReclaimerOdstHudRecord(static_cast<uint32_t>(token), static_cast<int>(token >> 32));
    g_reclaimerOdstHudSearchInFlight.store(false, std::memory_order_release);
    return 0;
}

// 0.10.2's table: its five safe-frame pairs get the layout when they differ, the table read whole and checked first.
// False: there is none, or it is no longer the client's.
static bool ApplyReclaimerOdstHudTable(uint32_t generation, float horizontal, float vertical, bool reportReadback)
{
    const uintptr_t table = g_reclaimerOdstHud.table.load(std::memory_order_relaxed);
    if (!table)
        return false;
    uint8_t bytes[reclaimer_odst_hud::kTableBytes]{};
    if (!ReadOwnMemory(table, bytes, sizeof(bytes)) || !reclaimer_odst_hud::TableAt(bytes))
    {
        g_reclaimerOdstHud.table.store(0, std::memory_order_relaxed);
        LOG("SAFEFRAME [Halo 3]: Project Reclaimer's table of ODST's layouts at %p is gone; not written again",
            reinterpret_cast<void*>(table));
        return false;
    }
    const float pair[2]{horizontal, vertical};
    int holding = 0;
    for (int i = 0; i < reclaimer_odst_hud::kBlockRecords; ++i)
    {
        const size_t at = static_cast<size_t>(i) * reclaimer_odst_hud::kTableStride + reclaimer_odst_hud::kTablePairOffset;
        if (memcmp(bytes + at, pair, sizeof(pair)) == 0)
        {
            ++holding;
            continue;
        }
        SIZE_T wrote = 0;
        AcquireSRWLockShared(&g_hudLayoutWriteLock);
        if (HudLayoutContextMatches(HudLayoutProfile::Halo3, generation))
            WriteProcessMemory(GetCurrentProcess(), reinterpret_cast<void*>(table + at), pair, sizeof(pair), &wrote);
        ReleaseSRWLockShared(&g_hudLayoutWriteLock);
    }
    if (reportReadback)
        LOG("SAFEFRAME [Halo 3]: Project Reclaimer's table of ODST's layouts: %d of %d already held %.4f/%.4f",
            holding, reclaimer_odst_hud::kBlockRecords, horizontal, vertical);
    return true;
}

// Writes the layout to the record when it differs. False: the record is no longer the client's.
static bool ApplyReclaimerOdstHudRecord(uint32_t generation, float horizontal, float vertical, float curvature,
                                        bool reportReadback)
{
    const bool tabled = ApplyReclaimerOdstHudTable(generation, horizontal, vertical, reportReadback);
    const uintptr_t record = g_reclaimerOdstHud.record.load(std::memory_order_relaxed);
    if (!record)
        return tabled;
    const uintptr_t listEntry = g_reclaimerOdstHud.listEntry.load(std::memory_order_relaxed);
    if (!ReclaimerOdstHudRecordHolds(listEntry, record))
    {
        g_reclaimerOdstHud.record.store(0, std::memory_order_relaxed);
        LOG("SAFEFRAME [Halo 3]: Project Reclaimer no longer lists ODST's layout record at %p; not written again",
            reinterpret_cast<void*>(record));
        return tabled;
    }
    uintptr_t records[reclaimer_odst_hud::kBlockRecords]{};
    if (!ReclaimerOdstHudBlockRecords(listEntry, records))
        return true;
    int holding = 0;
    for (int i = 0; i < reclaimer_odst_hud::kBlockRecords; ++i)
    {
        const uintptr_t slot = ReclaimerOdstHudSlot(records[i]);
        float pair[2]{};
        uint32_t depthBits = 0;
        if (!ReadOwnMemory(slot, pair, sizeof(pair)) ||
            !ReadOwnMemory(slot + kOdstHudLayoutAdapter.depthFromSlot, &depthBits, sizeof(depthBits)))
            continue;
        float baseDepth = 0.0f;
        const uint32_t baseBits = g_reclaimerOdstHud.baseDepthBits[i].load(std::memory_order_relaxed);
        memcpy(&baseDepth, &baseBits, sizeof(baseDepth));
        // As the records ApplyHudLayoutOnce writes: 0 flat, 1 fully curved, 0.5 the authored value.
        const float depth = baseDepth + (0.30f - 0.60f * curvature);
        uint32_t targetBits = 0;
        memcpy(&targetBits, &depth, sizeof(targetBits));
        if (!HudDestinationPlausible(targetBits))
            continue;
        if (reportReadback && i == 0)
            LOG("SAFEFRAME [Halo 3]: Project Reclaimer's ODST record at %p reads %.4f/%.4f, writing %.4f/%.4f to "
                "all five", reinterpret_cast<void*>(records[i]), pair[0], pair[1], horizontal, vertical);
        if (pair[0] == horizontal && pair[1] == vertical && depthBits == targetBits)
        {
            ++holding;
            continue;
        }
        AcquireSRWLockShared(&g_hudLayoutWriteLock);
        if (HudLayoutContextMatches(HudLayoutProfile::Halo3, generation))
            SafeFrameStoreLayout(slot, kOdstHudLayoutAdapter, depth, horizontal, vertical);
        ReleaseSRWLockShared(&g_hudLayoutWriteLock);
    }
    if (reportReadback)
        LOG("SAFEFRAME [Halo 3]: Project Reclaimer's ODST records: %d of %d already held the layout", holding,
            reclaimer_odst_hud::kBlockRecords);
    return true;
}

// Whether the running level is ODST's (Game_ReclaimerOdstLevel, end of game.cpp), as the HUD tick below last judged it.
std::atomic<bool> g_reclaimerOdstLevel{false};

// Present thread, from HudLayoutAutoTick while a Halo 3 level runs on Project Reclaimer: starts the search on an ODST
// level, then keeps the record holding the layout the player's settings and the Aim Frame want.
static void ReclaimerOdstHudTick(uint32_t generation, uint64_t now, float horizontal, float vertical,
                                 float curvature)
{
    const bool found = (g_reclaimerOdstHud.record.load(std::memory_order_relaxed) ||
                        g_reclaimerOdstHud.table.load(std::memory_order_relaxed)) &&
        g_reclaimerOdstHud.generation.load(std::memory_order_acquire) == generation;
    g_reclaimerOdstLevel.store(
        found ||
            g_reclaimerOdstHudMarkerGeneration.load(std::memory_order_relaxed) == generation ||
            Halo3LoadedScenarioIsOdst(),
        std::memory_order_relaxed);
    if (found)
    {
        static uint64_t s_lastApplyMs = 0, s_lastReadbackMs = 0;
        static float s_lastH = 0.0f, s_lastV = 0.0f, s_lastCurvature = -1.0f;
        const bool changed = horizontal != s_lastH || vertical != s_lastV || curvature != s_lastCurvature;
        if (!changed && now - s_lastApplyMs < 1000)
            return;
        s_lastApplyMs = now;
        s_lastH = horizontal;
        s_lastV = vertical;
        s_lastCurvature = curvature;
        const bool readback = now - s_lastReadbackMs >= 5000;
        if (readback)
            s_lastReadbackMs = now;
        ApplyReclaimerOdstHudRecord(generation, horizontal, vertical, curvature, readback);
        return;
    }

    if (!g_activeTitleLevelRunning.load(std::memory_order_acquire) ||
        g_reclaimerOdstHudSearchInFlight.load(std::memory_order_acquire))
        return;
    const bool odstLevel = g_reclaimerOdstHudMarkerGeneration.load(std::memory_order_relaxed) == generation ||
        Halo3LoadedScenarioIsOdst();
    if (!odstLevel)
        return;
    constexpr uint32_t kTries = 4;
    constexpr uint64_t kRetryMs = 3000;
    if (g_reclaimerOdstHudSearchGeneration.load(std::memory_order_relaxed) != generation)
    {
        g_reclaimerOdstHudSearchGeneration.store(generation, std::memory_order_relaxed);
        g_reclaimerOdstHudSearchTries.store(0, std::memory_order_relaxed);
    }
    else if (g_reclaimerOdstHudSearchTries.load(std::memory_order_relaxed) >= kTries ||
             now - g_reclaimerOdstHudSearchLastMs.load(std::memory_order_relaxed) < kRetryMs)
        return;
    if (g_reclaimerOdstHudSearchInFlight.exchange(true, std::memory_order_acq_rel))
        return;
    const uint32_t attempt = g_reclaimerOdstHudSearchTries.fetch_add(1, std::memory_order_relaxed) + 1;
    g_reclaimerOdstHudSearchLastMs.store(now, std::memory_order_relaxed);
    const uintptr_t token = static_cast<uintptr_t>(generation) | (static_cast<uintptr_t>(attempt) << 32);
    HANDLE thread = CreateThread(nullptr, 0, ReclaimerOdstHudSearchThread, reinterpret_cast<LPVOID>(token), 0, nullptr);
    if (thread)
        CloseHandle(thread);
    else
        g_reclaimerOdstHudSearchInFlight.store(false, std::memory_order_release);
}
