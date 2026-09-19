# Automatic smooth turn in vehicles

New Controls & Turning option: **Automatically smooth turn in vehicles**.
`vehicle_smooth_turn` is global, persisted and default-off. Effective smooth
mode is the saved smooth preference OR enabled override with proven occupancy.
It never writes `turn_smooth`. The existing smooth-speed slider remains visible
when this option is enabled, alongside the on-foot snap increment when relevant.

Halo 3's accepted smooth rate, deadzone, high-resolution clock and held-stick
snap latch are the reference. H3/ODST/Reach keep their shared turn path and native
driver-stick ownership; a physical steering wheel still releases that stick for
VR turning. H4 retains its own yaw reference and uses its independently verified
local seat reader. CE evaluates its frozen setting only after the native turn
phase proves current vehicle ownership. No new engine hook or guessed binding.

Halo 2 already bypasses snap reference changes while seated and retains continuous
native vehicle controls in both renderers. Its native limits and steering remain
unchanged. Vehicle first-person rendering does not select the override in the
shared path. Stale, invalid or wrong-generation occupancy uses the saved mode.

The existing snap latch continues observing the stick in smooth mode: exiting a
seat or disabling the option while held cannot create an extra snap. Centering
and moving the stick again restores the saved snap behavior. Native steering
limits, vehicle follow, saved turn settings and on-foot behavior remain separate.

Release, all 52 CTest suites and the Reach consistency gate pass. The new fixture
passes 55 production/config checks. Logs: out/refinement-20260918/
build-vehicle-turn-final.log, tests-vehicle-turn.log and gate-vehicle-turn.log.
Production turn functions are included directly by
the new vehicle-turn fixture, with a deterministic clock and native occupancy
services stubbed; CE's existing production native-call fixture covers seat entry,
exit, held-stick draining and return to snap. No headset acceptance is claimed.
