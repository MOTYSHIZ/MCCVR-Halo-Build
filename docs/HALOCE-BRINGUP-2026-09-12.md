# Latest - September 15: tracked-view construction correction

User tested 6e31b25: 803 captured pairs and faster visible Anniversary switching,
but displaced/noclipped right eye and flat/head-attached left; Original remains
flat. Exact request/log preserved under out/test-runs/6e31b25-ce-partial-failed-20260915.
Prior delivery chat was read. This is NOT acceptance; do not resend 6e31b25.

After separate disable f63254b, a correction now stages each tracked eye BEFORE
native append builds its independent position metadata. Finalization retains
native per-eye fields and applies the primary native clip range to both eyes.
See E-CE-12 and HALOCE-CONSTRUCTION-2026-09-15.md for proof, tests and limits.
Release/eight suites, Reach gate, pinned/generated and mapped-PE checks pass.
The displaced-view cause is not fully isolated; no headset success claimed.

Package the correction without -Install with current notes, verify both ZIPs,
deliver build/source here, then WAIT for headset result/instructions. Read
out/ce-current-handoff.json for exact final identity. No installation, MCC launch,
game-folder modification or PR/publishing. Both editions remain supported.
Accepted 4e01f28 and all existing input/gesture/other-title work remain preserved.
Original stereo, CE independent tracked hands/weapons, controller aim, HUD/reticle,
locomotion and state/vehicle parity remain REQUIRED and unfinished. Physical
melee/world collision still await functional injection confirmation. All standing
and deferred items remain below; do not describe this candidate as full CE VR.

# Halo CE Anniversary VR bring-up — September 12, 2026

## September 14 late: actual headset failure and correction

e17a664 was delivered and tested. User reports working input/graphics gesture,
flat Classic, black Anniversary VR and mismatched stacked desktop views. The
log shows 734 prepared frames and zero captured pairs, with half-height eye
textures. Full report/log: out/test-runs/e17a664-ce-anniversary-failed-20260914/.
The initial resume missed that newer conversation reply; ACTIVE-WORK-CHECKPOINT
now preserves it and points to the final out/ce-current-handoff.json record.

Failed rendering was disabled separately in 736f0c5. The new correction preserves
two exact primary cameras through native auxiliary culling views and selects
proven source-texture dimensions before projection/culling rebuild. Failure-stage,
copy-mask, raster and camera-position diagnostics are included. E-CE-11 records
the proof and uncertainty: the earlier log does not establish which rejection
condition occurred each frame, or conclusively explain the displaced view.
Regressions fail under the old count guard and pass under the correction;
Release/eight suites, Reach consistency, pinned/generated contracts and mapped
PE checks pass. New notes: HALOCE-REPAIR-2026-09-14.md. Package build/source
without -Install, deliver and wait for the new headset result. Accepted source
remains 4e01f28; every unfinished/deferred task and both editions are retained.

## September 14: connected Anniversary candidate

Recovered uncommitted runtime work beyond a6a507a was preserved before edits at
`out/checkpoints/20260914-215646-ce-finish-resume`. The native job/builder/output
hooks, source descriptor registry, owned eye capture, generation retirement,
title admission and shared OpenXR submission are now connected. The head-side
movement-stick graphics gesture is connected through shared Back/View input.

This continuation additionally closed early texture-discovery coverage, recenter
revision races, elapsed-time staleness, raster/source dimension mismatch,
descriptor retirement, CE pause-mode publication and misleading title logging.
The full native frame is never replayed. See E-CE-10 in the render evidence.

Release and eight suites pass locally, including actual WARP pixels through the
production capture scopes. Pinned identity/witness, mapped-data PE, generated
contract and Reach checks accompany packaging. No native game routine has been
executed by these tests. No headset acceptance is claimed.

The candidate now meets the implementation threshold for the user's first
Anniversary stereo/6DoF test: a complete native preparation-to-GPU-to-OpenXR path
exists with explicit ownership/pose evidence. This is a judgment about test
readiness, not proof of headset operation or completion of all CE work. Package
build/source ZIPs without -Install, with `HALOCE-CANDIDATE-2026-09-14.md` as the
current release notes; deliver them and wait for testing per the user contract.

