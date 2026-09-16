#pragma once

#include "runtime_types.h"

constexpr bool ManualVrRecoverySupported(GameTitle title) noexcept
{
    return title >= GameTitle::Halo3 && title <= GameTitle::Halo2;
}

// One atomic word keeps the title and its module generation inseparable.
constexpr uint64_t ManualVrRecoveryToken(GameTitle title, uint32_t generation) noexcept
{
    return ManualVrRecoverySupported(title) && generation
        ? (uint64_t(generation) << 32) | uint32_t(title) : 0;
}
constexpr GameTitle ManualVrRecoveryTitle(uint64_t token) noexcept
{ return static_cast<GameTitle>(uint32_t(token)); }
constexpr uint32_t ManualVrRecoveryGeneration(uint64_t token) noexcept
{ return uint32_t(token >> 32); }
constexpr bool ManualVrRecoveryMatches(uint64_t token, GameTitle title,
    uint32_t generation) noexcept
{ return token && token == ManualVrRecoveryToken(title, generation); }

enum class ManualVrRecoveryPoll { None, Waiting, Retired };

// Called only by a title's management worker. A failed retirement stays pending
// and prevents installation; a stale request cannot retire a different epoch.
// Native binding/load/tracking proofs still run after a successful retirement.
template<class Retire, class Reset>
ManualVrRecoveryPoll PollManualVrRecovery(uint32_t& requestedGeneration,
    uint32_t generation, bool activeAndRuntimeAvailable, bool cleanupAllowed,
    Retire retire, Reset reset)
{
    if (!requestedGeneration) return ManualVrRecoveryPoll::None;
    if (requestedGeneration != generation || !activeAndRuntimeAvailable)
    {
        requestedGeneration = 0;
        return ManualVrRecoveryPoll::None;
    }
    if (!cleanupAllowed) return ManualVrRecoveryPoll::Waiting;
    if (!retire()) return ManualVrRecoveryPoll::Waiting;
    reset();
    requestedGeneration = 0;
    return ManualVrRecoveryPoll::Retired;
}
