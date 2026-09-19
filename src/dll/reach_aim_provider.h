#pragma once

// Reach's aim provider -- the FIRST per-game implementation of aim_solve::IAimProvider
// (src/common/aim_provider.h). Reach is closest to CampE, so it leads.
//
// STATUS: OBSERVE scaffold. Wired behind the `aim_provider` config flag (default 0), this runs the
// engine-free orchestrator (solve_and_actuate) against a ReachAimProvider and LOGS what it would do
// -- which strategy the caps select, the intent angles -- WITHOUT touching the shipped aim path
// (Game_ComputeAimStick still owns the live stick). It exists to validate, in-game, that the
// interface routes Reach correctly with zero regression risk.
//
// NEXT (needs the build + in-headset + co-op loop): the ACTIVE takeover -- map the intent into the
// game-aim basis and drive it by reusing Game_ComputeAimStick's own desired-aim/stick helpers (not a
// duplicate), then the direct-drive state write once Reach's replicated angular control record is
// located (its unit+0x214 is only the derived vector) and co-op host-follow-verified.

void ReachAimProvider_ObserveTick();
