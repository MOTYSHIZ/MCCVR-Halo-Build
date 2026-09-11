# Latest priority - recovered September 10 headset feedback

The roomscale package 644148a was tested: user reports improved collision/melee,
bad left-hand misalignment and roomscale body movement doing nothing, including H3.
Preserve collision/melee improvements and pause that work. Restore left-handed
presentation from the latest user GitHub release (MCCVR-d77c9dd) as default;
gate newer anatomical correction behind default-off Fix Hand Alignment
(Experimental), available only with left-handed mode enabled. Improve experimental
alignment if evidence permits, without blocking the stable fallback. Then diagnose
and fix real physical movement moving the native body across supported titles,
without drift, duplicate movement, height errors or breaking sticks/controller aim.
Read ROOMSCALE-LEFT-HAND-REFINEMENT-REQUEST-2026-09-10.md for the full recovered
user message and packaging requirements. After implementation/checks, deliver a
clean user-facing build ZIP plus matching source ZIP with accurate fresh/update
installation instructions. No install, game writes, launch, PR or publishing.
The older immediate-delivery handoff below is superseded.

Resume: clean HEAD 644148a; previous chat only investigated after receiving feedback.
Downloaded reference: out/release-reference/moistman42069/ (release metadata and
source ZIP). Log: out/test-runs/644148a-roomscale-left-hand-feedback/user.log
SHA256: 8E1FF4F9567BDCC62F669E04EE7C511AA51364D036270841C5BB682C28FFE75A.
Steam / SteamVR OpenXR 2.17.9 / Oculus-family headset, panel 120 Hz. Exact headset
model not identified by this log. Accepted pointer stays 4e01f28: no cumulative
acceptance of this failed roomscale/alignment package.

# Active MCCVR work checkpoint - September 10, 2026

## Current roomscale/package handoff (supersedes every older stop/hold below)

User resumed with: "get roomscale working on all games and package an updated
build with updated install instructions and details within it". During this chat
user chose **Preserve controller aiming for this package** when told H3/H4 native
body yaw follows gun aim. Thus horizontal roomscale body movement and head-relative
walking are this package's scope; independent head-following body yaw is deferred,
not completed. User then said continue/forget nothing.

Implementation: src/common/roomscale_logic.h and src/dll/roomscale.{h,cpp}, config
roomscale_movement (default off), F1 Controls, native XInput walking and all five
title camera integrations (both H2 renderers). Native travel consumes horizontal
tracking reference only; no new native hook, teleport, velocity or guessed field
write. Positive native on-foot admission, manual-stick priority, 100 ms command
expiry, generation/input-epoch checks and tracking-jump resets. See
ROOMSCALE-IMPLEMENTATION-2026-09-10.md and ROOMSCALE-CANDIDATE-2026-09-10.md.

Preserved cumulative local anatomical handedness, equipped-model bounds/melee,
snap turning, slider arrows, recovery/lifecycle and vehicle guards. H3 dual firing
remains disabled by 9569690. Prior worktree backup, including the half-written
roomscale helpers recovered at chat start: out/checkpoints/20260910-roomscale-resume-161058.

Release build, 3 CTest suites, Reach consistency and pinned legacy/H4 weapon-bound
verifiers pass locally; packaging repeats build/tests at the committed identity.
Package command now writes build ZIP, matching git source ZIP and SHA256 sidecar.
Run tools/package-candidate.ps1 WITHOUT -Install. Updated MANUAL-README.txt says
KEEP saved config and accurately describes titles, recovery and unresolved work.
Do not ship the stale September 9 melee notes as current candidate instructions.

Delivery: attach BOTH ZIPs in chat, then WAIT for user testing/instructions.
No install/game-folder writes, launch, PR or publishing. Do not advance
CURRENT-STATE.md; accepted source remains 4e01f28. No headset result for the new
roomscale/snap/handedness/bounds work. Exact package commit/hashes are in its
manifest/ZIP filenames under out/candidates; the latest roomscale package is the
handoff, not an accepted pointer. Further refinements remain in the standing list.

## Historical checkpoint entries (preserved for continuity)

# Active MCCVR work checkpoint — September 9, 2026

## Latest continuation instruction (supersedes the historical WIP hold below)

LATEST STOP/HANDOFF: user asked to finish snap turning, resend the full goals
list with completed items marked, THEN WAIT for their instruction before the
next task (roomscale movement). Do not start roomscale automatically. No ZIPs
requested. Weapon-bounds/melee coverage and snap-turn implementation passes
are complete locally. Read SNAP-TURN-STATUS-2026-09-10.md: H2's missing snap
path was added for both renderers; H3/ODST/Reach/H4 handlers and handoff guards
audited. Release/3 CTest pass; headset validation remains pending. Full status
ledger is in CONTINUATION-REFINEMENT-LIST.md. Preserve all uncommitted WIP.

