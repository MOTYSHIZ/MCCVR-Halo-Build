# CE rendering consumer audit — September 15, 2026

## Result and scope

The user rejected `b9662cd8a9f1b55e74f1ff8fa9a66cf2730685e5` with the same
displaced right eye / flat left-eye appearance. Its supplied Steam / SteamVR
OpenXR 2.17.9 / Oculus-family / 90 Hz log ends with 557 prepared frames,
555 captured pairs and six dropped attempts. Both logged camera origins match
their camera pose positions and both clipping ranges are 0.025 / 10000.
Those observations do not establish correct rendered images. Correcting the
origin table and clipping range was insufficient; do not repeat that change
as another purported visual fix. All three rejected Anniversary enable flags
remain off while the replacement is developed.

The complete user-supplied log was recovered from the prior attachment during
this continuation and is now preserved verbatim at
`out/test-runs/b9662cd-ce-failed-20260915/HaloMCCVR-user-pasted.log`, SHA-256
`5FB3EA1D5A1325C6D05D1FA728CF24177A1A3A530B3D9AF040F0CB43816FB1B5`.
The directory previously contained only a request note. That old log predates
the `CE CONSUMERS` WIP and cannot establish the new ledger's runtime results.

This audit preserves the distinction between verified native code, fixture
results, and the user's actual headset result. No MCC process was launched,
no game files changed, and no package was produced by this audit.

## Native consumers verified independently

The native depth and scene loops in `0x455A10` select camera
`renderer + 0xF0 + view * 0x3C8`. Their camera upload calls are at
`0x4562BA` (depth) and `0x456A81` (scene). The shading path in `0x457A20`
passes its supplied camera to the same uploader at `0x457C02`.

Uploader `0x2EB9F0` receives the camera in R8 and the backend in RDX. At
`0x2EBA33` it stores that exact camera pointer in `renderer + 0xBE98`.
It calls matrix preparation `0x2EBBE0`, then renderer virtual slot `+0x48`
with the resulting view/projection matrices and the supplied camera's origin
and clipping values. These are the actual native render-consumption sites,
not the view-list builder.

Projection assembly `0x2EBFA0` reads horizontal/vertical FOV at camera
`+0x14C/+0x150`; its horizontal projection shift comes from the near-plane
origin/width at `+0x12C/+0x134`. `0x11AE90` rebuilds those latter fields
symmetrically from FOV and near clip. No mismatched projection axis or stale
off-center field was established by the offline review.

The particle callback `0x497BB0` passes its supplied camera through to
`0x675F10` and particle drawing `0x6770D0` (classification established by the
native shader reflection below). Camera `+0x220` remains
the source-camera/player identity. It is distinct from the native primary-view
index stored outside the camera. With source count one, the ordinary secondary
branch at `0x454C1E..0x454C93` loads the same source pointer as primary:
`0x454C36` reads the first camera-array entry. Append copies all 0x398 bytes
and does not change `+0x220`. A different initial secondary-player value is
therefore not supported by this branch's code. Downstream corruption still
requires a live check, which the new admission ledger performs.

`0x6770D0` also builds a private camera with vertical FOV bits `0x425DA6D7`
and a corresponding horizontal FOV, then uploads two particle projection
matrices. This audit does not change that FOV or treat this particle branch as
the first-person hand/weapon mesh renderer.

## Corrected source-state facts

The historical E-CE-5 wording that `0x45E2B0` "flushes native drawing" was
not justified by the cited call. `0x1DC1E0` pushes a target-state snapshot and
`0x1DC2F0` pops/restores it. They are not GPU completion waits or flush calls.
Backend virtual `+0x28` (`0x203C00`) likewise invokes D3D11 `ClearState`
and clears cached bindings; it is not `Flush`.

The target restore reaches backend virtual `+0xF8` (`0x205E40`). Its complete
body continues beyond the first unwind fragment. At `0x20613A` it calls
`OMSetRenderTargets` on context `backend + 0xCE0`, after resetting the
viewport. Native surface transfer `0x204C40` uses that same context field for
`CopySubresourceRegion` / `CopyResource`. This supplies ordering within the
native context; this audit found no basis for adding a GPU wait or explicit
flush to either eye's hot path.

