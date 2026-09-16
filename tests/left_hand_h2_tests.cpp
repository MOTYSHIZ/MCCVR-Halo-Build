#include "../src/common/halo2_render_logic.h"
#include <cstdio>
#include <cstring>
#include <initializer_list>
#include <limits>

namespace
{
unsigned checks = 0, failures = 0;
void Check(bool condition, const char* message)
{
    ++checks;
    if (!condition) { ++failures; std::printf("FAIL: %s\n", message); }
}
bool Near(float a, float b) { return std::fabs(a - b) < 0.0003f; }
bool Same(const Halo2FirstPersonTransform& a, const Halo2FirstPersonTransform& b)
{
    if (!Near(a.scale, b.scale)) return false;
    for (int axis = 0; axis < 3; ++axis)
        if (!Near(a.translation[axis], b.translation[axis])) return false;
    for (int value = 0; value < 9; ++value)
        if (!Near(a.rotation[value], b.rotation[value])) return false;
    return true;
}
Halo2FirstPersonTransform Transform(float x, float y, float z, float yaw, float scale = 1)
{
    Halo2FirstPersonTransform out{};
    out.scale = scale; out.translation[0] = x; out.translation[1] = y; out.translation[2] = z;
    const float q[4]{0, 0, std::sin(yaw * 0.5f), std::cos(yaw * 0.5f)};
    Halo2QuaternionToFirstPersonBasis(q, out.rotation);
    return out;
}
Halo2FirstPersonTransform Read(const float* nodes, unsigned node)
{
    Halo2FirstPersonTransform out{};
    Check(Halo2ReadFirstPersonTransform(nodes + node * 13, out), "published node remains valid");
    return out;
}
Halo2FirstPersonTransform Palm(const float* nodes, unsigned node,
    Halo2FirstPersonRigKind rig, bool left)
{
    Halo2FirstPersonTransform marker{}, palm{};
    Check(Halo2AnatomicalGripMarker(rig, left, marker) &&
        Halo2ComposeFirstPersonTransforms(Read(nodes, node), marker, palm), "semantic palm is composable");
    return palm;
}
constexpr unsigned count = 6;
constexpr int32_t remap[count]{0, 1, 2, 3, 4, 5};
constexpr int32_t secondaryRemap[count]{-1, 1, -1, 3, -1, -1};
Halo2FirstPersonArmBinding Binding(Halo2FirstPersonRigKind rig)
{
    Halo2FirstPersonArmBinding out{};
    out.valid = true; out.count = count; out.leftWrist = 1; out.rightWrist = 2;
    out.leftSubtree = (1ull << 1) | (1ull << 3);
    out.rightSubtree = (1ull << 2) | (1ull << 4);
    out.leftDirectChildren = 1ull << 3; out.rigKind = rig;
    return out;
}
struct Packets
{
    float hands[count * 13]{}, gun[2 * 13]{}, secondary[2 * 13]{};
    Packets()
    {
        const Halo2FirstPersonTransform nodes[count]{
            Transform(0, 0, 0, 0), Transform(.4f, .12f, -.03f, -.4f),
            Transform(.35f, -.09f, -.02f, .6f), Transform(.44f, .10f, -.01f, -.2f),
            Transform(.39f, -.06f, .01f, .5f), Transform(.1f, 0, -.1f, 0)};
        for (unsigned i = 0; i < count; ++i) Halo2WriteFirstPersonTransform(nodes[i], hands + 13 * i);
        for (unsigned i = 0; i < 2; ++i)
        {
            Halo2WriteFirstPersonTransform(Transform(.38f + .2f * i, -.06f, .04f, .3f), gun + 13 * i);
            Halo2WriteFirstPersonTransform(Transform(.42f + .2f * i, .15f, -.03f, -.7f), secondary + 13 * i);
        }
    }
};
}

