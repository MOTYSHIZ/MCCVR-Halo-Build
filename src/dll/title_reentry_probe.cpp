#include "title_reentry_probe.h"

#include <array>
#include <atomic>
#include <windows.h>

#include "../common/halo2_render_logic.h"
#include "../common/haloce_contracts.generated.h"
#include "../common/title_registry.h"
#include "../common/level_load_gate_logic.h"

namespace
{
    constexpr uint64_t kPresentHintFreshMs = 250;
    constexpr uint64_t kActivityFreshMs = 250;
    constexpr uint8_t kAlreadyRunningSamples = 120; // 6 s at 50 ms worker.

    struct ProbeEvidence
    {
        GameTitle title;
        size_t slot;
        uintptr_t playerViewRva;
        size_t stride;
    };

    constexpr ProbeEvidence kEvidence[] = {
        { GameTitle::Halo3, 0, 0x02D2F680, 0x2820 },
        { GameTitle::Halo3ODST, 1, 0x02D73590, 0x2810 },
        { GameTitle::HaloReach, 2, 0x029F2B90, 0x0A40 },
        { GameTitle::Halo4, 3, 0x030AD1C0, 0x0AD0 },
        { GameTitle::HaloCE, 4, 0, 0 },
        { GameTitle::Halo2, 5, 0, 0 },
    };

    struct ActivityState
    {
        uintptr_t moduleBase = 0;
        uint64_t fingerprint = 0;
        uint64_t lastChangeMs = 0;
        uintptr_t clockObject = 0;
        uint64_t lastSampleMs = 0;
        uint8_t sawStill = 0;
        uint8_t changeRun = 0;
    };

    std::array<ActivityState, std::size(kEvidence)> g_activity{};
    std::atomic<GameTitle> g_presentHint{GameTitle::None};
    std::atomic<uint64_t> g_presentHintMs{0};

    bool ReadableCommittedRange(uintptr_t address, size_t bytes,
                                uintptr_t expectedAllocationBase) noexcept
    {
        MEMORY_BASIC_INFORMATION mbi{};
        if (!address || !bytes ||
            VirtualQuery(reinterpret_cast<const void*>(address), &mbi,
                         sizeof(mbi)) != sizeof(mbi) ||
            mbi.State != MEM_COMMIT ||
            mbi.AllocationBase != reinterpret_cast<void*>(expectedAllocationBase) ||
            (mbi.Protect & (PAGE_GUARD | PAGE_NOACCESS)) != 0)
            return false;
        const uintptr_t begin = reinterpret_cast<uintptr_t>(mbi.BaseAddress);
        const uintptr_t end = begin + mbi.RegionSize;
        return address >= begin && address <= end && bytes <= end - address;
    }

    bool FingerprintPlayerView(uintptr_t moduleBase, uintptr_t rva,
                               size_t stride, uint64_t& out) noexcept
    {
        const uintptr_t address = moduleBase + rva;
        if (!ReadableCommittedRange(address, stride, moduleBase))
            return false;
        uint64_t hash = 1469598103934665603ull;
        __try
        {
            const auto* bytes = reinterpret_cast<const uint8_t*>(address);
            for (size_t i = 0; i < stride; ++i)
            {
                hash ^= bytes[i];
                hash *= 1099511628211ull;
            }
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            return false;
        }
        out = hash ? hash : 1;
        return true;
    }

    bool FingerprintHalo2(uintptr_t moduleBase, uint64_t& out,
                          bool& explicitlyStill) noexcept
    {
        explicitlyStill = false;
        const uintptr_t slot = moduleBase + kHalo2GameTimeSlotRva;
        if (!ReadableCommittedRange(slot, sizeof(uintptr_t), moduleBase))
            return false;
        uintptr_t object = 0;
        __try { object = *reinterpret_cast<const uintptr_t*>(slot); }
        __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
        if (!object)
            return false;
        MEMORY_BASIC_INFORMATION mbi{};
        if (VirtualQuery(reinterpret_cast<const void*>(object), &mbi,
                         sizeof(mbi)) != sizeof(mbi) ||
            mbi.State != MEM_COMMIT ||
            (mbi.Protect & (PAGE_GUARD | PAGE_NOACCESS)) != 0)
            return false;
        const uintptr_t begin = reinterpret_cast<uintptr_t>(mbi.BaseAddress);
        const uintptr_t end = begin + mbi.RegionSize;
        if (object < begin || object + kHalo2GameTimeCurrentTickOffset + 4 > end)
            return false;
        uint8_t initialized = 0;
        uint32_t tick = 0;
        __try
        {
            initialized = *reinterpret_cast<const uint8_t*>(
                object + kHalo2GameTimeInitializedOffset);
            tick = *reinterpret_cast<const uint32_t*>(
                object + kHalo2GameTimeCurrentTickOffset);
        }
        __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
        if (initialized > 1)
            return false;
        if (!initialized)
        {
            explicitlyStill = true;
            out = object ? object : 1;
            return true;
        }
        out = (static_cast<uint64_t>(tick) << 32) ^ object;
        if (!out) out = 1;
        return true;
    }

