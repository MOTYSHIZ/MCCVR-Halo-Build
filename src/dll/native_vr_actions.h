#pragma once
#include "../common/vr_action_mapping.h"

// Optional post-device action bridge. Transport routing remains in use until
// the converter and owned abstract-state reader have both been observed on
// the input thread for the current title generation.
void NativeVrActions_Poll();
// Halo 3 on Project Reclaimer, a zoom trace: a change in the zoom button handed to the game (`kind` 0), the mapper's
// zoom (1) or the support trigger (2); and the game's zoom level changing, which writes the trace out when no pull of
// the trigger came just before it, or when it zoomed in and back out within 0.6 s.
void NativeVrActions_NoteZoomInput(unsigned kind, bool down);
void NativeVrActions_Halo3ZoomChanged(int previous, int level);
// Worker, right after the Halo 3 core retired its hooks: the Halo 3 bridge lets go of halo3.dll in the same pass,
// while MCC still owns the DLL, so the next game gets a fresh image.
void NativeVrActions_Halo3CoreRetired();
void NativeVrActions_BeginPoll() noexcept;
void NativeVrActions_EndPoll() noexcept;
bool NativeVrActions_CanRoute(GameTitle title, uint64_t now) noexcept;
// `extras`: the extra rows pressed (a bit per vr_mapping::ExtraAction) among `extrasServed`, those the bridge carries.
void NativeVrActions_Publish(const void* rawState, GameTitle title, uint64_t now,
    uint32_t actions, bool direct, uint32_t blockedActions=0, uint32_t extras=0, uint32_t extrasServed=0) noexcept;
// The titles whose bridge also carries extra rows (vr_mapping::ExtraBridgeOnly): those whose player-control builder
// reads a flyer's climb and dive and a Banshee's brake as actions of their own -- Halo 3, ODST, Reach and Halo 4. Halo 2
// and Halo CE have no climb or dive.
constexpr bool NativeVrActions_RoutesExtras(GameTitle title) noexcept
{
    return title == GameTitle::Halo3 || title == GameTitle::Halo3ODST || title == GameTitle::HaloReach ||
        title == GameTitle::Halo4;
}
bool NativeVrActions_GestureRouting(GameTitle title, uint64_t now) noexcept;
// ODST (halo3odst.dll, MCC) only: whether a tutorial prompt waits on accept or cancel, and whether the VISR map (the
// PDA) is up, for controller 0, as the game's control builder last saw them. False: not proven, not ODST, or stale (no
// control tick for a quarter second); both answers are then false. `raw`: the published bits, for the log.
bool NativeVrActions_OdstPromptState(uint64_t now, bool& prompt, bool& map, uint32_t* raw = nullptr) noexcept;
constexpr uint32_t NativeVrActions_GestureBit(vr_mapping::Action action) noexcept
{ return action < vr_mapping::Count ? 1u << (18u + unsigned(action)) : 0; }
// Project Reclaimer only: whether ODST's VISR map (the PDA) is up for player 0, read as the client's own
// pda_is_active_deterministic reads it. False before the map's records are found, outside an ODST level, or on a failed
// read. A few guarded memory reads, no allocation or logging: the render thread may ask it once per view.
bool NativeVrActions_ReclaimerOdstMapOpen() noexcept;
