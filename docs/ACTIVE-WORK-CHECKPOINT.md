# Active MCCVR work checkpoint — September 9, 2026

**User-requested WIP packaging checkpoint.** The user interrupted further
development due to usage limits and asked to wrap up/package. After delivering
the build/source ZIPs, wait for testing and new instructions. Do not continue
feature development automatically. This explicit WIP request overrides the
earlier packaging hold for this handoff only.

Read this alongside CURRENT-STATE.md and
MELEE-HANDEDNESS-DUAL-WIELD-2026-09-08.md when resuming. Preserve unfinished
worktree files; a new chat is continuation, not a request to reset progress.

## Latest user steering

- Focus now on optional left-handed main weapon/aim and independent dual wield.
  The user explicitly released the earlier stop-everything flat-mode priority
  after learning that an H3 recovery change already exists.
- Flat-mode recovery is NOT fully confirmed: H3's 2-second stale-camera
  retirement exists in 11eb89e but has no headset acceptance. The multiplayer
  logs are preserved and hash-verified in
  out/test-runs/4e01f28-multiplayer-feedback/{user,friend}.log. The user's
  stereo stops at 10:35:34.999 without later hook retirement/reinstall; the
  friend records a new install and stereo recovery. Exact loader cause remains
  unproven. Audit also found H4's sceneTargetMissing latch after repeated
  uncaptured eyes; it disables stereo for the level. No new recovery edits
  were made in this session. Keep this on the unresolved ledger.
- Preserve physical melee/world contact, weapon alignment and reticle sliders,
  slider precision arrows, and restoration of snap turning in the full scope.
  Slider arrows are implemented in the existing worktree and previously passed
  build/tests; do not redo or discard them. Snap turning remains to investigate.
- Do not package until the requested refinements are complete, unless the user
  explicitly requests a WIP ZIP due to usage limits. In that case checkpoint
  exact progress, list unfinished items and what may work in the ZIP, and
  deliver build and matching source ZIPs. Never install, launch, write game
  folders or publish/open a PR without a new explicit request. Both editions.

## Recovered state

HEAD 11eb89e descends from accepted physical-melee source 4e01f28. Acceptance
pointer is unchanged. Existing modified/untracked files are backed up with
hashes and a binary patch in out/checkpoints/20260909-handedness-resume-*.
No candidate ZIP was created. The older September 8 paused checkpoint is
historical; its missing-implementation claims have been superseded.

Existing handedness swaps primary/support pose, velocity, trigger/grip and
haptic roles while retaining physical sticks/buttons and D-pad preference.
It has a default-off F1 option, but anatomical hand/arm presentation is still
unfinished. H2 has an optional verified firing scope for independent rays.
H3/ODST/Reach/H4 direction, presentation and ordinary-campaign acquisition
(ODST/Reach/H4) still need work. Detailed retained evidence and prior validation
are in MELEE-HANDEDNESS-DUAL-WIELD-2026-09-08.md.

## Work completed during this session

- Added H2/H3/ODST secondary-presentation exclusion from support-grip coupling,
  independent of world collision/melee. Generation + 150 ms expiration,
  clock-wrap coverage, release-before-regrab on dual drop or handedness change.
- Implemented optional H3 independent firing rays using H3EK-matched retail
  3683A0 -> 3524B0. Native origin preserved; full local weapon/owner handles,
  title/tracking generation, age and install checks. Coherent independent
  controller publication, no render/firing locks, producer/consumer quiescence,
  isolated failure and worker telemetry. This supersedes the earlier statement
  that H3's implementation is entirely missing. Headset acceptance is pending.
- Added tools/verify-halo3-dual-bindings.py. It and the recovered H3 melee
  selector verifier pass against the pinned module. Initial cumulative Release
  build, both CTest targets and Reach consistency gate pass; packaging repeats
  build/tests for the final committed identity. Package manifest identifies it.
- Recovered and retained all prior sliders, melee range, contact smoothing,
  world-object target and H3 response-selection edits. None is newly accepted.
- Full user-facing scope/risks: MELEE-HANDEDNESS-CANDIDATE-2026-09-09.md, copied
  to MELEE-CANDIDATE-NOTES.md in the build ZIP. Accepted pointer stays 4e01f28.

## Exact resume point

No anatomical mesh change was made. H2 review stopped at
Halo2OwnFinalFirstPersonPackets and Halo2OwnDualFirstPersonPackets in
src/common/halo2_render_logic.h. They still bind anatomical right to primary
and anatomical left to support/secondary even after controller roles swap.
Do not fix this by blindly swapping carriers: main-gun ownership, authored grip
relation, contact volume ownership and both renderer packet paths must agree.
H3/ODST shared solver ReconstructVisiblePaletteSource has the same role/anatomy
distinction. More details and evidence: DUAL-WIELD-REFINEMENT-2026-09-09.md.

Next, subject to the user's test results: anatomical left-hand presentation;
ODST/Reach/H4 independent firing and ordinary-campaign acquisition; remaining
melee/contact/alignment cases; restore snap turn. Retain the unconfirmed
multiplayer recovery and H4 capture-latch finding. Do not assume this WIP ZIP
completes any of those requirements. Existing out/ evidence remains preserved.