    constexpr auto kCeClockAnchor = [] {
        for (const auto& entry : halo_ce::contract::entries)
            if (entry.rva == halo_ce::contract::initialized_clock_render_gate)
                return entry;
        return halo_ce::contract::Entry{};
    }();
    constexpr uint32_t kCeClockSlot = [] {
        for (const auto& relative : halo_ce::contract::relatives)
            if (relative.rva == kCeClockAnchor.rva + 6 &&
                relative.size == 7 && relative.displacement == 3)
                return relative.target;
        return uint32_t{};
    }();
    static_assert(kCeClockAnchor.pattern && kCeClockSlot);
    constexpr size_t kCeClockAnchorBytes = [] {
        size_t bytes = 0;
        for (const char* pattern = kCeClockAnchor.pattern; *pattern; ++bytes)
        {
            pattern += 2;
            if (*pattern == ' ') ++pattern;
        }
        return bytes;
    }();

    bool FingerprintHaloCE(uintptr_t moduleBase, uint64_t& out,
                           bool& explicitlyStill, uintptr_t& clockObject) noexcept
    {
        // E-CE-2: HCEEK's initialized game-time singleton/tick is shared by
        // both graphics modes. These generated anchors were uniquely verified
        // against the pinned image. This bounded read only chooses an adapter;
        // it does not install hooks, pin the module, or grant VR ownership.
        if (reinterpret_cast<uintptr_t>(GetModuleHandleW(L"halo1.dll")) != moduleBase ||
            !ReadableCommittedRange(moduleBase, sizeof(IMAGE_DOS_HEADER), moduleBase) ||
            !ReadableCommittedRange(moduleBase + kCeClockAnchor.rva, kCeClockAnchorBytes, moduleBase) ||
            !ReadableCommittedRange(moduleBase + kCeClockSlot, sizeof(uintptr_t), moduleBase))
            return false;
        __try
        {
            const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(moduleBase);
            if (dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew < sizeof(*dos) ||
                static_cast<uint32_t>(dos->e_lfanew) > 4096 - sizeof(IMAGE_NT_HEADERS64) ||
                !ReadableCommittedRange(moduleBase + dos->e_lfanew,
                                        sizeof(IMAGE_NT_HEADERS64), moduleBase))
                return false;
            const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(moduleBase + dos->e_lfanew);
            if (nt->Signature != IMAGE_NT_SIGNATURE ||
                nt->FileHeader.Machine != IMAGE_FILE_MACHINE_AMD64 ||
                nt->FileHeader.TimeDateStamp != halo_ce::contract::timestamp ||
                nt->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC ||
                nt->OptionalHeader.SizeOfImage != halo_ce::contract::imageSize)
                return false;
            const char* pattern = kCeClockAnchor.pattern;
            const auto* code = reinterpret_cast<const uint8_t*>(moduleBase + kCeClockAnchor.rva);
            for (size_t i = 0; *pattern; ++i)
            {
                if (pattern[0] != '?')
                {
                    const auto hex = [](char c) { return c <= '9' ? c - '0' : c - 'A' + 10; };
                    if (code[i] != ((hex(pattern[0]) << 4) | hex(pattern[1])))
                        return false;
                }
                pattern += 2;
                if (*pattern == ' ') ++pattern;
            }
            const uintptr_t object = *reinterpret_cast<const uintptr_t*>(moduleBase + kCeClockSlot);
            MEMORY_BASIC_INFORMATION memory{};
            if (!object || VirtualQuery(reinterpret_cast<const void*>(object), &memory,
                                       sizeof(memory)) != sizeof(memory) ||
                memory.State != MEM_COMMIT || (memory.Protect & (PAGE_GUARD | PAGE_NOACCESS)))
                return false;
            const uintptr_t begin = reinterpret_cast<uintptr_t>(memory.BaseAddress);
            if (object < begin || object - begin > memory.RegionSize ||
                16 > memory.RegionSize - (object - begin))
                return false;
            const uint8_t initialized = *reinterpret_cast<const uint8_t*>(object);
            const int32_t tick = *reinterpret_cast<const int32_t*>(object + 0xC);
            if (initialized > 1 || (initialized && tick < 0) ||
                object != *reinterpret_cast<const uintptr_t*>(moduleBase + kCeClockSlot) ||
                initialized != *reinterpret_cast<const uint8_t*>(object) ||
                reinterpret_cast<uintptr_t>(GetModuleHandleW(L"halo1.dll")) != moduleBase)
                return false;
            explicitlyStill = !initialized || tick == 0;
            clockObject = object;
            out = explicitlyStill ? 1 : static_cast<uint64_t>(tick) + 1;
            return true;
        }
        __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
    }