`0x45DCF0` starts the per-view postprocessing targets from `BB_MNG_POOL`,
resets its two-entry target-selection state and binds the current target.
`0x45DD70` switches those targets as postprocessing proceeds. Consequently
the two entries in `0x2D62890` are postprocessing targets, not automatically
left/right images. The source selected by `0x45E2B0` is the final current
target; the actual D3D resource is obtained only after `0xAD5F0` resolves
wrapper variants. It is valid for one native resource to be recycled between
the two eyes when each eye is copied before reuse. Resource-pointer inequality
alone would be an incorrect stereo admission requirement.

## Admission gap closed, without enabling rejected rendering

Previously a matching frame-entry camera list plus two successful native
output copies could create a VR pair. Neither checked what the native renderer
actually consumed between those points. A new CE-only camera-upload hook now
records and checks the exact primary camera addresses, pose/view/projection
prefixes and source-player identities at depth, scene and shading consumption.
After native upload it also verifies `renderer + 0xBE98` names that supplied
camera. Auxiliary records can never grant primary-eye identity.

An eye can be captured only after its depth, scene and shading consumption,
with that eye still the most recent primary scene. Missing, foreign, changed,
or mismatched consumption drops the current pair and preserves the title
lifecycle. Actual source-wrapper/resource/context identities and native selector
bits are frozen into the diagnostic snapshot; cold polling emits them in
`CE CONSUMERS`. The hook adds no rendering, native camera writes, logging,
allocation, COM queries or locks to those hot paths.

The production WARP fixtures cover missing depth/shading, an unrelated camera
with identical bytes, a camera changed after frame entry, source-player changes
outside the old compared prefix, and recovery on the next correct frame.
Successful fixture output proves these guards and pixel preservation only.
It does not execute the native game's renderer or establish headset stereo.

This admission change is not presented as the visual correction. Actual native
shader/target consumption and both renderers' headset behavior still require
verification. Original VR, controller-directed aim, two independent hands and
weapons, HUD/reticle and all standing parity requirements remain required.

## Reproducible records

- Existing pinned CE SHA-256:
  `0A12DC561780F449D3F4D0DF10BB8D3BC7BE7840A5BEB2B236F672EB6CD42E6C`.
- Camera-upload signature, three exact relative call edges and pointer-store
  witness: `out/ce-camera-upload-contract-fragment.json` and
  `out/ce-camera-upload-contract-verification.txt`, merged into the central
  manifest/generated contract before building.
- Independent decompilation: `out/ce-audit-projection-consumers-20260915.txt`,
  `out/ce-audit-source-state-20260915.txt`, and
  `out/ce-audit-source-state-continued-20260915.txt`.
- Target restore first-fragment disassembly:
  `out/ce-audit-target-restore-disasm-20260915.txt`; complete body is in the
  final decompilation above. The source-state first query mistyped the selector
  VA as `180ad5f0`; the continued query corrected it to `1800ad5f0`.
- Earlier preserved native traces remain authoritative leads to recheck:
  `out/ce-resume-saber-frame.txt`, `out/ce-resume-render-scope.txt`,
  `out/ce-two-view-builder-disasm.txt`, and
  `out/ce-failure-view-storage-disasm.txt`.

## Resume: native arithmetic checked independently

`tools/re/verify_ce_native_camera_math.py` now runs the actual pinned x64
instructions for view rebuild `0x11ABA0`, projection rebuild `0x11AE90`,
culling rebuild `0x11E5F0`, projection assembly `0x2EBFA0`, matrix multiplication
`0xF9B80` and bounding-box helper `0x12AB90` in Unicorn. The file's exact SHA-256
is required before mapping it into the emulator. The mapped image is read-only;
only synthetic scratch/stack memory can be written. Execution outside the
bounded functions is rejected. CRT `sqrtf`/`tanf` and stack-cookie return are
modeled; no DLL entry point, game process, Windows loader or game file write is
involved.

The verifier passes 189 checks across 324 native calls / 134,172 instructions.
Cases include the failed log's `(98.925,180.641,304.072)` camera location, much
larger world coordinates, combined yaw/pitch/roll, the actual `2912x1050` source,
full-height and square rasters, near/far culling corners, bounds, projection and
opposite eye offsets. Native world-to-camera transformation agrees with the
requested basis/position; culling corners agree with that same view/FOV; the
two eye offsets produce the expected horizontal disparity without vertical
separation. Maximum view error is `0.000386` Saber units and maximum tested
normalized projection error is `2.31e-8`.

