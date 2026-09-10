# Roomscale movement candidate - September 10, 2026

This package continues the accepted `4e01f28` baseline with the preserved local
refinements and optional roomscale body movement. Its exact source and binary
hashes are in CANDIDATE-MANIFEST.json. It is **awaiting headset testing**.

Supported: H2 Classic/Anniversary, H3, ODST, Reach, H4; Steam and Microsoft Store.
Halo CE is outside current VR coverage. Use MANUAL-README.txt for installation.
**Keep your existing config when updating.** New settings inherit source defaults.

## New in this handoff

- Default-off roomscale body movement in F1 > Controls. Physical horizontal
  displacement drives ordinary native walking, and observed character travel
  consumes the tracked offset to avoid moving the view twice. No teleport or
  guessed character-position write is introduced.
- The user explicitly chose to preserve controller aiming for this package.
  Walking remains head-relative; independent head-following native body rotation
  is deferred. This is not a claim that body yaw has been decoupled from gun aim.
- Preserved anatomical left-hand presentation across the supported titles,
  automatic equipped-model weapon bounds, physical-melee coverage refinements,
  slider precision arrows and recovery/lifecycle work from the prior session.
- H2's new snap-turn path serves both renderers. H3/ODST/Reach/H4 snap handlers
  and title-handoff guards were audited. These local changes need headset tests.
- Corrected installation/update guide, config description and stale feature
  status. The failed H3 independent dual-fire experiment remains disabled.

## Test this build

1. Install manually with MCC closed, preserving your config. Enable Roomscale
   body movement, head tracking and positional tracking. Recenter with F3.
2. In a level open area, leave the left stick idle. Take a small forward,
   backward and sideways step. Check that your character follows without the
   world moving twice, sliding continuously or oscillating after you stop.
3. Face another direction and repeat; then use the movement stick. Forward
   should stay head-relative and manual movement should take priority. Check
   controller shooting, hand/gun placement, collision and physical melee.
4. Approach a wall and a step. Normal character collision should remain active;
   this does not prevent tracked head leaning through geometry. Test F3, toggle
   off/on, pause/resume, F1, D-pad gesture and tracking interruption for drift.
5. Enter/exit vehicles and turrets, die/respawn, load another level and switch
   titles. Body following should suspend outside eligible on-foot play. Check
   H2 in BOTH renderers and run a Halo 3 regression. Check snap turns separately.
6. Send the log and result for each tested title, edition, runtime and headset.
   `Roomscale:` lines report admitted/unavailable samples and movement polls.
   An unavailable optional feature leaves the existing VR camera active.

## Limits and retained work

The new roomscale controller is locally validated, not headset-proven. Catch-up
speed depends on native movement and input settings. Vertical head movement
remains leaning/crouching; it is not physical jumping. A large tracking jump,
teleport or stale sample resets demand rather than causing an unattended walk.
Movement-stick use cancels accumulated follow demand. Unknown native occupation
leaves roomscale unavailable; it does not disable the camera core. H2 uses fresh
on-foot and first-person-packet evidence because it has no installed cinematic
classification publisher; unarmed/hidden-hand and cinematic transitions require
particular testing. H4's on-foot proof reuses its existing native contact binding.

The accepted melee report does not prove all new model bounds, all custom weapons,
unarmed/secondary damage or every damageable object. Independent dual trajectories
and ordinary campaign dual acquisition in ODST/Reach/H4 remain unfinished. H3's
failed dual-fire hooks stay disabled; do not infer independent dual aim from
left-handed presentation. H2 retains its optional independent firing path.

Manual recovery remains H3-only, and broad all-title transition recovery is not
accepted. H4's reported damage blackout and the minor H2 tank-exit reticle report
remain deferred. Further contact smoothing, per-weapon calibration, full scope
parity, visibility edges and H2/H4 first-person vehicles remain on the standing
list. The complete ledger is in the matching source ZIP under docs/.

No game installation, launch, PR or publishing was performed for this handoff.
The accepted pointer remains unchanged pending the user's headset result.
