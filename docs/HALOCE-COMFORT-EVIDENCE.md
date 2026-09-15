# CE Anniversary motion-blur parity

## E-CE-COMFORT-1 — per-player history and the shared off setting

Halo 3's reference behavior honors `motion_blur`, which defaults off. Its
accepted implementation explains that sequential eyes otherwise share a
previous-camera history and introduce false camera velocity. CE requires its
own binding for this behavior.

Pinned CE SHA-256:
`0A12DC561780F449D3F4D0DF10BB8D3BC7BE7840A5BEB2B236F672EB6CD42E6C`.
All addresses are RVAs in that image. No game process was started or attached.

The Anniversary postprocessing dispatcher `0x450B20` calls `0x446580` at
`0x45107A`. Arguments are the effect object, fullscreen quad, quarter-resolution
source, selected camera, and a floating-point strength. The native implementation
checks backend-configuration byte `+0x223` at `0x4465EE`; when off, it returns
before updating history or drawing. When enabled, it calls `0x446110`.

That updater indexes history by **source player** at camera `+0x220`, with
stride `0x7C`. It derives the camera-to-world matrix from the supplied camera,
compares it with the stored position/orientation, updates filtering, and stores
the current matrix at effect `+0x48 + player * 0x7C`. Both VR eyes deliberately
retain source player zero. Consequently eye one sees eye zero's position as its
previous position, and the next eye zero sees eye one's. Changing source-player
identity would also change unrelated gameplay/effect selection and is not a fix.

`test_ce_motion_blur_native.py` executes the complete native updater and matrix
inverse, including its chained unwind fragments, using the existing native camera
verifier. Six updates reproduce the cross-eye history at a stationary headset
and verify isolation of player-zero/player-one history. Result:
`out/ce-motion-blur-native-verification-20260915.json` (30 native calls,
14,086 instructions). This proves the history interaction, **not** that motion
blur was enabled in the user's failed run or caused the displaced world image.

## Runtime adaptation and isolation

The independent `haloce_comfort` transaction verifies its own unique hook and
callsite contracts. At that exact caller, it honors the **rendered pair's frozen**
`motion_blur` setting, supplied only for the current primary Anniversary scene
camera after matching depth, scene and shading receipts. A stale camera,
different caller, auxiliary view, renderer switch, retired generation, lost
tracking or stock render cannot acquire that ownership.

With the setting off, the adapter returns before native history/draw work,
matching the native off branch. With it on, it passes all arguments unchanged.
It never writes a native setting or history record. Binding failure leaves only
motion blur stock and is logged. Native exceptions retire callback ownership
before propagating. `haloce_comfort_tests` exercises the production dispatch,
both admitted eyes, stock/retired/foreign cases and exception recovery; the core
runtime fixture independently tests the exact-eye admission accessor.

## Postprocessing review limits

The reviewed subsequent passes include native depth-of-field (`0x445950`),
radial blur (`0x448F30`/`0x446F70`), effect overlays and HDR/bloom (`0x4D3600`).
Their observed camera-dependent branches receive the current per-eye camera;
source-player-dependent effect parameters remain shared deliberately. This
review did not establish replacement of the rendered world by a stock frame.
Records: `out/ce-postprocess-{primary-passes,final-consumers}-20260915.txt`
and `out/ce-motion-blur-history-20260915.txt`. It is a bounded static review,
not complete GPU execution or headset evidence.

The failed Anniversary world-render result remains unresolved. This correction
does not authorize enabling rejected stereo paths or advancing acceptance.
