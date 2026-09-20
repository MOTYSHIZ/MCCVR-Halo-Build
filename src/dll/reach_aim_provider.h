#pragma once

// Reach's aim provider -- the FIRST per-game implementation of aim_solve::IAimProvider
// (src/common/aim_provider.h). Reach is closest to CampE, so it leads.
//
// STATUS: ACTIVE (StickLoop delegation). Wired behind the `aim_provider` config flag (default 0),
// Reach only. Runs the engine-free orchestrator (solve_and_actuate) to pick the actuation strategy
// and log diagnostics, then DELEGATES the actuation to Game_ComputeAimStick -- the unchanged
// closed-loop path Reach already uses. Because the StickLoop rung is that same function, routing the
// aim through the provider is a no-op BY CONSTRUCTION: with the flag on, the stick is bit-identical
// to the shipped path, so the in-headset check is a confirmation, not a gate. Flag off = the shipped
// call is untouched.
//
// NEXT (needs the co-op loop): the direct-drive WriteState rung -- once Reach's replicated angular
// control record is located (its unit+0x214 is only the DERIVED vector, a mirror) and co-op
// host-follow-verified, caps.can_write_state flips true, select_actuation returns WriteState, and the
// switch below writes the record instead of servoing a stick. Until then WriteState fails SAFE back
// to the stick loop, so aim never dies on an unverified path.

// Runs the provider solve (selection + diagnostics) and produces the aim right-stick for this tick.
// Returns true and fills outRx/outRy when the stick is driven, false when aim is not steering
// (matches Game_ComputeAimStick's contract, which it delegates to for the StickLoop rung).
bool ReachAimProvider_ProduceStick(float& outRx, float& outRy);
