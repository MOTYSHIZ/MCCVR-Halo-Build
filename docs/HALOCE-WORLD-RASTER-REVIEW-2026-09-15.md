# CE Anniversary world raster review - September 15, 2026

## Result

This follow-up does not establish a correction for the user's `b9662cd`
displaced-right / flat-left world result. It checks the native target/raster
boundary separately from the already verified camera arithmetic. A different
axis, unit scale, FOV, GPU flush or forced native device-stereo setting is not
supported by this review.

The inspected module remains the pinned CE image with SHA-256
`0A12DC561780F449D3F4D0DF10BB8D3BC7BE7840A5BEB2B236F672EB6CD42E6C`.
All work was offline and read-only against that image. No game process or
installation was changed.

## Native target and raster path

- Target binder `0x205E40` resolves existing native surface variants through
  `0xAD5F0`, obtains existing RTV/DSV identities through the wrapper virtuals,
  derives dimensions through the selected wrapper's width/height accessors,
  and calls viewport/scissor setter `0x206160`. Its full body extends beyond
  the first unwind fragment. The viewport starts at zero and spans the selected
  surface dimensions; it is not inherently the full desktop height.
- Consequently, the proven native half-height world source is different from
  the separately diagnosed HUD callback's full-height raster. The HUD issue
  does not itself justify multiplying the world camera's height or FOV again.
- Final color sources may be reused after each eye is captured. Depth images
  must survive both depth passes before their corresponding scene passes.
  The current depth receipt checks therefore remain material even when both
  final color captures succeed.

Existing complete native records are
`out/ce-hud-layout-target-d3d-consumer-20260915.txt`,
`out/ce-hud-layout-target-d3d-tail-20260915.txt`,
`out/ce-hud-layout-target-viewport-20260915.txt`, and
`out/ce-hud-layout-view-accessors-20260915.txt`.

## Deferred-worker check

An initially plausible target-selector race was checked against the native
control flow. The early depth prequeue in `0x455A10` explicitly requires
active-list flag bit 0 to be clear. The prepared two-primary-view list has
flags `1`, so this early single-view optimization is excluded. It cannot be
treated as the established cause of this candidate's eye mismatch.

The scene path constructs per-view jobs through `0x312D00`. It carries the
selected view address at batch `+0x38`, target-related pointers at
`+0x50/+0x58/+0x60`, and the native target descriptor at `+0xB8`.
Worker `0x310930` rebuilds the camera constants, begins its command list, and
applies that descriptor with backend virtual `+0xF8`. `0x3134F0` waits for
native completion before executing a queued command list; otherwise it uses
the immediate draw path. These facts supply no permission to replay a whole
frame or change worker completion counts.

Records: `out/ce-resume-saber-frame.txt`,
`out/ce-world-view-selection-20260915.txt`,
`out/ce-world-worker-camera-selection-20260915.txt`,
`out/ce-0915-render-consumers.txt`, and the fresh read-only decompilation
`out/ce-world-batch-target-review-20260915.txt`.

## Rechecked validation

```text
python tools/re/test_ce_depth_targets_native.py --image out/deps/re-tools/inputs/halo1.dll
```

The existing native-instruction test passes all seven cases. The ordinary
independent children bind `2912x1050` with matching viewport/scissor state.
Disabled split configuration, unavailable children, aliased wrappers, aliased
DSVs and different DSVs sharing a resource demonstrate why width/height or
DSV identity alone cannot prove independent eye depth. Output is preserved at
`out/ce-world-raster-depth-review-verification-20260915.json`.

The D3D endpoints and geometric ownership in this test are modeled. This
result is not a GPU scene capture or headset result. The new candidate's actual
camera/depth/source ledger and the user's headset report must determine whether
the guarded integrated path produces correct world stereo.

## Rejected `be2140f`: projection mode and remaining color evidence

The user confirmed that the displaced view also appears in one headset eye.
The stacked desktop image is therefore not the entire failure. The native
output routine `0x45E2B0` deliberately copies primary view 0 at destination Y=0
and primary view 1 at Y=source height. The current bounded copy wrapper retains
that native desktop arrangement when it fits. Changing only the desktop mirror
cannot establish correct headset world rendering.

