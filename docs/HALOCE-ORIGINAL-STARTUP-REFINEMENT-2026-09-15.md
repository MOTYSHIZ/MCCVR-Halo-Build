# CE Original startup render ownership - September 15, 2026

## Behavior and evidence

Match Halo 3's player experience: ordinary gameplay frames keep tracked stereo
and the same tracked weapon lens from initial entry, without requiring a
graphics-mode roundtrip. The user confirms good Original output after a
roundtrip on `e524d21`, but flicker before it. Preserve the gun scale and native
camera/output paths; no native binding or graphics-setting change is needed.

In the first supplied log's steady Original interval before 15:28:23,
stereo pairs grow by roughly 30/s while
stock render entries grow about 15/s. No source misses or rejected eye pairs
are recorded in those steady intervals. The stock entries and fixed-lens
fallbacks largely stop after the first graphics roundtrip. This proves mixed
stock/tracked render entries; the old aggregate counters do not identify the
particular guard rejecting each stock entry.

Source exposes a specific reproducible rejection: `PrepareBody` held
`preparationBusy` around the entire native Anniversary preparation job even
in Original mode. `ClassicGameRenderBody` uses the same nonblocking guard to
protect tracking-reference work and falls back to native stock rendering if
it is occupied. The Original-mode job does no tracked-reference work: the
builder's existing `Anniversary()` gate already prevented that. Native jobs
can overlap rendering. Their active/copy preparation paths and worker dispatch
are pinned in E-CE-5/7/8 of `HALOCE-RENDER-EVIDENCE.md` (native job `0x455170`,
worker queue from `0x3BBA10`, caller `0x87F90`). This is a source-level
contention defect consistent with the log, not a measured count of live guard
collisions or proof that all reported flicker had this single cause.

## Correction

Separate two existing responsibilities with bounded atomic claims:

- `jobPreparationBusy` serializes the native preparation bookkeeping and its
  active/copied-list receipts, including Original-mode stock jobs.
- `preparationBusy` continues to serialize actual tracked camera-reference
  work. A preparation job claims it only when Anniversary is selected.
- `JobScope::cameraOwned` is mandatory before the builder can force a tracked
  secondary view. If an Original-mode job observes a graphics switch after
  entering, it remains stock for that job; it cannot borrow another camera
  owner's reference. The next properly owned Anniversary job can track.

Stock list bookkeeping and native scene-visibility refresh remain active on
returning to Original, so old synthetic lists still retire correctly. Failed
claims never wait; they preserve the existing fail-open native path. Both
atomic claims and thread-local scope restore through native exception unwind.
Original's render-only replay, eye-source identity checks, exact source camera
restoration, simulation-tick guards and successful per-eye lens are unchanged.

## Reproduction and checks

The production WARP fixture interleaves `ClassicGameRenderBody` while
`PrepareBody` is still inside a stock Original-mode job. With old code, it
fails `stock Anniversary preparation cannot force a cold Original frame out
of stereo` (`out/ce-stability-classic-regression-before.txt`). Corrected code
passes, verifies two actual distinct eye images, and confirms there was no
stock fallback. It also covers stock-job native exceptions and subsequent
recovery, plus Anniversary's exclusive camera claim and release on unwind.
Records: `out/ce-stability-classic-regression-after*` and final cumulative
CTest output. The Anniversary runtime fixture separately runs production
`BuilderHook` with and without camera ownership across mode selection: an
unowned stock job cannot consume recenter or force a secondary view.

This deterministic interleaving exercises production guard/capture code with
native-call substitutes and real WARP textures. It does not execute a running
MCC renderer or establish headset acceptance. Initial Original gameplay,
graphics switches, effects, reloads and Anniversary hand stability require the
new candidate's headset test. No performance or complete crash-fix claim is
made. No other title's code path is changed by this correction.
