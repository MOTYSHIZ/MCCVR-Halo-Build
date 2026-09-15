# CE render evidence — September 12, 2026

Inputs are hash-pinned in `HALOCE-EVIDENCE-MANIFEST.json`. These are offline
findings, not headset results. Accepted source remains `4e01f28`.

## E-CE-1: native camera and per-window render

HCEEK `halo_tag_test.exe` RVA `0x426C30` contains render.c assertions explicitly
naming `window->render_camera` and `window->rasterizer_camera`. The corresponding
retail `halo1.dll` function is RVA `0xBBCA64`: same fog near/far clamp, same
two-camera frustum construction, same optional reflection camera, same main
render-view call. Kit and retail cameras start at window `+4` and `+0x58`.

Kit frustum builder `0x427C90` matches retail `0xB8F690`, independently through
viewport subtraction, vertical-FOV tangent, forward/up cross products,
world/view matrices, six planes, bounds and projection output. Camera fields
confirmed in both: position `+0`, forward `+0xC`, up `+0x18`, FOV `+0x28`,
viewport y0/x0/y1/x1 `+0x2C/+0x2E/+0x30/+0x32`, window rectangle `+0x34`,
near/far `+0x3C/+0x40`. Camera span is `0x54`; per-window stride is `0xAC`.
The intervening bytes and trailing plane are not permission for arbitrary writes.

Kit `0x428CD0` and retail `0xB8F144` derive frustum bounds from viewport/window
rectangles. Retail camera builder `0xAC450C` converts observer output into the
raster camera and copies the full `0x54` bytes into the render camera unless
the native alternate-camera flag is set. Observer layout is independently
visible there: position `+0`, forward `+0x20`, up `+0x2C`. No observer hook is
approved by this finding.

Kit outer `0x4267B0` and retail `0xBBCE28` iterate windows. Retail loop starts
at global `0x2E9FE80`, advances `0xAC`, and calls `0xBBCA64` only for a normal
window with player index not `-1`. Exact call is `0xBBCEE6`; main render-view
call inside the window is `0xBBCCAD -> 0xBBCF30`. Reflection uses the same inner
function with a different class and must remain outside a claimed primary eye.

Retail main-game render `0xAC47C0` prepares player windows then appends a
non-player/UI window and calls the outer renderer. Its normal host caller is
`0xADD09` within `0xADA90`; a second caller is `0xAC2704` within `0xAC269C`.
Host rendering continues after `0xAC47C0`. Consequently a completed native
window alone does NOT prove Anniversary final-output capture.

## E-CE-2: initialized game clock

Kit `0x180570` asserts `game_time_globals && game_time_globals->initialized`,
then returns `+0xC`. Kit interpolation `0x181070` consumes `+0x20/+0x24`.
Retail outer-render timestamp reads the current tick at `+0xC` through global
slot `0x2E9FD68`; host render checks pointer/nonzero initialized byte before
calling main-game render. These establish bounded read-only liveness leads;
full unique-signature verification is still required before runtime use.

## Preserved offline output and tools

Ignored `out/ce-kit-camera-functions.txt`, `ce-kit-render-tree.txt`,
`ce-kit-camera-build.txt`, `ce-kit-hud-time.txt`; retail
`ce-retail-window-disasm.txt`, `ce-retail-main-render.txt`,
`ce-retail-bridge-verified.txt`, `ce-retail-output.txt` retain the derivations.
`tools/re/inspect_ce_pe.py` reads PE identities, strings, disassembly and x64
unwind ranges. Unwind fragments are not necessarily whole functions; the host
example's fragment starts `0xADC8F` but its actual function starts `0xADA90`.

Ghidra projects under ignored `out/deps/re-tools/projects`: CEKit analyzed;
CERetail saved after its 900-second analysis timeout (incomplete analysis);
CEFast imported without analysis for targeted decompilation. Use CERetail for
containing-function/caller queries. `DumpFunctionAt.java` arguments are absolute
virtual addresses, not RVAs. No failed script output is proof of the functions
it never reached.

## E-CE-3: native-to-Anniversary camera bridge (September 13)

Retail `0xB29268` takes the HCEEK-matched observer/camera fields from E-CE-1
and calls `0x7B480` at `0xB293EB`. The arguments are player index, position,
up, forward, horizontal FOV degrees, vertical FOV degrees. It selects either
observer output (`0x2D9BDD4`, stride `0x2A8`) or the already-built raster camera
(`0x2D9CA30 + (player+1)*0xAC + 0x58`) according to `0x2E9FE0C`.
The bridge can be explicitly disabled by `0x2E3B8F1`; this is not a license to
clear that byte. The update `0xAB0D10` calls `0x89B70`, which builds those
cameras and invokes the bridge. This update advances native game ticks and
MUST NOT be doubled for stereo.

