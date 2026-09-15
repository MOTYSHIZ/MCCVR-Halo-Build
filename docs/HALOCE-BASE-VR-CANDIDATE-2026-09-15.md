# CE Classic / Anniversary base VR test

This cumulative candidate integrates base VR for both CE graphics modes while
preserving Halo 2 Classic/Anniversary, Halo 3, ODST, Reach and Halo 4. The same
build supports Steam and Microsoft Store. It is packaged for headset testing;
the accepted source remains `4e01f28`.

**The previous Anniversary displaced-right/flat-left failure has not been
confirmed fixed.** Native camera, projection and viewport checks have not
isolated its cause. This candidate contains the substantive changes below,
but local tests cannot establish that the native game now looks correct.

## Changes in this candidate

- Classic gains a separate native stereo/6DoF render and per-eye capture path.
- Anniversary validates actual camera consumers and independent native depth
  ownership before accepting a pair; a rejected frame can recover next frame.
- Both modes gain independently tracked hands/weapon, controller shot direction
  and native aim-assist direction, with native player/state guards.
- Anniversary weapon color, depth and special-effect shaders use the matching
  world-eye projection instead of the fixed first-person projection. Native
  skin conversion also preserves configured hand/weapon scale.
- Anniversary replays its skipped HUD callback into each eye, maps full-height
  native HUD coordinates to the half-height eye image and restores target,
  viewport and scissors. Shared HUD controls and authored reticle capture are
  integrated; optional failures are logged and isolated from the camera core.
- CE uses the shared snap/smooth-turn settings and head-relative stick movement.
  Anniversary now publishes the native center-camera reference those controls
  and controller aiming require.
- Anniversary honors the existing motion-blur setting. The default disabled
  setting avoids the native history being updated alternately by both eyes.
- Hand-feature retirement handles all nine hooks within the shared cleanup
  helper's eight-hook batch limit, preserving safe title switching.

CE physical roomscale body following, physical melee and world collision remain
disabled/deferred. Existing-title refinements, H2 vehicle work, all-title zoom
and every standing/deferred request remain preserved in the source/checkpoint.
Vehicle controls retain native CE handling; broader vehicle/state parity and
live animation, muzzle, HUD/reticle and material behavior need headset results.

## Install or update

Follow `MANUAL-README.txt`. For an update, close MCC, back up the old mod files,
replace `HaloMCCVR.dll` and `HaloMCCVRLauncher.exe`, and **keep your existing
`halomccvr.cfg`**. For a fresh install, use the supplied configuration too.
If you use both editions, update their separate `Halo_MCC_VR` folders.
The ZIP itself does not install or launch anything.

## Headset test

1. Start CE in Anniversary graphics, load a campaign and recenter with F3.
   Check that both eyes show the same location with stereo depth. Turn your
   head, lean sideways and crouch; the world should remain stable.
2. Move each controller separately. Check both hands, the gun, reticle and
   bullet direction. Check normal walking and your configured turn mode.
3. Check the HUD in both eyes and its F1 size/aspect/height controls. Keep
   motion blur off for the first test.
4. Switch graphics using the physical left hand beside the left side of your
   head plus movement-stick click. Repeat the basic checks in Classic and
   switch back. Check pause/resume and a checkpoint reload.
5. Run a brief Halo 3 regression because shared input/render integration changed.

Report the edition, runtime/headset, graphics mode and whether the old eye
failure remains. Include `HaloMCCVR.log`; the manifest records the exact source
commit and DLL hash. Other titles' prior acceptance does not accept this build.

## Verification scope

The candidate passes the cumulative Release build, CTest suites, Reach
consistency gate, generated contract check and pinned/mapped CE binding checks
before packaging. Additional offline checks execute pinned native camera,
depth, HUD, motion-blur, skin and material instructions; WARP tests exercise
production capture/restoration and actual Anniversary shader bytecode. These
are bounded local checks, not execution of MCC or a headset acceptance result.
