#pragma once
#include "../common/title_runtime_state.h"
#include <cstdint>
// Cold worker; independent optional hooks, never grants or revokes camera ownership.
void NativeReloadPolicy_Poll();
// Simulation-thread ownership check. The caller independently proves owner getter.
void* Game_ReloadPolicyWeapon(GameTitle title, uint32_t weapon);

// Called only from an existing admitted native simulation update with an
// independently verified local, on-foot dual pair. No XR-thread engine calls.
void NativeReloadPolicy_DualShakeTick(GameTitle title,uint32_t generation,
    uint32_t unit,const uint32_t weapons[2]) noexcept;

// Input-to-simulation handoff; returns one-shot logical weapon slot bits.
unsigned VR_DualWieldReloadTick(GameTitle title,uint32_t generation,uint32_t unit,
    const uint32_t weapons[2]) noexcept;

// Cold-worker input admission and clock diagnostics.
void VR_ReportDualWieldReloadInput(GameTitle title);