`0x7B480` selects a camera from pointer array slot `0x2B17B90`, bounded by
count `0x2B17B98`. It maps native `(x,y,z)` to Saber `(x,z,-y)` and multiplies
position by **3.048**, then adds the three-component world offset and the
existing forward bias. The first `0x40` bytes are right/up/forward/position
rows; the right vector is `forward cross up`. `0x11ABA0` normalizes the basis
and rebuilds its view matrix. Horizontal FOV setter `0x11B020` writes `+0x14C`,
derives vertical `+0x150` using ratio `+0x154`, and calls `0x11AE90`.
Vertical setter `0x11B0B0` does the inverse derivation and calls the same helper.
The complete derived camera span and final scene consumer are not yet approved.
`halo_ce::BuildSaberPose` reproduces only this verified pose conversion; it does
not write process memory, call the setters, or claim a render-ready camera.

`0x3D36A0/0x3D6690/0x3D7210/0x3D48F0` were investigated as camera-array
consumers but are cinematic setup/update, not a proven replayable scene render.
`0x4150F0/0x415200` manage split-screen camera allocation/FOV/rectangles;
`0x415B90` sets a scripted camera. `0x2D5C90` writes a colour/gamma lookup
resource, not the scene render. Do not restart those false leads as render hooks.

Preserved exact output: `out/ce-resume-camera-bridge.txt`,
`ce-resume-saber-camera.txt`, `ce-resume-saber-consumers.txt`,
`ce-resume-saber-render.txt`, `ce-resume-saber-view.txt`, and
`ce-resume-saber-render-narrow.txt`. These bridge findings are native CE camera
semantics matched into MCC's remastered host, not borrowed H2 Saber layouts.

## Offline executable verification (September 13)

`tools/re/verify_ce_render_evidence.py` validates both pinned source identities,
six unique file-backed executable signatures, x64 function-entry unwind records,
and eight relative call/data operands. Contracts are recorded in
`HALOCE-EVIDENCE-MANIFEST.json`; result is
`out/ce-render-contract-verification.json` (`PASS_OFFLINE_ONLY`). Signatures
prove identity, not replay safety or rendered-eye capture. Runtime admission
must still verify the loaded module; the approved-runtime-hook list stays empty.
The verifier reads files only and accepts explicit paths for moved evidence.

## E-CE-4: Anniversary camera copies and rebuilds

Recovered September 13 output independently establishes the Saber camera span
as `0x398`: `0x2EA720` copies seven `0x80` chunks and a final `0x18` into a
native view record. Record stride is `0x3C8`; its camera starts at `+0x30`.
The list has a `0x10` header and count at `+8`, so the first camera is list
`+0x40`. Render-side list is renderer `+0xB0`, first camera `+0xF0`.

`0x11ABA0` normalizes the pose basis and rebuilds the view matrix; `0x11AE90`
uses horizontal/vertical FOV degrees and near clip to rebuild projection bounds
and raster scales; `0x11E5F0` rebuilds camera-local culling planes, corners and
bounds. `0x2EA720` invokes the projection and culling helpers on its private
camera copy. `0x2EB9F0` consumes the chosen camera during rendering and uploads
derived view/projection data through the renderer's virtual methods. These are
CE bindings; no Halo 2 Saber object layout has been imported.

The camera is not an arbitrary writable `0x398` block: native list reset
`0x2EACD0` destroys non-null resources belonging to records, and `0x2EA720`
zeros four opaque camera fields at list-entry `+0x240..+0x258` after copying.
Private byte copies in the staging helpers do not own those references and
must never invoke native destruction. Camera copies must be published before
culling starts, not patched while worker consumers may be reading them.

## E-CE-5: native two-view path and full-frame replay hazard

New exact outputs: `out/ce-native-two-view-investigation.txt`,
`ce-two-view-builder-disasm.txt`, `ce-two-view-prepare-disasm.txt`, and
`ce-two-view-output-disasm.txt`. The preparation function's x64 unwind entry
ends at `0x4551A4`; this is a fragment, not the end of the Ghidra-established
whole function. The call edges below were checked through the complete body.

Verified facts:

- `0x4547E0` takes the view-list pointer in RDX and an explicit secondary-view
  boolean in R8B. It appends primary flags `0x10B` (or `0x110B` for stock zoom)
  through `0x2EA720` at `0x454A69`. When the boolean is true, it sets list bit 0
  and appends secondary flags `0x20B` (or `0x120B`) at `0x454C93`. The append
  helper derives native view identity 0/1 from flags `0x100/0x200`.
- The explicit secondary-view branch uses the second authored camera when
  available, otherwise the first. An actual second player's view must never
  be overwritten. A separate native stereo setting, backend `+0x238`, adds
  convergence/IPD shifts and list bit 1; that setting is not a VR binding or
  permission to change the graphics device configuration.