This is a negative result for a simple native camera inverse/projection-axis
theory. It does not run the game's scene shaders or establish that the native
renderer uses those matrices for every draw. It also does not execute the
production C++ tracking transform; the existing C++ suites cover that transform
separately. The script is not a new stereo implementation and none of the three
rejected Anniversary enable flags was changed.

Reproduce with `out/pydeps` on `PYTHONPATH`, installing `unicorn` there if needed,
then run:

```text
python tools/re/verify_ce_native_camera_math.py out/deps/re-tools/inputs/halo1.dll
```

Result: `out/ce-native-camera-math-verification-20260915.json`.
Independent complete culling disassembly is preserved at
`out/ce-native-camera-culling-math-disasm-20260915.txt`.

## Resume: secondary-view flag and first-person follow-up

The native frame loop sets target-selector bit `0x10000` before both the depth
and scene uploads using each record's native view index. A missing selector
update at those two loop boundaries was not found. This is code evidence, not
a report of actual GPU bindings from the failed headset run.

The separate native stereo bit also changes work beyond camera shifts. The
newly inspected `0x520EC0` branch handles `SNIPER_SCREEN_TMP`, and `0x6D4220`
receives a one- or two-camera array for a separate effects subsystem. These
observations do not justify changing the list to native stereo mode or changing
the backend's device/stereo configuration. Complete follow-up output:
`out/ce-anniversary-stereo-flags-consumers-20260915.txt`.

The Anniversary particle draw at `0x6770D0` writes its supplied projection and the
separate fixed-FOV projection into the per-draw buffer at `+0x00/+0x40`, copies
the `0xB40` emitter-data block at `+0x150`, and writes pose data selected by
renderer `+0xBE98` at `+0x120`. The first review called that block
"palette-related" without consumer evidence. Actual shader reflection below
corrects that classification: it is particle emitter data, not a hand/weapon
bone palette. None of this establishes the cause of the whole-scene
displaced-right-eye failure. Actual per-eye draw/target selection remains
unresolved, and full-CE packaging remains held.

## Resume: native producer through the production adapter

The optional `--adapter-exe` mode now starts with native `0x7B480`, supplying
only CE-native position/up/forward, viewport/configuration and near/far inputs.
Its pose/view memory starts poisoned. That actual producer establishes
`(x,y,z) -> (x,z,-y)`, position scale `3.048`, the world-offset addition and
forward-bias addition before native view/projection construction. Both zero and
nonzero optional vertical-FOV inputs are exercised.

The resulting camera records pass through the compiled production
`StageSaberEye` helper using a frozen translated/yawed headset and opposite eye
offsets. Native view/projection/culling and actual renderer virtual calls then
execute against each staged camera. Renderer construction `0x4F05F6` installs
vtable `0x1815378`; its `+0x48` slot is `0x2351E0`, and its `+0x110` slot
is the real CPU shader-constant writer `0x235860`. The tested configuration
sets backend capability bit 26 and the nonreflection camera mode. Native
execution writes the transposed rotation-only view/projection to the buffer at
`backend + 0xD8 -> block + 8 -> data + 0x270`, and writes the camera origin
to data `+0x240` and `+0x320`.

The expanded run passes 54 native producer cameras / 108 production staged
eyes, plus the original 189 independent arithmetic checks: 918 native calls,
390,654 instructions. Zero/failed-log/far locations, varied native orientation,
three rasters, world offset and forward bias are covered. It supplies no
evidence for an axis, inverse or unit-scale correction to the rejected path.
Native CRT `atanf` is additionally modeled, and the producer's ancillary
debug-status-byte clear is modeled without making mapped image memory writable.

```text
cmake --build --preset release --target halomccvr_ce_view_pair_tests
python tools/re/verify_ce_native_camera_math.py out/deps/re-tools/inputs/halo1.dll --adapter-exe out/build/release/Release/halomccvr_ce_view_pair_tests.exe
```

Result: `out/ce-native-camera-producer-adapter-verification-20260915.json`.
Native producer and renderer decompilations are preserved in
`out/ce-resume-saber-camera.txt`,
`out/ce-renderer-matrix-upload-consumer-20260915.txt` and
`out/ce-renderer-constant-buffer-upload-20260915.txt`.
This executes the native producer and CPU constant writer, not the GPU, the
native game scene or the headset. It does not establish which shader/resources
the failed game run selected for each eye. Rejected enable flags remain false.

