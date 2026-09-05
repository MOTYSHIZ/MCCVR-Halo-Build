#pragma once

#include <cstddef>
#include <cstdint>

struct LegacyWeaponCollisionBounds
{
    uint32_t runtimeImportChecksum;
    float minimum[3];
    float maximum[3];
};

// Exact compression-position bounds exported from the official Gen3
// editing-kit first-person render models. Retail selects a record using the
// immutable runtime-import checksum at render_model+0x08. Unknown models fail
// open to hand-only collision; no dimensions are inferred from another title.

inline constexpr LegacyWeaponCollisionBounds kH3OdstWeaponCollisionBounds[]{
    {0x1C080C11u, {-0.0765114f,-0.0175f,-0.0924675f}, {0.362303f,0.0175f,0.0924675f}},
    {0x161A180Bu, {-0.0948338f,-0.0510653f,-0.288198f}, {0.0898477f,0.0511029f,0.379993f}},
    {0x16030911u, {-0.044123f,-0.044123f,-0.0396582f}, {0.044123f,0.044123f,0.0424905f}},
    {0x1315191Fu, {-0.0168503f,-0.0257963f,-0.0151019f}, {0.0544665f,0.0442221f,0.0671944f}},
    {0x14030708u, {-0.0180401f,-0.0169674f,-0.143224f}, {0.0158947f,0.0169674f,0.853957f}},
    {0x1A100F1Eu, {-0.0376933f,-0.00924871f,-0.036708f}, {0.105948f,0.00924871f,0.0501774f}},
    {0x170C0C09u, {-0.0553821f,-0.0372337f,-0.0500737f}, {0.117508f,0.0372337f,0.091716f}},
    {0x16060815u, {-0.0379337f,-0.00924871f,-0.0257782f}, {0.0642435f,0.00924871f,0.0537088f}},
    {0x10190A19u, {-0.0780779f,-0.0349575f,-0.0738431f}, {0.112074f,0.0349576f,0.104357f}},
    {0x150A000Cu, {-0.0262419f,-0.0115153f,-0.0422352f}, {0.0914929f,0.0115153f,0.0477655f}},
    {0x13120D11u, {-0.0943898f,-0.0137153f,-0.0178377f}, {0.211322f,0.0154403f,0.0758663f}},
    {0x18070710u, {-0.0943898f,-0.0137153f,-0.0178377f}, {0.211322f,0.0154403f,0.0758663f}},
    {0x12040E16u, {-0.128072f,-0.00963842f,-0.0119715f}, {0.252416f,0.0128121f,0.0920034f}},
    {0x1F0D1510u, {-0.11078f,-0.024458f,-0.0892794f}, {0.503498f,0.0317856f,0.045432f}},
    {0x13100C11u, {-0.115823f,-0.0143712f,-0.0609295f}, {0.300425f,0.0239281f,0.0732902f}},
    {0x16120C11u, {-0.11837f,-0.0310359f,-0.067564f}, {0.0968321f,0.0309668f,0.0553705f}},
    {0x191D0A13u, {-0.11837f,-0.0310359f,-0.067564f}, {0.0968321f,0.0309668f,0.0553705f}},
    {0x1E1D0514u, {-0.102519f,-0.0161916f,-0.0209284f}, {0.296369f,0.0208831f,0.0623893f}},
    {0x1F000B10u, {-0.102519f,-0.0161916f,-0.0209284f}, {0.296369f,0.0208831f,0.0623893f}},
    {0x171C0B10u, {-0.121123f,-0.0123869f,-0.0308718f}, {0.0953032f,0.0134303f,0.0501848f}},
    {0x1E130811u, {-0.121331f,-0.0123869f,-0.0308718f}, {0.175696f,0.0140356f,0.064151f}},
    {0x12081802u, {-0.126494f,-0.0430213f,-0.0328069f}, {0.474487f,0.0430213f,0.0880174f}},
    {0x1B010D0Cu, {-0.126494f,-0.0430213f,-0.0328069f}, {0.474487f,0.0430213f,0.0880174f}},
    {0x15130F16u, {-0.0640429f,-0.0373677f,-0.0421782f}, {0.204042f,0.0373677f,0.0503406f}},
    {0x11140D13u, {-0.0640429f,-0.0373677f,-0.0421782f}, {0.204042f,0.0373677f,0.0503406f}},
    {0x141B1915u, {-0.232017f,-0.0372964f,-0.0677009f}, {0.195082f,0.0377762f,0.13317f}},
    {0x151A0912u, {-0.282088f,-0.0406118f,-0.0331575f}, {0.190359f,0.0294055f,0.102279f}},
    {0x1E010110u, {-0.282088f,-0.0406118f,-0.0331575f}, {0.190359f,0.0294055f,0.102279f}},
    {0x1F100F1Fu, {-0.244793f,-0.0191142f,-0.0191833f}, {0.144083f,0.0229988f,0.111498f}},
    {0x1C050612u, {-0.244793f,-0.0191142f,-0.0191833f}, {0.144083f,0.0229988f,0.111498f}},
    {0x17141007u, {-0.267823f,-0.0289519f,-0.113778f}, {0.366802f,0.0289519f,0.127649f}},
    {0x14131A0Bu, {-0.02955f,-0.0889765f,-0.0300838f}, {0.178999f,0.0425785f,0.128329f}},
};

