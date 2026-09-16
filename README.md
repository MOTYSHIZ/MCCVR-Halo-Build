# MCCVR Halo Build

_**OpenXR Toolkit may cause performance or compatibility issues. Disable or
remove it while troubleshooting.**_

An independently maintained continuation of
[Halo-MCC-VR by pancreations](https://github.com/pancreations/Halo-MCC-VR),
maintained here by **moistman42069**. Original contributor credit, history, and
the MIT license are preserved.

## Latest release: Alpha 0.4.0 — All Campaigns Release

This is the first cumulative release in which every Halo: The Master Chief
Collection campaign has a playable VR path:

- Halo: Combat Evolved Anniversary — Original and Anniversary graphics
- Halo 2: Anniversary — Classic and Anniversary graphics
- Halo 3
- Halo 3: ODST
- Halo: Reach
- Halo 4

The same build supports Steam and Microsoft Store / Xbox app. This remains an
alpha prerelease: complete campaign coverage does not mean every feature,
mission, transition, headset, or runtime combination is finished.

Read the [complete Alpha 0.4.0 release notes](releases/0.4.0/RELEASE-NOTES.md)
for the detailed title-by-title breakdown, exact limits, planned work, and
artifact hashes.

## Read this before playing

- **Wait at least seven seconds after entering a title or level before using
  Force Inject / Recover VR.** Some titles need about seven seconds to load and
  establish their native camera/display proofs. Using recovery earlier can make
  normal initialization look like a failure.
- **Switching from one campaign to another can occasionally crash MCC or leave
  the next campaign flat.** Fully close MCC, restart it through the supplied
  launcher, and load the destination campaign again.
- **Do not switch Halo CE Original/Anniversary graphics during cinematics.**
  Switch during gameplay after the cinematic has ended.
- Keep your existing `halomccvr.cfg` when updating.
- Launch only with anti-cheat disabled. Do not use the mod in matchmaking.

## Downloads and exact identity

- Build ZIP:
  `HaloMCCVR-5ac02f5-all-title-reentry-recovery-20260916-010940918Z-Build.zip`
- Matching source ZIP:
  `HaloMCCVR-5ac02f5-all-title-reentry-recovery-20260916-010940918Z-Source.zip`
- SHA-256 sidecar:
  `HaloMCCVR-5ac02f5-all-title-reentry-recovery-20260916-010940918Z-SHA256.txt`
- Runtime source: `5ac02f53a7896ffd6b8dff37ddc5bc4700890559`
- Build ZIP SHA-256:
  `1E75D939B0D65136AAFD29718EEE0BEAC269C44E4930E1E5C0C1D3E7DB958C2C`
- Source ZIP SHA-256:
  `FB3563A2DE33CD5EFC1002B641FCA289FF8830D5849203621C259FAAD6B10A8A`
- DLL SHA-256:
  `55646EFF6AEF8FAD0C16E9FDA67685292637B97E0B6437B7DD670C4E4B840A63`
- Launcher SHA-256:
  `BF009D5D54CCC60D01BD6B3FFB5EBCF43F8511F845FFF242A21A47D0A16FACC1`

The published build is the exact headset-accepted candidate. It was not rebuilt
for release. The matching source ZIP reproduces the runtime commit's Git bytes;
the release tag adds release-facing documentation after acceptance.

## Required MCC settings

Set these separately in every campaign before judging scale, aim, or
performance:

| Setting | Required value |
| --- | --- |
| Video > Max Frame Rate | 120 |
| Video > V-Sync | Off |
| Campaign Field of View | **120° in each title** |
| MCC FSR | Off |

For ODST, also set Look Sensitivity to maximum and Look Acceleration off so
native aim can keep up with the controller reticle.

## Major features

- Per-eye stereo rendering and 6DOF head tracking across all six campaigns.
- Motion-controller aiming, tracked hands and weapons, melee, grenades, and
  haptics, with optional left-handed primary-weapon routing.
- Native or title-appropriate HUD and reticle handling, title-specific HUD
  placement controls, scopes where currently supported, and F1 configuration.
- Halo CE Original and Anniversary rendering, native HUD/reticles, muzzle
  effects, gameplay graphics switching, contact, and physical melee.
- First-person vehicles in Halo 3, ODST, and Reach, with per-seat adjustment.
- Room-fixed 3D cutscene theater where supported.
- Separate world-contact, true physical-melee, and gesture-melee controls.
- Experimental roomscale body translation for H2, H3, ODST, Reach, and H4.
- Snap and smooth turning, head-relative movement, comfort options, resolution
  scaling, weapon alignment, HUD controls, and fine-step slider arrows.
- Force Inject / Recover VR routed through each selected title's verified
  lifecycle rather than an H3-only blind retry.
- Cross-title re-entry that tolerates MCC retaining inactive title modules.

## Campaign status

| Campaign | Current VR coverage | Notable limits |
| --- | --- | --- |
| Halo CE | Original/Anniversary stereo and 6DOF, hands, weapons, aim, native HUD/reticles, muzzle effects, gameplay graphics switching, contact and melee | Do not switch graphics during cinematics; body-following is deferred; exact custom/replacement mesh contact is not guaranteed |
| Halo 2 | Classic/Anniversary stereo and 6DOF, hands/weapons, controller aim, reticle work, contact/melee, snap turn and handedness | Complete VR HUD and first-person vehicles remain unfinished; independent secondary-gun trajectory is unfinished |
| Halo 3 | Mature stereo/6DOF path, articulated arms/hands, native HUD/reticle, scopes, cutscenes, vehicles, contact/melee and comfort controls | Independent dual-fire remains disabled; some visibility/calibration work remains |
| ODST | Stereo/6DOF, hands/weapons, HUD/reticle, cutscenes, vehicles, contact/melee and recovery | First captioned opening scene can be black; broader vehicle/co-op coverage is open |
| Reach | Stereo/6DOF, hands/weapons, HUD/reticle, cutscenes, vehicles, contact/melee and native HUD height | HUD curvature is unavailable; passenger hands and some effects/clarity reports remain open |
| Halo 4 | Stereo/6DOF, hands/weapons, HUD/reticle controls, cutscenes, contact/melee, handedness and comfort | Floating hands rather than full arm IK; first-person vehicles are unfinished; reported damage blackout is deferred |

## Fresh installation

1. Fully close MCC and extract the **Build ZIP** somewhere temporary.
2. Open MCC's installation root—the folder containing `MCC\Binaries\Win64`:
   - Steam: Library > Halo: The Master Chief Collection > Manage > Browse local files.
   - Xbox app: MCC > Manage > Files > Browse, then open `Content` if needed.
3. Create a folder named exactly `Halo_MCC_VR` in that root.
4. Copy `HaloMCCVR.dll`, `HaloMCCVRLauncher.exe`, and `halomccvr.cfg` from the
   extracted Build ZIP into `Halo_MCC_VR`. Do not put the Source ZIP there and
   do not replace files in `MCC\Binaries\Win64`.
5. Start your headset connection and active OpenXR runtime. SteamVR is the
   recommended/tested route.
6. With MCC closed, run `HaloMCCVRLauncher.exe` from `Halo_MCC_VR` and choose
   the anti-cheat-disabled path. For Microsoft Store, let the launcher activate
   MCC through the Xbox app; do not rename the executable.
7. Apply the required settings in every campaign, load a level, allow normal
   initialization, and press F3 to recenter.

## Updating

1. Close MCC and back up the existing `Halo_MCC_VR` folder.
2. Replace `HaloMCCVR.dll` and `HaloMCCVRLauncher.exe`.
3. **Keep your existing `halomccvr.cfg`** so alignment, controls, seats, and
   rendering preferences remain saved.
4. Update each edition's separate `Halo_MCC_VR` folder if both Steam and Store
   versions are installed.
5. Do not load obsolete `halo3xr.dll` files alongside this build.

## Force Inject / Recover VR

VR normally enters automatically. If it does not:

1. Make sure the level is fully loaded and gameplay has begun.
2. **Wait at least seven seconds.** This gives the active engine time to load
   its module and publish current camera, display, and resource proofs.
3. Use Force Inject / Recover VR from the launcher or F1 > Status once.
4. Allow the normal camera-ready delay. Do not repeatedly inject.
5. If the OpenXR session is lost, MCC crashed, or the title stays flat, fully
   close MCC and restart it through `HaloMCCVRLauncher.exe`.

Recovery does not bypass engine identity or safety checks and cannot recreate a
lost OpenXR session.

## Known issues

- Campaign-to-campaign switching can occasionally crash or require a full MCC
  restart before the destination enters VR.
- Halo CE graphics switching is unavailable during cinematics.
- Independent dual-wield bullet trajectories are unfinished; H3's failed
  experimental dual-fire hooks remain disabled.
- H2/H4 first-person vehicles, complete all-title weapon-side zoom windows, and
  several scope/HUD parity items remain unfinished.
- Sustained/sliding contact, arbitrary modded weapon geometry, fully unarmed
  damage, secondary-weapon damage selection, and some world-object targets need
  further work and testing.
- Per-title/per-weapon automatic alignment profiles, gun-stock calibration,
  independent native body-yaw following, and CE roomscale body translation are
  planned refinements.
- Microsoft Store can appear frozen for several seconds on first load. Wait
  before assuming it crashed.
- If performance locks to half refresh, lower `resolution_scale` and disable
  runtime motion smoothing/ASW.
- Hardware/runtime, co-op, multiplayer, mission, vehicle, and long-session
  coverage remains incomplete. MCC updates can invalidate native signatures.

## Reporting problems

Attach `HaloMCCVR.log` and `HaloMCCVRLauncher.log`, and include:

- campaign and mission;
- Classic/Anniversary or Original/Anniversary mode where applicable;
- Steam or Microsoft Store edition;
- headset and connection method;
- OpenXR runtime and refresh rate;
- what happened before the failure, especially any title or graphics switch.

## Validation

- Exact Release x64 candidate packaging passed.
- All 28 CTest suites passed.
- Reach consistency checks passed.
- The separate manual cold-retry fixture passed 46 checks.
- Build ZIP, source ZIP, sidecar, staged files, embedded source identity, and
  exact source-archive bytes were verified before acceptance.

This is an unofficial derivative of
[pancreations/Halo-MCC-VR](https://github.com/pancreations/Halo-MCC-VR), under
the MIT license. It is not affiliated with Microsoft or Halo Studios and
contains no MCC game binaries or editing-kit assets.
