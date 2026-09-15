# CE performance audit - September 15, 2026

This is a bounded review of the two user-supplied `a2b526a` logs and the CE
camera, eye-cache, and first-person hot paths. It introduces no performance
experiment. Files are preserved under
`out/test-runs/a2b526a-ce-refinement-feedback-20260915/`.

## Measured timing

Both runs identify Steam, SteamVR/OpenXR 2.17.9, an Oculus-family headset and
a 90 Hz panel. App cadence is the separate period reported by `xrWaitFrame`;
90 Hz panel refresh is not evidence that the game renders 90 distinct frames.

Representative windows that do not straddle graphics switches:

| Run and window ending | `renderWindow` p95 | App cadence near window | Observed game fps |
| --- | ---: | ---: | ---: |
| CE Anniversary, 14:11:08.755 | 36.46 ms | 30 Hz | 31 |
| CE Original, 14:11:28.751 | 7.23 ms | 45 Hz | 45 |
| CE Anniversary, 14:11:48.745 | 26.06 ms | 30 Hz | 27 |
| Halo 3 after recovery, 14:14:05.591 | 23.97 ms | 45 Hz | 42 |
| Halo 3 after recovery, 14:14:15.600 | 23.75 ms | 45 Hz | 44 |

The CE renderer classification uses the native output descriptor transitions
and the user's graphics-gesture lines: Anniversary's cache is 2912x1050,
format 90; Original's is 2912x2100, format 28. Early windows overlap startup
and switching and are not treated as steady renderer comparisons. Halo 3 was
flat until its recovery completed at 14:13:53.291, so its earlier timings are
not a stereo baseline.

These samples establish heavier Anniversary render windows in this run, not
the cause. The missions, views, effects, and engine workloads differ; ratios
between these windows cannot measure mod overhead or prove title parity.
The p95 values from different timing categories are not additive frame costs.
The CE wait-handoff p95 is 0.00 ms and DXGI Present p95 during gameplay is
0.08-0.09 ms. No frame-order failure or duplicate is recorded. The existing
eye-publication GPU timer covers shared upload/post work, not CE native
rendering, the separate desktop mirror, or every CE hook.
Its reported window means span 0.598-1.555 ms for CE (37 reports) and
0.608-0.822 ms for Halo 3 (16 reports); these are not per-frame p95 values.

## Source and resource review

- CE eye resources are prepared six times, matching initial ownership and
  five graphics switches. The log does not show continuous cache allocation
  in steady gameplay. Shared upload-view creation similarly occurs at those
  resource transitions, followed by hits and zero steady new creations.
- `EyeCache::Capture` validates the already-recorded descriptor and issues one
  bounded GPU copy per eye. It creates no resource, calls no COM descriptor
  discovery, maps no staging buffer, and does not synchronously wait for the
  GPU. Preparation and retirement own resource creation/release.
- Eye-cache admission and CE snapshots use bounded atomic attempts. A busy
  owner declines the operation; neither path spins or blocks a render hook.
- The Anniversary frame wrapper invokes the native full-frame renderer once.
  Original invokes its independently verified render-only path for each eye;
  it does not repeat the native simulation update or the Anniversary worker.
- Scene-visibility refresh is requested on mono/stereo camera-count changes,
  not every stable frame. The supplied log's refresh counter stays unchanged
  between graphics switches.
- Camera, palette, skin/projection, and copy hooks retain bounded memory work
  and native-call guards. No added signature scanning, file I/O, allocation,
  sleep, blocking lock, or GPU-readback loop was found in those hot bodies.
  First-person graph reads and matrix validation are bounded; the log does
  not measure their individual cost. Native services retain their own engine
  implementation, which this source review does not characterize as free.
- The desktop mirror creates shaders/views only when missing or when its
  resource identity changes. Its steady work is one draw plus state save and
  restore. There is no measured evidence here that removing that draw would
  change the cadence threshold, and its accepted single-view behavior is kept.

No evidence-backed accidental repeated full-frame work or blocking operation
was identified to remove. No timing-driven optimization is claimed in this
candidate. Its corrected HUD and first-person output need fresh headset timing;
the supplied timings describe the previous source, not the new build.

## Pause and desktop mirror interaction

The shared compositor computes `stereo` with `!pausedPresentation`. CE uses
that result as `stereoWorldFrame`; it has no H2-specific presentation claim.
The final mirror is gated by `ceImages && ceOwned && stereoWorldFrame`.
Therefore an active pause screen cannot be overwritten by the CE eye mirror,
even if a previous eye pair is still borrowable or its ownership timer remains
fresh. The native screen copy is also performed before the optional desktop
mirror. During the entry comfort fade, stereo remains selected until the
existing transition switches to the pause screen; the new pause refinement
does not alter that shared transition.