    bool SampleEvidence(size_t index, uintptr_t moduleBase, uint64_t nowMs)
    {
        ActivityState& state = g_activity[index];
        if (!moduleBase)
        {
            state = {};
            return false;
        }
        if (state.moduleBase != moduleBase)
        {
            state = {};
            state.moduleBase = moduleBase;
        }

        uint64_t fingerprint = 0;
        bool explicitStill = false;
        const ProbeEvidence& evidence = kEvidence[index];
        uintptr_t clockObject = 0;
        const bool readable = evidence.title == GameTitle::HaloCE
            ? FingerprintHaloCE(moduleBase, fingerprint, explicitStill, clockObject)
            : evidence.title == GameTitle::Halo2
            ? FingerprintHalo2(moduleBase, fingerprint, explicitStill)
            : FingerprintPlayerView(moduleBase, evidence.playerViewRva,
                                    evidence.stride, fingerprint);
        if (!readable)
        {
            if (evidence.title == GameTitle::HaloCE)
            {
                state = {};
                state.moduleBase = moduleBase;
            }
            state.changeRun = 0;
            return false;
        }
        if (evidence.title == GameTitle::HaloCE)
        {
            if (state.clockObject != clockObject ||
                (state.lastSampleMs && (nowMs <= state.lastSampleMs ||
                    nowMs - state.lastSampleMs > kActivityFreshMs)))
            {
                state = {};
                state.moduleBase = moduleBase;
            }
            state.clockObject = clockObject;
            state.lastSampleMs = nowMs;
        }
        if (!state.fingerprint)
        {
            state.fingerprint = fingerprint;
            if (explicitStill) state.sawStill = 1;
            return false;
        }

        const bool changed = fingerprint != state.fingerprint;
        state.fingerprint = fingerprint;
        if (explicitStill || !changed)
        {
            state.sawStill = 1;
            state.changeRun = 0;
            return false;
        }

        state.lastChangeMs = nowMs;
        if (state.changeRun < kAlreadyRunningSamples)
            ++state.changeRun;
        return state.sawStill || state.changeRun >= kAlreadyRunningSamples;
    }
}

