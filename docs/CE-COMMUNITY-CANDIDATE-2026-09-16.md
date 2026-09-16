# CE community-report refinement candidate

This is an unaccepted test candidate built on the accepted d47a98c all-campaign
release. It supports Steam and Microsoft Store and keeps the existing shared
configuration, D-pad controls, hand alignment and haptics. Keep your current
`halomccvr.cfg` when updating. Both matching ZIPs are delivered for testing;
this task performs no installation, game launch or GitHub publication.

## CE corrections

- Classic accepts the complete native render when CE's internal raster size
  differs from its final desktop output size. Both eyes retain the proper
  native field of view. Cropped/split views and incompatible source textures
  still fail safely.
- Initial CE hook installation waits for verified native simulation activity.
  This removes premature installation during loading. The custom-campaign log
  does not contain the stalled thread stack, so whether this resolves Cursed
  Halo Again and Minecraft 2 still requires the user's test.
- A briefly rejected stereo frame retains the previous complete eye pair and
  its original pose for compositor reprojection, within the existing short
  freshness limits. Resource replacement failures can retry even when returning
  to the previous output size.
- Tracked controls and reticle ownership can reinstall after a camera gap.
  The corrected native turn-hook entry has the unwind metadata required by
  safe retirement instead of becoming permanently stuck in cleanup.
- Physical melee gives a speed-qualified hand or gun swing up to 20 cm of
  additional reach along its actual motion. Walls and other first obstructions
  still block damage; native target identity, damage and swing latches remain.
- On-foot body facing follows the head and native grenade aim follows the
  primary controller. The native camera angle state remains separate, avoiding
  doubled head rotation. Both biped update paths preserve head-relative walking
  while the controller aims elsewhere. Native release timing and grenade flight
  remain owned by CE.
- Spatial sound follows the headset's full orientation while preserving the
  native listener position, world velocity, Doppler and environment settings.
- Native camera recoil and shake no longer add their returned transform to
  the tracked on-foot camera. Native effect state and weapon simulation still
  advance normally.
- Anniversary flare sprites are clipped when their light is behind the eye's
  near plane or has a nonfinite screen projection. Native lighting and valid
  visible flares remain enabled. Unproven ownership or camera data stays stock.

These optional features retire independently and report fallback/cleanup in
the log; they do not gate the CE camera core. Positional body following remains
part of the earlier deferred scope, separate from the new body-facing change.

Evidence is recorded in `HALOCE-CLASSIC-STARTUP-2026-09-16.md`,
`HALOCE-FRAME-RETENTION-2026-09-16.md`,
`HALOCE-CONTROLS-RETIREMENT-2026-09-16.md`,
`HALOCE-UNIT-CONTROL-2026-09-16.md`, `HALOCE-ORIENTATION-2026-09-16.md`,
`HALOCE-FLARE-GUARD-2026-09-16.md` and `HALOCE-MELEE-REACH-2026-09-16.md`.

## Local verification

The cumulative Release build, all **35 CTest suites**, the Reach consistency
gate, all **140 pinned CE contracts**, generated-contract consistency and
**23 production binding groups** pass. Final test objects include the actual
native movement-output preservation check. All eight affected native hook
entry patterns in the linked DLL have Windows unwind coverage; this does not
execute live game-thread suspension.

Additional native execution checks cover five Classic output-scaling cases;
15 contact/damage/search cases; 384 flare projections; six camera-effect cases;
audio producer/backend orientation and the actual production-generated listener
packets; 16 unit-control packet writes, 16 aim/grenade handoffs, 64 independent
camera headings and 20 native movement-vector cases. The production orientation
fixture passes 52 checks. Emulator services and collision/flight limits are
listed in the individual evidence records. Independent reviews found no
remaining production blocker in frame retention, flare handling or the final
body/grenade/movement transaction.

The package script repeats the cumulative build, tests and Reach gate from the
committed source. Archive verification checks every build file, embedded source
identity, hashes and the source ZIP's exact Git archive bytes. The resulting
handoff identity is saved under `out/ce-community-current-handoff.json`.

## Test in the headset

1. Start CE directly in Classic, including Cursed Halo Again and Minecraft 2.
   Confirm the loading screen finishes, VR starts, and both eyes track.
2. Play Halo in Anniversary and pass through the first tunnel. Check for black
   flashes and vertical light streaks, and switch graphics both ways.
3. Turn your head, walk, aim/fire away from your original direction and verify
   the native reticle. Repeat after an ordinary pause or level transition.
4. Check body/grenade direction, sound direction, and firing comfort with the
   assault rifle and other weapons. Try punches and gun strikes against Grunts
   and Jackals with both hands, including an enemy behind a nearby wall.
5. Run the required Halo 3 regression and ordinary campaign switching. Record
   the edition, OpenXR runtime, headset, graphics mode and mission with the log.

Local builds and tests establish implementation and bounded failure handling;
they do not establish headset acceptance or prove every community symptom had
the same cause. The accepted pointer remains d47a98c. The new Game Pass-specific
investigation is deferred by the user until its log arrives; existing edition
support remains. Earlier vehicle/zoom/dual-wield and other standing or explicitly
deferred work stays preserved in `CONTINUATION-REFINEMENT-LIST.md`.
