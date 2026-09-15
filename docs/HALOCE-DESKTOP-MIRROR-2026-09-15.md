# CE single-view desktop output

## Requested behavior and scope

The user's `3.PNG` already demonstrates the failure: the desktop has two
vertically stacked images, with normal ground/weapon above and below-world
geometry below. The user separately confirmed that one headset eye shows the
bad view. The desktop layout and the headset world-render defect are both
required fixes; changing the desktop cannot establish a corrected headset eye.
The original screenshot and log remain in
`out/test-runs/be2140f-ce-partial-failed-20260915/`.

Halo 3 is the experience reference: its native render order leaves one final
eye on the desktop (`game.cpp`, normal two-pass render). CE's native output
instead packs two eye surfaces vertically. The new Present-only mirror draws
the completed left eye into the desktop rectangle, using the frozen camera's
aspect ratio to center-crop without stretching. Selecting the left eye is a
deliberate presentation choice; it adds no native camera or game-memory binding.

## Implementation

`haloce_desktop_mirror.inl` runs only with an admitted CE completed-pair borrow,
CE presentation ownership and a stereo world frame. Submission holds the same
borrow through the desktop draw. All existing headset/screen image copies happen
first. The mirror rejects either input eye as its destination and never writes
the headset textures.

The source shader-resource view's actual typed format selects sRGB conversion.
The finished eye bypasses the shared blit's size-based reticle alpha repair.
The cached desktop render-target view is released on resize and title detach;
optional preparation/draw failure retains native desktop output and is logged,
without changing camera ownership or the OpenXR session.

The mirror restores render targets, depth, blend, viewport, rasterizer, input
layout/topology, the changed texture/sampler slot, and all five graphics shader
stages with their dynamic class bindings. A zero-viewport entry state remains
zero. The shared state backup cannot restore shader class bindings, so its
shader fields are cleared before its restore; complete mirror-private bindings
then restore the native shaders. The WARP test reproduced a crash when a linked
pixel shader was temporarily rebound with no class instance, establishing why
this ordering is necessary.

## Validation and remaining work

`haloce_desktop_mirror_tests.cpp` executes the production include on a real
D3D11 WARP device. It checks distinct eye colors and byte preservation,
non-square eye cropping on both axes, sRGB/UNORM/typeless source handling,
512-square alpha preservation, restrictive native render state, tessellation,
dynamic shader linkage, invalid/retired borrow identities, destination aliasing,
resource failures, resize/release lifetime and next-frame recovery.

Final local checks passed: cumulative Release build, all 20 CTest suites and
the Reach consistency gate. Records are
`out/ce-mirror-final-release-20260915.txt`,
`out/ce-mirror-final-ctest-20260915.txt` and
`out/ce-mirror-final-reach-gate-20260915.txt`. These tests exercise local code;
they do not enable or deploy the rejected CE core.

The mirror is source work under the CE packaging hold. All four rejected CE
core enables remain false. Neither these local tests nor the desktop mirror
resolve the Anniversary scene geometry defect, prove Classic headset behavior,
or replace the required CE and Halo 3 headset results. No package or deployment
is authorized by this document, and the accepted source pointer is unchanged.
