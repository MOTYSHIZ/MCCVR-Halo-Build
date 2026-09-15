# CE corrections held for actual world-render evidence

## Live follow-up

Source corrections below are committed as `eacd81b`. The user subsequently
authorized one diagnostic launch of the existing installed Steam `be2140f`
through RenderDoc. It succeeded at 12:40 local: MCC PID 26064 and launcher
PID 7844. The mod and RenderDoc are both loaded. No source fix was installed.
The user is loading the original failing Anniversary scene and has been asked
to press F12 once only after reproducing the displaced eye. No MCC capture or
new headset result is available yet. The launch authorization does not remove
the package hold. Current process IDs and pending state must be rechecked
before acting on them; never start another copy just to resume this work.

Portable-tool evidence and launch settings are in
`out/ce-renderdoc-evidence/README.md`. An isolated WARP capture succeeded.
Command-line export recovered both immediate/deferred draw bindings, distinct
RTV/depth resources, raw CB0 origin/matrix/color values and matching copies;
`fixture-export-verification.json` passes. This avoids changing UI preferences.
GUI GPU replay remains unverified. The earlier preference-change/retry attempt
was rejected before execution with only `blocked by policy`; no app setting
was changed. The successful export is a separate, safer analysis route.

## Source-correction state before that launch

The latest request is both CE Classic and Anniversary at the established VR
baseline, with no ZIP until there is credible evidence they function correctly.
The user confirmed that one Anniversary headset eye, as well as the lower
desktop view, shows displaced geometry. The accepted source remains
`4e01f28b3ec5f5f8f533ac66d94978509cbcea54`.

The failed `be2140f` integrated behavior was disabled in its own commit,
`72694b6`. All four rejected CE core flags remain false. The source corrections
below are retained development work, not a newly enabled or accepted candidate.
No new ZIP, installation, MCC launch, game-folder change or PR has occurred.

## Corrected source defects

1. **Classic source ownership.** Native kind-zero output comes from output
   owner `+0x500`; array element zero at `0x2E3B910` is not initialized by the
   native kind 1..8 loop. The adapter follows the real owner, verifies its
   selected RTV against the native cache, freezes ownership across both eyes,
   and records the source-rejection stage. See E-CE-C5 in
   `HALOCE-CLASSIC-EVIDENCE-2026-09-15.md`.
2. **Classic startup metadata.** The native DXGI backbuffer import bypasses the
   hooked texture creation/import boundaries. The existing Present-owned
   `GetBuffer`/`GetDesc` observation now records that exact resource; repeated
   observations preserve its revision, and resize/detach revoke it. A real
   hidden WARP DXGI swapchain tests discovery, cache preparation, independent
   eye capture, missing metadata, resize and recovery without fabricated
   creation notifications or retained COM references. See E-CE-C6.
3. **Anniversary HUD admission and isolation.** The frame now preserves its
   native HUD-enable flags. Full frame-to-output integration tests exercise
   production raster mapping, capture and restoration. Native HUD exceptions
   and unavailable optional layout installation remain feature-only fallbacks
   when cleanup is verified; unknown cleanup drops only the affected frame.
   See E-CE-AHUD-5 in `HALOCE-ANNIVERSARY-HUD-EVIDENCE-2026-09-15.md`.

These correct demonstrated defects. They do not establish Classic headset
parity or fix the Anniversary world's displaced eye.

## Remaining release blockers

- **Anniversary world rendering:** current CPU camera/depth/source receipts do
  not connect actual queued world draws, shader constants and render targets
  through postprocessing into the final copied image. Independent source-copy
  review found no proven copy ordering or resource retention defect. Native
  scene output executes eye 0 before eye 1. The adapter copies each actual
  source before the native packed desktop copy.
- **Clipping/alternate state:** the native oblique projection branch has now
  been executed in 16 additional cases. Origin and clip X/Y/W remain identical;
  clip depth changes. This does not establish the failed run's mode or rule out
  wrong clipping. Surface callback dispatch was traced but its complete primary
  per-eye lifecycle remains unproven. No forced mode reset is justified.
- **Material transforms:** native GLT, tessellated GLT and terrain material
  writers do not produce the common world camera/model matrices. The finite
  negative finding is in `HALOCE-WORLD-MATERIAL-EVIDENCE-2026-09-15.md`.
- **Desktop presentation:** native vertical eye packing remains. A coherent
  single-view desktop mirror still needs implementation/validation after the
  actual headset world rendering works; changing packing cannot fix this test.
- **Left-hand angle:** user-confirmed tracking/aim are preserved. Official
  first-person tag geometry was checked; direct uncalibrated wrist replacement
  would introduce an unproven orientation convention. No guessed hand rotation
  is applied.
- **Runtime acceptance:** both CE graphics modes, switching, native HUD/hands,
  aiming, head movement and Halo 3 regression require the user's result.

The decisive next world evidence is an actual graphics frame capture, or a
properly scoped native draw/worker diagnostic. An asynchronous external memory
snapshot cannot tie a camera to a particular GPU draw and is insufficient to
lift the package hold. RenderDoc capture feasibility is being checked against
an isolated test program only. Any MCC launch still requires a new explicit
request under `AGENTS.md`; it has not been authorized by this source work.

## Local validation

- `out/ce-corrections-final-release-20260915.txt`: complete Release build passes.
- `out/ce-corrections-final-ctest-20260915.txt`: all 19 suites pass after the
  final optional-layout fallback regression.
- `out/ce-corrections-reach-gate-20260915.txt`: Reach consistency gate passes.
- Generated runtime contracts match the evidence ledger; pinned Classic
  verification passes in `out/ce-corrections-classic-contracts-20260915.json`.
- The production mapped-image verifier passes every CE native/optional group
  against the pinned PE (`out/ce-corrections-mapped-bindings-20260915.txt`).
- Native kind-zero initializer/selector execution passes 12 cases in
  `out/ce-classic-source-native-20260915.json`.
- Extended actual native camera/adapter/worker/oblique proof passes in
  `out/ce-native-camera-mode-review-20260915.json`.

These are local tests of the correction and bounded native behavior, not
headset acceptance. The shared Present metadata bridge also awaits the
required Halo 3 runtime regression before a candidate can be accepted.

## Continuity

The available prior CE chats and actual `be2140f` delivery were recovered, with
notes at `out/ce-hand-angle-audit/recovered-history-and-hand-review.md`.
The latest user log, image, hashes and exact counters are recorded in
`HALOCE-BE2140F-TEST-2026-09-15.md`. The installed Steam DLL was independently
read and matches the delivered `be2140f` SHA-256:
`FB5EDB7271C94153F4F7A27882375D6055FFDD301A40857C7D64F710E1005B35`.

Roomscale/body following, physical melee/collision, H2 vehicles, all-title zoom
and every other standing refinement remain preserved in
`CONTINUATION-REFINEMENT-LIST.md`. Both MCC editions remain supported. The
current Steam diagnostic investigation does not change that scope.
