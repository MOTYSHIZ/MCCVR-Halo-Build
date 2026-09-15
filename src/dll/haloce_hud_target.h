#pragma once
#include <d3d11.h>
#include <array>
#include <cstdint>

struct CeHudTargetSnapshot
{
    uintptr_t moduleBase{},backend{};
    ID3D11DeviceContext* context{};
    uint32_t count{};
    // Native-owned identities only. The adapter never AddRefs or Releases them.
    ID3D11RenderTargetView* rtvs[4]{};
    ID3D11DepthStencilView* dsv{};
    std::array<uint8_t,0x48> descriptor{};
    uintptr_t wrappers[5]{},surfaces[5]{},resources[5]{};
};
enum class CeHudTargetRestoreResult : uint8_t { Unchanged,Changed,Unavailable };
// Cold caller verifies contract::hud_target and retains the module across use.
// Render owner/title/generation validation remains the caller's responsibility.
bool HaloCEHudTarget_Read(uintptr_t moduleBase,ID3D11DeviceContext* expectedContext,
    CeHudTargetSnapshot& out) noexcept;
CeHudTargetRestoreResult HaloCEHudTarget_Restore(const CeHudTargetSnapshot&) noexcept;
