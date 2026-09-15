# CE Original and Anniversary: horizon, hands and HUD

This candidate addresses the `fba9ee6` headset report. Original's successful
hands, weapon scale, HUD, controller aim and effects are preserved. It supports
both CE graphics modes and both Steam and Microsoft Store. Keep your existing
`halomccvr.cfg`.

## Corrections

- Recenter now uses horizontal heading and position, matching Halo 3. Looking
  up, down or tilting your head during recenter no longer becomes a tilted room
  reference. Eyes, hands, movement and controller shot direction share this frame.
- Anniversary's weapon lens correction now runs during its actual material
  preparation. The previous draw-time check rejected every observed preparation,
  leaving hands and weapons on the fixed native lens. Later draws use each eye's
  own world projection while preserving the native hand/weapon scale conversion.
- Anniversary first-person visibility distinguishes the two VR eyes from two
  separate local players. The exact native admission evidence and correction are
  recorded in the first-person evidence accompanying this source.
- Anniversary's HUD is handled during its ordinary late native callback, after
  the world images have been packed. Its gameplay HUD is framed into both eye
  regions and then copied into the VR eyes. Shared size, aspect and height
  settings remain available. The failed manually invoked callback stays disabled.

The HUD verifies the actual native render target against both preceding eye
copies, its live resource revision, the frozen frame, native player count and
raster restoration. Unknown state retains the already captured world images.
Scissors keep each HUD inside its eye region; native scissor enforcement is
verified. Original's existing HUD path stays active.

## Verification and remaining headset checks

The cumulative Release build, all 20 CTest suites, 111 pinned native contracts,
17 mapped-image binding groups and Reach consistency check pass. Native tests
execute material preparation, visibility admission, worker submission and the
late HUD sequence. WARP GPU tests verify actual eye pixels, shader projection,
packed HUD placement, native ClearState handling and recovery after invalid
state. Exception tests verify the new callbacks release their in-flight counts.
Packaging repeats the build and required checks for the committed source.
Details and explicit fixture boundaries are in the source evidence documents.
These checks do not substitute for your headset result.

Please check:

1. **Original:** recenter while looking up/down, then look level. Check hands,
   weapon scale, controller aim and HUD.
2. **Anniversary:** check hands and weapon separately through each eye, then
   the HUD, recentering and muzzle flashes while moving the gun.
3. Switch graphics both ways, pause/resume and return to the MCC menu.
4. Briefly check Halo 3 after leaving CE, covering the retained shared pause
   and title-transition regression requirement.

Visible alignment across every weapon/animation, long-session transition
stability, repeated native HUD descendant behavior and frame-rate parity remain
headset checks. No other-title feature change, CE body following, physical melee
or world-collision expansion is included. All standing/deferred work is retained.
The accepted source remains `4e01f28` until explicit headset acceptance.

Build and matching source ZIPs are delivered together. No installation or game
launch is performed; wait for the user's test after delivery.
