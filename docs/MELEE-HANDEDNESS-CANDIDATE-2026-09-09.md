# Handedness, H2 dual aim, ODST contact and transition recovery test

This is an unaccepted continuation of the headset-tested `4e01f28` baseline.
It supports Halo 2 Classic/Anniversary, Halo 3, ODST, Reach and Halo 4 on both
Steam and Microsoft Store. The exact source commit and file hashes are in
`CANDIDATE-MANIFEST.json`. Packaging performs no installation or game launch.

## Changes to test

- **Left-handed main weapon:** in F1, Weapon & Aim, enable this option to put
  primary weapon tracking, aiming and firing on the physical left controller.
  The right controller becomes support/secondary. Sticks, face buttons and the
  selected physical D-pad hand keep their existing assignments. Contact haptics
  follow the physical hand. This is controller-role routing; anatomical mesh
  mirroring is unfinished, so the authored hands/arms may look wrong.
- **Crosshair direction:** the Crosshair category now contains trajectory
  pitch/yaw/roll and a reset button. Those controls adjust shots and the reticle.
  Visual gun/stock and support-hand positioning remain in Weapon & Aim. Moving
  the support-hand art no longer moves the two-controller aiming line.
- **Halo 2 dual aim:** while two owned weapons are equipped, each gun gets its
  own controller ray. Both Classic and Anniversary need testing. Single-weapon
  aim retains the prior path. The hook rejects stale ownership/tracking and
  falls back independently if its native binding cannot be verified.
- **ODST secondary contact:** the documented secondary weapon palette now owns
  its own cached weapon bounds and left-role contact samples. This addresses a
  source-level restriction, but the reported Mythic Overhaul SMG miss's cause
  is not proven. Unknown custom models still fall back to hand bounds.
- **Halo 3 transition recovery:** a camera hook that stops producing samples
  for two seconds is retired by the worker. Reinstallation must pass the normal
  level-liveness checks again. This targets the supplied multiplayer transition
  report and requires an actual session/level-transition test.

## Known unfinished scope

Ordinary-campaign dual acquisition in ODST, Reach and Halo 4 is **not enabled**.
Complete independent secondary firing in H3/ODST/Reach/H4 is not implemented by
this candidate. Their existing native/modded behavior remains in place. These
requirements are retained in `docs/MELEE-HANDEDNESS-DUAL-WIELD-2026-09-08.md` in
the matching source ZIP; they are not being marked complete or abandoned.

Fully unarmed and secondary-weapon melee damage-response selection also remain
unfinished. Gesture mode and those damage-selection cases were not separately
accepted by the previous ordinary physical-melee headset report.

## Headset checks

1. Start with the accepted settings and check Halo 3 melee, aiming, hands and
   pause/resume. Then exit/re-enter a level and repeat the multiplayer session
   transition that previously left stereo off.
2. In Halo 2, equip two weapons and aim/fire them in different directions.
   Repeat in Classic and Anniversary. Check firing both at once and dropping or
   swapping the secondary.
3. Enable left-handed main weapon and confirm the main weapon/trigger, support
   hand and contact haptics follow the intended physical hand. Check the other
   supported titles too; note any wrong arm or hand appearance.
4. Adjust visual gun/stock and support-hand placement while aiming at a fixed
   target, including two-handed aim. The trajectory should stay fixed. Adjust
   Crosshair direction separately and confirm shots follow it.
5. Recheck the ODST Mythic Overhaul secondary SMG contact case with the original
   melee settings. Retain the log even if the strike still fails.

Report the title, MCC edition, OpenXR runtime/headset, result and matching log.
The accepted-build pointer stays at `4e01f28` until explicit headset acceptance.
