#include <cstdio>
#include "../src/common/title_runtime_state.h"
#include "../src/common/reach_render_logic.h"

namespace
{
TitleRuntimeAvailabilitySnapshot before{}, after{};
GameTitle activeBefore = GameTitle::HaloReach, activeAfter = GameTitle::HaloReach;
uint32_t generationBefore = 3, generationAfter = 3;
unsigned reads = 0, activeReads = 0, generationReads = 0, failures = 0, checks = 0;
TitleRuntimeAvailabilitySnapshot TitleAdapter_GetAvailability()
{ return reads++ ? after : before; }
GameTitle TitleAdapter_GetActiveTitle()
{ return activeReads++ ? activeAfter : activeBefore; }
uint32_t TitleAdapter_GetGeneration(GameTitle)
{ return generationReads++ ? generationAfter : generationBefore; }

#include "../src/dll/reach_display_admission.inl"

void Check(bool value, const char* name)
{
    ++checks;
    if (!value) { ++failures; std::printf("FAIL: %s\n", name); }
}
void Reset(uint32_t mask = TitleRuntimeAvailabilityBit(GameTitle::HaloReach))
{
    before = {};
    before.stable = true;
    before.availabilityMask = mask;
    before.availabilitySetEpochMs = 100;
    before.revision = 4;
    for (size_t i = 0; i < kTitleRuntimeSlotCount; ++i)
        if (mask & (1u << i)) before.moduleBases[i] = 0x10000000 + i * 0x10000000;
    after = before;
    activeBefore = activeAfter = GameTitle::HaloReach;
    generationBefore = generationAfter = 3;
    reads = activeReads = generationReads = 0;
}
ReachDisplayAdmission Read()
{
    reads = activeReads = generationReads = 0;
    ReachDisplayAdmission result{};
    ReadReachDisplayAdmission(result);
    return result;
}
}

int main()
{
    constexpr uint32_t reach = TitleRuntimeAvailabilityBit(GameTitle::HaloReach);
    // Every resident-title combination includes CE retained after either graphics mode.
    for (uint32_t mask = 0; mask <= kTitleRuntimeAvailabilityMask; ++mask)
    {
        Reset(mask);
        auto result = Read();
        Check(result.selected == bool(mask & reach), "selected Reach supports every resident-module combination");
        for (size_t i = 0; i < kTitleRuntimeSlotCount; ++i)
        {
            const GameTitle title = TitleRuntimeSlotTitle(i);
            if (title == GameTitle::HaloReach) continue;
            activeBefore = activeAfter = title;
            Check(!Read().selected, "another selected title cannot publish Reach resources");
        }
        activeBefore = activeAfter = GameTitle::Unknown;
        Check(!Read().selected, "ambiguous selection stays blocked");
        activeBefore = activeAfter = GameTitle::None;
        Check(!Read().selected, "shell stays blocked");
    }
    for (int change = 0; change < 8; ++change)
    {
        Reset(kTitleRuntimeAvailabilityMask);
        switch (change)
        {
        case 0: after.stable = false; break;
        case 1: ++after.revision; break;
        case 2: ++after.availabilitySetEpochMs; break;
        case 3: after.availabilityMask ^= 1; break;
        case 4: ++after.moduleBases[0]; break;
        case 5: activeAfter = GameTitle::HaloCE; break;
        case 6: ++generationAfter; break;
        case 7: before.stable = false; break;
        }
        Check(!Read().coherent, "concurrent title/module/generation change rejects admission");
    }
    Reset(); generationBefore = generationAfter = 0;
    Check(!Read().selected, "zero generation blocked");
    Reset(); before.moduleBases[2] = after.moduleBases[2] = 0;
    Check(!Read().selected, "missing Reach module blocked");
    Reset(kTitleRuntimeAvailabilityMask);
    const auto first = Read();
    Check(ReachSameDisplayAdmission(first, Read()), "stable current resource admission");
    ++before.availabilitySetEpochMs; after = before;
    Check(!ReachSameDisplayAdmission(first, Read()), "old availability epoch cannot publish");
    Reset(kTitleRuntimeAvailabilityMask); ++generationBefore; generationAfter = generationBefore;
    Check(!ReachSameDisplayAdmission(first, Read()), "old generation cannot publish");
    Reset(kTitleRuntimeAvailabilityMask); ++before.moduleBases[2]; after = before;
    Check(!ReachSameDisplayAdmission(first, Read()), "reloaded module cannot publish");
    std::printf("Reach production display admission: %u checks, %u failures\n", checks, failures);
    return failures ? 1 : 0;
}