inline constexpr LegacyWeaponCollisionBounds kReachWeaponCollisionBounds[]{
    {0x140C1111u, {-0.0536084f,-0.0162417f,-0.0841924f}, {0.362337f,0.0162413f,0.0841924f}},
    {0x10110A0Du, {-0.0938173f,-0.0541517f,-0.28785f}, {0.108661f,0.0541099f,0.380568f}},
    {0x16120907u, {0.0147443f,-0.241468f,-0.245904f}, {0.0460106f,0.247491f,0.245904f}},
    {0x13140A07u, {-0.0933467f,-0.0496707f,-0.0685199f}, {0.16885f,-0.0018972f,0.0759779f}},
    {0x171E1214u, {-0.0419121f,-0.0419121f,-0.0392606f}, {0.0419121f,0.0419121f,0.0408384f}},
    {0x17081411u, {-0.0739181f,-0.0857473f,-0.16455f}, {0.476885f,0.085665f,0.0220981f}},
    {0x18111314u, {-0.0241183f,-0.0795236f,-0.305459f}, {0.0498177f,0.0795236f,0.75181f}},
    {0x1615080Du, {-0.300076f,-0.0498177f,-0.534983f}, {0.0795236f,0.0241184f,0.522285f}},
    {0x141E1314u, {-0.0382651f,-0.0282177f,-0.035946f}, {0.0382651f,0.0284208f,0.035946f}},
    {0x15150C0Cu, {-0.0404585f,-0.00943417f,-0.028129f}, {0.0758992f,0.00943409f,0.0541266f}},
    {0x11091511u, {-0.127919f,-0.0377778f,-0.0576258f}, {0.115972f,0.0357561f,0.11633f}},
    {0x100B1711u, {-0.0192444f,-0.00882073f,-0.0412482f}, {0.101929f,0.0132388f,0.0568027f}},
    {0x110B151Eu, {-0.035497f,-0.039685f,-0.0174027f}, {0.0696364f,0.0233503f,0.0696735f}},
    {0x150D1012u, {-0.0974775f,-0.0144884f,-0.01504f}, {0.218779f,0.0144884f,0.0857201f}},
    {0x11150101u, {-0.108134f,-0.0286602f,-0.0485355f}, {0.229722f,0.0307763f,0.0747699f}},
    {0x15011515u, {-0.12623f,-0.00981186f,-0.0126045f}, {0.231383f,0.0160766f,0.0909794f}},
    {0x110D171Eu, {-0.102504f,-0.029185f,-0.0469046f}, {0.373775f,0.0363635f,0.104883f}},
    {0x120C1715u, {-0.0247994f,-0.0309295f,-0.0113817f}, {0.229666f,0.0260579f,0.097155f}},
    {0x1615110Eu, {-0.049806f,-0.0396061f,-0.0207088f}, {0.365628f,0.0393184f,0.102027f}},
    {0x1208171Eu, {-0.0934478f,-0.0157771f,-0.0372873f}, {0.226053f,0.0158437f,0.077406f}},
    {0x1409161Eu, {-0.109718f,-0.025196f,-0.0510984f}, {0.0941085f,0.025196f,0.0713384f}},
    {0x180C1511u, {-0.102288f,-0.0145304f,-0.0128154f}, {0.28451f,0.0251537f,0.0749696f}},
    {0x110B141Eu, {-0.098883f,-0.0354552f,-0.0164927f}, {0.44592f,0.0208059f,0.0951575f}},
    {0x13140A0Du, {-0.0384029f,-0.0372184f,-0.0213941f}, {0.229682f,0.0372185f,0.0744172f}},
    {0x160D101Eu, {-0.228253f,-0.0294929f,-0.0466448f}, {0.200778f,0.0311599f,0.133229f}},
    {0x100A141Eu, {-0.188145f,-0.0286847f,-0.0341205f}, {0.168831f,0.0334794f,0.121627f}},
    {0x19110C0Cu, {-0.282039f,-0.0406108f,-0.0396821f}, {0.19145f,0.0419464f,0.113503f}},
    {0x1606141Eu, {-0.261474f,-0.0219779f,-0.0180503f}, {0.135797f,0.0356302f,0.113711f}},
    {0x11130A0Du, {-0.032157f,-0.111912f,-0.126035f}, {0.493423f,0.052683f,0.0396892f}},
};

template <size_t Count>
inline const LegacyWeaponCollisionBounds* LegacyFindWeaponCollisionBounds(
    const LegacyWeaponCollisionBounds (&catalog)[Count],
    uint32_t checksum) noexcept
{
    for (const LegacyWeaponCollisionBounds& bounds : catalog)
        if (bounds.runtimeImportChecksum == checksum)
            return &bounds;
    return nullptr;
}

// A final-palette callback may present the combined first-person body before
// it presents the held render model.  Retain only a very recent, generation-
// exact authored weapon identity so the next combined publication can use the
// model that the renderer actually submitted.  An unknown/new model naturally
// expires to hand-only collision instead of inheriting stale dimensions.
inline bool LegacyWeaponCollisionCacheCanSupply(
    uint64_t nowMs, uint64_t observedAtMs, uint32_t generation,
    uint32_t observedGeneration, uint64_t maximumAgeMs = 150) noexcept
{
    return generation != 0 && generation == observedGeneration &&
        observedAtMs != 0 && observedAtMs <= nowMs &&
        nowMs - observedAtMs <= maximumAgeMs;
}

inline bool LegacyMappedWeaponRootIsUsable(
    int32_t mappedRoot, int32_t sourceCount, uintptr_t observedSource,
    uintptr_t currentSource) noexcept
{
    return observedSource != 0 && observedSource == currentSource &&
        sourceCount > 0 && sourceCount <= 64 && mappedRoot >= 0 &&
        mappedRoot < sourceCount;
}