Classic stereo, CE controller-directed aim/tracked weapons, separate HUD/reticle
capture, native state/vehicle integration, head-relative movement, snap turning
and body following remain unfinished. Initial CE controls retain native stick
aim. Physical melee/world collision stay deferred until injection confirmation.
Preserve all existing-title work and standing/deferred items, both editions,
and accepted pointer 4e01f28. No install, launch, game writes or publishing.

## User scope and reference behavior

September 13 overrides earlier delivery wording: continue CE VR and do not
package a ZIP until the implementation is reasonably expected to function with
6DoF similar to the other supported games. A passing camera fixture or a probe
alone does not meet that threshold.

User requests a test ZIP as soon as a CE implementation is reasonably expected
to work. Initial stereo/6DOF injection is the first test milestone; do not delay
it for full feature parity. Package matching source and clear testing/known-limit
notes without installing or launching. Offline research alone is not working VR.

Latest staging instruction: match H2 graphics switching exactly (left hand raised
to left side of head, click movement stick). User must confirm VR injection before
the next physical-melee/world-collision implementation stage. Existing CE VR mod
may be consulted for techniques, with title/version-specific verification; no
copied PC/Custom Edition offsets count as MCC bindings.

User explicitly requested Halo 1 Anniversary matching the existing titles for
stereo injection, 6DOF, HUD, crosshair and overall VR behavior. H2 vehicle work
is checkpointed/deferred as manageable; all-title zoom remains a retained task.

Halo 3 reference: two independently rendered eyes in one OpenXR frame, headset
rotation and room-space translation with recenter/world-scale controls, stable
menu/level transitions, controller input/aim and tracked hands/weapons, readable
configurable HUD and controller-aligned crosshair. Each optional feature must fail
locally without tearing down valid camera/stereo ownership. CE's own rendering,
simulation, HUD and Anniversary bridge must establish the implementation; no
H2/H3 offsets or tag semantics are CE evidence.

## Verified starting point

- Source HEAD cfb22eda5c968fc10b1083b56817a710855bd546.
- No post-delivery vehicle/zoom feature edits exist. The prior checkpoint files
  are preserved. Accepted pointer remains 4e01f28.
- `title_registry.cpp` recognizes `GameTitle::HaloCE` / `halo1.dll` but grants
  no runtime/admission capabilities. Recognition is not existing VR support.
- `title_reentry_probe.cpp` excludes CE from present-hint/retention routes;
  no title-specific CE adapter/evidence/render core found on initial file search.
- Official kit policy names HCEEK under N:/SteamLibrary/steamapps/common/HCEEK,
  but N: does not exist in this current environment. Locate actual installed
  kit/archive and retail module before assuming evidence is unavailable.
- Existing pinned RE inputs cover H2/H3/ODST/Reach/H4 only, not CE.

Located current installed evidence after checking Steam libraries:
`D:/SteamLibrary/steamapps/common/HCEEK` (halo_tag_test.exe, tool.exe,
guerilla.exe, sapien.exe, HCEEK.7z and extracted data/tags), and
`C:/Program Files (x86)/Steam/steamapps/common/Halo The Master Chief Collection/halo1/halo1.dll`.
The earlier N: path is historical. Hash/pin these files before binding work.
Existing third-party reference checkout: `out/deps/HaloCEVR-reference`.

Pinned identities are now recorded in HALOCE-EVIDENCE-MANIFEST.json; both inputs
were copied into ignored out/deps/re-tools/inputs. Kit is x86, retail is x64;
the differing pointer/ABI model requires explicit matching. Ghidra projects
CEKit and CERetail were started for offline analysis, logs out/ce-kit-import.*
and out/ce-retail-import.*. `tools/re/inspect_ce_pe.py` provides offline PE
identity, retained-string operand leads and architecture-correct disassembly;
it does not open any process. No runtime address is approved at this point.

The old CE mod's README explicitly excludes MCC. Its useful design reference is
the draw-frame bracket: save camera/targets, render eyes independently, capture
HUD/crosshair separately, then restore. Its D3D9 wrapper and 2003 PC function
signatures are not suitable MCC bindings. MCC kit source strings identify CE's
own render.c, render_cameras.c, camera/observer.c, interface/hud_draw.c and
game/game_time.c for the native investigation. Kit render.c operand leads include
RVA 426C30; no retail homolog or runtime render contract is established yet.

