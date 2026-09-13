# Latest user override - September 13, 2026

Continue Halo CE VR. Do not package a ZIP until the implementation is reasonably
expected to function with 6DoF comparable to the other games. This supersedes
any earlier suggestion to package an injection-only or research milestone.
No installation, MCC launch, game-folder writes or publishing. Physical melee
and world collision remain deferred until the user confirms CE injection.

September 13 later continuation: CE loaded-image verifier/native camera rebuild
adapter and explicit active/copied-list receipt logic are now implemented and
tested. Read the NEW later-continuation section of HALOCE-BRINGUP-2026-09-12.md
and E-CE-7 in HALOCE-RENDER-EVIDENCE.md. `0x4556B0` signals completion; it is
NOT a worker wait. Native scope/scheduling, GPU descriptor/lifetime/capture,
OpenXR/title admission and Classic/controller/HUD integration remain unfinished.
Six Release tests, mapped pinned-PE verification, SHA/witness verification and
Reach consistency pass. No hooks/injection, native game code execution, ZIP,
install or launch occurred. Preserve both editions, deferred tasks and accepted
pointer 4e01f28. Pre-edit WIP backup: out/checkpoints/20260913-155919-ce-runtime-resume.

Earlier September 13 continuation: CE private two-view preparation and native transfer
shape guards now implemented/tested. The previous full-frame replay direction is
not safe: CE consumes worker completion once per frame. E-CE-5/6 trace its native
two-view builder before culling and exact D3D surface handoff/variant selection.
Read the new continuation section of HALOCE-BRINGUP-2026-09-12.md and
HALOCE-RENDER-EVIDENCE.md before runtime integration. No CE runtime .cpp or
OpenXR/title admission is wired yet; no claim of functioning injection. Release,
five tests and Reach consistency pass; packaging remains held. Accepted pointer
stays 4e01f28. Do not repeat completed offline investigation or package scaffolding.

# ACTIVE — September 12, 2026: Halo CE Anniversary VR bring-up

LATEST delivery instruction: package a test ZIP as soon as the CE implementation
reaches a state reasonably expected to work. Do not wait for complete CE parity
or deferred vehicle/zoom/melee/collision work. Deliver matching source ZIP and
accurate implemented/unverified limits; no installation or MCC launch.

Latest CE steering: graphics switching must match H2's gesture (physical left
hand beside the left side of the head, click movement stick). Confirm VR injection
first; true physical melee and world collision are explicitly deferred until the
user confirms it. User permits using the existing CE VR mod as a reference while
acknowledging its different implementation. Verify MCC-native bindings independently.

User resumed and explicitly reprioritized: current vehicle controls are manageable;
checkpoint/defer that investigation. Focus now on Halo 1 / Combat Evolved
Anniversary matching the other supported titles: stereo injection, 6DOF, HUD,
crosshair and the same overall VR experience, translating proven workflows through
CE-specific evidence. This explicitly adds CE to scope, superseding older CE
exclusions. All-title zoom and H2 vehicle refinements remain deferred, preserved
in VEHICLE-ZOOM-PAUSED-2026-09-11.md; their old combined packaging hold does not
require completing them before CE work. No new packaging instruction yet.

Verified source at resume: cfb22eda5c968fc10b1083b56817a710855bd546, with only
the three preserved checkpoint documents modified/untracked. No vehicle changes
were implemented. CE registry entry exists but grants zero capabilities; no
CE-specific adapter/render/evidence files found on initial source search.
Read HALOCE-BRINGUP-2026-09-12.md for current CE progress and exact blockers.
Preserve existing title behavior, both MCC editions, and accepted pointer 4e01f28.
No installation, MCC launch, game-file modification or GitHub action is authorized.

# Historical pause — September 11, 2026: weekly usage checkpoint

User explicitly requested saving this checkpoint and stopping; wait until they
say to resume. Latest exact stopping point is the new first section in
[VEHICLE-ZOOM-PAUSED-2026-09-11.md](VEHICLE-ZOOM-PAUSED-2026-09-11.md).
Verified HEAD remains cfb22eda5c968fc10b1083b56817a710855bd546. This session
changed checkpoint documents only: NO vehicle/zoom source edits or tests yet.
The proposed stable seated heading reference and convergence test were announced
but NOT written. GitHub release task was cancelled because user uploaded it.
Preserve main-gun-hand-directed vehicle controls; do not replace with wheel/stick.
H2 AI feedback and source/history explanation are recorded in the detailed pause.

