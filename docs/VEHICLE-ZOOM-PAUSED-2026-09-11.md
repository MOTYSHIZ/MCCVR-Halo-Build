# Vehicle and zoom continuation — paused September 11, 2026

## September 12 priority change

User says current vehicle control is manageable and requests checkpointing it
here while focusing on Halo CE Anniversary stereo/6DOF/HUD/crosshair bring-up.
Preserve all findings below, with NO vehicle source changes implemented. Do not
resume vehicle or all-title zoom refinements ahead of the new CE priority.

## LATEST pause: weekly usage limit, after controller-steering clarification

User: "usage is almost done for the week. save checkpoint here and i will tell
you when to resume later". STOP and wait for explicit resume. This section
supersedes the older investigation priorities below where they conflict.

### Verified exact state

- HEAD remains `cfb22eda5c968fc10b1083b56817a710855bd546`.
- Only checkpoint documents were edited this session. No feature source edits,
  commits, builds, tests, package creation, installation or MCC launch occurred.
- `ACTIVE-WORK-CHECKPOINT.md` and `CONTINUATION-REFINEMENT-LIST.md` remain
  modified, and this detailed checkpoint remains untracked; preserve all three.
- User manually uploaded the GitHub release and explicitly cancelled that task.
  No release/tag/asset writes were performed by this assistant. A GitHub new
  release form was opened but never populated/submitted; do not resume it.
- Accepted pointer remains `4e01f28`; do not advance it from this feedback.

### Latest user requirements, overriding wheel/stick interpretation

User reports all other supported games currently steer/aim vehicles wherever
the right controller / MAIN GUN HAND points and explicitly wants that retained.
Implement this same hand-directed behavior in H2 Classic AND Anniversary.
Do not replace the requested default with raw-turn-stick or two-hand wheel
steering; do not change the other titles' working vehicle controls. The older
seat/vehicle-type/wheel investigation below is retained evidence, not the next
required implementation. Correct handedness must follow the main gun hand.

Then add H3-style weapon-side zoom boxes to H2 Classic/Anniversary, ODST, Reach
and H4, preserving H3. Packaging remains held until BOTH additions are implemented
and checked. Then build/source ZIPs only, without -Install; no GitHub work, game
folder writes or launch. Other standing tasks and deferred issues remain intact.

### H2 AI feedback and source-history check (no AI edits)

User reports more responsive/attentive AI in BOTH H2 renderers, apparently fixed;
asked to double-check but preserve behavior unless a change is demonstrably needed.
Source/history check found commit `8240c87` changed
`Halo2NativeAimUpdateDetour` in `src/dll/halo2_observer_6dof.cpp`: the original
native updater now runs whenever the exact local-player override was NOT applied,
including enemies/allies. Previously original dispatch was gated by `!directVrAim`
before ownership was established, skipping non-owned units while VR aim was active.
Current source retains the fix (around lines 3182-3200), with
`g_nativeAimStockPassed` telemetry. This is consistent with the positive report;
no new log/mission/build/edition/runtime/headset identity was provided, so do not
claim a newly reproduced runtime cause or cumulative headset acceptance. No AI
behavior change is needed on present evidence.

### EXACT investigation stopping point (no implementation yet)

The assistant announced a stable seated heading reference and convergence test,
but user paused BEFORE either was written. Resume by proving the camera/control
math in a focused test; do not assume the announced change already exists.

Verified source relationships:

- `Halo2BuildTrackedCenterCamera` (`src/common/halo2_render_logic.h`, ~1181)
  composes headset orientation relative to recenter onto the STOCK observer yaw.
- `Halo2BuildControllerCarrier` (same header, ~1388) maps controller-relative-to-
  head pose through that tracked camera. Therefore the desired controller world
  yaw inherits current stock yaw.
- `ComputeHalo2ControllerAimStick` (`src/dll/game.cpp`, ~43798) builds the carrier
  from `publication.tracked`, then measures yaw error against `publication.stock`.
  For a fixed hand/head offset, native yaw appears in both sides and cancels;
  this is a source-derived explanation for a non-converging steering command,
  NOT a headset-confirmed diagnosis. Write a regression reproducer first.
- Candidate approach: maintain a stable seated orientation reference so the hand
  selects a world heading and native right-stick feedback can converge to it.
  Keep visible head/hands/reticle/aim in the SAME adjusted frame. Changing only
  servo target math would leave visible aiming inconsistent and is insufficient.
