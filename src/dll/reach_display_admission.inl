// Shared by production Present/worker admission and its offline regression.
    struct ReachDisplayAdmission
    {
        TitleRuntimeAvailabilitySnapshot availability{};
        ReachModuleEpoch epoch{};
        GameTitle activeTitle = GameTitle::None;
        bool coherent = false;
        bool resident = false;
        bool selected = false;
    };

    bool SameReachAvailability(
        const TitleRuntimeAvailabilitySnapshot& left,
        const TitleRuntimeAvailabilitySnapshot& right) noexcept
    {
        return left.stable && right.stable &&
            left.availabilityMask == right.availabilityMask &&
            left.availabilitySetEpochMs == right.availabilitySetEpochMs &&
            left.revision == right.revision &&
            left.moduleBases == right.moduleBases;
    }

    bool ReadReachDisplayAdmission(
        ReachDisplayAdmission& admission) noexcept
    {
        admission = {};
        constexpr GameTitle title = GameTitle::HaloReach;
        constexpr size_t slot = TitleRuntimeSlotIndex(title);
        constexpr uint32_t bit = TitleRuntimeAvailabilityBit(title);
        static_assert(slot < kTitleRuntimeSlotCount);
        static_assert(bit != 0);

        const TitleRuntimeAvailabilitySnapshot before =
            TitleAdapter_GetAvailability();
        const GameTitle activeBefore = TitleAdapter_GetActiveTitle();
        const uint32_t generationBefore =
            TitleAdapter_GetGeneration(title);
        const TitleRuntimeAvailabilitySnapshot after =
            TitleAdapter_GetAvailability();
        const GameTitle activeAfter = TitleAdapter_GetActiveTitle();
        const uint32_t generationAfter =
            TitleAdapter_GetGeneration(title);
        if (!SameReachAvailability(before, after) ||
            activeBefore != activeAfter ||
            generationBefore != generationAfter)
        {
            return false;
        }

        admission.availability = after;
        admission.activeTitle = activeAfter;
        admission.coherent = true;
        admission.resident =
            (after.availabilityMask & bit) != 0 &&
            after.moduleBases[slot] != 0 && generationAfter != 0;
        if (admission.resident)
        {
            admission.epoch = {
                after.moduleBases[slot], generationAfter};
        }
        // The adapter resolves the active game even when MCC retains other
        // title modules. Requiring a one-bit module mask deadlocked Reach's
        // display proof after CE. Selection admits resource verification only;
        // exact native swapchain/device/epoch proofs still precede ownership.
        admission.selected = admission.resident && activeAfter == title;
        return true;
    }

    bool ReachSameDisplayAdmission(
        const ReachDisplayAdmission& left,
        const ReachDisplayAdmission& right) noexcept
    {
        return left.coherent && right.coherent && left.resident && right.resident &&
            left.selected && right.selected &&
            left.activeTitle == right.activeTitle &&
            left.availability.availabilityMask ==
                right.availability.availabilityMask &&
            left.availability.availabilitySetEpochMs ==
                right.availability.availabilitySetEpochMs &&
            ReachSameModuleEpoch(left.epoch, right.epoch);
    }
