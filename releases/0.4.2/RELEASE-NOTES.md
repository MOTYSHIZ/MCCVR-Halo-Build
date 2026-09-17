# Halo MCC VR Alpha 0.4.2 — Quick Patch Update

Alpha 0.4.2 is a small patch update to 0.4.1, refining **experimental manual reload** and retaining **weapon holsters and the controller menu pointer**, while retaining playable VR paths for all six MCC campaigns: CE Original/Anniversary, Halo 2 Classic/Anniversary, Halo 3, ODST, Reach and Halo 4. One build supports Steam and Microsoft Store / Xbox app / Game Pass.

This remains an alpha. The author has approved the latest **1a9766c** candidate as the new release baseline; the experimental features and broader mission, custom-mod, hardware and edition coverage still need feedback.

## New in 0.4.2 — manual reload refinements

Two independent options are now available under **F1 > Weapon & Aim > Manual Reload**, across CE, Halo 2, Halo 3, ODST, Reach and Halo 4, including both CE/H2 graphics modes:

- **Disable automatic reload:** prevents the native empty-trigger automatic reload so you can use manual reload gestures. Normal reload buttons remain available.
- **Skip reload and weapon-ready animations:** skips the identified first-person reload/equip playback and shortens native reload waits, allowing magazine insertion to complete promptly through the game's own ammo handling.

Both options are **off by default** and require **Manual Reload** to be enabled. Enable either or both; keep your existing config when updating. Halo still owns ammo eligibility, reserves and inventory. Custom weapon delays can remain; this does not promise zero delay for every weapon. If an optional feature cannot be verified, that feature stays stock and logs the fallback while VR continues.

All previous manual reload, Needler shake, holster, menu pointer, campaign and edition support is retained.

## Quick controller reference

- **VR settings:** click both controller sticks together, or press F1.
- **D-pad:** hold the left hand beside the left side of your head and move the left stick.
- **CE/H2 graphics switch:** with the left hand beside your head, click the left stick. For CE, wait until any cinematic has ended.
- **CE compatibility:** avoid stacking graphics wrappers or render-interception mods on top of MCCVR; they can interfere with injection.

## Retained from 0.4.1 — experimental interactions

All three features are **off by default**, including when their settings are absent from an existing configuration. Keep your current config when upgrading and enable the features you want in F1.

### Manual reload and visible reload parts

Enable **Manual Reload** under **Weapon & Aim**. Reach to the support-side hip, hold support-hand grip to take a reload part, bring it just below the gun hand and release to request a reload. Releasing elsewhere cancels. Grab and completion provide haptic feedback.

- Includes 42 reload-part meshes across all six games, with the same accessory used in both CE/H2 graphics modes. Separate sliders adjust grab and insertion radii.
- Recognized weapon models use their catalogued part. Unfamiliar valid modded models can use an optional **blue generic reload item**; this does not extract a custom gun's own magazine or infer its ammo type.
- Optional **Shake to reload needle weapons:** enable it alongside Manual Reload, then make one rapid out-and-back shake of the gun hand in any direction. **No grip press is required.** Let the hand settle before repeating; minimum stroke distance is adjustable. Applies to recognized Needlers across the six titles and Reach's Needle Rifle.
- Match **MCC Reload button** to each game's controller layout. Normal reload buttons remain available; Halo retains ammo rules; automatic reloads and animation timing stay native unless the new options above are enabled.

**Current visual limits:** parts have simple grey shading without native textures or world occlusion and may appear through nearby surfaces. With animation skipping disabled, the original gun still plays its normal reload animation and retains its native magazine until that animation removes it. Skipping playback does not add simulated magazine removal from the original gun. These are optional interaction visuals, not fully simulated magazine mechanics.

### Weapon holsters

Enable **Weapon Holsters** under **Weapon & Aim**. Use the weapon-side shoulder zone, or select the weapon-side hip. Adjust the holster radius and draw distance to suit seated or standing play.

- **Slide / draw:** hold weapon-hand grip in the zone and draw away while holding.
- **Click:** press weapon-hand grip in the zone to switch once. If both modes are enabled, click takes precedence. Release grip before another switch.
- Match **MCC Switch Weapon button** to the game's controller layout. Gestures mirror in left-handed mode and exchange the native carried weapons; they do not add inventory slots, visible body-mounted guns or a persistent empty-hand mode.

Reload and holster gestures operate during focused, tracked, on-foot, single-weapon gameplay. They cancel during menus, vehicles, death, tracking loss and other transitions. Dual-wielding keeps native controls. Native weapon/ammo rules determine whether a requested action succeeds.

