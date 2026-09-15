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
