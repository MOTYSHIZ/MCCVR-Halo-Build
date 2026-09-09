#pragma once
#include <atomic>
#include <cmath>
#include <cstdint>

struct DualWeaponAimSnapshot
{
    uint32_t generation = 0, unit = UINT32_MAX;
    uint32_t weapons[2]{UINT32_MAX, UINT32_MAX};
    uint64_t trackingEpoch = 0, sampleMs = 0;
    int64_t timeNs = 0;
    float positions[2][3]{}, directions[2][3]{};
};

inline bool DualWeaponAimFresh(const DualWeaponAimSnapshot& sample,
    uint32_t generation, uint32_t unit, uint64_t trackingEpoch,
    uint64_t nowMs, uint64_t installedAtMs, int64_t trackingTimeNs) noexcept
{
    return generation && sample.generation == generation &&
        unit != UINT32_MAX && sample.unit == unit &&
        trackingEpoch && sample.trackingEpoch == trackingEpoch &&
        sample.sampleMs >= installedAtMs && sample.sampleMs &&
        nowMs >= sample.sampleMs && nowMs - sample.sampleMs <= 100 &&
        sample.timeNs > 0 && trackingTimeNs >= sample.timeNs &&
        trackingTimeNs - sample.timeNs <= 100000000;
}

// Keep the engine's authoritative origin and converge its shot toward the
// selected controller ray at the shared reticle distance. Visual gun offsets
// are deliberately absent. Build into scratch so refusal never changes stock.
inline bool BuildIndependentWeaponDirection(const float origin[3],
    const float hand[3], const float forward[3], float distance,
    float result[3]) noexcept
{
    if (!origin || !hand || !forward || !result ||
        !std::isfinite(distance) || distance <= 0) return false;
    float forwardLength = 0;
    for (int i = 0; i < 3; ++i)
    {
        if (!std::isfinite(origin[i]) || !std::isfinite(hand[i]) ||
            !std::isfinite(forward[i])) return false;
        forwardLength += forward[i] * forward[i];
    }
    if (!std::isfinite(forwardLength) || forwardLength < 1e-8f) return false;
    const float gain = distance / std::sqrt(forwardLength);
    float ray[3]{}, length = 0;
    for (int i = 0; i < 3; ++i)
    {
        ray[i] = hand[i] + forward[i] * gain - origin[i];
        length += ray[i] * ray[i];
    }
    if (!std::isfinite(length) || length < 1e-8f) return false;
    length = std::sqrt(length);
    for (int i = 0; i < 3; ++i) result[i] = ray[i] / length;
    return true;
}

// Bounded, allocation-free publication. Payload fields are atomic too, so a
// rejected overlapping snapshot is still free of C++ data races. A busy writer
// drops its publication; engine and render callbacks never wait for one another.
class DualWeaponAimPublication
{
public:
    bool Publish(const DualWeaponAimSnapshot& value) noexcept
    {
        uint32_t version = sequence.load(std::memory_order_acquire);
        if ((version & 1) || !sequence.compare_exchange_strong(
                version, version + 1, std::memory_order_acq_rel)) return false;
        generation.store(value.generation, std::memory_order_relaxed);
        unit.store(value.unit, std::memory_order_relaxed);
        epoch.store(value.trackingEpoch, std::memory_order_relaxed);
        sampleMs.store(value.sampleMs, std::memory_order_relaxed);
        timeNs.store(value.timeNs, std::memory_order_relaxed);
        for (int slot = 0; slot < 2; ++slot)
        {
            weapons[slot].store(value.weapons[slot], std::memory_order_relaxed);
            for (int axis = 0; axis < 3; ++axis)
            {
                positions[slot][axis].store(value.positions[slot][axis], std::memory_order_relaxed);
                directions[slot][axis].store(value.directions[slot][axis], std::memory_order_relaxed);
            }
        }
        sequence.store(version + 2, std::memory_order_release);
        return true;
    }
    bool Read(DualWeaponAimSnapshot& value) const noexcept
    {
        const uint32_t version = sequence.load(std::memory_order_acquire);
        if (version & 1) return false;
        DualWeaponAimSnapshot sample{};
        sample.generation = generation.load(std::memory_order_relaxed);
        sample.unit = unit.load(std::memory_order_relaxed);
        sample.trackingEpoch = epoch.load(std::memory_order_relaxed);
        sample.sampleMs = sampleMs.load(std::memory_order_relaxed);
        sample.timeNs = timeNs.load(std::memory_order_relaxed);
        for (int slot = 0; slot < 2; ++slot)
        {
            sample.weapons[slot] = weapons[slot].load(std::memory_order_relaxed);
            for (int axis = 0; axis < 3; ++axis)
            {
                sample.positions[slot][axis] = positions[slot][axis].load(std::memory_order_relaxed);
                sample.directions[slot][axis] = directions[slot][axis].load(std::memory_order_relaxed);
            }
        }
        std::atomic_thread_fence(std::memory_order_acquire);
        if (sequence.load(std::memory_order_acquire) != version) return false;
        value = sample;
        return sample.generation != 0;
    }
private:
    std::atomic<uint32_t> sequence{0}, generation{0}, unit{UINT32_MAX};
    std::atomic<uint32_t> weapons[2]{UINT32_MAX, UINT32_MAX};
    std::atomic<uint64_t> epoch{0}, sampleMs{0};
    std::atomic<int64_t> timeNs{0};
    std::atomic<float> positions[2][3]{}, directions[2][3]{};
};