## Resume: actual compiled Anniversary shader consumers

`tools/re/inspect_ce_shader_cache.py` reads the installed Anniversary `.sdc`
cache's declared compressed region, validates its concatenated zlib streams,
extracts bounded DXBC blobs and uses the system `D3DDisassemble` API. Source
cache hashes and every extracted shader hash are stored in the output manifest.
All outputs remain in ignored `out/ce-anniversary-shaders`; the tool never
writes to the game installation or replaces a shader.

The inspected `base` (344 shaders), `terrain` (244), `zfill` (1,286), `glt`
(1,423) and `particles` (150) caches retain HLSL constant-buffer reflection.
For example, `base/0001-010.asm` names `VS_REG_COMMON_VIEW_POSITION` at
`CB0[50]` (`+0x320`) and `VS_REG_COMMON_VIEWPROJ_MATRIX` at `CB0[39..42]`
(`+0x270`). Its actual instructions subtract that origin from input world
position, then take four dot products with those matrix rows. Terrain and
skinned base examples use the same final camera-relative transform. Thus the
camera-relative interpretation in the emulator is now supported by actual
shader instructions, rather than inferred only from matrix contents.

`particles/0000-000.asm` names `CB_PASS_PARTICLES`: projection at `+0x00`,
first-person particle projection at `+0x40`, camera-to-world rows at `+0x120`,
and `VS_REG_PARTICLES_EMIT_DATA[180]` at `+0x150`, size `0xB40`. Those exact
offsets identify `0x6770D0`'s upload as particle data. The native function also
requests the `PART` renderer identifier and generates particle quads. This
corrects the earlier unsupported association with a hand/weapon skin palette.

Native `0x1DC460` tests backend dirty byte `+0x2C8`, copies each dirty CPU
constant block into its native GPU buffer wrapper and binds it through backend
virtual `+0xC0`. Draw `0x2063C0` calls that commit before the D3D draw.
Decompilations: `out/ce-shader-dirty-submit-20260915.txt` and
`out/ce-shader-draw-commit-20260915.txt`. This establishes an actual native
constant-to-draw route, but does not establish the failed run's selected eye,
draw, constant-buffer revision or target contents.

The existing capture cache already requires the exact native immediate-context
identity during cold preparation, capture and submission. Its source copy is
queued within the native source transfer before that source can be reused for
the next eye. No evidence supports adding `Flush` or a GPU wait. The remaining
stereo investigation must establish actual per-eye draw/selection and any
alternate camera/constant paths; passing arithmetic and shader inspection alone
do not resolve the user's displaced-right-eye/flat-left-eye failure.

## Resume: camera-write readiness and render replay feasibility

The native camera writer's `config + 0x128` bit 26 is set by the successful
D3D11 device/backend setup `0x1EF660` at instruction `0x1EFE02`. That routine
creates or acquires the device/context, initializes native resources, and sets
the bit unconditionally at its successful end. The inspected setter has no
per-eye or source-player predicate. An executable operand search found this
direct `OR [object+0x128],0x04000000` and no exact `0xFBFFFFFF` clear-mask
operand. This is evidence against the proposed per-eye dirty-optimization
interpretation, not an exhaustive proof that no whole-field write can clear it.
Full decompilation: `out/ce-camera-capability-and-skin-virtuals-20260915.txt`.
The separate dirty flags at `backend+0x2C8` / constant-block `+0x30` are set
by every `0x235860` call and consumed before native draw as described above.

Two fresh primary-only renders were also assessed as an alternative to the
rejected forced-secondary path. A reusable render-only transaction has not yet
been established. The existing source proves these constraints:

- `0x455170`'s render variant resets native pools, invokes scene-registered
  callbacks, calls both culling phases and mutates shared scene state. Its
  copied-list and active-list variants have distinct ownership and event state.
- The surrounding `0x87F90` assembles a graph of shared native jobs,
  dependencies, counters and events, then pumps/waits for those jobs. Calling
  the preparation body directly does not recreate that graph.
- `0x455A10` consumes a worker completion count at `0x456F1D`, retires
  per-frame resources and completes the frame. A second call without a new,
  properly owned native job graph would repeat that consumption/retirement.
