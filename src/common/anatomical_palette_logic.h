#pragma once
#include "halo4_render_logic.h"

struct AnatomicalPalmMarkers
{
    Halo4FloatingTransform right{}, left{};
    int rightNode = -1, leftNode = -1;
};

inline bool BuildAnatomicalPalmMarker(const float position[3], const float quaternion[4],
    Halo4FloatingTransform& marker) noexcept
{
    Halo4FloatingTransform result{};
    if (!Halo4QuaternionToBlamBasis(quaternion, result.rotation)) return false;
    std::memcpy(result.translation, position, sizeof(result.translation));
    if (!Halo4FloatingTransformValid(result)) return false;
    marker = result;
    return true;
}

// A marker's authored scale is its editor display radius (zero in H3/ODST),
// not skeletal scale. Its rigid wrist-local pose is the semantic grip frame.
// Values below are independently exported from each official title's FP tag;
// checksum + node count select that exact model, never a cross-title rig guess.
// See docs/LEFT-HAND-SHARED-2026-09-15.md.
inline bool Halo3AnatomicalPalmMarkers(uint32_t checksum, int count,
    AnatomicalPalmMarkers& output) noexcept
{
    AnatomicalPalmMarkers result{};
    result.leftNode = 5; result.rightNode = 6;
    if (checksum == 504041493u && count == 37) // masterchief/fp
    {
        const float lp[]{.0227497f,-.0085608f,.000395799f};
        const float lq[]{.0160779f,.0736134f,.0850798f,-.993521f};
        const float rp[]{.0227475f,.0115156f,.000395675f};
        const float rq[]{.00894828f,.0748107f,-.0105367f,-.997102f};
        if (!BuildAnatomicalPalmMarker(lp,lq,result.left) ||
            !BuildAnatomicalPalmMarker(rp,rq,result.right)) return false;
    }
    else if ((checksum == 269159697u || checksum == 268439051u) && count == 31)
    {
        const float lp[]{.0330884f,-.00931482f,.000442446f};
        const float lq[]{-.00185394f,.00150134f,-.0018332f,-.999995f};
        const float rp[]{.0330884f,.0030534f,.000442446f};
        if (!BuildAnatomicalPalmMarker(lp,lq,result.left)) return false;
        if (checksum == 269159697u) // elite/fp_arms has both native markers
        {
            if (!BuildAnatomicalPalmMarker(rp,lq,result.right)) return false;
        }
        else // dervish/fp lacks right: its OWN wrist/digit pairs prove local Y
        {
            const float mirroredP[]{lp[0],-lp[1],lp[2]};
            const float mirroredQ[]{-lq[0],lq[1],-lq[2],lq[3]};
            if (!BuildAnatomicalPalmMarker(mirroredP,mirroredQ,result.right)) return false;
        }
    }
    else return false;
    output = result;
    return true;
}

inline bool OdstAnatomicalPalmMarkers(uint32_t checksum, int count,
    AnatomicalPalmMarkers& output) noexcept
{
    if ((checksum != 286525724u && checksum != 403178001u) || count != 37)
        return false; // odst_recon/fp and odst_oni_op/fp, independently exported
    AnatomicalPalmMarkers result{};
    result.leftNode = 5; result.rightNode = 6;
    const float lp[]{.0227497f,-.0085608f,.000395799f};
    const float lq[]{.0160779f,.0736134f,.08508f,-.993521f};
    const float rp[]{.0227475f,.0115156f,.000395675f};
    const float rq[]{.00894813f,.0748107f,-.0105366f,-.997102f};
    if (!BuildAnatomicalPalmMarker(lp,lq,result.left) ||
        !BuildAnatomicalPalmMarker(rp,rq,result.right)) return false;
    output = result;
    return true;
}

