# CE tracked-view construction test - September 15, 2026

This is the next Anniversary stereo correction after your 6e31b25 test.
That build captured 803 eye pairs and switched into Anniversary without the
previous black screen, but the displaced right eye and flat/head-attached
appearance remain a failed VR result.

## Change

Tracked eye cameras now enter CE before its native view construction. CE builds
each view and its separate position metadata from that same tracked camera.
Previously we changed the cameras after CE had stored the original positions.
Both eyes also receive the primary camera's native clipping range, following
CE's own stereo branch. Native rendering still runs once per frame.

This fixes a verified preparation mismatch. It is NOT proof that the mismatch
caused every reported visual problem. New CE ORIGINS log lines record both
stored positions and clip ranges to help check the live result.

## Update and test

1. Close MCC. Extract the Build ZIP and follow MANUAL-README.txt. Replace the
   mod DLL and launcher together; KEEP your saved halomccvr.cfg. The same
   package serves Steam and Microsoft Store. Source ZIP is for source/rebuilds.
2. Start through the launcher, load CE Anniversary, wait for the view to settle,
   and recenter with F3 while facing comfortably forward.
3. Check whether both eyes show the same location and stereo depth. Turn your
   head, lean sideways and forward, and crouch physically. Check whether the
   scene responds correctly to translation rather than only head rotation.
4. Check pause/resume and switch graphics with the existing left-head-side
   movement-stick gesture. Original remains flat in this candidate; check that
   returning to Anniversary recovers. Check Halo 3 stereo/input as a regression.
5. Send HaloMCCVR.log, the mission, headset model, edition and your visual result.

## Still unfinished

Original/Classic VR and CE controller-directed aim, two independently tracked
hands/weapons, separate HUD/crosshair, snap turning, head-relative walking,
roomscale body following and full vehicle/zoom integration are unfinished.
The stock gun can still appear attached to the head. This candidate does not
claim full CE parity or completion of your request. Both renderers and both
tracked hands remain required work. Physical melee/world collision still await
functional injection confirmation. Co-op/cutscenes/performance remain unverified.

All existing-title work and deferred requirements are retained. Accepted source
remains 4e01f28. Preparation of this package installs nothing and launches no game.
