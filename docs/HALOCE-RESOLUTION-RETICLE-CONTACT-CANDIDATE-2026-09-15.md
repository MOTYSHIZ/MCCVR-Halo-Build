# CE Original and Anniversary: resolution, reticle and contact

This candidate addresses the `58f71a4` feedback. Preserve your existing
`halomccvr.cfg`. Both CE graphics modes and both Steam and Microsoft Store
remain supported, alongside the other titles. Original's confirmed working
camera and hands are the base for this update.

## Changes

- Anniversary renders independent eyes at the full native render height, with
  matching color/depth allocation and a larger combined output. The native
  allocation lifecycle handles resizing; the HUD keeps its own authored canvas.
- Both CE modes use a visible controller-directed marker until a nonblank native
  crosshair has been measured and published. Late Anniversary HUD receipts,
  pending artwork measurement and failed uploads retain correct ownership.
  Crosshair size/distance controls remain separate from HUD size/aspect/height.
- Anniversary material workers can read the same hand/weapon lens policy
  concurrently without falsely rejecting one another. Enabled tracking updates
  retain the previous valid sample while publishing its replacement.
- Colliding texture identities no longer displace a live eye texture's metadata.
  Released resources revoke pending publications. These defects were reproduced
  locally; the supplied black-screen log does not establish every failure's cause.
- CE world collision and physical melee use CE's native query/damage paths and
  both tracked hands. They have separate settings and work through the common
  first-person palette used by Original and Anniversary.

CE HUD curvature remains flat; its unavailable slider is disabled. Contact
samples come from the actual hand/weapon nodes. Complete weapon mesh-surface
coverage and every custom weapon are not established by that representation.
Physical melee currently targets native biped characters; damage to vehicles
and other world objects remains open. Both hands use the native held-weapon
melee damage definition while armed.
CE body following remains deferred. All earlier standing/deferred work is kept.

## Installation and settings

With MCC closed, follow `MANUAL-README.txt` to replace the mod DLL and launcher
in your existing `Halo_MCC_VR` folder. Keep your saved config. The same files
support both editions. This handoff does not install or launch anything.

Enable **World collision** and **True physical melee** independently under
**F1 > Body & Hands** to test
them; fresh configs leave both off. The physical-melee default remains 5 m/s,
and saved custom settings are retained. Gesture melee is a separate control.

## Headset test

1. **Original:** check visible gun-directed crosshair and its sliders, while
   confirming the camera, hands, HUD and weapon scale still look right.
2. **Anniversary:** compare sharpness, move the gun while checking crosshair
   aim, adjust HUD size/aspect/height, and inspect hand/weapon flicker in each eye.
3. Switch graphics both ways, pause/resume, quit MCC fully, then relaunch and
   repeat the Anniversary check. Confirm the desktop mirror continues updating.
4. With the separate settings enabled, test hand/weapon contact with walls and
   objects, then physical strikes against biped characters with each hand in
   both modes. Check ordinary firing and weapon swaps too.
5. Briefly check Halo 3 after CE for the required shared-code regression.

Include `HaloMCCVR.log`, the edition, headset/runtime and which graphics mode
shows any issue. Local Release, native-code and GPU checks support this
candidate; they do not establish headset acceptance, universal weapon coverage,
long-session stability or frame-rate parity. Accepted source stays `4e01f28`.

Build and matching source ZIPs are delivered together, then work waits for the
user's headset result and instructions.

## Local verification

The cumulative Release build and all 22 CTest suites pass. Native allocation
verification passes 18 cases; native contact passes 3 resolver and 10 melee
cases. The material dispatcher passes 36 cases and three actual skinned shaders
pass 48 WARP draws. All 123 pinned contracts and 19 production binding groups
pass. Packaging repeats the required build/tests for the final source commit.
