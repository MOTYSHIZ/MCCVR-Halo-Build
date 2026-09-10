#pragma once
#include "halo4_render_logic.h"

// Pure VR-side transform arithmetic. Adapters supply their own proven wrist
// indices and hand-only masks; no engine offset or anatomical layout is shared.
// Halo4FloatingTransform is a value type here, never a view of game memory.
template <size_t Capacity>
bool RouteLeftHandedFloatingPalette(
    Halo4FloatingTransform (&palette)[Capacity], size_t count,
    int rightWrist, uint64_t rightHandMask, int leftWrist, uint64_t leftHandMask,
    const Halo4FloatingTransform& root,
    const Halo4FloatingTransform& primaryCarrier,
    const Halo4FloatingTransform& supportCarrier) noexcept
{
    if (!count || count > Capacity || rightWrist < 0 || leftWrist < 0 ||
        rightWrist >= 64 || leftWrist >= 64 || size_t(rightWrist) >= count ||
        size_t(leftWrist) >= count || (rightHandMask & leftHandMask) ||
        !(rightHandMask & (uint64_t{1} << rightWrist)) ||
        !(leftHandMask & (uint64_t{1} << leftWrist)) ||
        (count < 64 && ((rightHandMask | leftHandMask) >> count))) return false;
    Halo4FloatingTransform right{}, left{}, delta[2]{}, inverseRoot{};
    if (!Halo4ComposeFloatingTransforms(root, palette[rightWrist], right) ||
        !Halo4ComposeFloatingTransforms(root, palette[leftWrist], left) ||
        !Halo4InvertFloatingTransform(root, inverseRoot)) return false;
    const auto oldRight = right, oldLeft = left;
    if (!Halo4RouteLeftHandedWristTargets(primaryCarrier, supportCarrier, right, left) ||
        !Halo4BuildFloatingWorldDelta(right, oldRight, delta[1]) ||
        !Halo4BuildFloatingWorldDelta(left, oldLeft, delta[0])) return false;
    for (auto& d : delta)
    {
        Halo4FloatingTransform deltaRoot{}, local{};
        if (!Halo4ComposeFloatingTransforms(d, root, deltaRoot) ||
            !Halo4ComposeFloatingTransforms(inverseRoot, deltaRoot, local)) return false;
        d = local;
    }
    Halo4FloatingTransform staged[Capacity];
    std::memcpy(staged, palette, count * sizeof(*palette));
    for (size_t node = 0; node < count && node < 64; ++node)
    {
        const uint64_t bit = uint64_t{1} << node;
        const int hand = (leftHandMask & bit) ? 0 : (rightHandMask & bit) ? 1 : -1;
        if (hand >= 0 &&
            !Halo4ComposeFloatingTransforms(delta[hand], palette[node], staged[node]))
            return false;
    }
    std::memcpy(palette, staged, count * sizeof(*palette));
    return true;
}