inline bool ReachAnatomicalPalmMarkers(uint32_t checksum, int count,
    AnatomicalPalmMarkers& output) noexcept
{
    AnatomicalPalmMarkers result{};
    if (checksum == 404622103u && count == 47) // spartans/fp
    {
        result.leftNode = 14; result.rightNode = 11;
        const float rp[]{.0227475f,.0115156f,.000395678f};
        const float rq[]{-.00894813f,-.0748107f,.0105367f,.997102f};
        if (!BuildAnatomicalPalmMarker(rp,rq,result.right)) return false;
        // Native left_hand is identity directly on l_hand.
    }
    else if (checksum == 419566353u && count == 41) // elite/fp
    {
        result.leftNode = 13; result.rightNode = 14;
        const float lp[]{.0330885f,-.00931554f,.000442443f};
        const float lq[]{.00185393f,-.00150127f,.00183321f,.999995f};
        // No right marker exists. This tag's own bilateral wrist/digit
        // evidence derives the counterpart of left_hand_elite in local Y.
        const float rp[]{lp[0],-lp[1],lp[2]};
        const float rq[]{-lq[0],lq[1],-lq[2],lq[3]};
        if (!BuildAnatomicalPalmMarker(lp,lq,result.left) ||
            !BuildAnatomicalPalmMarker(rp,rq,result.right)) return false;
    }
    else return false;
    output = result;
    return true;
}

inline bool Halo4AnatomicalPalmMarkers(AnatomicalPalmMarkers& output) noexcept
{
    AnatomicalPalmMarkers result{};
    result.leftNode = 37; result.rightNode = 29;
    const float lp[]{0,0,0}; // identity child b_l_hand_marker_offset, node 54
    const float lq[]{-.706223f,.701140f,-.0353406f,.0916652f};
    const float rp[]{-.0142352f,.0211017f,.00151397f};
    const float rq[]{.0610816f,-.0990494f,.751709f,.649147f};
    if (!BuildAnatomicalPalmMarker(lp,lq,result.left) ||
        !BuildAnatomicalPalmMarker(rp,rq,result.right)) return false;
    output = result;
    return true;
}

// Move the anatomical palms to the two already-solved role grips. A wrist
// origin is not a palm: copying it loses the native lateral grip offsets.
// Consuming the final wrists also preserves exactly one gun calibration and
// the authored rigid support grip, without mixing in raw controller frames.
inline bool RouteLeftHandedPalmTargets(
    const Halo4FloatingTransform& rightPalmLocal,
    const Halo4FloatingTransform& leftPalmLocal,
    Halo4FloatingTransform& anatomicalRight,
    Halo4FloatingTransform& anatomicalLeft) noexcept
{
    Halo4FloatingTransform inverseRight{}, inverseLeft{}, primaryGrip{}, supportGrip{};
    Halo4FloatingTransform desiredRight{}, desiredLeft{};
    if (rightPalmLocal.scale != 1.0f || leftPalmLocal.scale != 1.0f ||
        !Halo4InvertFloatingTransform(rightPalmLocal,inverseRight) ||
        !Halo4InvertFloatingTransform(leftPalmLocal,inverseLeft) ||
        !Halo4ComposeFloatingTransforms(anatomicalRight,rightPalmLocal,primaryGrip) ||
        !Halo4ComposeFloatingTransforms(anatomicalLeft,leftPalmLocal,supportGrip) ||
        !Halo4ComposeFloatingTransforms(primaryGrip,inverseLeft,desiredLeft) ||
        !Halo4ComposeFloatingTransforms(supportGrip,inverseRight,desiredRight)) return false;
    anatomicalRight = desiredRight;
    anatomicalLeft = desiredLeft;
    return true;
}

// Pure VR-side transform arithmetic. Adapters supply their own proven wrist
// indices and hand-only masks; no engine offset or anatomical layout is shared.
// Halo4FloatingTransform is a value type here, never a view of game memory.
template <size_t Capacity>
bool RouteLeftHandedFloatingPalette(
    Halo4FloatingTransform (&palette)[Capacity], size_t count,
    int rightWrist, uint64_t rightHandMask, int leftWrist, uint64_t leftHandMask,
    const Halo4FloatingTransform& root,
    const Halo4FloatingTransform& rightPalmLocal,
    const Halo4FloatingTransform& leftPalmLocal) noexcept
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
    if (!RouteLeftHandedPalmTargets(rightPalmLocal, leftPalmLocal, right, left) ||
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
