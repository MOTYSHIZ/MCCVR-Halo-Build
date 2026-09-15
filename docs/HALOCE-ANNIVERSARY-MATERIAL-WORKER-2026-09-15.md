# CE Anniversary material preparation correction - September 15, 2026

Halo 3 behavior being matched: tracked hands and weapons keep their physical
scale and use the actual eye's world projection for depth, color and effects.

## Proven scheduling defect

The `fba9ee6` headset log records zero Anniversary first-person projection
applications and 11,774 fallbacks. Its independent particle draw correction
does apply. The previous eye-ownership correction tested the material helper
inside a synthetic render eye, but native material writers run earlier.

Pinned `halo1.dll`, SHA-256
`0A12DC561780F449D3F4D0DF10BB8D3BC7BE7840A5BEB2B236F672EB6CD42E6C`,
establishes this preparation chain:

| Native code | Observed responsibility |
| --- | --- |
| `0x2D1390` | Work callback distributes skinned objects to `0x2D0D80` |
| `0x2D0D80` | Prepares visibility, packs bones, then calls `0x2F35B0` |
| `0x2F35B0` | Constructs material records for their eligible native views |
| `0x2F3460` | Allocates material constants and invokes class vtable `+0x98` |
| GLT `0x264080`, ZFILL `0x26FF00`, SFX `0x26E960` | Write the first-person lens selector from model `+0x28 & 0x10000000` |

`0x2F3460` passes the model at record `+0x58` as argument three. Its TLS write
at `0x2F3553` stores the preparation view index. It neither uploads a camera
nor enters a render-stage eye scope. The selector is cached material policy;
the actual world/first-person matrices arrive separately at draw time.
Requiring render-thread eye TLS here is therefore invalid. This also explains
why the particle correction, which operates during the actual particle draw,
can succeed while every material writer refuses.

## Correction

The existing three material hooks still call their native writers first. They
admit only the native first-person model flag and canonical four-lane selector.
A fresh, current Anniversary gameplay/reference context authorizes world-lens
selection while VR is armed. The helper rechecks context lifetime and blocked
presentation before its bounded write. Classic mode, stale or revoked context,
foreign title/generation and noncanonical selector values retain native bytes.

No eye-specific matrix is cached by this correction. Each later depth, color or
effect draw selects the world matrix that its own native camera uploaded.
The two disproven latest-palette and render-eye gates remain disabled in the
source. Original's successful first-person lens path, copied-bone scale and
particle draw ownership remain unchanged. New cold counters identify policy,
selector and changed-context refusals without logging in the material hooks.

## Verification

The production first-person regression clears the latest palette receipt and
removes all eye-scope ownership while retaining fresh gameplay policy. Old
production code fails this test; corrected code passes. Preserved records:
`out/ce-projection-before-build-20260915.txt`,
`out/ce-projection-before-test-20260915.txt`, and corresponding `after` records.
Additional cases cover all three selector offsets, unrelated bytes, blocked
presentation, graphics mode, reference/renderer/XR-space/generation changes,
expired tracking and revocation during the correction.

`tools/re/test_ce_fp_projection_native.py` now executes the complete pinned
`0x2F3460` dispatcher and all three complete material writers. Size, allocation
and thread-ID services use explicit synthetic fixtures; the native dispatch,
ABI argument construction, TLS publication and selector writers execute their
actual instructions. Thirty-six material cases, including both native
preparation view indices, pass through both the pure helper
and compiled production transaction with **no render-eye scope**. Outputs agree
byte for byte and preserve every unrelated constant byte. Result:
`out/ce-fp-worker-projection-native-20260915.json`.

The actual pinned GLT, ZFILL and SFX skinned vertex shaders separately pass 48
D3D11 WARP draws across two asymmetric eyes, two camera locations, both lens
choices and both native bind-array branches. Native conversion, packing and
upload also execute. Result: `out/ce-fp-worker-shaders-native-20260915.json`.

These checks establish the corrected native scheduling and shader behavior.
The user's headset still determines visible hand/weapon scale, right-eye
visibility and coverage of every weapon/animation/material variant. The
accepted build pointer does not advance from these local checks.

Preserved native decompilations: `out/ce-fp-writer-scheduling-20260915.txt`,
`out/ce-fp-writer-dispatch-20260915.txt`,
`out/ce-fp-writer-worker-20260915.txt`, and
`out/ce-fp-material-preparation-proof-20260915.txt`.
