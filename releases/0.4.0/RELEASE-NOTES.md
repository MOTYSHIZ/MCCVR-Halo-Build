# Halo MCC VR Alpha 0.4.0 — All Campaigns in VR

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

### New in this release

- **Halo CE Anniversary is now playable in VR**, alongside Original graphics:
  stereo/6DOF, tracked hands and guns, native HUD and reticles, full-resolution
  Anniversary eye targets, muzzle effects, gameplay graphics switching, world
  contact, physical melee, and restored vibration on both controllers.
- **Fixed opt-in left-hand alignment across all six titles**, including both
  CE and Halo 2 graphics modes. Enable Left-handed main weapon, then Fix Hand
  Alignment (Experimental). It uses title-specific authored grips, preserves
  gun placement, and remains off by default. Native finger animations and
  unusual/custom weapon grips can still need refinement.
- **Adjustable D-pad head radius:** 10–50 cm, with the existing 30 cm default.
- **Optional Quest 3 thumb-rest D-pad:** hold the physical left thumb rest and
  move the physical right stick for D-pad directions. Release to restore normal
  stick use. This stays independent of weapon handedness and is off by default.
  Existing head gestures and graphics-switch clicks remain available; sensor
  delivery depends on your runtime/controller connection.
- **All-title automatic re-entry and Force Inject / Recover VR improvements**,
  plus Reach's native HUD-height control and retained CE haptics.

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
| Halo CE | Original and Anniversary stereo/6DOF, hands, weapons, aiming, native HUD/reticles, muzzle effects, gameplay graphics switching, both-controller haptics, contact and melee | Do not switch graphics during cinematics; CE body-following is deferred; exact replacement/custom mesh contact is not guaranteed |
| Halo 2 | Classic and Anniversary stereo/6DOF, hands/weapons, controller aim, native HUD/reticle handling, contact/melee, snap turn and handedness | HUD presentation/control parity still needs refinement; full H3-style vehicle policy and first-person vehicles remain unfinished; independent secondary-gun trajectory remains unfinished |
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
- The previously reported Halo 2 reticle issue after leaving a tank remains
  open; it is not claimed fixed by this release.
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

For a fresh install, extract `Halo-MCC-VR.zip` and place `HaloMCCVR.dll`,
`HaloMCCVRLauncher.exe`, and `halomccvr.cfg` in a folder named `Halo_MCC_VR` at
the MCC installation root. Start the headset connection/OpenXR runtime, then
launch through `HaloMCCVRLauncher.exe` with anti-cheat disabled.

When updating, close MCC, replace `HaloMCCVR.dll` and
`HaloMCCVRLauncher.exe`, and **keep your existing `halomccvr.cfg`**. Steam and
Microsoft Store installations have separate mod folders if both are installed.

Set MCC's per-title campaign FOV to 120°, V-Sync off, maximum frame rate 120,
and MCC FSR off. Recenter with F3 after entering gameplay.

## Downloads and accepted build

**Most players need only [Halo-MCC-VR.zip](https://github.com/moistman42069/MCCVR-Halo-Build/releases/download/MCC_VR_ALPHA_0.4.0/Halo-MCC-VR.zip).**
It contains exactly four files at its root: `HaloMCCVR.dll`,
`HaloMCCVRLauncher.exe`, `halomccvr.cfg`, and `README.txt`. Instructions,
credits, and bundled component licenses are combined in the README.

Developers: [Halo-MCC-VR-Source.zip](https://github.com/moistman42069/MCCVR-Halo-Build/releases/download/MCC_VR_ALPHA_0.4.0/Halo-MCC-VR-Source.zip)
contains the complete source tree, build instructions, tests, evidence documents,
and updated release documentation. Runtime source is **d47a98c947dc60dd98d7259a29a7582d5f46df7f**;
the release tag adds documentation only. No runtime was rebuilt for publication.
The DLL, launcher and default config are byte-identical to the tested candidate.
[SHA256.txt](https://github.com/moistman42069/MCCVR-Halo-Build/releases/download/MCC_VR_ALPHA_0.4.0/SHA256.txt) identifies both downloads and runtime files.

The user accepts this build after testing the campaign lineup. The supplied log
corroborates all six titles in one Steam session, SteamVR/OpenXR 2.17.9,
Oculus-family headset at 90 Hz (Quest 3 identified by the user). It is a campaign
smoke test, not proof of every mission, optional toggle, weapon, runtime or
Store-edition scenario. Previous limitations remain open unless explicitly
listed as corrected above.

Validation: Release x64, all **33 automated test suites**, Reach consistency,
eight title-specific palm-marker comparisons, and twelve stock CE weapon graphs
at three scales passed. Both graphics modes share the relevant hand corrections.

```text
Runtime source  d47a98c947dc60dd98d7259a29a7582d5f46df7f
HaloMCCVR.dll   ADAB506E9E3BFB1E04DBBF767FDD907EFD414526863AB5C837FD65E7FAB95922
Launcher        EB9B23F68F5B32FA83120308513CEB9B2B7DEB644CF7914AE3B6FA5091524845
Config          D5AC7F5653ACEC01CA76F902E4DB155F4F4B259AB5D0268E4F79D349C2AD8CA8
```