int main()
{
    Halo2FirstPersonTransform chiefLeft{}, chiefRight{}, eliteLeft{}, eliteRight{};
    Check(Halo2AnatomicalGripMarker(Halo2FirstPersonRigKind::MasterChief, true, chiefLeft) &&
        Halo2AnatomicalGripMarker(Halo2FirstPersonRigKind::MasterChief, false, chiefRight) &&
        Near(chiefLeft.translation[1], -.008561f) && Near(chiefRight.translation[1], .011516f),
        "independent H2EK Chief markers preserve opposite lateral grip offsets");
    auto invalidMarker = Transform(1, 2, 3, .4f);
    const auto markerBefore = invalidMarker;
    Check(!Halo2AnatomicalGripMarker(Halo2FirstPersonRigKind::Unknown, true, invalidMarker) &&
        std::memcmp(&invalidMarker, &markerBefore, sizeof(invalidMarker)) == 0,
        "unidentified rig cannot publish a guessed grip marker");
    Check(Halo2AnatomicalGripMarker(Halo2FirstPersonRigKind::Elite, true, eliteLeft) &&
        Halo2AnatomicalGripMarker(Halo2FirstPersonRigKind::Elite, false, eliteRight),
        "Elite native left and deliberately derived right markers are available");
    Check(Near(eliteLeft.translation[0], eliteRight.translation[0]) &&
        Near(eliteLeft.translation[1], -eliteRight.translation[1]) &&
        Near(eliteLeft.translation[2], eliteRight.translation[2]), "Elite reflection uses its proven local Y plane");
    for (int col = 0; col < 3; ++col)
        for (int row = 0; row < 3; ++row)
            Check(Near(eliteRight.rotation[3 * col + row], eliteLeft.rotation[3 * col + row] *
                (((col == 1) != (row == 1)) ? -1.0f : 1.0f)), "Elite frame reflection keeps a proper rotation");

    for (auto rig : {Halo2FirstPersonRigKind::MasterChief, Halo2FirstPersonRigKind::Elite})
    for (int renderer = 0; renderer < 2; ++renderer)
    for (bool dual : {false, true})
    for (bool twoHand : {false, true})
    for (float scale : {0.5f, 1.0f, 2.0f})
    {
        const auto binding = Binding(rig);
        Halo2CameraBasis primary{}, support{}, compose{};
        primary.position[0] = -.35f; primary.position[1] = .5f; primary.position[2] = -.1f;
        primary.forward[1] = 1; primary.up[2] = 1;
        support.position[0] = .3f; support.position[1] = .2f; support.position[2] = -.2f;
        support.forward[0] = -1; support.up[2] = 1;
        compose.forward[renderer] = 1; compose.up[2] = 1;
        Packets native{}, corrected{};
        auto own = [&](Packets& packets, bool alignment) {
            Halo2FinalPacketOwnershipResult out{};
            return dual ? Halo2OwnDualFirstPersonPackets(packets.hands, count, remap, binding,
                packets.gun, 2, secondaryRemap, binding, packets.secondary, 2, compose,
                primary, support, scale, .7f * scale, 3.048f, out, alignment) :
                Halo2OwnFinalFirstPersonPackets(packets.hands, count, remap, binding,
                    packets.gun, 2, compose, primary, support, twoHand,
                    scale, .7f * scale, 3.048f, out, alignment);
        };
        Check(own(native, false) && own(corrected, true), "single/dual packet math accepts Chief/Elite under distinct compose bases");
        Check(std::memcmp(native.gun, corrected.gun, sizeof(native.gun)) == 0 &&
            std::memcmp(native.secondary, corrected.secondary, sizeof(native.secondary)) == 0,
            "opt-in hand alignment leaves both gun packets byte-identical");
        Check(Same(Palm(corrected.hands, 1, rig, true), Palm(native.hands, 2, rig, false)) &&
            Same(Palm(corrected.hands, 2, rig, false), Palm(native.hands, 1, rig, true)),
            "actual palms exchange complete solved grips across rotations and unequal scales");
        const auto leftWrist = Read(corrected.hands, 1), oldRight = Read(native.hands, 2);
        float gapSquared = 0;
        for (int axis = 0; axis < 3; ++axis)
            gapSquared += std::pow(leftWrist.translation[axis] - oldRight.translation[axis], 2.0f);
        Check(gapSquared > 0.0000001f, "palm correction does not repeat the displaced-wrist coincidence defect");
        Check(Near(leftWrist.scale, oldRight.scale), "role-specific hand scale survives anatomical routing");
        const auto before = corrected;
        const int32_t duplicate[count]{0, 1, 2, 1, 4, 5};
        Check(!Halo2RouteLeftHandedPacketHands(corrected.hands, count, duplicate, binding,
            duplicate, binding, primary, support) && std::memcmp(&before, &corrected, sizeof(before)) == 0,
            "ambiguous wrist map leaves every packet untouched");
        corrected = native;
        corrected.hands[4 * 13 + 10] = std::numeric_limits<float>::quiet_NaN();
        const auto invalid = corrected;
        Check(!Halo2RouteLeftHandedPacketHands(corrected.hands, count, remap, binding,
            dual ? secondaryRemap : remap, binding, primary, support) &&
            std::memcmp(&invalid, &corrected, sizeof(invalid)) == 0,
            "late invalid finger rejects atomically after valid wrist staging");
    }
    std::printf("Halo 2 left-hand alignment: %u checks, %u failures\n", checks, failures);
    return failures ? 1 : 0;
}
