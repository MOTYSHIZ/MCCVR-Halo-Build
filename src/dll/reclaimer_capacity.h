#pragma once

// PROJECT RECLAIMER'S LIST OF WHAT IT ENLARGED, read from its own log (src/common/reclaimer_capacity_logic.h).
//
// Once the recompiled engine is in use (lifted::Poll), the worker reads this launch's client log for its "[capacity]"
// lines, logs the sizes the mod depends on, and logs which sizes changed since the newest log of another client
// version. Log-only, apart from the candidates it hands the first-person block's check (game.cpp
// Halo3ProveFpLayout), which still proves each in memory.

#include <cstdint>

#include "../common/reclaimer_capacity_logic.h"

namespace reclaimer_capacity
{
// Worker thread, every poll while the engine is in use; reads the log at most once a second until this launch's
// list is found or a minute has gone by.
void Poll();
// The first-person node capacities to try (64, then the client's stated value, then 128). Any thread.
NodeCandidates FpNodes();
} // namespace reclaimer_capacity
