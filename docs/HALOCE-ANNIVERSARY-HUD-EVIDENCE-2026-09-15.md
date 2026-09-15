# CE Anniversary per-eye HUD callback - September 15, 2026

## E-CE-AHUD-5: be2140f failed admission and integrated replay correction

The supplied be2140f headset log recorded zero replays, 4,890 fallbacks and
failure 1. `FrameScope::renderFlags` was zero-initialized and never assigned
from the native frame argument. Consequently its HUD-enable bit check always
failed before callback/source/target validation. `FrameBody` now retains the
actual native flags. This fixes a demonstrated admission defect; it is not a
correction for the independently reported displaced world in one headset eye.

The production runtime suite now follows `FrameBody -> OutputBody -> replay ->
callback -> source copy -> EyeCache`, using real WARP textures and production
HUD raster observation/mapping/restoration. Native callback/preamble/target
push/pop are explicit fixture services. Both callback images reach their eye
captures only when bit 4 is enabled. Missing stack capacity leaves world eyes
available, and the following valid transaction recovers. Exact GPU raster,
borrowed output and native stack depth restoration are checked. Earlier
standalone target/layout tests never exercised this frame-level admission.

Structured exceptions in the optional replay are contained after its cleanup
blocks run. Verified cleanup records HUD failure 4 and preserves world capture;
unverifiable cleanup records failure 5 and drops that frame, retaining the core
and hooks. Fault fixtures cover callback and preamble exceptions, a mismatched
native stack, refusal to guess a pop, and recovery after native state is valid
again. No render-hook logging, allocation, COM getter or lock is added.

Replay admission also reports whether a rejected raster transaction was
untouched or successfully restored. An unavailable optional layout installation
can leave numeric raster observations available while rejecting replay; that
case must preserve the world pair. A production integration regression keeps
observation active with layout installation absent, verifies zero native HUD
callbacks and both original world images, then verifies recovery after layout
installation. An attempted transaction with unknown cleanup still drops only
the affected frame.

Focused Release build/runtime tests pass locally. Actual native HUD visibility,
authored-reticle behavior and full CE headset parity remain unconfirmed. All
four rejected core enable flags remain false during this correction work.

## E-CE-AHUD-1: native callback omitted by the forced two-view list

Halo 3 behavior being matched: the native gameplay HUD reaches both eyes,
uses the common size/aspect/height settings, and keeps authored reticle capture
independent of gameplay HUD framing. CE's optional implementation uses its own
native callback and target stack; no H3 native offsets are reused.

The pinned `halo1.dll` SHA-256 is
`0A12DC561780F449D3F4D0DF10BB8D3BC7BE7840A5BEB2B236F672EB6CD42E6C`.
All addresses here are RVAs. Read-only native traces are preserved in
`out/ce-anniversary-hud-order-ultra.txt` and
`out/ce-native-hud-{bridge,scope-pair,viewport,restore,host,composite}-ultra.txt`.

The Anniversary frame tests render-flags bit 4 at `456D04`, a callback owner
at `456D0F`, and native list flags bit 1 at `456D24..456D2F`. The forced two-view
list uses bit 0 and therefore skips that native per-eye HUD branch. This is a
separate missing-feature finding; it does not explain the displaced-world
headset failure. The native branch calls the HUD callback before per-eye output
`45E2B0`, making the output hook an explicit replay boundary before exact source
copy. No graphics-mode or player-count global is changed to force the branch.

## E-CE-AHUD-2: callback, native setup and target ownership

Initialization `8D088/8D08F` publishes `740B0` at callback pointer `1C33FE0`.
Native preamble `4255A0` pushes/pops the target stack, clears native shader
constant `+350` and marks its publication dirty, then tail-calls that callback.
The adapter verifies those exact bindings independently of core camera hooks.

Callback `740B0` temporarily borrows target kinds 1/2 and selects the current
output wrapper `2E3D0D0` when present. Its native cleanup returns the prior kind
wrappers. The production replay borrows this output pointer for the verified
eye-source root only during the native callback. It requires one native player,
Anniversary mode, enabled native HUD state, fresh CE eye ownership and a
descriptor/registry source matching that eye's exact raster.

The wrapper/descriptor binder and owned RTV/DSV semantics are separately proven
in `HALOCE-HUD-TARGET-EVIDENCE-2026-09-15.md`. Outer native push `1DC1E0` and pop
`1DC2F0` preserve the pre-HUD descriptor. Native push grows its `0x48`-stride
storage when capacity is exhausted, so replay admits only existing nonnull
storage with room for the outer push and the preamble's inner push. Capacity
and depth are bounded before arithmetic. The adapter adds no allocator or COM
query; the ordinary native HUD callback retains its existing engine services.

Before the final pop, backend, context, stack-storage identity and expected
depth must still match. The return value, final depth and exact descriptor are
then checked. The borrowed output pointer is restored only while it still has
the adapter's selected identity. Unknown native state is not popped or silently
overwritten. A failed optional HUD transaction increments a failure counter and
retains the camera path.

## E-CE-AHUD-3: full-height authoring versus half-height eye sources

Callback `740B0` dispatches native interface `B31D88` for player zero at `74345`.
The split-HUD half-height branch depends on a real native player count greater
than one; a synthetic camera pair still has one native player. Native callback
raster authoring therefore remains at full desktop height. Its unconditional
final viewport at `743E1` also reads full dimensions from `config+118`.

`HaloCEHudLayout_BeginEyeReplay` establishes known full-height authored numeric
raster state and maps viewport/scissor Y to half height throughout that callback.
It requires native width equal to eye width and native height exactly twice eye
height. Gameplay size/aspect framing composes before this mapping. Configured
height translation is compensated so it remains measured in output pixels.
Curvature remains the separately logged native-flat limitation.

Authored-reticle suspension removes gameplay framing but retains the eye
mapping. Private reticle capture bypasses both transforms. Its saved numeric
snapshot deliberately remains in authored pixels; the existing native-setter
restore applies the eye mapping once. Returning an already-halved snapshot
would halve restoration twice. Layout state slots cannot evict an in-flight
eye replay, even when no gameplay scope is open.

## E-CE-AHUD-4: exact raster restoration and validation

A native target pop sets viewport/scissors from target dimensions, which does
not preserve an earlier custom numeric viewport. The adapter therefore saves
the observed pre-preamble raster separately and restores it after the verified
outer pop. Inner callback exit restores its own entry raster first. Native
descriptor and numeric raster restoration are checked independently.

Validation records:

- `out/ce-anniversary-hud-bindings-20260915.json`: all four signatures are unique,
  have x64 function entries, and match every pinned relative/body witness.
- `out/ce-hud-completion-build-20260915.txt`: production HUD layout and target
  Release WARP suites pass. Layout exercises both-eye replay, full/half raster
  mapping, output-pixel height, native reticle capture/restore, nested admission,
  exact callback raster restoration, invalidation and next-transaction recovery.
- `out/ce-hud-target-replay-native-20260915.json`: native pinned push, pop,
  preamble and binder execute for two replay transactions with an existing
  stack entry. The original stack depth/storage/descriptor return exactly and
  each native binder reaches its D3D setters. The gameplay callback and D3D
  endpoints are explicit fixtures; the native callback's pixels are not tested.

These checks establish bindings and restoration behavior locally. Actual HUD
visibility and the Anniversary world stereo/6DoF failure still require headset
testing; no accepted-build pointer is advanced by this work.