- `0x455170` calls the view builder at `0x455308` for the active render list,
  or `0x455611` for a preparation list. It also has a path that copies an
  already prepared `0xBDE8` list into renderer `+0xB0`. Culling submission
  `0x4AA740` follows at `0x455373/0x455506`. An adapter must handle the list
  handoff and associate both render cameras with the exact tracking serial.
- Full-frame renderer `0x455A10` processes both primary records in native
  loops, including their depth and scene passes. At `0x456DD0` it calls
  per-view output helper `0x45E2B0` with the current view index.
- `0x45E2B0` flushes native drawing, transfers the selected per-view surface
  through backend virtual slot `+0x190`, and invalidates the surface selector.
  In the multiple-view branch, the first view starts at destination row 0 and
  the second at source-height. The combined resource kind is `0x04000001`.
  This proves vertical packing at this internal handoff, **not** final DXGI
  backbuffer dimensions, format, crop or a valid OpenXR capture source.
- `0x455A10` consumes worker completion at `0x456F1D` (`lock xadd` with -1),
  then retires resources and completes the frame. Calling this entire function
  twice without a new native preparation is not a verified stereo transaction.
  Do not enable the earlier proposed full-frame replay wrapper.

Implementation direction, not a runtime finding: have CE construct two primary
views before culling, replace their cameras with one exact tracking pair, and
capture each native output before its surface is recycled. This avoids replaying
the whole frame. GPU resource identity, preparation/render-thread association,
HUD routing and lifecycle guards must be finished before enabling ownership.

`haloce_view_pair.h` implements private all-or-nothing camera preparation for
this direction. It requires exactly the established two-primary-view shape,
matching stock poses (rejecting an independent second player), no native stereo
shift, and one tracking generation/epoch/serial. It stages both eyes before any
native rebuild callback, retains the native source bytes, and rejects changed
pose, clip, raster or FOV after rebuild. The native callback in its unit tests
is a failure-injection fixture, not an emulation or execution of the game.
The helper is not connected to a hook or OpenXR submission yet.

At this stage the manifest contained 14 unique executable signatures, 18 relative operands
and 15 body-instruction witnesses. These include the actual stride, camera
offset, view-index store, secondary-view branch, output handoff and worker
counter decrement. The verifier checks pinned file identities before these
contracts. Runtime approval list remains empty.

## E-CE-6: exact native surface transfer and source selection

The backend created by `0x1EF660` calls constructor `0x203580` at `0x1EFA2C`
and assigns its result to global `0x2E3BDE0` at `0x1EFA3D`. Constructor
`0x203580` installs derived vtable `0x17F9D10`; that table's `+0x190` slot
(`0x17F9EA0`) contains `0x204C40`. This resolves the per-view output virtual
copy from E-CE-5 through CE's actual backend, not an assumed Halo 2 class.

`0x204C40` takes a `0x38` transfer request in RDX: native source/destination
wrappers at `+0/+8`, source x/y/mip/array at `+0x10..+0x1C`, destination
x/y/mip/array at `+0x20..+0x2C`, width/height at `+0x30/+0x34`. For a bounded
rectangle it forms a six-component D3D box and calls context virtual slot
`+0x170` with the `CopySubresourceRegion` argument order; its full-resource
branch calls `+0x178` with the `CopyResource` argument order. The immediate
context is backend `+0xCE0`, populated by the constructor. The actual resources
come from **resolved** native wrappers at `+0xE0`.

Do not read `sourceWrapper+0xE0` unconditionally. Native leaf accessor
`0xAD5F0` chooses the original wrapper or variants at `+0xA8/+0xB0`, depending
on renderer flags `0x8000/0x10000` and display configuration bit 9. Both source
and destination go through that accessor immediately before the native copy.
This is another instance of the repository's consumer-selects-among-copies rule.

`haloce_surface_transfer.h` represents that request and checks the exact full
primary-view transfer shape, including eye-dependent destination row, mip/array,
dimensions and wrapper identity. It neither dereferences wrappers nor obtains
or retains COM resources. The future adapter still needs source descriptor and
lifetime proof, preallocated compatible caches, exact caller/serial attribution,
and ordered GPU completion before submitting a pair. No frame-wide guessing of
the scene target is authorized by this finding.

The script-bound `IsLegacyRenderMode` function `0x84730` reads global
`0x1B7AA84 == 0`. Its script registration was already preserved in
`ce-retail-anniversary-routing.txt`; the function body is now in
`out/ce-native-mode-reader.txt`. This is a read-only mode lead, not permission
to force a renderer or a completed graphics-switch gesture.

Preserved new derivation: `out/ce-backend-create-disasm.txt`,
`ce-backend-constructor-disasm.txt`, `ce-native-surface-transfer-disasm.txt`,
and `ce-native-surface-transfer.txt`. Manifest verification now includes the
transfer function, variant selector, constructor, mode reader and exact vtable
pointer, in addition to E-CE-1..5. A short constructor prologue was rejected
because it matched five functions; only the verified longer unique prefix is
recorded. The leaf selector legitimately has no x64 unwind entry.

