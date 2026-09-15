# CE graphics-toggle camera continuity - September 15, 2026

## Reference behavior and user evidence

The Halo 3 behavior being matched is one stable tracking origin: headset
rotation owns on-foot pitch/roll and contributes yaw, while the configured
right-stick snap/smooth turn changes native body yaw. Rendering settings do
not redefine the direction the player is facing or consume a physical lean.
CE Original and Anniversary share the same native player camera through the
independently verified CE-to-Saber bridge (E-CE-3); they need one tracking
reference across the graphics toggle.

The user reports both CE modes visible in source `a2b526a`, with direction
misalignment after changing graphics mode and no right-stick pitch response.
The supplied CE log starts at 14:10:20.985: Steam, SteamVR/OpenXR 2.17.9,
Oculus-family headset, 90 Hz panel. It reports 2,355 admitted turn updates,
no control exceptions, and stock fallback during the later pause/exit interval.
It does not contain head-reference samples sufficient to measure the reported
angular jump. Right-stick vertical pitch was already deliberately suppressed
by the accepted-H3-shaped on-foot controls; this change does not accumulate a
second independent pitch on top of the headset.

## Reproduced defect and correction

`CeObserveRendererMode` previously invalidated renderer receipts and also set
`recenter=true` on every Original/Anniversary transition. Both rendering entry
points consume that flag by replacing `Reference` with the current HMD pose.
If the player turns their head after recentering, a graphics toggle therefore
maps their current direction back onto the native body's stock direction. It
also consumes the current room translation. This behavior is established by
the production code and a regression fixture; no new native binding is needed.

The transition still advances the renderer epoch and reference receipt revision
and clears the completed pair. It now preserves the reference pose and any
explicit pending user recenter request. Old-mode eyes, gameplay receipts and
first-person receipts remain invalid. Anniversary observes the mode transition
before freezing the new preparation revision, avoiding a first new-mode pair
that is immediately invalidated during gameplay-center publication.

Explicit recentering, tracking-space changes, title-generation changes and
existing camera-heartbeat recovery retain their reference reset behavior.
The working scene-refresh/visibility implementation and the four disabled
experiments are retained.

## Verification

`haloce_classic_runtime_tests` runs the production renderer entry against native
call fixtures and WARP textures. The added regression starts with approximately
63 degrees of yaw, 26 degrees of pitch, 14 degrees of roll and nonzero room
translation. It compares actual consumed Original eye cameras with Anniversary
staging, changes mode back through the production Original entry, and checks:

- Matching forward/up, position and physical eye separation across modes.
- Old mode context/image rejection despite preserving the tracking origin.
- A valid first fresh Original pair after the return toggle.
- Explicit pending recenter survives a graphics toggle and still takes effect.
- Tracking disabled for pause produces one native stock render; restoring
  tracking resumes two eyes and gameplay ownership without rebasing the pose.

Before the correction, the new test failed specifically on the automatic
Anniversary recenter and the changed consumed Original pose after returning.
After the correction, the focused Release build and CTest pass. Existing
generation, tracking-space, stale receipt, teardown, real DXGI resize and
native-state restoration cases remain covered by the same production fixture.

These are local behavior and math checks, not headset acceptance or proof of
every native material/weapon. The accepted source pointer is unchanged.

## Primary draw ownership for the Classic first-person lens correction

The optional Classic lens correction needs a narrower interval than the
prepared-frame context used to build first-person palettes. `BBCF30` class 2
reflections enter several of the same native effect/model stage roots as the
primary view; the retained body does not prove that all five first-person lens
callers are unreachable in reflections. A recent palette alone also cannot
authorize changes after the primary view has returned.

`HaloCE_GetClassicPrimaryEyeContext` therefore uses the existing verified
`ClassicViewBody` receipt: exact primary return `BBCCB2`, class 1, the staged
window camera identities/bytes, distinct frusta, player, eye, clock/tick and
current frame/reference/renderer lifetime. It is available only while the
original primary `BBCF30` is executing. Every auxiliary invocation masks the
scope, including nested views. Native unwind restores the prior scope through
`__finally`. Broader palette/HUD context timing is unchanged.

The seventh native argument, currently named `reflected`, must **not** be
required to equal zero. In `out/ce-retail-window-disasm.txt`, `BBCAA5` clears
ESI; the reflection at `BBCC77` receives class 2 and SIL=0. `BBCC82` sets SIL
to 1 only after that reflection returns. The primary at `BBCCAD` then receives
class 1 and SIL=1. It is therefore normal for a verified primary eye to carry
this byte when it follows a reflection. View kind and exact caller/cameras
prove primary ownership instead.

The production Classic fixture verifies both eyes admit the narrow context,
including this primary byte=1 path, and denies it before/after primary draw,
in a reflection and in a nested reflection that raises a native exception.
The primary scope is restored after that exception and fully retired after
the frame, while both eye outputs still complete normally.
