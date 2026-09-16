# CE Classic output scaling and cold loading admission

Halo 3's reference behavior is automatic VR entry after the engine is ready,
independent complete eye images, and recovery after one rejected frame. The
accepted runtime remains d47a98c. These corrections are locally verified and
require new headset results, including custom campaigns and Halo 3 regression.

## Classic native raster is distinct from the final host output

Community report 14 launches a 3786x2730 DXGI output. CE's observed Anniversary
native raster is 3788x2732. Classic has 1,290 rejected attempts, zero completed
pairs, 1,348 native outputs, zero source misses and failure 4, PairPreparation.
The old log combines view staging and cache admission under that failure and
does not record Classic's raster dimensions. Therefore the dimension failure
below is reproduced in production code and compatible with this report, but
the report alone cannot prove which preparation branch ran on that machine.

The old Classic cache required its native viewport to equal the kind-0 DXGI
texture dimensions. The old final-output adapter repeated this equality.
Native code does not impose it:

- Verified `B51670`, kind 0, obtains the actual output texture descriptor and
  installs a viewport covering that texture. Only kind 1 uses the native
  player viewport globals.
- Verified `B51A14` samples complete kind 1, builds four vertices with UVs
  spanning 0..1, and uploads a matrix with 2/rectangle-width and
  -2/rectangle-height. An origin-zero full rectangle covers clip space -1..1
  regardless of its size or the separately selected output size.
- Existing Classic contracts already pin both functions and the final-output
  call route. No new hook, offset or native-memory write is introduced.

The adapter now admits a complete origin-zero primary viewport whose window
matches it, keeps the cover FOV calculated from that native raster, and copies
the separately proven complete host output. Split/cropped windows still fail.
Both eyes must use the same final quad rectangle and identical native source
ownership, descriptor, context and resource revision. GPU copy dimensions still
come exclusively from the actual texture descriptor and matching cache.

`tools/re/test_ce_classic_output_scaling_native.py` executes the pinned native
quad geometry/UV/matrix construction and output viewport setter for five
equal/upscaled/downscaled sizes, including 3788x2732 into 3786x2730 and
4368x3152 into 4368x3150. Driver, shader setup, GetDesc and actual draw calls
are explicit fixtures. All cases pass. Disassembly/decompilation is preserved
at `out/ce-classic-output-map-20260916.txt`; emulator output is
`out/ce-classic-output-scaling-native-20260916.json`.

The production Classic WARP fixture now separates raster and output dimensions,
checks current-frame identity, angular coverage and both complete eye images,
and retains real DXGI resize, cropped-window, source-change and native consumer
rejections. Restoring just the former cache size check makes all three unequal
raster cases fail to publish the current pair. Corrected tests pass; transcripts
are `out/ce-classic-size-before-20260916.txt` and
`out/ce-classic-size-after-20260916.txt`. The expected failure-stage change for
actual DXGI replacement is now OutputShape: source descriptor mismatch still
rejects the frame before any incompatible GPU copy.

Cold telemetry now separately reports Classic staging result, cache admission,
native raster and host output sizes, rather than repeating only failure 4.

An additional cold replacement recovery defect was reproduced: `Prepare`
retires old GPU storage even when a replacement is unsupported or fails. The
previous `allocated` snapshot survived failure; returning to that old size then
mistakenly skipped allocation forever. Failed preparation now clears only the
allocation receipt, so normal cold polling retries. The production WARP test
injects an unsupported replacement, returns to the former dimensions, and
requires a fresh complete current-frame pair. No render-hook allocation is added.

## CE installed before proving level activity

The custom-campaign attempt 1 log detects CE at 23:45:03 and completes native
hook installation at 23:45:07.629. For the rest of the capture it records zero
Classic calls/outputs, zero Anniversary frame work and no camera heartbeat.
This proves installation preceded recorded camera activity. It does not prove
the exact point at which the game's loader stopped, or that the hook install
caused the stall. There is no captured stalled native thread stack.

Code independently confirms a cold-loading admission gap: CE inherited
`activeLevelRunning=true`, had no level gate case, and called `Install`, which
takes a module reference, verifies/scans native code and installs detours.
Other titles already wait for engine activity before these operations.

CE now uses its existing pinned bounded game-clock reader from
`title_reentry_probe.cpp`. E-CE-2 in `HALOCE-RENDER-EVIDENCE.md` establishes the
official HCEEK initialized clock and current tick; the existing generated
entry, PE and memory-range checks remain in force. No new native binding is
invented. Cold installation requires forward simulation after a frozen/reset
sample, or the existing six-second sustained-activity rule for a midgame
attachment. There is no unconditional timeout. Invalid/uninitialized clocks,
generation/module/clock replacement, backwards time and stale sample intervals
cannot inherit admission. An opened clock also expires after 250 ms without
advancement. Reader calls take no module pin and make no native writes.

The gate controls initial installation only. Installed camera/list/resource
lifetimes remain owned through ordinary transitions and retain their existing
safe retirement. Manual recovery cannot bypass the loading proof. CE selection
and all other titles' admission policies remain unchanged.

Production probe tests exercise real committed memory, native PE/anchor reads,
SEH guards, long loading waits, reset/replacement, sustained simulation and
freshness. The Classic production fixture verifies a closed gate never reaches
an install attempt, module pin or rejected-generation latch. This closes the
observed policy gap; Cursed Halo Again and Minecraft 2 still need a headset
loading test to establish whether it resolves their reported stall. Do not
relabel the later camera-heartbeat timeout as the original load-stall cause.
