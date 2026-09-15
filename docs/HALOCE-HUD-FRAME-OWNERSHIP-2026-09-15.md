# CE Anniversary HUD frame ownership - September 15, 2026

Halo 3 behavior being matched: both eyes receive the native gameplay HUD while
the world camera continues independently of optional HUD admission failures.
The CE callback, native target stack and full-to-half raster adapter remain
CE-specific and preserve their existing restoration checks.

## User evidence

The two supplied `e524d219` logs identify Steam, SteamVR/OpenXR 2.17.9 and an
Oculus-family headset at 90 Hz. The 15:26 run ends with zero Anniversary HUD
draws and 768 refusals; the 15:29 run ends with zero draws and 1,472 refusals.
Both name failure 11, `primary-eye-ownership`. Successful world pairs report
both eye images and all three native camera-consumer stages. This establishes
an optional HUD admission failure, rather than a missing native source image.
It does not identify which individual condition inside the old combined
ownership guard failed in every native invocation.

## Reproduced lifetime defect

The existing pinned preparation contract has two distinct lifetimes:

1. `PreparedHandoff` owns the current worker source list. Starting its next
   builder invalidates the previous source ticket immediately.
2. `PrepareBody` freezes the verified receipt after the native list copy into
   the renderer. `FrameBody` then owns that immutable receipt while the active
   renderer draws its cameras.

`HaloCE_GetAnniversaryEyeTracking` incorrectly rechecked
`handoff.Current(receipt.ticket)` during per-eye drawing. The next copied-list
worker can legitimately recycle its private source after handoff, without
changing the active renderer's cameras. This source-ticket check then refused
the current frame's HUD even though its camera receipts and world copies
remained valid. The original handoff contract already explicitly required
freezing the receipt rather than looking up the latest worker while rendering.

The eye reader now uses the frozen frame receipt. It still verifies the exact
renderer camera address and bytes, source-player identity, current selection,
depth/scene/shading consumption, title generation, tracking space and age,
reference revision, active capture and core ownership. It never substitutes a
new worker receipt or a newer pose into the current image. A new preparation
still immediately revokes its source ledger; only the completed handoff owns
an independent in-flight receipt.

Optional HUD rejection now records the particular eye guard (frame scope,
core ownership, eye index, camera identity, consumers, reference, generation,
tracking age, changed camera, selected camera or changed ownership). These are
numeric results in the hook, with existing cold-poll logging. No hook logging,
allocation, locking or new native binding is added.

## Verification and limits

The production WARP runtime fixture now follows a copied native preparation
through `PrepareBody` and `FrameBody`, then rebuilds that worker's private
source after the frame has frozen it. The active renderer and its texture
sources remain untouched. Before correction the world pair completed, but
the two native HUD callbacks did not execute, their pixels were absent from
both captures and the old ownership failure persisted. The new regression
failed those three checks with exit code 1. After correction the same runtime
suite passed with exit code 0, including both HUD images and exact native
target-stack, borrowed-output and numeric raster restoration.

Existing tests still reject foreign or modified cameras, changed native
selection, missing shading, dropped frames, stale XR input, changed reference
spaces and revisions, other graphics modes and work after the frame returns.
Optional callback faults and unavailable raster state retain their established
feature-only fallback or frame-only rejection behavior.

This reproduces and corrects a concrete admission defect consistent with the
user's log. The old log cannot exclude another ownership condition or a later
native HUD guard; the added reasons make any remaining refusal attributable.
Actual Anniversary HUD visibility and final frame stability still require the
user's new headset test. Accepted source `4e01f28` remains unchanged.