- The generic dispatcher signals completion after the virtual body returns;
  its completion lock does not exclude concurrent camera/list consumers.

These facts are in `out/ce-scheduling-resume.txt`,
`out/ce-scheduler-flow.txt`, `out/ce-scheduler-execution.txt`, and
`out/ce-resume-saber-frame.txt`. A valid alternative needs a separately proven
reusable per-view draw boundary, or a complete fresh render-job graph with
simulation/update callbacks excluded and all worker lifetimes accounted for.
Neither boundary is proven here. No full-frame replay, counter adjustment,
native stereo-device flag change, or rejected render path was enabled.

## Resume: actual world batches and alternate first-person projection

The world scene's draw path has now been traced separately from particles.
`0x457A20` constructs draw keys containing an array view index at `+0x14`
and that view record's `+0x0C` index at key `+0x18`. Its `0x3134F0`
consumer either draws immediately through `0x3136B0` or waits for a queued
batch's native completion event and executes its D3D11 command list. The
immediate draw selects `renderer + 0xF0 + key[0x18] * 0x3C8`, calculates
its matrices through `0x2EBBE0`, and supplies them to actual mesh draw
`0x30F9A0`. The current two-primary-view contract sets both indices to
the same eye number. No index mismatch was established in this path.

The queued path is an additional camera consumer that **does not call
`0x2EB9F0`**, so the WIP `CE CONSUMERS` hook alone does not cover it:

- `0x455A10` passes the active `renderer + 0xB0` list into `0x3112E0`.
  Each batch's `+0x38` points at that list's `+0x10 + eye * 0x3C8`
  view record. This is an address into the active list, not a separately
  discovered stock-camera copy.
- Batch job vtable `0x18083D8`, slot `+8`, is `0x313220`. It calls
  `0x313450`, which obtains a worker backend and invokes `0x310930`.
- `0x310930` reads the batch view's camera at `+0x30` and its pose
  position at view `+0x60`. Both primary flags `0x10B` and `0x20B`
  use this branch; the alternative shadow matrix path requires bit `4`,
  which neither primary record has. It computes `0x2EBBE0` and invokes
  renderer virtual `+0x110` / `0x235860` directly on the worker backend.
- Worker command-list begin `0x2062A0` invokes backend `+0x28` /
  `0x203C00`. That clears bindings and marks constants dirty; the inspected
  body does not overwrite common view origin or either projection matrix.
  Command-list finish is `0x2062E0`; execute is `0x206320`, which restores
  target state after invalidating the immediate backend's bindings.

These are static native facts, not a record of which queued batches executed
in the failed headset run. Preserved decompilations are
`out/ce-world-view-selection-20260915.txt`,
`out/ce-world-draw-context-20260915.txt`,
`out/ce-world-worker-context-20260915.txt`,
`out/ce-world-worker-camera-selection-20260915.txt`, and
`out/ce-world-relative-object-consumer-20260915.txt`. The batch constructor
was already preserved in `out/ce-audit-source-state-continued-20260915.txt`.

There is also a genuine alternate projection, distinct from the world matrix.
Both `0x3136B0` and `0x310930` copy the selected camera into a private
camera, replace its **vertical** FOV at `+0x150` with bits `0x425DA6D7`
(approximately 55.413 degrees), rederive horizontal FOV, rebuild projection,
and calculate a separate matrix. This replaces the tracked eye cover in
that private first-person projection. Worker `0x310930` uploads it through
renderer `+0x90` / `0x2357B0` to common constants `+0x2B0`.

The installed GLT vertex shader `glt/0002-003.asm` explicitly selects
common `CB0[43..46]` (`+0x2B0`) instead of world `CB0[39..42]`
(`+0x270`) when `CB7[23].x > 0.5`. Reflection identifies that selector
as `VS_REG_GLT_USE_FPWEAPON_PROJ_MATR`. Both choices subtract the same
common view origin. The inspected base shader variants mark the alternate
matrix unused, so the existence of this switch does not prove all first-person
meshes use it. The native producer of the GLT switch and actual hand/weapon
material selection remain to be established before a scoped correction.

Separately, draw-record flags `+0xDE & 0x100` cause `0x30F9A0` to call
`0x30F280` before drawing and restore both matrices afterwards. The callee
multiplies both matrices by a depth-range transform containing `0.8` and
`0.2`; it is not a demonstrated switch from the world lens to the first-person
lens. Preserve this distinction. Full body:
`out/ce-world-fp-projection-consumer-20260915.txt`.

