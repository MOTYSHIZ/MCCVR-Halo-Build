#pragma once
#include "haloce_eye_cache.h"

// CE Anniversary builds two views before culling. No whole-frame replay.
// Classic, weapon IK, melee, collision and HUD extraction remain separate.
bool HaloCE_Poll(uintptr_t base,size_t size,uint32_t generation,bool active) noexcept;
bool HaloCE_Armed() noexcept;
void HaloCE_Recenter() noexcept;
void HaloCE_PublishTracking(const halo_ce::Tracking& tracking,bool enabled) noexcept;
void HaloCE_PresentResources(ID3D11Device* device,ID3D11DeviceContext* context) noexcept;
bool HaloCE_AcquirePair(ID3D11DeviceContext* context,uint64_t currentSerial,
    uint64_t spaceEpoch,halo_ce::EyeCache::Completed& pair) noexcept;
void HaloCE_ReleasePair(uint64_t borrowId) noexcept;
bool HaloCE_OwnsPresentation() noexcept;
// Successful creation owns this texture, even before CE loads. Metadata only.
void HaloCE_RecordTextureCreated(ID3D11Texture2D* texture,
    const D3D11_TEXTURE2D_DESC& descriptor) noexcept;