- Observer integration point: `ApplyTrackedPose` area ~913-1008 in
  `halo2_observer_6dof.cpp` (verify function name/lines before editing). Existing
  recenter resets `g_reference`, `g_snapTurn`; seated state cancels pending snaps;
  `Halo2SnapReference` produces the effective reference before camera construction.
  Preserve on-foot snap/smooth behavior and roomscale admission/offset consumption.
- Useful existing math: `src/common/halo2_snap_turn_logic.h` and tests at
  `tests/core_tests.cpp` ~16430-16494. Existing tests demonstrate keeping view/gun
  at a fixed target while native yaw converges, including yaw seam, frame rates
  and inverted axes. Reuse those proven transforms deliberately for seated aim.
- `Halo2ObserverPosePublication` (~1680 in render logic header) publishes stock,
  tracked, effective reference, generation, serial, ring index and coherent pose
  snapshot. It currently has NO seated-reference validity/identity field. Ensure
  input cannot consume stale/on-foot publication immediately after seat entry.
- Current `g_vehicleSeatSample` is an atomic packed timestamp+seated bit, published
  in the exact local-unit native aim callback (~3155), cleared at install/teardown,
  and expires after 100 ms. `VehicleControlActive` and `OnFootFresh` consume it.
  Only the signed seat index +0x210 is verified by the existing native predicate.
  No new parent/type field binding was proven this session. If adding seat identity,
  use verified fields and coherent bounded publication, not a guessed layout.
- Preserve native updates for seated units and ALL non-owned units. Handle seat
  changes, recenter/tracking epoch, generation/title changes, stale samples, paused
  input, disabled VR aim and exit without persistent steering/view offsets. Keep
  render hooks free of logging/allocation/COM/locks. Optional failure stays local.
- `Game_ComputeAimStick` ~44298 admits H2 only while seated and obtains
  `VR_GetAimPose`, which follows main-hand routing. Avoid changing shared H3 behavior.

Zoom investigation remains exactly as recorded below: no implementation. H3's
third-view render/cache, H2's mandatory render-then-copy path and the distinct
ODST/Reach/H4 per-eye transactions require deliberate title-local integration.

### Failed read-only tool invocation

Attempted a further kit string search via analyzeHeadless using a needle containing
`vehicle_definition->`. Windows batch parsing treated `>` as redirection and failed
with `> was unexpected at this time.` before useful analysis. Output:
`out/h2-vehicle-helpers-kit.txt`; no Java process remained at final checkpoint.
An accidental root file `vehicle_get_type` was created by that failed invocation;
it is moved into ignored out/ as `h2-vehicle-helpers-batch-redirection.txt` to
preserve evidence without leaving root clutter. Avoid shell metacharacters in
batch-script arguments; use simpler string needles or a purpose-built script.

## Earlier pause and recovered evidence (historical)

User explicitly requested: "save a checkpoint here and stop. i will tell you
later when to resume work." Do not continue until that instruction arrives.

## Exact prior-chat handoff recovered

Do not restart from the older September 10 roomscale implementation checkpoint.
The prior chat was recovered directly from the local transcript:
`C:/Users/Shadow/.codex/sessions/2026/09/10/rollout-2026-09-10T23-29-50-01a08e83-b069-72a1-913b-3a043c4d400b.jsonl`.

1. At 2026-09-11 08:27:06 UTC, the assistant delivered the build and source ZIPs
   for `cfb22eda5c968fc10b1083b56817a710855bd546`:
   `out/candidates/HaloMCCVR-cfb22ed-roomscale-left-hand-update-20260911-034759932Z-{Build,Source}.zip`.
   `out/candidates/cfb22ed-CHAT-DELIVERY.json` records that handoff. Both archives
   already exist and were verified. Do not repackage or redeliver this handoff
   as if it were unfinished.
2. User then asked to clean up the GitHub root. In a separate checkout
   `out/github-cleanup`, the assistant moved 141 historical files into
   `docs/archive/development-stages/`, fixed documentation links and pushed
   documentation-only commit `c57160d` to `moistman42069/MCCVR-Halo-Build` main.
   The development branch and delivered ZIPs were untouched. This is recovered
   transcript evidence; this session performed no new remote verification/write.
3. At 08:35:38 UTC the user asked:
   > If possible, see if you can package another build without regressing any of
   > the current progress. be sure you fix halo 2 vehicle controls to match the
   > other games along with adding the zoom in screen that halo 3 has to all
   > other games. do not package another zip until those two additions are made

The prior chat acknowledged that request, then was interrupted before any
feature edits. This chat recovered that exact sequence and investigated only.

## Preserved starting state and packaging hold