### Controller menu pointer

Enable **F1 > Controls > Point at game menus**. Aim the primary controller at the displayed MCC/game menu and use its trigger to click or drag. A visible white cursor shows pointer position. The pointer follows handedness and requires MCC to have focus. Existing F1 pointing remains available independently.

The pointer now handles MCC's menu even when multiple campaign modules remain loaded. It applies to the native menu screen, not normal gameplay or cinematic theater; individual native widgets and transition cases still need broader testing.

### Per-game controller layouts and interaction settings

Open **F1 > Weapon & Aim > Reload and holsters** (or click both sticks together to open VR settings). Enable **Manual Reload** or **Weapon Holsters** to reveal the shared setup controls.

- **Controller layout for:** choose Halo CE, Halo 2, Halo 3, ODST, Reach or Halo 4. The selector follows the active title when it changes; you can also select a game yourself. Button choices are saved separately for each title. CE's two graphics modes share one set, as do Halo 2's.
- **MCC Reload button:** choose the native controller button your selected game's MCC layout uses for reload. Visible with Manual Reload enabled; defaults to **X**.
- **MCC Switch Weapon button:** choose the native controller button your selected game's MCC layout uses to switch weapons. Visible with Weapon Holsters enabled; defaults to **Y**.
- Both button selectors offer **X, Right bumper, Left bumper, B, Y, A, Left trigger and Right trigger**. These settings tell the VR gestures which native button to send; they do not change MCC's controller preset. Match your actual in-game layout (Recon, Reclaimer, or whichever preset you use). If a gesture performs the wrong action or does nothing, check these mappings in the selected campaign.
- **Pouch / hip depth below head:** adjust the shared vertical placement from **25–85 cm** for seated or standing reach.

The smaller interaction adjustments are available alongside these mappings:

| Feature | Available controls |
| --- | --- |
| Manual reload | Independent enable toggle; **Magazine grab radius** 8–40 cm; **Magazine insertion radius** 6–30 cm |
| Reload behavior | Independent **Disable automatic reload** and **Skip reload and weapon-ready animations** options; both default off and require Manual Reload |
| Unfamiliar weapons | **Generic reload item for unknown weapons** toggle; uses a blue interaction item for valid unfamiliar models, without claiming a custom magazine mesh or ammo-type detection |
| Needle weapons | **Shake to reload needle weapons** toggle and **Minimum shake stroke** 6–20 cm; one rapid gun-hand out-and-back, no grip required, then let the hand settle |
| Holster placement | **Weapon-side shoulder** or **Weapon-side hip**, plus **Holster grab radius** 8–40 cm |
| Holster activation | Separate **Holster slide / draw gesture** and **Holster click gesture** toggles; click takes precedence if both are on; enable at least one |
| Slide-only holster distance | **Minimum holster draw distance** 10–50 cm; shown when slide is on and click is off |
| Menu interaction | **F1 > Controls > Point at game menus** toggle; primary-controller aim, trigger click/drag, visible white cursor, and handedness support |

Reload/holster gestures mirror for left-handed play and give a short vibration on grabs and completed gestures. They apply to focused, tracked, on-foot, single-weapon gameplay and cancel during menus, vehicles, death or tracking loss. Regular controller buttons remain available; Halo's native ammo and inventory rules still apply. Release grip before repeating a holster switch. These controls supplement the retained D-pad head-radius slider, optional Quest 3 thumb-rest/right-stick D-pad, handedness/hand-alignment options and precision arrows beside F1 sliders described below.

### Additional corrections

- **CE Anniversary:** bounds residual offscreen beam-flare halos while retaining native lighting.
- **Halo 3 Cortana sequence:** tightens cinematic-facing transitions so an unknown scene state cannot be mistaken for a real cutscene exit.
- Preserves earlier CE vehicle steering and seated crosshairs, D-pad settings, handedness, haptics, recovery and all-title campaign support.

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

## Carried forward — CE rendering, controls, comfort and vehicles

This release retains the CE rendering, controls, comfort and vehicle improvements from Alpha 0.4.0, including controller-directed steering/aiming and seated crosshair capture in Original and Anniversary graphics.