## E-CE-7: explicit prepared-list copy and completion distinction

September 13 later continuation verified the complete-list copy operands:
`0x45526C` loads renderer global `0x1BEA9E0`, `0x455273` supplies preparation
object `+0x70` as source, `0x455277` supplies renderer `+0xB0` as destination,
and `0x45527E/0x455284` call native memcpy with exactly `0xBDE8` bytes.
The alternate builder call `0x455611` likewise receives object `+0x70`.
These are now instruction/relative-operand witnesses in the manifest.

`0x4556B0`, conditionally called before that copy, is **not a worker wait**.
It calls `0x2E1060` with the preparation list, calls `0x4AA740` with phase 0,
sets object `+0xBE60=1`, and tail-calls `SetEvent` using object `+0xBE68`.
Neither an event signal nor a copied camera establishes an exclusive ownership
barrier. Caller scheduling/source-list exclusion still needs proof before
runtime attachment. Do not infer a wait from this helper's position before copy.

Exact new output: `out/ce-runtime-resume-handoff-resource.txt` and
`out/ce-runtime-resume-precopy-disasm.txt`; the complete preparation instruction
stream is also preserved in `out/ce-pair-binding-witnesses.txt`.
The pool refresh `0x20B880` and surface-view accessor `0x22B5D0` were inspected
in the same output. They do not establish retained source-texture lifetime or
safe cold descriptor access, so no new GPU ownership binding is approved.

## Implemented binding and handoff components (not runtime admission)

`src/dll/haloce_native_bindings.cpp` now implements the loaded-image verifier
and the CE-native camera rebuild adapter. It checks the pinned mapped x64 PE
identity and bounded non-overlapping sections, all 19 unique executable entry
signatures, required unwind entries, body witnesses, relative operands and the
rebased backend-vtable pointer. The generation-tagged result is cleared on any
failure. Scanning is cold-only; the future owner must retain the module through
binding and callback retirement. No hook installer or title admission invokes
this component yet.

`StageBoundNativePair` binds the existing private pair preparation to CE's
one-argument view, projection and culling rebuild routines, in that order.
Native invocation is SEH-guarded; stale binding generations fail before calling
the engine. Camera staging also rejects non-finite derived view, projection,
raster, culling-corner and bounds fields, without treating opaque resource
pointers as floats. No game rebuild routine was executed during offline tests.

`haloce_prepared_handoff.h` tracks separate explicit active-list and copied-list
origins. Every builder attempt revokes its previous receipt before native work,
including stock/failed attempts. Publication requires one generation/space epoch/
tracking serial and the committed two-eye camera prefix. Reading requires the
exact source list from the native handoff. Pose similarity across a ring is
deliberately insufficient: stationary cameras may be identical across frames.
The consumer must freeze that receipt for the full render frame before a new
preparation starts. Worker-owned opaque tail fields are not used as pose identity.
This receipt logic is implemented/tested but is not yet attached to native hooks;
the exclusive native scope and GPU capture are still outstanding.

`tools/re/generate_ce_contracts.py` maintains the tracked runtime contract header
from the evidence manifest; `--check` rejects drift. Six Release CTest suites
pass, including mutation tests of the actual production loaded-image verifier
and concurrent receipt publication. The same production verifier passes against
the pinned retail PE mapped into **non-executable private data**, with only the
tested absolute vtable pointer rebased. This is not LoadLibrary, injection, or a
headset result. The independent Python SHA-256/offline witness verifier also
passes. Results: `out/ce-runtime-resume-{build,tests,mapped-pe-test,reach-gate}.txt`
and `out/ce-runtime-resume-binding-verification.json`.

## E-CE-8: native preparation job execution and completion

September 13 evening continuation traced the preparation job beyond the
previously identified `0x455170` body. Its vtable at `0x1810040` points slot
`+8` to `0x455170`. Constructors `0x454FA0` and `0x18440` initialize the
static job at `0x1BAA780`; its list is job `+0x70`. The constructor creates
the separate preparation event at job `+0xBE68`, and obtains a generic job
completion object through `0xC1860`, storing it at `0x1BB6600`.

- Generic dispatcher `0xC2560` calls job virtual `+8` at `0xC2599`. Only
  AFTER that call returns does it enter job `+0x10`'s critical section,
  update completion/dependencies, and signal the generic completion event.
  The completion lock is therefore not an exclusive lock around camera
  construction. Do not claim it protects a cold camera/list read.
- `0xC24D0` resets and queues a job. `0xC2D50` pumps the main queue through
  `0xC2860` and `0xC2560`, returning after executing the exact requested job;
  while empty, it waits on the queue event with a 15 ms timeout.
