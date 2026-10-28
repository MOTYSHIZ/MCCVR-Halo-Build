#pragma once
#include "../common/title_runtime_state.h"
#include <cstdint>
// Cold worker; independent optional hooks, never grants or revokes camera ownership.
void NativeReloadPolicy_Poll();
// Simulation-thread ownership check. The caller independently proves owner getter.
void* Game_ReloadPolicyWeapon(GameTitle title, uint32_t weapon);
