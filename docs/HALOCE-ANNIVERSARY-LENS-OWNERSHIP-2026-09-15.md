# CE Anniversary lens ownership - September 15, 2026

Halo 3 behavior being matched: native first-person depth, color and effect
materials use the current eye's world lens consistently. Controller placement
and material lens selection are independent features.

## Reproduced scheduling defect

The `e524d21` first-person material helper consulted `CurrentPaletteContext`,
which reads the newest CPU palette receipt. Each native first-person prepare
sets `lastApplied=0` before rebuilding. That can occur between material
consumers of an already prepared model. A rejected lookup makes that material
retain its fixed first-person lens while other material passes use the eye's
world lens. The newly reproduced failure is separate from the HUD's stale
worker-ticket query and the skin converter's scale omission.

The regression retains an independently valid current eye, clears the latest
palette receipt, then runs the production material helper for the GLT, ZFILL
and SFX selector offsets. The previous implementation fails the first call.
The captured failing run is `out/ce-stability-projection-before.txt`; its build
record is `out/ce-stability-projection-before-build.txt`.

## Current-eye admission

The correction does not keep an old palette alive or guess which model was
last tracked. It deliberately makes the material lens its own current-eye
feature. The native first-person model flag and exact four-lane selector remain
required. A native stock-positioned first-person model also uses the world
lens while a proven VR eye draws it; a palette-placement failure remains
separately logged by that feature.

Existing pinned native camera consumers establish three material stages:

| Stage | Verified camera-upload return |
| --- | --- |
| Depth, including ZFILL | `0x4562BF` |
| Scene | `0x456A86` |
| Shading | `0x457C07` |

These callsites and `renderer+0xBE98` selected-camera publication were already
verified by the core's camera-consumption contracts; this adds no native
binding. `CameraUploadBody` revokes material ownership before every upload and
grants it only after the exact primary camera has passed the existing
before/after identity and byte checks. Unknown and auxiliary uploads cannot
grant ownership. Foreign/auxiliary depth entry and eye output revoke it.
Nested frame calls mask the outer material ownership before any native work;
normal return and structured-exception unwind restore the previous mask.

The new `HaloCE_GetAnniversaryPrimaryEyeTracking` reader requires that stage's
actual camera receipt, exact selected primary address and camera/player bytes,
active synthetic capture, current title generation/reference, and bounded XR
space/serial/age. It returns the frame's frozen settings. A material rechecks
this ownership before writing its selector, and blocked presentation remains
stock. The old latest-palette gate remains inert in the source.

HUD replay and motion-blur admission still use their stricter reader requiring
all depth, scene and shading receipts. That reader is too late for the depth
material itself and is intentionally not substituted for the new stage-aware
reader. Classic's narrow primary-view and exact-palette gate is unchanged.

## Validation and limits

The production core fixture observes both eyes through all three native stage
boundaries. Ownership is absent during native upload, output and after frame
exit. It rejects auxiliary/reflection and unknown uploads, changed camera
bytes or selection, missing stage consumption, rejected frames, changed title
generation, changed reference or XR space, and expired/old tracking input.
The production material fixture preserves all unrelated constant bytes and
rejects noncanonical selectors and absent/changed eye ownership. The existing
native shader verifier separately exercises the actual GLT/ZFILL/SFX lens
selectors and skin uploads.
Nested-frame tests also verify refusal before the nested frame's first camera
upload, and outer ownership restoration after a native structured exception.

This corrects a locally reproduced cause of inconsistent material selection.
The supplied logs do not identify which palette guard caused each refusal, and
the current-eye stage behavior still needs the user's headset result. It is
not a claim that every Anniversary animation/effect flicker is resolved. The
native contracts prove the material writers and camera-upload boundaries;
synthetic fixtures alone cannot prove that every live material variant is
called within those intervals. Refused variants remain stock and logged.