bool TitleReentryProbe_CeLevelAllowsInstall(uintptr_t moduleBase,
    uint32_t generation,uint64_t nowMs) noexcept
{
    struct ColdAdmission
    {
        uintptr_t base{},clock{};
        uint32_t generation{};
        uint64_t tick{},sampleMs{},advanceMs{};
        LevelLoadGateLogic logic;
    };
    static ColdAdmission state;
    uint64_t tick{};uintptr_t clock{};bool still{};
    if (!moduleBase||!generation||!nowMs||
        !FingerprintHaloCE(moduleBase,tick,still,clock))
    { state={};return false; }
    if (state.base!=moduleBase||state.generation!=generation||state.clock!=clock||
        (state.sampleMs&&(nowMs<=state.sampleMs||nowMs-state.sampleMs>kActivityFreshMs))||
        (state.tick&&tick<state.tick))
        state={};
    state.base=moduleBase;state.generation=generation;state.clock=clock;
    const bool sampled=state.sampleMs!=0;
    const bool changed=sampled&&!still&&tick>state.tick;
    state.tick=tick;state.sampleMs=nowMs;
    if (changed) state.advanceMs=nowMs;
    // An uninitialized/reset clock closes previous readiness. A frozen tick
    // followed by forward simulation is the same load-boundary proof used by
    // the other title gates. First observation of an already ticking clock
    // is not itself a frozen sample or permission to touch a loading module.
    if (still) { state.logic.Reset();state.advanceMs=0; }
    if (still||sampled)
        return state.logic.Observe(changed)!=LevelLoadGateLogic::Decision::Hold&&
            state.advanceMs&&nowMs-state.advanceMs<=kActivityFreshMs;
    return false;
}

void TitleReentryProbe_PublishPresentCaller(const void* caller,
                                             uint64_t nowMs) noexcept
{
    if (!caller || !nowMs)
        return;
    MEMORY_BASIC_INFORMATION mbi{};
    if (VirtualQuery(caller, &mbi, sizeof(mbi)) != sizeof(mbi) ||
        !mbi.AllocationBase)
        return;
    const uintptr_t allocation = reinterpret_cast<uintptr_t>(mbi.AllocationBase);
    size_t count = 0;
    const TitleDescriptor* titles = TitleRegistry_All(count);
    for (size_t i = 0; i < count; ++i)
    {
        HMODULE module = GetModuleHandleW(titles[i].moduleName);
        if (reinterpret_cast<uintptr_t>(module) != allocation)
            continue;
        // Publish the timestamp last. Clearing it first makes a concurrent
        // worker either see no hint or a fully paired title+timestamp, never a
        // newly written title with the previous title's still-fresh stamp.
        g_presentHintMs.store(0, std::memory_order_release);
        g_presentHint.store(titles[i].title, std::memory_order_release);
        g_presentHintMs.store(nowMs, std::memory_order_release);
        return;
    }
}

GameTitle TitleReentryProbe_Resolve(const TitleRuntimeModuleSet& modules,
                                    uint64_t nowMs,
                                    GameTitle retainedTitle) noexcept
{
    const GameTitle hintBefore =
        g_presentHint.load(std::memory_order_acquire);
    const uint64_t hintMs =
        g_presentHintMs.load(std::memory_order_acquire);
    const GameTitle hintAfter =
        g_presentHint.load(std::memory_order_acquire);
    const size_t hintSlot = TitleRuntimeSlotIndex(hintAfter);
    if (hintBefore == hintAfter && hintMs != 0 &&
        hintSlot < kTitleRuntimeSlotCount && modules.moduleBases[hintSlot] &&
        nowMs >= hintMs && nowMs - hintMs <= kPresentHintFreshMs)
        return hintAfter;

    GameTitle winner = GameTitle::None;
    unsigned candidates = 0;
    for (size_t i = 0; i < std::size(kEvidence); ++i)
    {
        const ProbeEvidence& evidence = kEvidence[i];
        const bool active = SampleEvidence(
            i, modules.moduleBases[evidence.slot], nowMs);
        const ActivityState& state = g_activity[i];
        if (!active || !state.lastChangeMs || nowMs < state.lastChangeMs ||
            nowMs - state.lastChangeMs > kActivityFreshMs)
            continue;
        winner = evidence.title;
        ++candidates;
    }
    if (candidates == 1)
        return winner;

    // Keep the already selected raw adapter stable while its module remains
    // resident. This is intentionally weaker than runtime ownership: the
    // existing lifecycle/heartbeat resolver can still revoke all capabilities
    // immediately when the title stops running. A different title's unique
    // fresh liveness evidence above always preempts this retention.
    const size_t retainedSlot = TitleRuntimeSlotIndex(retainedTitle);
    if (retainedTitle != GameTitle::None &&
        retainedTitle != GameTitle::Unknown &&
        retainedSlot < kTitleRuntimeSlotCount &&
        modules.moduleBases[retainedSlot])
    {
        return retainedTitle;
    }
    return GameTitle::None;
}
