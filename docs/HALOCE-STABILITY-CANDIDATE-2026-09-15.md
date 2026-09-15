# Combat Evolved hands, HUD and stability candidate

This build follows your `e524d21` test: both graphics modes inject, gun scale
looks right in both, and Original's HUD works. It preserves that progress and
supports Steam and Microsoft Store. Keep your existing configuration.

## Changes

- **Separate floating hands:** hidden CE arm geometry now shrinks at its own
  tracked wrist. The previous camera-origin shrink caused triangles to span
  between the wrist and camera. Hand, finger, gun and object-root transforms
  retain their existing scale and placement.
- **Original startup stability:** background Anniversary preparation no longer
  takes camera ownership away from Original's stereo rendering. This corrects
  a reproduced path that alternated tracked and stock frames before a graphics
  roundtrip.
- **Anniversary hand consistency:** each copied skin bone retains its own scale
  when the next palette preparation starts. Weapon depth and color rendering
  use the currently verified eye's world lens independently of that preparation.
  This corrects two reproduced paths that could change hand appearance within
  a frame. Stock scale-one bones remain unchanged.
- **Anniversary HUD:** the current frame keeps its verified eye information when
  the next background job reuses its preparation list. That reuse previously
  could reject the HUD while the world still rendered. Invalid cameras and
  stale title/tracking state remain guarded.
- **Leaving CE:** ending CE presentation clears its old head-locked pause state,
  including when CE remains loaded in the MCC menu. Stale native pause data
  cannot put the shell back into that state.

Halo 3 behavior is unchanged. Both CE renderers retain the successful camera
reference across graphics switching, native weapon lens, tracked aim and
effects, Original HUD, and single-eye desktop mirror.

## Verification and remaining limits

The package process rebuilds Release, runs all CTest suites and the Reach
consistency check. Pre-package cumulative Release and all 20 test suites pass,
as do pinned/generated binding checks and the Reach gate. The changed paths also have regressions for actual WARP eye
and HUD pixels, background-job interleaving, native exception cleanup, pause
ownership, and CE arm geometry from the official editing kit. The accompanying
source includes the evidence documents and reproducible verifier code; game
binaries and proprietary mesh fixtures are not included.

Additional local checks cover 101 pinned native contracts, 15 production
mapped-image binding groups, 12 official weapon graphs, 15 official hand-mesh
cases, 30 native scale cases and 48 native GLT/ZFILL/SFX shader draws. Evidence
documents identify exact inputs, guards, regression failures and limits.

These are local checks. Your headset test determines whether the visible
stretching and flicker are resolved. Broad weapon/reload/effect coverage and
frame-rate parity remain unconfirmed. The supplied logs show transition stalls
without a crash address or stack; this candidate corrects the menu-state leak
but is not a proven fix for every reported crash. CE physical melee, world
collision and body following remain deferred. Existing other-title work and
standing requests are preserved. Accepted source stays `4e01f28`.

## Short test

1. Enter CE in **Original** and stay there first. Check that the view, gun and
   hands remain steady without switching graphics; inspect both arms for spikes.
2. Switch to **Anniversary**. Check the HUD, hand stability, reloads and firing,
   then switch back and confirm scale and facing stay consistent.
3. Pause/resume, then **Save & Quit**. Check that MCC's menu stays in place when
   you turn your head, and try entering CE again or another title.

The build ZIP's manifest and adjacent SHA-256 file identify the exact source
and binaries. Delivery is build plus matching source ZIP, then your test.
