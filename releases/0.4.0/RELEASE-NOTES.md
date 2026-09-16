# Halo MCC VR Alpha 0.4.0 — All Campaigns Release

Alpha 0.4.0 is the first cumulative MCCVR release in which every Master Chief
Collection campaign has a playable VR path:

- Halo: Combat Evolved Anniversary, in Original and Anniversary graphics
- Halo 2: Anniversary, in Classic and Anniversary graphics
- Halo 3
- Halo 3: ODST
- Halo: Reach
- Halo 4

One build supports both Steam and the Microsoft Store / Xbox app edition. This
is still an alpha prerelease: campaign coverage is now complete, but individual
features and transition cases remain under active refinement.

## Most important usage notes

1. **Wait at least seven seconds after entering a title or level before using
   Force Inject / Recover VR.** Some engines need about seven seconds to load
   their module and establish native camera, display, and resource proofs. An
   earlier recovery request can make normal initialization look like a failure.
2. **An occasional cross-title crash is still possible.** If MCC crashes or a
   destination campaign does not enter VR after you switch away from another
   game, fully close MCC, restart it through `HaloMCCVRLauncher.exe`, and load
   the destination again. It normally works on the clean restart.
3. **Do not switch CE Original/Anniversary graphics during a cinematic.** The
   graphics switch is supported during gameplay, but cinematic switching is not
   currently supported. Wait until the cinematic has ended.
4. Keep your existing `halomccvr.cfg` when upgrading. Replace the DLL and
   launcher; do not overwrite a tuned configuration unless you want defaults.
5. Launch only with anti-cheat disabled. Do not use the mod in matchmaking.

## Headline additions

### Halo CE joins the playable campaign lineup

Halo CE now has stereo rendering and 6DOF in both Original and Anniversary
graphics, controller-directed aiming, tracked hands and weapons, native
reticles, native HUD support, full-resolution Anniversary eye targets, muzzle
effects, pause/resume handling, a one-eye desktop mirror, world contact, and
physical melee. Original/Anniversary switching during normal gameplay is
supported in both directions.

CE's native renderer and HUD resources now survive the managed graphics rebuild
used during a graphics-mode change. The native HUD art is retained instead of
being replaced by a generic overlay, and the resource path fails open per
feature rather than taking down a working VR camera. All twelve official CE
weapons contribute conservative, live-bone contact envelopes for barrels,
stocks, magazines, and other major gun surfaces.

Current CE boundaries are documented rather than hidden: cinematic graphics
switching is unsupported; physical roomscale body following is deferred; exact
Anniversary replacement meshes and arbitrary custom weapon surfaces are not
claimed; and broader melee targets and a distinct bare support-hand damage
selector remain future work.

### Better title switching and recovery

The active title can now enter VR even when MCC keeps another campaign module
resident. Reach no longer requires itself to be the only loaded game module,
and CE can safely retain its native cleanup lifetime without blocking the next
title. CE also participates in active-title selection through its verified
native clock.

Force Inject / Recover VR now routes through the selected engine's own verified
setup and cleanup path for CE, Halo 2, Halo 3, ODST, Reach, and Halo 4. It does
not bypass native signature, camera, level-load, display, generation, or OpenXR
checks. Recovery is a retry, not a blind injection. Wait seven seconds after a
title or level begins before invoking it.

### Reach HUD placement

Reach's HUD height setting is connected to its native HUD anchor. Positive
values raise the HUD, negative values lower it, and zero restores native
placement. The authored aiming reticle remains on its separate aim ray; HUD
size and width remain independent. Reach HUD curvature is still not implemented.

### Interaction and comfort refinements carried forward

- Optional left-handed primary weapon routing across the supported titles,
  including aim, trigger, support grip, melee, contact, and haptics.
- An optional experimental anatomical hand-alignment correction, off by
  default so the established placement remains available.
- Separate controls for world contact, true physical melee, and gesture melee.
- A 5 m/s default physical-melee threshold with a configurable 0.30–10 m/s
  range, plus velocity fallback for runtimes that omit useful native velocity.
- Broader live weapon-bound contact in H2, H3, ODST, Reach, H4, and CE, with
  guarded local fallback when model data is unavailable.
- Snap-turn handling across all supported engines, including Halo 2's shared
  renderer path.
- Optional experimental roomscale body translation for H2, H3, ODST, Reach,
  and H4. Halo retains ownership of walking and collision; native independent
  body-yaw following and CE body translation remain deferred.
- Precision arrow buttons beside F1 sliders for repeatable fine adjustment.

## Campaign-by-campaign status

