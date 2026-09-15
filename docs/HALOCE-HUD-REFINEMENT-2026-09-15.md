# CE HUD refinement after a2b526a - September 15, 2026

Halo 3 behavior being matched: native crosshair art follows calibrated
controller aim, independently of native gameplay-HUD framing. An optional
capture failure retains native art and the working VR camera.

## Authored crosshair validation order

The supplied CE log reports `CE crosshair native-art fallback:
hud_target_release`. This is reproduced locally with a real MinHook patch in
private fixture memory. The `hud_target` contract witnesses native texture
release `0x22B0A0`, which is also the core's resource-retirement hook. Core
installation precedes optional HUD preparation. A later raw-image signature
scan necessarily rejects the core's own installed jump.

The core now verifies the complete optional `hud_target` contract while the
retained module's native bytes are untouched, before creating/enabling hooks.
The exported proof requires the exact module base, size and generation, active
CE ownership and a nonretiring installed core. Retirement revokes the proof
before any draining attempt. The HUD feature consumes this proof instead of
rescanning patched code. It clears its local proof on feature retirement.
No signature requirement was removed or weakened. Failed optional proof
leaves camera ownership available and native crosshair drawing intact.

Validation includes real hook install/remove against private mapped fixture
bytes (never executed), the production native-image verifier on the pinned PE,
production core proof lifetime checks and production HUD preparation/fallback
tests. The raw scan passes before installation, fails at `hud_target_release`
while installed, and passes again after retirement. A missing proof still
refuses capture without removing native HUD scope.

## Anniversary gameplay HUD admission

`a2b526a` still reports zero native eye replays with failure 1. The existing
log combines many independent native guards into that value. Native-order
review confirms the current pre-output callback boundary: the eye camera has
been uploaded for scene and shading, and native output clears source selection
only after the copy. This does not justify weakening eye/source ownership.

Rejections now identify the actual guard: native HUD frame bit, primary-eye
ownership, callback owner, player count, initialization/disabled/rendering
state, native availability, display mode/dimensions, stock camera, tracking
reference or output source. Success clears the old error. Logging remains in
the cold poll; the hook only stores a numeric reason.

Production WARP integration covers an unavailable stock HUD camera: world
eyes remain available, no callback runs, and a subsequent valid frame restores
HUD pixels and clears the rejection. Existing callback, native stack,
exception and exact raster-restoration checks remain.

The crosshair ordering defect is established and corrected locally. The
Anniversary gameplay-HUD failure cannot yet be assigned to a particular native
guard from the supplied log. Do not describe its visible replay as confirmed,
or confuse it with the separately corrected pause compositor transition.