# Historical resume — September 11, 2026: Halo 2 vehicles and all-title zoom

LATEST vehicle clarification: user observes the other games following the right
controller/main gun hand wherever it points and explicitly wants that retained.
Match that controller-directed steering/aim in H2. Do not substitute raw-stick
or wheel steering as the requested default, or change the other titles' working
behavior. Existing optional controls are not a reason to change this priority.

Latest user instruction: disregard GitHub release work (user uploaded it
manually), resume the MOST RECENT progress, prioritize H2 Classic/Anniversary
vehicle controls matching the other titles, then H3's zoom box in the remaining
supported titles. Verified starting state: HEAD cfb22ed; only the existing
checkpoint/standing-list edits and VEHICLE-ZOOM-PAUSED-2026-09-11.md were dirty.
No post-delivery feature implementation existed. Resume the investigation in
that document; preserve the delivered build's changes. No GitHub actions.

User reports H2 AI is now more responsive/attentive in both renderers and appears
fixed. Preserve its current behavior; inspect existing source/history only unless
new evidence makes a change necessary. No new log or exact test identity supplied;
this is positive user feedback, not cumulative build acceptance. Vehicle controls
and zoom remain the priorities. Packaging stays held until both are implemented
and checked, then deliver build/source ZIPs without installation or game launch.

# Historical pause — September 11, 2026: exact delivered-build continuation

User requested a checkpoint and STOP; wait for their explicit resume instruction.
Read [VEHICLE-ZOOM-PAUSED-2026-09-11.md](VEHICLE-ZOOM-PAUSED-2026-09-11.md)
for the recovered prior-chat sequence and exact investigation stopping point.
The cfb22ed build/source ZIPs were ALREADY DELIVERED, followed by the separate
GitHub root cleanup c57160d. HEAD remains cfb22ed. No feature source changes were
made after that delivery; this session performed read-only investigation only.
Next requested work is H2 Classic/Anniversary vehicle-control parity and H3-style
zoom screens in the other supported titles. Do NOT package until BOTH additions
are implemented and checked. Preserve all delivered progress. The older
roomscale delivery instructions below are historical, not outstanding work.

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

Recovered starting point was clean HEAD 644148a; the previous chat only investigated
after receiving feedback. The refinement implementation is recorded below.
Downloaded reference: out/release-reference/moistman42069/ (release metadata and
source ZIP). Log: out/test-runs/644148a-roomscale-left-hand-feedback/user.log
SHA256: 8E1FF4F9567BDCC62F669E04EE7C511AA51364D036270841C5BB682C28FFE75A.
Steam / SteamVR OpenXR 2.17.9 / Oculus-family headset, panel 120 Hz. Exact headset
model not identified by this log. Accepted pointer stays 4e01f28: no cumulative
acceptance of this failed roomscale/alignment package.

## Refinement implementation/package resume point

Latest recovered request above is now implemented locally. Left-hand default
restoration and experimental toggle: d5bed1e; failed roomscale disabled first
in f868203. Corrected roomscale has all-title admission and one VR input merge
per nested native XInput poll. See LEFT-HAND-REFINEMENT-2026-09-10.md and
ROOMSCALE-REFINEMENT-2026-09-10.md for source evidence and validation limits.
Experimental anatomical correction remains unproven; default released placement
is the fallback. Weapon-bound and melee improvements preserved, further melee
work paused. User-friendly package notes are ROOMSCALE-LEFT-HAND-RELEASE-NOTES-2026-09-10.md.
Release build, 3 CTest suites (including actual roomscale transport fixture and
60/90/120 Hz simulation), and Reach gate pass. Packaging repeats checks at its
final committed identity; both ZIPs and hashes go under out/candidates. Deliver
the build and matching source ZIP here, then wait for testing/instructions.
Do not redeliver 644148a as the update. No headset acceptance of these fixes yet.


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