- `0x87F90` sets job `+0xBE58=1` and `+0xBE5C=1`, then queues/pumps this job
  at `0x88503/0x8850F`. `0x3BBA10`, called from the camera/game update, sets
  `+0xBE58=0` and queues the preparation variant on a worker queue once per
  eligible tick. `0x3BD820` waits on the generic job completion event before
  its subsequent work when the copied-list route is enabled.

These are verified scheduling edges, not proof that an arbitrary observer has
exclusive access to either list. Native source-list association still belongs
inside the exact preparation/copy scope; workers must not mutate cameras after
culling submission. No new scheduler hook is installed. In particular,
`0x4556B0` remains a completion signal, not a wait.

Preserved derivation: `out/ce-scheduler-{owner,construction,refs,flow,execution}.txt`,
`ce-scheduler-execute-disasm.txt`, `ce-scheduler-wait-disasm.txt`, and
`ce-builder-consumers.txt`. The first refs run's final request for unrecognized
`0x22A100` failed; it does not invalidate its preceding successful scheduler
queries and is not texture-function evidence. The manifest now checks the two
dispatcher/pump entry signatures and exact execution/order witnesses.

## E-CE-9: texture descriptor paths and restricted source lifetime

The native copy's bounded branch calls the D3D context at `0x204D9E` while
inside its native critical section (`EnterCriticalSection` at `0x204C92`,
leave at `0x204E0A`). Its arguments contain the resources actually selected by
`0xAD5F0`; an adapter at this exact call can use that live source for an
immediate GPU copy. It must not retain a borrowed pointer past this scope or
substitute a frame-wide texture guess. The transfer's lock protects this
operation; it does not by itself prove safe cold acquisition of arbitrary
wrapper/resource pointers.

Texture constructor `0x2298B0` installs vtable `0x17FB608`. Its width/height
accessors `0x1F4B80/0x1F4BC0` read signed 16-bit `+0x10/+0x12`, except when
renderer bit `0x8000` redirects them through wrapper `+0xA8`. This is not the
same conditional selection as the resource accessor in E-CE-6.

`0x229B70` builds texture descriptors. Its ordinary 2D branch passes a full
D3D11 descriptor to device virtual `+0x28` at `0x22A036`, writing the result
into wrapper `+0xE0`. This branch supplies sample count 1/quality 0, uses
format `+0xD0`, and derives width/height through the native accessors. Separate
branches create 1D/3D resources; do not treat every `+0xE0` as a 2D eye.

**Negative finding:** imported resources are a different case. `0x22A1E0`
calls resource virtual `+0x50` (2D GetDesc) and saves dimensions, mip count,
format and the resource pointer into the wrapper, but does not retain every
descriptor field, including sample count/quality. `0x1EFE90` imports the
swapchain backbuffer similarly. `0x22A360` is the separate 3D import shape.
Reading a plausible wrapper width/height/format cannot prove full copy
compatibility or single-sample ownership. The current GPU adapter therefore
requires an actual, independently obtained source descriptor.

`0x22B0A0` releases resource/view references and clears `+0xE0`. Destructors
`0x2299A0/0x229A60` call it before freeing wrapper storage. Their allocator
lock is not a source-resource lifetime lock. Inspected map/update routines
`0x22ADF0/0x22B2F0` use the transfer's native lock, but they do not establish
that every destruction/import path shares it. No cold pointer-retention
binding is approved by this investigation.

Preserved output: `out/ce-scheduling-resume.txt`,
`ce-texture-descriptor-paths.txt`, `ce-resource-lock-ownership.txt`, and
`ce-import-descriptor-disasm.txt`. The attempted decompilations in
`ce-texture-shape-lifetime.txt` and `ce-texture-release.txt` failed on missing
function definitions; they are not evidence. Width/height leaves were then
inspected with the architecture-correct PE disassembler. The manifest checks
creation/import/release entry identities and the actual copy call witnesses.

## Implemented D3D11 eye storage (not native capture admission)

`src/dll/haloce_eye_cache.{h,cpp}` now allocates compatible owned eye textures
on a cold path, retains the verified immediate context, and accepts only an
exact generation/space/tracking/resource key. Capture issues one bounded GPU
copy with a separately proven live source descriptor, without querying or
retaining the source. Copies, native-frame completion and one-time submission
borrowing are separate stages. Partial, repeated, out-of-order, wrong-context,
resized and stale-resource frames cannot become a completed pair. A failed
frame can recover on the next serial. Retired resource epochs and submission
borrow IDs cannot be reused. Cold replacement declines while a submission
holds the eye textures; a stale release cannot release a newer borrow.

The standalone D3D11 WARP test performs real GPU copies and full pixel
readback: left/right receive distinct colors from one recycled source; both
remain intact after that source is overwritten and released. It also tests
frame rejection/recovery and concurrent cold retirement during a borrow.
This validates the storage implementation, not CE rendering, OpenXR submission,
gamma/cropping, or headset performance. Native source acquisition and calling
this component from verified CE scopes remain unfinished.