The latest camera-consumer receipt checks the supplied native camera address,
its pose/projection prefix and player field, and the renderer's selected camera
pointer. It does not observe every native renderer mode or the completed GPU
world draws. A fresh review found an alternate projection branch in renderer
virtual `+0x48`, `0x2351E0`, selected by renderer `+0x9C` bit `0x20` when the
backend configuration's `+0x128` bit 26 enables the common constant upload.
The preceding tests explicitly seeded renderer flags `1` and did not exercise
this branch.

Native setter `0x2EC2F0` (renderer virtual `+0x88`) ORs a nonzero mode into
`+0x9C`; an argument of zero clears bits `0x30`. Surface callbacks `0x528A30`
and `0x52B210` can set mode `0x20`, with plane position/normal at renderer
`+0x10/+0x1C`. Callback `0x528B00` clears it. Their invocation lifecycle has
not been established here, so a leaked reflection mode remains a hypothesis.
Generic prop-container dispatchers `0x48F2B0` and `0x48F460` invoke child
virtuals `+0x50/+0x58` over the same two child arrays, but this review did not
tie those wrappers to the primary per-eye scene lifetime. They do not prove
that the clipping mode is paired correctly in the failing frame. The inspected
`0x520EC0` is a sniper-screen copy and `0x310640` constructs render packets;
neither supplies that missing lifecycle evidence.

`tools/re/verify_ce_native_camera_math.py` now executes the alternate native
branch, including actual inverse `0xFED50..0xFF468` and common CPU constant
writer `0x235860`; only CRT double `sqrt` is modeled. Sixteen combinations of
two camera positions, two yaw angles and four clipping planes pass. The
uploaded origin and matrix columns for clip X, Y and W remain identical
(maximum error **0**); only clip depth changes. This is evidence against this
mode itself translating the eye to another world position. Incorrect clipping
is still possible if the mode is wrong; its runtime value is not in the log.

The complete native/production-adapter run also passes its existing 189 camera
checks, 54 native producer cameras, 162 adapter checks and 108 queued-worker
camera setups. It remains a CPU proof, not a rendered scene or headset result.
Reproduce with:

```text
python tools/re/verify_ce_native_camera_math.py out/deps/re-tools/inputs/halo1.dll --adapter-exe out/build/release/Release/halomccvr_ce_view_pair_tests.exe --output-dir out/ce-native-camera-mode-review-20260915
```

Records: `out/ce-native-camera-mode-review-20260915.json`,
`out/ce-scene-reflection-mode-setters-20260915.txt`,
`out/ce-scene-view-begin-modes-20260915.txt`, and the existing complete uploader
record `out/ce-renderer-matrix-upload-consumer-20260915.txt`.
The bounded callback follow-up is preserved in
`out/ce-surface-container-pre-post-20260915.txt`,
`out/ce-surface-callback-root-dispatch-20260915.txt`, and
`out/ce-reflection-callback-lifetime-20260915.txt`.

### Unobserved color target relationship

The latest log identifies independent final color resources and independent
depth receipts. It does not tie actual world color draws to those final color
children. Native target binder `0x205E40` copies its descriptor to backend
`+0x18`, records color wrapper roots at `+0x28..+0x40`, RTV count at `+0xCF8`,
RTVs at `+0xD00..+0xD18`, and DSV at `+0xD20`, before calling D3D
`OMSetRenderTargets`. These are verified native CPU records, not proof that an
arbitrary later cached resource received the expected draws.

At the existing scene-camera hook, native `0x45DCF0` has just selected and bound
`BB_MNG_POOL`. Subsequent postprocessing may ping-pong through `0x45DD70` and
`BB_MNG_POOL_1`; therefore a direct equality requirement between the initial
world target and final output source would be unsupported. A bounded future
diagnostic can record both endpoints, selected surface/resource/RTV identities,
and the intermediate native target bindings. Queued worker backends need
their own records. No such diagnostic or target-routing change is implemented
by this review, and no rendering correction is claimed.