## September 13 evening continuation: owned GPU eye storage

Resumed from clean commit `73c9cd2`, descending from accepted `4e01f28`.
User reiterated using all existing titles as the working VR baseline. Retain
their proven tracking/input/recenter/frame-submission behavior; verify only
CE-specific engine bindings and scheduling rather than inventing a parallel
VR product. All retained/deferred standing tasks remain unchanged.

Implemented `haloce_eye_cache.{h,cpp}` in the cumulative DLL: cold D3D11 eye
texture allocation, exact prepared tracking/resource keys, ordered per-eye
copy, native-frame completion, one-time submission borrows, and guarded cold
retirement. No COM queries, source retention, allocation, locks or waits in
the eye-copy operation. The adapter needs a separately proven live source
descriptor; it does not guess one from the swapchain or wrapper dimensions.

New WARP GPU test verifies actual distinct pixel copies after source reuse
and release, plus invalid/partial/stale frame recovery and submission-borrow
retirement. Release and all seven suites pass, as does Reach consistency.
Generated contracts and pinned SHA/witness verification pass; the manifest
now contains 24 entry signatures. Logs: `out/ce-gpu-resume-*`.

E-CE-8/9 in HALOCE-RENDER-EVIDENCE.md preserve the new scheduling and resource
findings and exact successful/failed offline queries. Key negative findings:
the generic job lock starts AFTER its virtual body; it is not camera-write
exclusion. Imported texture wrappers omit some D3D descriptor fields, so they
cannot independently prove sample/format compatibility. The native transfer
lock is not yet proven to cover all wrapper destruction/import paths.

Next work remains actual runtime integration, not more eye math/cache fixtures:

1. Attach preparation receipts inside CE's exact builder/copy/job scopes.
   Scheduler edges are now traced; establish source-list exclusion and preserve
   the particular prepared receipt across handoff. Do not replay the full frame.
2. Finish real source-descriptor acquisition/lifetime and connect the exact
   per-view copy to EyeCache. Check the native packed destination's allocation
   dimensions when forcing two views; transfer shape alone is insufficient.
3. Wire the title/core hooks, generation retirement, shared OpenXR tracking,
   complete-pair submission and recenter. Classic/switching, controller aim/
   hands, HUD/crosshair and the H2 left-head-side movement-stick gesture remain
   unfinished. Physical melee/world collision remain deferred until the user
   confirms CE injection. No CE runtime acceptance or credible test ZIP yet.

No install, launch, game-file modification, publishing or package occurred.
Accepted pointer stays `4e01f28`; both editions remain required. The code is
an implemented GPU storage component, not enabled CE VR support. Packaging
remains held for the user's comparable-6DoF threshold.

## September 13 later continuation: native binding and handoff components

User resumed with "continue halo ce vr work. forget nothing." The prior WIP was
preserved before edits at `out/checkpoints/20260913-155919-ce-runtime-resume`.
The complete standing refinement list and deferred vehicle/zoom report remain
preserved. Both editions remain in scope; accepted source stays `4e01f28`.

Implemented this pass:

- Production CE loaded-image verifier and native camera rebuild adapter in
  `src/dll/haloce_native_bindings.{h,cpp}`, compiled into the cumulative DLL.
  The verifier rejects changed/multiple signatures, wrong function boundaries,
  operands, body instructions, backend pointer, image identity or generation.
  Generation-safe private staging now calls the bound native rebuild path.
  No CE hook/admission calls it yet; no native game routine has been executed.
- Tracked generated contract header and generator/check command keep loaded-
  image validation tied to the pinned evidence manifest.
- Explicit active/copied-source preparation receipts, with revocation on every
  new builder attempt, exact pair matching and independent per-frame snapshots.
  Sequential failure cases and actual concurrent publications are tested.
- Finite derived-camera guards after native rebuild, preserving opaque fields.
- E-CE-7 records exact native copy operands and a negative finding: `0x4556B0`
  submits preparation/culling and signals completion; it is not a worker wait.
  Source-list exclusion cannot be assumed from that function.

Validation: cumulative Release build, all six CTest suites, Reach consistency,
generated-contract drift check, pinned SHA-256/static verifier, and the production
mapped-image verifier against non-executable private copies of the pinned CE PE
pass. Logs use `out/ce-runtime-resume-*`. No game launch, DLL load/injection,
installation, game-folder modification, publishing, ZIP, or accepted-pointer change.