- HEAD remains `cfb22eda5c968fc10b1083b56817a710855bd546`; worktree was clean
  before writing this checkpoint. No source edits, new commits, build/test runs,
  new ZIPs, installation, game launch or GitHub writes occurred in this session.
- Preserve the delivered roomscale admission/nested-XInput corrections, restored
  released left-hand placement, default-off experimental alignment toggle,
  collision/melee improvements, slider arrows and existing recovery work.
- Accepted pointer remains `4e01f28`; no new headset acceptance was supplied.
- On resume: finish H2 Classic/Anniversary vehicle controls first, then H3-style
  weapon-side zoom screens in H2 Classic/Anniversary, ODST, Reach and H4. H3 is
  the reference and needs regression coverage. CE remains outside VR coverage.
- No packaging until BOTH additions are implemented and checked. Then package
  build + matching source with `tools/package-candidate.ps1` WITHOUT `-Install`.
  Both MCC editions remain supported. Do not install, launch, publish or open a PR.
- Retain the wider standing list; these two additions have priority. H2/H4
  first-person seats are a separate pending task, not silently included here.

## Vehicle investigation: verified source and outstanding evidence

Read `HALO2-VEHICLE-CONTROL-STATUS-2026-09-10.md`. Current source:

- `Halo2NativeAimUpdateDetour` in `src/dll/halo2_observer_6dof.cpp` (~3155)
  samples the exact local unit's signed parent seat, publishes a timestamped
  seated boolean, and preserves stock native aiming while seated.
- `Halo2Observer6Dof_VehicleControlActive` (~5747) requires native seat proof,
  armed gameplay and sample age <=100 ms.
- `ComputeHalo2ControllerAimStick` in `src/dll/game.cpp` (~43798) constructs a
  controller carrier from `publication.tracked`, aims through the hand reticle
  point, compares against `publication.stock`, and feeds yaw/pitch servos.
  `Game_ComputeAimStick` (~44295) routes seated H2 to it.
- H2 has no native seat/vehicle policy classification or wheel eligibility.
  Existing H3/ODST/Reach wheel plumbing is `Halo3SeatUsesWheel`,
  `Halo3SeatAuthorsSteeringNow` (~9082), `Game_Halo3UpdateVehicleWheel` (~43940),
  and the steering override (~44668). Two-grip double-click engages the wheel;
  a lone grip retains its native action. Follow-enabled look-steered drivers
  use raw turn stick or wheel; tanks/gunners retain controller aiming.
- H2 observer snap logic (~943) currently cancels snap while seated and excludes
  all seated states from turn ownership. Vehicle mapping must coordinate that
  with steering/wheel ownership. Existing roomscale admission must be retained.

Unproven hypothesis to investigate, NOT a finding: H2's desired controller
heading may follow the same stock observer yaw used for servo feedback,
preventing convergence while seated. Trace `Halo2BuildTrackedCenterCamera` and
carrier/reference math before changing it. Do not record this as a runtime cause.

Official H2EK investigation (read-only) preserved under ignored `out/`:

- `h2-vehicle-parity-kit.txt` / `.log`: `FindStringReferences.java` results.
- `h2-vehicle-seats-kit.txt` / `.log`: decompilation of kit RVAs `2CCB0`,
  `4DCD70`, `48EE40`.
- `h2-vehicle-layout-match.txt` / `.log`: BSim query of kit `1CA940` against
  the H2 retail database. Low similarities; NO homolog was established.
- `h2-vehicle-layout-retail.txt` / `.log`: examined candidates `6F3760`,
  `72B480`, `8C22B0`, `9D9330`. Do not use these as bindings. `72B480` is a
  basis validator, not the vehicle camera function.
- Earlier useful evidence: `out/contact-h2-vehicle-camera-body-console.txt`,
  `out/contact-h2-vehicle-native-console.txt`, and
  `out/contact-h2-vehicle-strings.txt`.

Kit facts observed (still need retail matching/unique proof before binding):

- Kit `1CA940` accesses unit parent at `+0x14`, parent seat at `+0x210`, vehicle
  definition seats at `+0x264`, stride `0xC0`; seat flag bit 7 selects an authored
  camera marker path. Parent forward/up are `+0x70/+0x7C` in this kit routine.
- Kit `4DCD70` scans seats and rejects duplicate driver flags using bit 2.
- Raw official kit enum table at RVA `AAE310`: human tank, human jeep, human
  boat, human plane, alien scout, alien fighter, turret (indices 0..6).
  Control table `AAE2F8`: normal, unused, tank (indices 0..2).
- Definition field records at `AAE740` (flags), `AAE750` (type), `AAE760`
  (control); enum descriptors `AAE32C` / `AAE304`. This identifies semantics,
  NOT the corresponding loaded retail definition offsets. Those remain open.

