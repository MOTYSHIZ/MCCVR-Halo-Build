# CE Anniversary stereo / 6DoF test candidate

This is the first connected CE Anniversary VR candidate, not completed CE parity
or a headset-accepted release. It retains the cumulative H2 Classic/Anniversary,
H3, ODST, Reach and H4 work. Both Steam and Microsoft Store use the same build.
Keep your existing configuration when updating.

## What this adds

- CE Anniversary builds native left/right views before culling, without replaying
  simulation or the whole render frame. Head rotation, physical leaning, IPD,
  world scale and recenter feed CE's verified native camera rebuilds.
- Actual per-view GPU output is copied into owned textures. The shared OpenXR
  compositor submits those images with the exact tracking pose and field of view
  used to draw them. Missing or invalid eye pairs drop the frame and can recover.
- Shared VR controller buttons/sticks and the F1/F3 controls are available.
  Raise the physical left hand beside the left side of your head and click the
  movement stick for the game's Back/View graphics-switch input, as in H2.
- Native texture creation/import and early D3D texture creation record source
  descriptors. Render callbacks borrow the actual native copy source only during
  that copy. Recenter, stale frames, changed dimensions and title retirement
  revoke incompatible images.

## First headset test

1. Close MCC, extract the build ZIP, and follow MANUAL-README.txt. For an update,
   replace the DLL and launcher together; keep halomccvr.cfg. Update each edition
   you use. The source ZIP is for rebuilding and is not needed to play.
2. Start your headset connection/runtime, then use the included launcher. Load a
   single-player CE campaign in **Anniversary graphics**. Allow a few seconds for
   the camera to settle, then press F3 while facing comfortably forward.
3. Check that the two eyes show real depth. Look left/right and up/down, lean
   sideways/forward, crouch physically, and recenter. Check scale and image edges.
4. Check ordinary controller buttons and sticks, pause/resume, death/restart,
   and a mission transition. Report whether HUD, reticle and stock weapons are
   visible; their final Anniversary composition has not been headset verified.
5. The graphics gesture switches to **stock Classic presentation**, which has
   no CE stereo implementation in this candidate. Switch back to Anniversary
   and check recovery. Do not mistake Classic's flat presentation for stereo.
6. Check Halo 3 afterwards, including loading, stereo, input, pause and exit.
   The early D3D metadata observation and compositor integration require that
   regression result. Broader existing-title regression remains requested.

Send HaloMCCVR.log and CANDIDATE-MANIFEST.json with the mission, edition,
headset model, OpenXR runtime and refresh rate. If it stays flat or black, keep
the log from that attempt. Do not repeatedly inject the DLL. The existing manual
force-recovery control remains H3-only; it is not CE recovery.

## Exact limits

- **Classic stereo, controller-directed CE aim, tracked CE hands/weapons,
  separate adjustable HUD/crosshair capture, CE snap-turn parity, head-relative
  walking, roomscale body following, and full vehicle/zoom parity are unfinished.**
  This candidate retains stock CE stick aiming and native weapon/HUD behavior;
  those are not motion-controller feature claims.
- Physical melee and world collision remain deferred until the user confirms
  CE VR injection. Existing titles keep their current implementations.
- Split-screen, unusual/MSAA source textures and unverified native camera
  variants are rejected. Co-op, cutscene behavior, gamma, performance, and all
  missions still need runtime testing. A missing native descriptor can prevent
  capture; the log's CE counters distinguish preparation and capture failures.
- Prior roomscale/left-hand fixes, disabled H3 dual firing, slider arrows and
  all standing/deferred tasks are preserved. H2 vehicles/all-title zoom,
  H4 damage blackout and the minor H2 tank-exit reticle report remain pending.
- Local tests prove implementation checks, not native game execution or headset
  success. The accepted pointer remains 4e01f28.

Packaging is local only. No installation, game-folder changes, MCC launch,
publishing, or PR was performed to prepare this candidate.
