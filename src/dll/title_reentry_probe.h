#pragma once

#include <cstdint>

#include "../common/runtime_types.h"
#include "../common/title_runtime_state.h"

// Process-wide, read-only hint used only to choose which title adapter may
// begin its normal level-liveness/cold-preflight path while MCC keeps several
// game DLLs resident. This NEVER grants runtime ownership or capabilities.
void TitleReentryProbe_PublishPresentCaller(const void* caller, uint64_t nowMs) noexcept;

// Returns one uniquely proven active title, or None. `modules` is the exact
// module-set snapshot from TitleAdapter_PollLoaded. The probe only reads
// title-owned liveness records; final ownership still requires the existing
// generation/lifecycle/heartbeat resolver.
GameTitle TitleReentryProbe_Resolve(
    const TitleRuntimeModuleSet& modules, uint64_t nowMs,
    GameTitle retainedTitle) noexcept;

// Worker-only CE cold-install admission. Shares the same pinned, bounded
// native clock reader with adapter selection; never pins or scans a module.
// Call with zero base/generation when CE is inactive to reset its history.
bool TitleReentryProbe_CeLevelAllowsInstall(uintptr_t moduleBase,
    uint32_t generation,uint64_t nowMs) noexcept;
