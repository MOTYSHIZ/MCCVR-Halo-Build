#pragma once
// Which way the left stick walks: where the head looks, or where a controller points (movement_direction).
// Every title rotates the stick so that "forward" follows the head. A hand source keeps that rotation and adds
// the horizontal angle between the chosen controller's aim and the head, so all titles share one answer.
#include <cmath>
#include <cstdint>

namespace movement_direction
{
enum Source : int { Head = 0, LeftHand = 1, RightHand = 2 };

inline int Clamp(int source) noexcept { return source == LeftHand || source == RightHand ? source : Head; }

inline float WrapPi(float angle) noexcept
{
    constexpr float kPi = 3.14159265f;
    if (!std::isfinite(angle)) return 0.0f;
    angle = std::fmod(angle + kPi, 2.0f * kPi);
    if (angle < 0.0f) angle += 2.0f * kPi;
    return angle - kPi;
}

// The horizontal heading of an OpenXR orientation (x, y, z, w) the way the move rotation measures it:
// atan2(forward.x, -forward.z), so looking right is positive. False when the forward axis is within about
// 70 degrees of straight up or down (horizontal part under `minHorizontal`): a controller pointed at the
// floor has no heading to walk along.
inline bool ForwardYaw(const float q[4], float& yaw, float minHorizontal = 0.35f) noexcept
{
    const float x = q[0], y = q[1], z = q[2], w = q[3];
    const float fx = -2.0f * (w * y + x * z);
    const float fz = -(1.0f - 2.0f * (x * x + y * y));
    if (!std::isfinite(fx) || !std::isfinite(fz) || fx * fx + fz * fz < minHorizontal * minHorizontal)
        return false;
    yaw = std::atan2(fx, -fz);
    return std::isfinite(yaw);
}

// Remembers the chosen hand's last usable heading, so pointing the controller at the floor keeps walking the
// way it last pointed and a short tracking dropout does not snap the walk to the head. Held only while the
// hand is tracked; after `kLostHoldMs` untracked it falls back to the head. One caller thread (the input poll).
struct Tracker
{
    static constexpr uint64_t kLostHoldMs = 500;
    int source = Head;
    bool have = false;
    float handYaw = 0.0f;
    uint64_t seenMs = 0;

    // The angle to add to the head's heading (same sign as ForwardYaw). 0 for the head, or when there is no
    // usable hand heading.
    float Offset(int wanted, bool headValid, const float head[4], bool handValid, const float hand[4],
        uint64_t nowMs) noexcept
    {
        wanted = Clamp(wanted);
        if (wanted != source) { source = wanted; have = false; }
        if (source == Head || !headValid) return 0.0f;
        float yaw = 0.0f;
        if (handValid)
        {
            if (ForwardYaw(hand, yaw)) { handYaw = yaw; have = true; }
            if (have) seenMs = nowMs;
        }
        else if (have && (nowMs < seenMs || nowMs - seenMs > kLostHoldMs))
            have = false;
        float headYaw = 0.0f;
        if (!have || !ForwardYaw(head, headYaw, 0.001f)) return 0.0f;
        return WrapPi(handYaw - headYaw);
    }
};
}