Exact next work (do not redo completed math/binding/receipt scaffolding):

1. Prove native scheduling/exclusive list ownership at the builder and copy
   handoff, then attach the generation-scoped preparation receipts. The
   `0x455170` body and E-CE-7 name both routes. Do not substitute a latest-pose
   lookup for the actual source receipt, or mistake `0x4556B0` for a wait.
2. Complete E-CE-6's source-texture descriptor/lifetime proof, cold compatible
   eye-cache preparation and exact per-view transfer capture. Native selector
   `0xAD5F0` is required; naked wrapper `+0xE0` can select the wrong resource.
   Pool refresh `0x20B880`/view accessor `0x22B5D0` have been inspected but do
   not establish lifetime. Do not introduce COM/allocation/locks into hot hooks.
3. Wire the CE stereo core, title/generation retirement, OpenXR publication,
   actual rendered-pair submission and recenter. No `haloce_stereo_core.cpp`
   or CE capability admission exists yet. Preserve stock outside owned frames;
   a failed owned eye pair drops only that frame.
4. Verify Classic mode and switching, then finish requested controller aim/
   hands, HUD/crosshair and left-head-side movement-stick graphics gesture.
   Physical melee/world collision remain deferred until injection confirmation.

Packaging stays held until comparable 6DoF is reasonably expected to function.
These local components and tests do not meet that threshold yet. No CE support
or runtime acceptance is claimed. The older continuation below is retained as
history; its "no CE runtime .cpp" wording predates the native binding component.

## Earlier September 13 continuation implementation and exact remaining work

Preserved resumed WIP before editing under
`out/checkpoints/20260913-104747-ce-continuation`. Branch still descends from
accepted `4e01f28`. CE runtime admission remains zero and no hook is installed.

New `haloce_view_pair.h` prepares a native two-camera pair atomically in private
storage, rejecting split-screen, unexpected native flags and tracking/pose/FOV
or raster disagreements. New `haloce_surface_transfer.h` validates the exact
native per-eye transfer request. Both are exercised by
`haloce_view_pair_tests.cpp`; the earlier camera math suite is retained.
These are implementation components, not functioning VR injection yet.

The important architecture correction is E-CE-5 in HALOCE-RENDER-EVIDENCE.md:
do not replay Anniversary `0x455A10` twice, because it consumes worker completion
and frame resources. CE already has native two-view construction before culling.
Use that direction. E-CE-6 traces the per-view handoff into the actual backend
surface-copy method and its conditional resource selector.

Next required implementation, in dependency order:

1. Bind verified CE camera rebuilds and the native pair-builder scope; distinguish
   active and copied preparation lists and attach one tracking serial to both.
2. Bind per-view output and resolve the current native source through `0xAD5F0`;
   establish live source descriptor/lifetime and preallocated compatible GPU
   caches. Observe exact native view indices and reject stale/partial pairs.
3. Connect CE title admission, generation/retirement, OpenXR tracking publication,
   complete-pair submission and recenter. Verify renderer switching and Classic
   behavior. Preserve stock behavior outside a claimed CE frame.
4. Finish the requested controller/HUD/crosshair integration and H2-style left
   head-side movement-stick graphics gesture. Physical melee/collision stay
   deferred until injection is confirmed. Do not call these pieces implemented.

Release build, all five CTest suites and Reach consistency pass. Logs:
`out/ce-continuation-{configure,build,tests,reach-gate}.txt`.
`out/ce-render-contract-verification.json` is the offline binding report.
Neither mathematical fixtures nor static call-edge checks establish headset
success. No ZIP, install, MCC launch, game-file write or accepted-pointer change.

## Workflow and limits

Locate/pin official HCEEK executables/tags and installed retail module, establish
CE-native lifecycle/camera/render/HUD semantics, match unique retail signatures,
then implement isolated ownership. Share proven OpenXR/math/input infrastructure
where appropriate, without treating engine layouts as interchangeable. Record
findings and negative results separately from hypotheses. No runtime success or
full parity is claimed until headset validation. No new game launch, installation,
game-folder write or GitHub work. Package only when a concrete candidate is ready
under the user delivery contract; preserve both editions and existing titles.
