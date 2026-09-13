#pragma once
#include <cstddef>
#include <cstdint>
#include "../common/haloce_render_logic.h"

// Planned stereo-core interface; no hook/admission implementation exists yet.
// haloce_native_bindings.cpp now implements loaded-image verification and the
// native camera rebuild adapter; prepared-handoff/transfer components are tested.
// CE will own its native camera bridge and remastered scene transaction only.
// Classic, weapon IK, melee, collision and HUD extraction remain separate.
bool HaloCE_Poll(uintptr_t base,size_t size,uint32_t generation,bool active) noexcept;
bool HaloCE_Armed() noexcept;
void HaloCE_Recenter() noexcept;
bool HaloCE_GetHalfFovs(uint64_t serial,float halfX[2],float halfY[2]) noexcept;

// Present publishes exact-frame OpenXR data and preallocates capture resources;
// CE render hooks only read snapshots and issue a validated GPU copy.
bool VR_HaloCEGetTracking(halo_ce::Tracking& tracking) noexcept;
bool VR_HaloCECaptureEye(int eye,uint64_t serial) noexcept;
void VR_HaloCEDropPair() noexcept;