Latest September 10 steering: finish the current weapon-bounds AND physical-melee
work, then verify/fix snap turning across ALL supported titles, report its actual
status, then implement an optional true roomscale tracking toggle. The requested
behavior is physical movement moving the character body and head turning making
the body follow. User confirmed: follow physical position AND head direction, including movement heading. This
inserts snap turning and roomscale ahead of the retained vehicle implementation
queue. Packaging remains on hold. This instruction preceded the completed local
snap-turn pass documented above; runtime acceptance is still pending.

Current weapon/melee progress: H3/ODST/Reach/H4 runtime equipped-model bounds
implemented and locally validated, including per-layout weapon-only melee
regression with stationary hand samples. Read RUNTIME-WEAPON-BOUNDS-2026-09-10.md
for native proof and explicit limits. H2 retains its live-verified reader.
Coverage implementation pass complete; headset results and the broader melee
refinement list remain pending. Snap-turn implementation also completed locally;
wait for user instruction before roomscale, per the newer stop above.

Latest steering also confirms BOTH weapon bounds and physical melee fixes are
in scope. Finish that work, then verify/report whether H2 vehicle controls use
the other titles' control method. A minor third-party H2 reticle-after-tank-exit
report is stored in docs/bug-reports/halo2-tank-exit-reticle.md and its image;
defer investigation. Do not confuse that report with the active vehicle audit.

LATEST user steering: Halo 4 damage black-screen/fade is DEFERRED. Disregard
that investigation until the user explicitly asks to resume it. Retain the
report only; do not spend implementation or research time on it. Physical melee
and automatic equipped-weapon contact remain the current active section.

September 10 latest completed section: all-title anatomical left-hand local
implementation and validation complete. See LEFT-HAND-IMPLEMENTATION-2026-09-10.md
for exact scope, tests and headset caveats. Release build, all three tests and
Reach gate pass. User informed of completion with headset validation pending.
Current section: automatic equipped-model weapon contact and physical melee.
The supplied H4 log is preserved under
out/test-runs/d77c9dd-20260910-weapon-contact-damage/user.log. Its frequent
hand-only fallbacks warrant model-bound/identity investigation. Do not claim
universal weapon collision or the damage blackout is fixed yet.

LATEST September 10 log/priority steering: finish all left-hand work, then
automatic equipped-model weapon contact/true physical melee (including modded
weapons), then H2 vehicle controls before first-person vehicles. Keep optional
dual trajectory and all other retained tasks afterward. New black-screen/fade
on damage (Promethean Knight melee example) is unresolved, not a proven effect
diagnosis. Supplied attachment 4ab61705-1189-4ecf-9a41-9bebeec16a0c/pasted-text.txt
identifies d77c9dd; verify identity rather than assuming the user's description
of the latest GitHub build means current worktree behavior.

September 10 steering: item 10 (slider precision arrows) is implemented and
explicitly removed from pending work; preserve it. User also explicitly requires
the manual VR force-injection/recovery button for failures to enter VR. Existing
F1 and launcher controls currently request H3 recovery only; retain that scope
limitation until additional title recovery is implemented and validated.

The user approved and requested persistent storage of the full 16-item list in
CONTINUATION-REFINEMENT-LIST.md. On every future "continue", use that list and
this checkpoint. Current task: verify existing stability/recovery work, report
its limits, then finalize anatomical left-handed support across all five titles
(H2 Classic/Anniversary), followed by optional independent dual trajectory.
Local implementation/builds/tests authorized; no packaging requested now.

Recovered HEAD is 9569690, which disables the d77c9dd H3 dual-fire experiment
after headset failure. The older "H3 implementation pending acceptance" text
below predates that failure. Accepted pointer remains 4e01f28. Preserve existing
uncommitted recovery/lifecycle, vehicle guard, H2, launcher/menu and test edits.
Current recovery implementation is being audited; do not assert all-title or
headset-confirmed recovery. The older September 9 WIP handoff is historical.

September 10 verification: recovered cumulative Release build, all three CTest
targets (including synthetic hook retirement/recovery-event tests), and Reach
gate pass. See RECOVERY-STATUS-2026-09-10.md for exact covered behavior and limits.
Proceeding with anatomical handedness, starting with both H2 renderer packets.

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
