# CE crosshair and HUD refinement after 58f71a4

## Reference and supplied observations

Halo 3's reference is a visible controller-directed reticle, with the common
size, distance and stabilization controls, independent of HUD size, aspect
and height. Preserve the user's working CE Original camera and hands.

The supplied 17:49 run reports Original authored capture completions while
the user sees no Original crosshair. It also shows successful Anniversary
gameplay-HUD draws increasing while crosshair capture completions remain
unchanged. Neither a completed native callback nor `heldArt=1` proves that the
captured texture contains visible pixels. The old log does not expose CE's
pixel-coverage value or reticle-compositor admission; it cannot establish the
precise native reason Original artwork was blank.

## Verified defects and changes

1. `VR_CeAuthoredReticleFrameMatches` demanded an exact OpenXR serial. The
   Anniversary natural HUD owns the frozen worker receipt, and the CE core
   deliberately admits that receipt through an age of eight serials after
   checking generation, renderer epoch, tracking space, reference and native
   callback ownership. A valid late callback could therefore draw its stock
   crosshair into both eye images because the independent reticle gate refused
   it. The reticle gate now uses the same bounded age. Future, zero and older
   receipts still refuse; every other admission/cleanup guard is retained.
2. The shared authored uploader previously permitted an empty first CE sample
   to become `g_reticleContainsAuthored=true`. CE now requires measured visible
   art and a completed upload before publishing the first authored image. Until
   then, the existing
   shared procedural marker follows controller aim and uses the common reticle
   controls. Unavailable pixel measurement also retains that marker. This is a
   deliberate temporary artwork fallback; it is not a claim that CE's native
   alpha or crop has been diagnosed. Native capture/suppression ownership is
   still required, so a declined native scope does not gain a second marker.
3. Cold CE logs now distinguish native-context refusal, measured artwork,
   published artwork, compositor admission and sampled pixel coverage. Hot
   native hooks only update counters.
4. The resumed source audit found that CE continued replacing its capture
   while the shared asynchronous coverage query was pending. A nonblank sample
   from frame A could therefore approve frame B's newer blank texture. CE now
   draws subsequent native phases into the discard target until the queued
   measurement is consumed. It retains A's artwork identity even if B belongs
   to a different weapon; a real B capture publishes the new identity afterward.
   A failed scope, expired context or renderer/reference change cancels the old
   pending sample before recovery, without ending stereo.
5. Measured art becomes valid before its compositor upload finishes. CE's
   procedural bootstrap now stays visible until measured art is also successfully
   published; a failed first upload cannot remove that fallback. A failed CE
   swapchain release is no longer recorded as a successful art publication.

These changes are CE-specific. The other titles retain their existing
bootstrap/pixel policies. No native address, game camera, native HUD anchor,
weapon palette or graphics-switch setting is changed by this work.

## HUD controls and validation

The F1 size/aspect/height ranges match the CE layout validator. Production
layout tests exercise original raster framing, Anniversary eye framing,
independent crosshair suspension, configured vertical height and restoration.
The supplied working HUD spans report zero layout fallbacks at their original
size. Full-resolution Anniversary separately requires preserving the desktop
HUD canvas while mapping it into the enlarged packed output; that refinement
must be validated with the native resolution work. CE curvature remains native
flat, and the previously misleading live curvature slider is now disabled for
CE with explanatory text.

The production HUD fixture now exercises all eight valid delayed receipt ages,
expired/future rejection and recovery. Existing generation/recenter/renderer
changes, nested callbacks, failed capture/suppression, native exception and
stock-fallback tests remain. The reticle test covers procedural bootstrap
selection; the production WARP redirect/layout/target suites pass. Command:

`ctest --preset release -R 'halomccvr_ce_(hud|reticle)' --output-on-failure`

The prior four suites passed before the resumed audit. New production CE HUD
tests now queue a real WARP nonblank source A, run both blank B/new-weapon phases,
and verify that only discard changes while A's source pixels and artwork key
remain intact. They also exercise renderer invalidation, failed suppression,
expired context and fresh-capture recovery. Bootstrap state tests cover measured
but unpublished artwork. Both updated targeted suites pass in the parent's
coordinated build; records are `out/ce-resolution-reticle-review-build-20260915.txt`
and `out/ce-resolution-reticle-review-tests-20260915.txt`. The GPU source tests
execute production CE scope decisions with shared D3D/XR calls supplied by
fixtures; they do not execute the entire shared OpenXR compositor or establish
real native art pixels. Full cumulative compilation/packaging is performed by the parent
task. Native pixels, slider appearance, graphics toggles and a Halo
3 regression still require the user's headset result; acceptance is unchanged.