Tools/environment for continuing this exact investigation:

- Pinned inputs: `out/deps/re-tools/inputs/{halo2.dll,halo2_tag_test.exe}`.
- Ghidra project `out/deps/re-tools/projects/H2Aim`, containing both programs.
  Headless executable: `out/deps/re-tools/ghidra/ghidra_12.1.3_PUBLIC/support/analyzeHeadless.bat`.
  Use `-readOnly -noanalysis`, `tools/re` or `tools/ghidra` script paths.
- Set process JAVA_HOME to the JDK directory beneath `out/deps/re-tools/jdk21`.
  Python PE/disassembly dependencies require `PYTHONPATH=out/pydeps`.
- No Java process remained at checkpoint time. Headless output files completed.
- Next evidence step considered: find small kit seat/vehicle helpers, then
  verify their retail homologs; a structural scan of retail readers of the
  kit-explained fields may help matching. Never ship a BSim guess or copied H3
  struct/enum/seat assumption. No new helper/hook was implemented.

## Zoom investigation: verified source, no implementation yet

Halo 3 behavior to match: R3 release toggles a weapon-side 4:3 mono zoom screen;
native R3 zoom is consumed so gun/body and wide main headset view stay visible;
right-stick Y adjusts 6x..24x runtime magnification, reset on reopening; shared
placement/width and refresh-divisor settings apply. Account for left-handed
aiming, menu chord cancellation, title/level transitions and stale images.

- Pure logic: `src/common/scope_logic.{h,cpp}`.
- Input: `src/dll/input.cpp` ~141; `scopeAvailable` excludes camera-only bringup.
- Only H3 calls `VR_ScopeShouldRenderThisFrame` and renders the image today:
  `BuildRightHandScopeCamera` / `RenderViewHook`, game.cpp ~9898 / ~10224.
  H3 builds a third world view, redirects to a private scope cache and restores
  compact/derived primary/secondary cameras. Preserve its working behavior.
- `g_scopeRenderActive` already suppresses certain H3/ODST/Reach FP/HUD paths;
  inspect all consumers before extending ownership to another engine.
- `vr.cpp` shared cache ~3354, quad submission ~10763, render scheduler ~14066,
  begin/capture/end ~14099, reset ~14648. Quad uses aiming pose but the horizontal
  placement offset is currently applied directly; handedness needs review.
- `VR_BeginScopeRaster` currently requires `g_sceneColorRtv` and creates cache
  via `EnsureScopeCache`. Do not introduce allocation/COM into hot camera hooks;
  resource preparation and exact target ownership need deliberate design.

Native rendering routes to inspect on resume:

- ODST game.cpp ~13587..13752 already snapshots/restores root AND nested compact
  and derived blocks using `layout`, rebuilds via its own viewport/matrix helpers,
  uploads camera state, calls prepareView/original, then captures the eye.
- Reach game.cpp ~22558..22773 snapshots workspace/player-view, rebuilds native
  frustum/projection/camera state/matrices, calls `commitOuterCamera`, rearms
  the native owner and renders with scoped suppressions. `VR_ReachCopyEye`
  (vr.cpp ~12345) copies the proven `ReachVrRenderAccess.source`. Scope must not
  overwrite prepared-eye serials, weaken the two-eye proof or fault camera core.
- H4 game.cpp ~38060..38320 writes an observer copy, reruns native setup/wrapper,
  captures each eye and restores mono state in `__finally`; FOV calibration is
  title-specific. Any optional scope render must not contaminate calibration,
  rig/contact publication or successful stereo frame status.
- H2 Classic: `halo2_stereo_core.cpp` inner per-eye transaction ~2080..2280;
  restores six vector/two FOV spans and re-arms the scene latch between renders.
- H2 Anniversary: `halo2_anniversary_stereo.cpp::EyeLoopBody` ~431..1040;
  `ApplyEyeCamera` ~324 rebuilds the Saber record; per-eye calls restore saved
  scene context and once-per-frame latch. Both eyes are copied before stock
  camera restoration; later host-UI replay restores/recaptures eye images.
- H2 MUST retain its proven render-then-copy target path. `VR_RedirectRenderTargets`
  ~14334 explicitly bypasses H2: redirecting the world target leaves native
  postprocess reading the old target, previously producing identical eyes.
  Do not reuse H3 target redirection for it. A separate mono render/capture is
  only a possible design, not an implemented or validated solution.

No behavioral section is complete. No tests were run because no source changed.
Resume investigation/implementation from this point only when the user asks.