The alternate first-person FOV is a concrete remaining parity concern. It
does not, by itself, explain the displaced right world and flat left world.
No arbitrary FOV/axis change or rejected stereo behavior was enabled here.

The native instruction verifier now also executes `0x310930`'s camera
setup prefix through both actual constant writers, stopping at `0x310CA4`
before thread, command-list or D3D operations. Its two active-view slots have
the native `0x10B` / `0x20B` flags and point at the real producer -> compiled
adapter -> rebuilt native eye cameras used by the existing test. Across
108 eye setups the worker world origin/projection matches the main uploader,
while the alternate FP matrix projects with the fixed vertical FOV and
differs from the tracked world cover. This supplies a reproducible negative
result for the proposed worker camera-index mismatch, and confirms the
separate lens numerically without claiming a GPU draw. Result:
`out/ce-native-camera-worker-prefix-verification-20260915.json`;
1,026 native calls / 492,498 instructions. The earlier producer-only result
is preserved separately and has not been relabeled as this expanded test.

## Ultra continuation: split lifetime and upstream visibility

The split-allocation enable is a native frame lifetime, not a missing VR
configuration toggle. Scheduler `0x87F90` sets global `0x1C33DFC` bit 9 at
`0x88514`, before constructing its render jobs, and clears it at `0x899C6`
after its completion waits. Preparation `0x455170` separately sets selector
bit 15 from the active list's multiple-view flag. The per-depth and per-scene
loops select bit 16 from their native view index. These are static execution
paths; they do not establish the failed game's depth allocation identities.
There is no evidence-backed reason to force either global configuration bit.
The retained depth resource admission remains required.

Upstream positional visibility was traced beyond the view-list builder.
`0x545250` loads the scene object through `0x9BDF0`; its registered scene
constructor thunk `0x545BC0` enters `0x545BD0`, which installs vtable
`0x1819658`. The different constructor `0x545A70` installs `0x1819928`
for a separately registered resource and must not be used as this scene's
vtable. The real scene slots `+0x230/+0x238` are `0x5434B0/0x543CB0`.

`0x454E40` invokes the first scene slot with active-list camera positions at
renderer `+0x120` and `+0x4E8`. The copied preparation branch supplies the
equivalent positions in its own list. These are the staged eyes' pose-position
rows. `0x5434B0` copies those positions to scene `+0x168/+0x174` and calls
`0x543550` with primary/secondary visibility masks `0x20/0x40`. That function
evaluates each non-null position separately through the scene region predicate
and updates the respective masks; it does not fetch a stock camera from the
global source-camera array. No second-player-only positional visibility gap
was established here. The native append itself derives view identities 0/1
from flags `0x100/0x200` and increments the primary-view count for both.

The separate effect-volume preparation `0x2E1060` walks the primary-view
count and uses each list camera at `list + 0x40 + view * 0x3C8` for its spatial
predicate. Its player filter reads that camera's `+0x220`, so two eyes sharing
source player 0 remain distinct spatial checks with the same player's filter.
This is not permission to change the second eye's source player to 1.

Records: `out/ce-world-actual-draw-and-clear-20260915.txt`,
`out/ce-world-scene-registration-20260915.txt`,
`out/ce-world-object-registration-continued-20260915.txt`,
`out/ce-world-scene-camera-virtuals-20260915.txt`, and
`out/ce-world-scene-visibility-masks-20260915.txt`. Scheduler instructions are
also preserved in the existing `out/ce-scheduler-flow.txt`. An exploratory
query for `0x545BC0` lacked a Ghidra function (it is a thunk); subsequent pinned
disassembly established the jump before tracing the actual constructor.

These negative results do not resolve the displaced-right/flat-left world
failure and do not enable any rejected stereo flag. The core now exposes a
bounded current-primary-eye tracking accessor for the separately implemented
post-effect transaction. It returns the frozen preparation settings only after
camera-address, native selected-camera, upload-stage, receipt, generation,
reference and XR freshness checks. Production runtime fixtures cover both
eyes, immutable settings, missing/foreign/mutated cameras, stale input and
scope recovery. They establish adapter ownership, not visible native stereo.
