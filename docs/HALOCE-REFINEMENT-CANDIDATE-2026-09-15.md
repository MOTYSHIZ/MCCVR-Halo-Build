# CE camera, weapons and pause refinement

This cumulative candidate follows your `a2b526a` test confirming both Original
and Anniversary are visible in VR. It supports Steam and Microsoft Store,
retains the other titles, and preserves your existing configuration.

## Changes

- **Stable facing when switching graphics.** Original and Anniversary retain
  the same headset origin instead of treating every switch as a recenter.
  Explicit recentering still works. Looking up/down follows your headset;
  the right stick uses the existing snap/smooth yaw controls, as in Halo 3.
- **Original gun, hand and effect lens correction.** The tracked weapon and
  hand palette uses the eye's world lens. The native first-person model and
  effect paths no longer replace it with their fixed flat-game field of view
  while a verified tracked palette owns that eye. Native depth handling and
  restoration remain in place. Anniversary retains its separate native
  skin/projection implementation.
- **Visible native pause presentation.** CE's native pause/resume state now
  requests the shared head-locked menu screen. It remains readable when player
  data is temporarily unavailable. The existing Y+B pause shortcut includes CE.
  Recent gameplay eye ownership no longer suppresses the active pause screen.
- **Authored crosshair preparation.** Optional HUD bindings are verified before
  the core hooks change native code, fixing the logged `hud_target_release`
  rejection. Native HUD failures remain isolated from the working VR view.
- **Useful Anniversary HUD diagnostics.** Any remaining replay refusal names
  the actual native guard instead of grouping unrelated failures as error 1.

Controller-directed shots, independent tracked hands, native marker-driven
weapon effects, the single-eye desktop mirror and scene-visibility correction
are retained. This candidate introduces no speculative shot-origin rewrite,
performance tuning or guessed per-weapon offsets.

## Local verification and limits

Packaging runs the cumulative Release build, all CTest suites and the Reach
consistency gate for the exact source commit. The pre-package cumulative
build and all 20 suites passed. Additional checks pass 101 pinned contracts,
15 mapped-image binding groups, 30 native Original lens cases, native
Anniversary shader/skin upload, eight target-restoration cases, 27 native
camera conversions, 10 native scene-refresh cases and 12 official weapon
graphs. Records are in the accompanying source evidence documents and the
local `out/ce-refinement-*` files.
The package manifest and SHA-256 sidecar identify the exact binaries/source.

These checks do not establish headset perfection. In particular:

- The camera-switch, weapon/effect-lens and pause changes need your visible
  test in both CE graphics modes. Every weapon, reload, muzzle effect, vehicle
  and campaign state has not been tested in a headset.
- Anniversary gameplay-HUD replay still had zero successful draws in your
  previous log. Its specific native admission failure was not recorded there;
  the new detailed reason makes a remaining refusal attributable. This is
  separate from the pause presentation correction.
- The engine retains actual firing origin and native spread. Previous
  `be2140f` feedback and its log confirmed controller-directed shots; the
  `a2b526a` log records no shot-hook calls and does not establish firing coverage.
- Your logs do not establish 90 Hz performance. Anniversary rendered slower
  in several measured windows; no new frame-rate claim is made.
- Multiplayer menus that keep simulation running are not established by the
  native single-player pause flag. Full roomscale body following, physical
  melee and world collision remain deferred for CE. Other standing work,
  including dual trajectory, vehicles and zoom, remains preserved.

The accepted cumulative pointer stays at `4e01f28`. The positive CE visibility
result is preserved; this candidate has not yet received headset acceptance.

## Short test

1. In CE, turn your head and move slightly, then switch Original/Anniversary
   both ways. Facing and physical scale should stay consistent.
2. Compare the pistol and another available gun in both modes: hand/gun size,
   support grip, reload, shots and muzzle flashes should follow the weapon.
3. Pause and resume in each graphics mode. The menu should be visible and
   usable, then return to the same VR view.

Halo 3 served as a regression check because shared VR presentation/input code
also affects it. Your recovery result is recorded: it initially stayed flat,
then recovery completed after the native load gate admitted it. This CE package
does not redesign that recovery mechanism. The CE-scoped pause changes still
touch shared presentation/input callsites, so the shared-code regression
requirement remains, without repeating the entire Halo 3 checklist.

Delivery is build ZIP plus matching source ZIP, then wait for your testing.
No installation, game launch, game-folder changes or publishing is performed.
