# CE Anniversary primary-eye / raster repair test

This replaces the failed e17a664 CE test. Your first test already confirmed
controller input and the left-head-side graphics gesture. It did NOT confirm
working CE VR: Anniversary was black in the headset, with mismatched vertically
stacked desktop views. The old build recorded 734 prepared frames and no complete
eye pairs. This is a correction candidate, not a headset-confirmed fix.

## What changed

- CE's culling system can add auxiliary views after the two primary eyes.
  The old capture receipt demanded exactly two total views and rejected the
  primary eyes when that happened. The corrected receipt accepts a bounded
  native list while still checking both primary eyes, their exact cameras,
  frame identity, generation and recenter revision.
- Eye cameras now use the dimensions of the actual native source texture before
  their projection and culling data are rebuilt. Your log recorded 2912x1050 eye
  textures against a 2912x2100 desktop. The correction preserves the exact raster
  check at capture and waits for compatible resources during startup/resizing.
- New `CE FRAME` log lines identify receipt, camera, raster, copy and incomplete
  pair failures, and record both camera positions. This makes the displaced-view
  report diagnosable if it remains. The prior log lacked enough detail to prove
  which guard caused every dropped frame or explain the displaced desktop view.

Local verification includes regressions that failed with the old total-view
guard and now pass, distinct WARP GPU pixels through the production capture
scopes, copied lists with auxiliary views, half-height source preparation,
changed-camera rejection, all eight Release suites, Reach consistency and pinned
CE binding checks. These tests do not execute MCC's rendering or prove headset
behavior. Halo 3 remains the experience reference.

## Update and test

1. Close MCC and extract the Build ZIP. Follow MANUAL-README.txt. Replace the
   mod DLL and launcher together and **keep your existing halomccvr.cfg**.
   The same package supports Steam and Microsoft Store; update each edition
   you use. The Source ZIP is for rebuilding, not installation.
2. Start the headset connection/runtime and use the included launcher. Load
   CE campaign in **Anniversary graphics**, allow a few seconds, then press F3
   facing comfortably forward.
3. Check whether both eyes show the same location with stereo depth. Turn your
   head, lean sideways/forward, crouch physically, and recenter. Check image
   edges, scale, HUD/reticle visibility and stock weapon visibility.
4. Check pause/resume and a restart. Try the already-working gesture: physical
   left hand beside the left side of your head, click movement stick. Classic
   still presents flat; switch back to Anniversary and check recovery.
5. Check Halo 3 loading, stereo, input, pause and exit as a regression.

Send the new HaloMCCVR.log with the result, mission, edition and headset model.
The log records source/runtime/refresh rate. Keep the failed attempt's log if
either eye is black or displaced. The new `pairs` count and `CE FRAME` line
will distinguish preparation from actual capture. Desktop vertical packing
alone is not evidence of VR success.

## Retained limits

CE Classic stereo, controller-directed aim, tracked hands/weapons, separate
adjustable HUD/crosshair, snap-turn parity, head-relative walking, roomscale body
following and full vehicle/zoom integration remain unfinished. CE currently uses
stock stick aim and weapons. Physical melee and world collision remain deferred
until working CE VR injection is confirmed. Co-op, cutscenes, other missions and
performance still need testing; split-screen and unsupported textures stay stock
or reject the affected frame.

Existing-title roomscale/left-hand/bounds/melee/snap-turn work and all standing
tasks are retained, including deferred H2 vehicle refinements, all-title zoom,
H4 damage blackout and the minor H2 tank-exit reticle report. H3 dual firing stays
disabled. Accepted source remains 4e01f28. Preparation of this package performs
no installation, game-folder writes, MCC launch, publishing or PR.