| Campaign | Playable VR coverage | Important remaining limits |
| --- | --- | --- |
| Halo CE | Original and Anniversary stereo/6DOF, hands, weapons, aiming, native HUD/reticles, muzzle effects, gameplay graphics switching, contact and melee | Do not switch graphics during cinematics; CE body-following is deferred; exact replacement/custom mesh contact is not guaranteed |
| Halo 2 | Classic and Anniversary stereo/6DOF, hands/weapons, controller aim, reticle work, contact/melee, snap turn and handedness | No complete VR HUD; full H3-style vehicle policy and first-person vehicles remain unfinished; independent secondary-gun trajectory remains unfinished |
| Halo 3 | Mature stereo/6DOF path, arms/hands, native HUD/reticle, scopes, cutscenes, first-person vehicles, contact/melee and comfort controls | Experimental independent dual-fire remains disabled; some lower-edge visibility and calibration work remains |
| Halo 3: ODST | Stereo/6DOF, hands/weapons, native HUD/reticle, cutscenes, first-person vehicles, contact/melee and recovery | The first captioned opening cinematic can still be black; broader vehicle/co-op coverage remains open |
| Halo: Reach | Stereo/6DOF, hands/weapons, native HUD/reticle, cutscenes, first-person vehicles, contact/melee, and native HUD height | HUD curvature remains unavailable; passenger hands and some mission/version-specific effects and clarity reports remain open |
| Halo 4 | Stereo/6DOF, hands/weapons, HUD/reticle controls, cutscene presentation, contact/melee, handedness and comfort controls | Floating hands rather than complete arm IK; first-person vehicle parity remains unfinished; the reported damage-blackout case remains deferred |

## Known issues and workarounds

- Switching campaigns in one MCC process can occasionally crash or leave the
  next campaign flat. Restart MCC through the supplied launcher and load the
  desired campaign again.
- Wait at least seven seconds after entering a title/level before Force Inject.
  Force Inject cannot recreate a lost OpenXR session; restart MCC if recovery
  still fails after the title has had time to initialize.
- CE graphics switching is not supported during cinematics. Switch only during
  gameplay, after the cinematic ends.
- The Microsoft Store edition can pause for several seconds on its first load.
  Do not assume a temporary frozen image is a crash.
- OpenXR Toolkit is known to cause performance problems with this mod and
  should be removed or disabled while troubleshooting.
- If cadence falls to half the headset refresh rate, lower `resolution_scale`
  and disable runtime motion smoothing/ASW before judging native performance.
- Vehicle seat positions are starting points. Tune the active seat in
  F1 > Vehicles; saved seat trims are intentionally personal.
- Independent per-gun projectile direction is unfinished. Presentation of two
  weapons does not imply independent bullet trajectories.
- Hardware/runtime, co-op, multiplayer, every mission, every vehicle, and
  long-session coverage remain incomplete. MCC updates can invalidate verified
  native signatures until the mod is updated.

## Planned refinement work

- Harden campaign-to-campaign switching so the restart workaround is no longer
  needed, including repeated switches and long sessions.
- Support CE Original/Anniversary graphics switching during cinematics.
- Finish independent dual-wield acquisition, aiming, firing, and damage routing
  for both handedness modes.
- Complete H2 vehicle controls and add H2/H4 first-person vehicle views.
- Extend H3-style weapon-side zoom windows, scopes, and handed placement across
  every title.
- Improve sustained/sliding hand and weapon contact, custom/modded weapon
  geometry, unarmed and secondary-weapon damage selection, and more damageable
  world objects.
- Add per-title and per-weapon alignment defaults, automatic weapon profiles,
  gun-stock calibration, and remaining visibility-edge fixes.
- Continue Reach rendering/clarity investigations, passenger-hand coverage,
  headset/runtime compatibility, co-op, and long-session testing.
- Add independent native body-yaw following and evaluate CE roomscale body
  translation without compromising controller-directed aiming.

## Install or update

For a fresh install, extract the Build ZIP and place `HaloMCCVR.dll`,
`HaloMCCVRLauncher.exe`, and `halomccvr.cfg` in a folder named `Halo_MCC_VR` at
the MCC installation root. Start the headset connection/OpenXR runtime, then
launch through `HaloMCCVRLauncher.exe` with anti-cheat disabled.

When updating, close MCC, replace `HaloMCCVR.dll` and
`HaloMCCVRLauncher.exe`, and **keep your existing `halomccvr.cfg`**. Steam and
Microsoft Store installations have separate mod folders if both are installed.

Set MCC's per-title campaign FOV to 120°, V-Sync off, maximum frame rate 120,
and MCC FSR off. Recenter with F3 after entering gameplay.

## Exact accepted identity

The release publishes the exact headset-accepted candidate; the DLL was not
rebuilt during release preparation.

```text
Runtime source  5ac02f53a7896ffd6b8dff37ddc5bc4700890559
Build ZIP       1E75D939B0D65136AAFD29718EEE0BEAC269C44E4930E1E5C0C1D3E7DB958C2C
Source ZIP      FB3563A2DE33CD5EFC1002B641FCA289FF8830D5849203621C259FAAD6B10A8A
HaloMCCVR.dll   55646EFF6AEF8FAD0C16E9FDA67685292637B97E0B6437B7DD670C4E4B840A63
Launcher        BF009D5D54CCC60D01BD6B3FFB5EBCF43F8511F845FFF242A21A47D0A16FACC1
Config          E67114F4BD30A06A4750F57EC925D0598775C617454D76DCD57FF3902292AB3B
```

The matching source archive contains the exact Git bytes for runtime commit
`5ac02f5`. The release tag additionally contains the release-facing
documentation prepared after headset acceptance.