The copy restrictions follow Microsoft's
[CopySubresourceRegion contract](https://learn.microsoft.com/en-us/windows/win32/api/d3d11/nf-d3d11-id3d11devicecontext-copysubresourceregion).
Only bounded ordinary color 2D sources are admitted; depth, arrays and MSAA
need a separately verified path. Queued copies must be consumed on the same
immediate context after native rendering completes. The component does not
claim that returning from a void D3D copy proves physical GPU completion or
that it alone permits OpenXR submission.

## E-CE-11: rejected e17a664 and primary-eye receipt/raster correction

Recovered actual previous-chat feedback at 2026-09-15 03:07 UTC. User reports
working buttons/sticks and graphics gesture; flat Classic; black Anniversary VR
and mismatched stacked desktop views. Preserved log/report are under
`out/test-runs/e17a664-ce-anniversary-failed-20260914/`. Log SHA-256:
`6D283B2FD811A5A587AD5F0EB192F6505C60EAF8FBCE18CF62B5BB7B133EDF57`.
Source e17a664, Steam, SteamVR/OpenXR 2.17.9, Oculus-family, 90 Hz. The log
records 734 builds, zero pairs, 734 drops, stage=0, descriptorMiss=0, and source
cache 2912x1050 format=90. Backbuffer is 2912x2100. No mission/model was supplied.
Failed rendering was disabled separately in `736f0c5`; input/gesture retained.

### Verified code-level rejection conditions

1. The initial `MatchesPreparedViews` required total view count exactly two.
   Culling `0x4AA740` calls `0x4ADA70`, `0x4AEC10`, `0x4AB4E0` at
   `0x4AA97C/0x4AA983/0x4AA9A4`. Those helpers append through `0x2EA720` at
   `0x4AE0DF`, `0x4AF40F/0x4AFA58`, `0x4AB8C6/0x4ABC88`. The exact append
   increments list+8 (`0x2EAB22/29`) and uses record stride 0x3C8. Culling loops
   against the resulting total (`0x4AA9E4`). Thus a prepared primary pair can be
   intact in a larger native list. Rejecting it solely for total count is wrong.
   Storage starts at list+0x10; the next array count starts at +0xBD20, followed
   by origins at +0xBD24 (`0x2EAB09/12`), bounding the view region to 50 records.
   New exact witnesses/call edges are in the manifest/generated contracts.
2. The initial GPU cache correctly required camera raster to equal actual source
   dimensions, but the adapter never selected that source raster before camera
   rebuild. A desktop-sized camera with a half-height source can therefore never
   begin capture. `0x415200` independently shows CE setting camera width/height
   and raster scales with full/half-height modes. No constant half-height guess
   or call to that mode-changing function is added: the corrected adapter stages
   only private cameras for dimensions of its generation-bound allocated cache,
   obtained from the witnessed native source. Actual copy-time raster/resource
   checks remain mandatory. Unknown dimensions bootstrap without a VR receipt.

The old log did not record total native view count or prepared camera dimensions.
These are reproduced implementation defects consistent with the runtime failure,
not proof of which guard rejected all 734 native frames. The displaced-view
report is not conclusively explained. New bounded `CE FRAME` snapshots report
reason, native count/flags, first differing camera byte, copy mask, raster/source
dimensions, and both actual primary positions. Logging remains on the worker.

### Correction and validation

Initial publication still requires exactly two primary records. Later receipt
matching accepts total count 2..50, compares only the exact two primary records,
and leaves all auxiliary views native. Pose/FOV/raster byte checks are retained;
a changed/displaced eye is rejected. No new native hook or shared-title behavior
was introduced. Existing native preparation/render scheduling remains in use.

The added pair and production-scope GPU regressions fail against the old count
guard (`out/ce-failure-regression-red.txt`) and pass after correction. GPU tests
now use actual camera staging with a full-height stock raster/half-height source,
plus auxiliary-view copied-list handoff, changed-camera rejection and explicit
incomplete-pair diagnostics. They still use fixture callbacks, not game code.
Release/eight suites and Reach consistency pass (`out/ce-repair-*`). Pinned SHA,
instruction/call witnesses and mapped-data loaded-image checks accompany delivery.
No headset success, accepted-pointer change, installation or launch is claimed.

Extra offline traces: `out/ce-failure-{view-storage-disasm,culling-complete-disasm,
prepare-disasm,view-consumer,domain-raster,camera-handoff}.txt`. The initial
`ce-failure-culling-disasm.txt` covered only the first unwind fragment; use the
complete file for the later calls. A first disassembly at 0x4AA960 began inside
an instruction; it is not evidence for that initial decoded instruction.
Domain-object constructor/visibility callback investigation did not establish a
new runtime binding; it is retained as investigation only.

## E-CE-10: connected Anniversary runtime and source metadata (September 14)

Recovered runtime WIP is now connected in `haloce_stereo_core.cpp`, the title
registry/worker and shared OpenXR preparation/submission. This is candidate
implementation, not evidence of execution inside MCC or headset acceptance.
The native reset entry `0x2EACD0` is uniquely verified and added to the manifest
and generated loaded-image contracts (25 entries total).

Native preparation wraps `0x455170`. Builder `0x4547E0` is intercepted only for
the exact active `renderer+0xB0` or copied `job+0x70` list in that job scope.
Its existing secondary-view argument constructs the second native view; private
camera staging/rebuild happens before returning to native culling submission.
The particular copied/active receipt is frozen after the job's native body.
The generic dispatcher completes the job after its virtual body returns
(E-CE-8); no additional wait or lock is inserted. An overlapping adapter scope
declines mutation. The renderer checks the actual primary pair against the
frozen receipt. None of this licenses an external concurrent list write.

The full native frame executes exactly once. `0x45E2B0` brackets per-view output;
the transfer is admitted only at return address `0x45E376`. Within its native
copy scope, D3D11 CopySubresourceRegion at return address `0x204DA0` exposes
the actual selected resources. Those raw resources are used immediately and
not retained beyond the copy. Existing CPU descriptors must match the resource
identity and the staged raster dimensions. Native source-wrapper `+0xE0` alone
still does not bypass variant selection.

Descriptors are obtained at native creation/import, where the native caller
owns its resource, and at early D3D11 CreateTexture2D completion so textures
created before title-hook attachment can be recognized. Device virtual slot 5
is the same creation call already witnessed at `0x22A036` (`+0x28`, E-CE-9).
GetDesc/QueryInterface occur in resource management only, never the output/copy
hooks. Only metadata is published; native resource release revokes it. Unrelated
ordinary textures do not replace render-target records. Hash collisions decline
the affected capture. Module retirement invalidates prior pointer identities.
Failure to install early metadata observation leaves existing titles unaffected.

The native packed destination is **not assumed** to accommodate two full-height
views. `0x20A9B0` derives allocation dimensions from pool flags; `0x20B510`
selects by those flags and pool identity. A matching native transfer alone does
not establish allocation height. The implementation validates actual source and
destination descriptors inside the copy. For a synthetic pair whose second
copy exceeds a known single-height destination, the desktop preview copy uses
y=0; the independently owned left/right VR copies remain distinct. Unknown,
incompatible or otherwise invalid copy shapes perform no synthetic GPU copy.
No native texture allocation-size or split-screen flag is patched.

Preserved offline derivations: `out/ce-live-integration-resource-evidence.txt`,
`ce-packed-target-allocation.txt`, `ce-packed-pool-and-culling.txt`,
`ce-packed-target-dimensions.txt`, and `ce-native-stereo-texture-shape.txt`.
The last file's request for undefined `0x1F3790` failed; it is not evidence for
that function. The successful preceding texture-construction output is retained.

The shared compositor borrows only complete, current-generation/resource pairs
and submits the exact older preparation poses/FOVs associated with those
pixels. Recenter has its own revision in addition to the OpenXR tracking-space
epoch. Old prepared/completed frames cannot cross it. A new native frame revokes
the preceding completion. Frame age is bounded by both serial and elapsed time.
Native list-reset hooks remain until synthetic lists drain before detour removal;
callback/trampoline quiescence and cache borrows protect retirement.

Production-scope WARP fixtures exercise explicit copied-list handoff, actual
distinct GPU pixels, native source reuse, bounded desktop preview, missing eye,
invalid copy boxes, changed raster dimensions, resource revocation/bootstrap,
recenter before/after rendering, stale age, foreign title/space, and retirement.
They execute fixture native callbacks, not game code. Eight Release suites pass;
generated-contract, pinned SHA/witness, mapped-data PE and Reach checks are run
for the candidate. The runtime tests do not establish native scheduling or
headset success by themselves. The first headset test protocol and remaining
limits are in `HALOCE-CANDIDATE-2026-09-14.md`.

## Still unresolved (historical September 13 list; see E-CE-10 above)

GPU capture source/lifetime and final image mapping; exclusive native preparation
scope and attachment of the implemented receipt logic; installed hooks/title admission;
Classic stereo integration; HUD/crosshair routing; controller aim/hands;
graphics-switch gesture wiring and runtime acceptance. No CE VR support is
claimed yet. Physical melee and world collision remain deferred until injection
is confirmed. Packaging is held by the September 13 user instruction until a
functioning comparable-6DoF implementation is reasonably expected. The owned
GPU cache above is implemented, but live source descriptor acquisition,
per-view output attachment and final color/crop mapping are not. Also verify
the `0x04000001` packed native destination dimensions under forced two-view
construction; E-CE-5 proves the transfer shape, not that allocation shape.

## E-CE-12: 6e31b25 partial headset result and tracked view construction

The September 15 user log identifies 6e31b255d3e242bf5b41bac36158a825fbe255b9,
Steam / SteamVR OpenXR 2.17.9 / Oculus family / 90 Hz. It records 805 builds,
803 pairs and 6 drops; 2912x1050 source, 2912x2100 desktop, auxiliary counts
6/12, and prepared eye separation approximately 0.071 Saber units. User reports
faster Anniversary switching and visible imagery, but the right eye remains
outside/displaced and the left appears flat/head-attached. Classic is flat.
This is partial capture progress, NOT stereo/6DoF acceptance. Preserved exact
request/log: out/test-runs/6e31b25-ce-partial-failed-20260915/. Log SHA256:
658DD33FFD2BC311702BD1C0138872745E0FC8B50C7235CE619FAA1BC05BFB4A.
Separate failure-disable commit: f63254b. Accepted source remains 4e01f28.

### Verified preparation discrepancy

Native append 2EA720 derives more than the camera record. It rebuilds camera
culling data at 2EA96D. Primary/secondary branches load the next origin index
from list+BD20 at 2EA9DC / 2EAA02. The copy at 2EAB04 reads camera position
xy (camera+30); 2EAB0F reads z. 2EAB09 increments the origin count, 2EAB12 / 1B
store the position at list+BD24+12*index, and 2EAB50 stores the origin index in
view+1E. The table precedes the primary count at BDE4 (16 position slots).
The old adapter replaced only view.camera after the native builder returned.
It therefore retained stock origin metadata alongside tracked camera positions.
This is a code-level mismatch, not a proven cause of every reported visual fault.
An analyzed-reference scan of BD24 found append/reset only; it does not prove
all consumers of the table. Rendering can address it through another base.

Native primary builder writes near/far from its settings at 454A7B / 454A8D.
The native stereo branch applies those same settings to the right at 454B52 /
454B5D. The ordinary secondary branch instead appends at 454C93 and returns
straight into the epilogue; it does not repeat that override. New construction
uses the primary's resulting clips for both eye cameras, then rebuilds their
projections/culling. No constant clip distance is imported from another title.
The actual prior log did not capture clip values; a runtime mismatch is unproven.

### Implementation and guards

Only exact native append calls returning to 454A6E / 454C98 inside the owned
builder/list may receive a private tracked camera. Tracking, generation, space,
raster and recenter reference are frozen for that construction. Both cameras
are staged before their own native append; native code creates the origin table,
view identity, resources and opaque fields. Finalization preserves each native
record, checks its requested pose/FOV/raster, applies coherent native clips and
rebuilds once more without applying tracking again. Split-player/foreign/auxiliary
calls remain native. Incomplete construction drops the frame; it never submits
mixed cameras. Old disabled post-build implementations are retained.

Append's four register arguments and twelve ABI stack slots are forwarded
without reinterpreting the native mixed-width stack data. A compiled mixed-type
caller/callee regression goes through the actual production hook and confirms
all arguments and the return value. The existing unique append signature is
used for installation; new body witnesses pin origin stores and clip branches.
Teardown includes the new hook in quiescence checks. No hot logging/allocation,
COM queries, file I/O, signature scanning or new locks were added.

New CE ORIGINS snapshots report native origin positions and both clip ranges
from the worker log. They are diagnostics; texture-copy success still does not
prove visually correct native drawing. No new shared-title behavioral code.

### Validation and limits

Release/eight suites and Reach consistency pass. Added construction regression
uses translated/rotated tracking, demonstrates stale origins under append-then-
patch ordering, verifies no double tracking, matching native clip ranges,
per-eye opaque-data preservation and partial/changed-camera rejection. Production
WARP scopes continue to verify real distinct copied pixels and frame recovery.
The pinned SHA/witness and production non-executable mapped-PE checks pass.
Tests use native-call fixtures; they do not execute MCC's renderer.

Offline traces: out/ce-0915-{render-consumers,view-rebuild-disasm,clip-consumers,
scene-callback-refs,scene-callback-bind,scene-initialize,actual-scene,
actual-scene-disasm,camera-slot,world-consumers,origin-consumers}.txt.
The scene callback resolves through constructor 8D032/39 and vtable 17F0A48
(+18 -> 497BB0). It passes the supplied camera onward into scene rendering.
Camera+220 is the source-camera/player index (4684C0 assigns it); the adapter
preserves it. It is not interchangeable with the manufactured eye's view index.
These traces do not establish a new controller, Classic or scene-render hook.
An initial JAVA_HOME lookup used a stale folder name, but Ghidra ran successfully
with its configured runtime; subsequent commands use jdk-21.0.12.1+1 explicitly.

Both CE renderers, two independent tracked hands, controller aim, HUD/crosshair,
locomotion and native state/vehicle parity remain requested and unfinished.
Physical melee/world collision remain deferred until functional injection is
confirmed. No install/launch/game writes/publishing or accepted-pointer advance.
