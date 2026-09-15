# CE Anniversary hand material stability under worker contention

Follow-up to the user's `58f71a4` headset report, September 15, 2026.
Halo 3 behavior being matched: tracked hand/weapon depth, color and effects
consistently use the corresponding eye's world lens and retain their scale.
Original's successful palette, floating-hand geometry and lens adapter are
unchanged. No new native binding or guessed offset is introduced.

## Observations and reproduced defects

Both supplied Steam / SteamVR OpenXR 2.17.9 / Oculus-family / 90 Hz logs show
zero stock fallbacks from the copied-bone scale adapter. Preserve that path.
In the second run at 17:54:33, material projection has 3,925 successful writes
and 237 native fallbacks, including 101 revalidation refusals. These remain
interspersed during active Anniversary gameplay. The logs cannot identify the
thread interleaving of an individual refusal or establish that every visible
flicker has one cause.

The exact native preparation chain and GLT/ZFILL/SFX selector consumers are
already established in `HALOCE-ANNIVERSARY-MATERIAL-WORKER-2026-09-15.md`.
Separate native preparation workers read the same policy for the independent
material passes. Two software races can reject that policy even when its
content and lifetime have not changed:

1. `Snapshot<T>::Read` pinned its slot with a single compare/exchange. Another
   reader changing the pin count caused that operation to fail. Eight workers
   calling the actual production `ApplyTrackedProjection` helper rejected
   106,798 of 160,000 material writes against one unchanged gameplay snapshot.
   This permits different native lens selectors for depth and color.
2. `HaloCE_PublishTracking` set `trackingEnabled=false` before publishing every
   enabled update. A worker could observe that artificial disable while the
   previous complete tracking sample remained valid. A concurrent observer of
   the actual production function recorded 170,593 such observations during
   100,000 enabled publications.

These are independently reproduced implementation defects. The fixtures do
not claim to reproduce the precise timing of the supplied game session.

## Correction and concurrency safety

Snapshot readers now acquire their pin with one atomic increment. Concurrent
readers can all use the immutable current slot. A reader encountering the
writer bit drops its pin and refuses without reading the payload. The writer
releases only its bit with an atomic subtraction, preserving any rejected
reader's outstanding pin. Clearing the whole state to zero would be wrong:
the reader's later decrement could underflow it. The 64-bit state reserves
63 bits for reader counts. There is no allocation, lock, wait or retry loop.

The writer still acquires only a zero-count inactive slot. Readers still
recheck the published index before copying. A real concurrent publication may
therefore refuse a read, and busy buffers may still refuse publication; no
partially written payload can be admitted. Existing generation, renderer,
reference, tracking-age and presentation guards remain in place.

An enabled tracking update now leaves the previous complete sample enabled
until the replacement is published. A failed publication leaves its original
timestamp intact, so it expires at the same 250 ms bound. Explicit disabled
input still immediately clears admission. This corrects CE publication
mechanics shared by its graphics modes; it changes no native camera or hand
geometry. Other title adapters do not use this CE snapshot implementation.

## Verification

The targeted Release builds and these compiled production suites pass:

- `halomccvr_ce_first_person_runtime_tests`: all 160,000 unchanged-policy
  material writes succeed with eight simultaneous workers; all prior palette,
  scale, lens, blocked/stale/revoked policy, visibility, exception and retirement
  cases still pass.
- `halomccvr_ce_runtime_tests`: 100,000 enabled updates produce zero artificial
  disables; explicit disable rejects tracking, and the next valid update
  recovers. Its existing native/WARP frame and ownership cases pass.
- `halomccvr_ce_view_pair_tests`: eight readers racing 50,000 publications
  observe no torn payload. Both slots accept publication after all readers
  retire, checking against leaked pins and writer-bit underflow. Existing
  preparation handoff lifetime tests also pass.

Before/after records are under `out/ce-hands-material-contention-*-20260915.txt`
and `out/ce-hands-tracking-contention-*-20260915.txt`; snapshot stress output is
`out/ce-hands-snapshot-contention-after-20260915.txt`. The final package repeats
the cumulative build and tests. Headset confirmation remains necessary for
Anniversary hand/weapon appearance and Original regression; this evidence does
not advance the accepted build pointer or prove every flicker eliminated.