- **Original graphics and startup:** accepts complete native frames when CE's internal render size differs from the desktop output, and waits for verified simulation activity before installing initial hooks.
- **Stereo frame recovery:** briefly rejected frames retain the previous complete eye pair and its pose within the existing freshness limits; failed output-resource replacements can retry.
- **Tracked controls and reticles:** corrects cleanup so controls and reticle ownership can reinstall after a camera gap.
- **Body, grenade and movement direction:** on-foot body facing follows the head; grenade aim follows the primary controller while head-relative walking and native camera angles remain separate.
- **Spatial audio:** follows full headset orientation while preserving native listener position, velocity, Doppler and environment settings.
- **Firing comfort:** native camera recoil and shake no longer add an extra transform to the tracked on-foot camera; weapon simulation still advances normally.
- **Anniversary flare handling:** guards against behind-eye and invalid flare projections while retaining valid flares and native lighting.
- **Physical melee:** speed-qualified hand or gun swings gain up to 20 cm of reach along their motion; walls and first obstructions still block damage.

- **Vehicle steering and gunner aim (Original and Anniversary):** the aiming controller supplies direction in supported normal following-camera seats; this is the right controller by default and follows the existing handedness setting. Head look remains independent, while native throttle, buttons, physics and seat/aim limits are preserved. On-foot controls are unchanged.
- **Vehicle and turret crosshairs:** corrects the seated capture gate that left native crosshair artwork face-centered, allowing it to use the existing controller-ray presentation while keeping the main HUD framing unchanged. The reticle shows controller direction; native barrel limits and projectile origins still apply, so it is not a new impact-point predictor.
- **Vehicle coverage:** the user confirmed right-hand steering on **115778a**. The new **35a4d09** seated crosshair correction still needs headset confirmation in Original and Anniversary. First-person/custom seated perspectives retain native behavior; broader seat/custom-vehicle coverage remains open.

**Cursed Halo Again requires both Halo CE Campaign and Halo CE Multiplayer installed.** The reported full-loading-bar stall also occurred without VR and was resolved by installing the missing multiplayer content. This was a game-content dependency, not a loading-hook fix. Follow each custom campaign's asset requirements.

The user confirmed CE Anniversary/rotation and successful Original-mode custom-campaign loading on the preserved d7dbfcb runtime source. Broader custom weapon rigs/contact, remaining community symptoms, fresh Halo 3 regression, Store-specific scenarios and long-session transitions still need coverage. These refinements do not claim every reported symptom or custom mod is fixed.

## Headline additions

### Earlier additions retained from Alpha 0.4.0

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
| Halo CE | Original and Anniversary stereo/6DOF, hands, weapons, aiming, native HUD/reticles, muzzle effects, gameplay graphics switching, both-controller haptics, contact/melee, controller-directed vehicle steering/aiming and seated crosshair capture | Normal following-camera seats only for new vehicle controls; seated crosshair headset confirmation pending; do not switch graphics during cinematics; CE body-following is deferred; exact replacement/custom mesh contact is not guaranteed |
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

## Downloads and build identity

**Players: [Halo-MCC-VR.zip](https://github.com/moistman42069/MCCVR-Halo-Build/releases/download/MCC_VR_ALPHA_0.4.2/Halo-MCC-VR.zip)** — DLL, launcher, default config and README, with installation guidance and licenses. Runtime files are byte-identical to the tested **1a9766c** candidate; no runtime changes or rebuild were introduced for publication.

**Developers: [Halo-MCC-VR-Source.zip](https://github.com/moistman42069/MCCVR-Halo-Build/releases/download/MCC_VR_ALPHA_0.4.2/Halo-MCC-VR-Source.zip)** — the exact matching source archive for `1a9766ca971a9e5f09b942d508abecd9753bdb35`. The new release tag points to that same source commit. [SHA256.txt](https://github.com/moistman42069/MCCVR-Halo-Build/releases/download/MCC_VR_ALPHA_0.4.2/SHA256.txt) lists archive and runtime hashes.

The author approved this candidate as the release baseline after headset testing. The supplied log identifies Steam, SteamVR/OpenXR 2.17.10 and an Oculus-family headset at 90 Hz. This does not establish exhaustive all-title, Store, custom-mod or long-session coverage. Local verification passed Release x64, **40 automated test suites**, the Reach consistency gate, pinned reload-binding verification and 387,629 native reload checks. Headset feedback remains the acceptance test for individual features and scenarios.

```text
Build source    1a9766ca971a9e5f09b942d508abecd9753bdb35
HaloMCCVR.dll   8B6FFB78884420588432DA19A5348F330689F201094660729FA8B9AB72C58F60
Launcher        095463D4F9CFC7FED181E852F71F0A87C644CC185DAEC66C73BE22F3F2A67C48
Config          2090082D9A01DD54676091E5BC1BD3AC6DF22F0C309B3ECC4B5B40B3F712550F
```
