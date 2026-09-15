# CE gameplay HUD raster layout - September 15, 2026

## E-CE-HUD-LAYOUT-1: native scope and primitive evidence

Pinned images are the same retail/HCEEK images in the central CE manifest:
retail SHA-256 `0A12DC561780F449D3F4D0DF10BB8D3BC7BE7840A5BEB2B236F672EB6CD42E6C`,
HCEEK SHA-256 `FC9E2B6193C6F6D9FF988278B0D39A983747F3FDBDECA7CAFADD28F1F0C53E73`.
All addresses below are image RVAs. No process or game file was modified.

The official HCEEK gameplay HUD main `1F1C30` is dispatched by native interface
`1CD9B0`. It resolves the current local player's datum, selects native HUD
resolution/scale and dispatches separate weapon, motion tracker and status/text
families. The corresponding retail main is `B12B08`; interface `B31D88` calls
it at `B32106`. `B12B20` reads local player `29AF2B8`, and `B12B33/3A` resolve
the datum from `(*2EA2D90)+B8+4*index`. No player/invalid index skips drawing.
This is distinct from global/menu UI dispatch `B321D8`.

The following geometry paths were checked independently of the crosshair:

- HCEEK `1F54F0` makes four vertices from a short pixel anchor, bitmap rectangle,
  UV rectangle, scale, rotation and alpha. Each vertex is 24 bytes: float X/Y/Z,
  alpha, U/V; Z is zero. Its dynamic geometry call `434A10` resolves to `446E30`,
  which names `rasterizer_dynamic_screen_geometry_draw` and the source
  `rasterizer/dx9/rasterizer_dx9_dynavobgeom.c`. Retail `B58ADC` is the matching
  generator, calling `B0EC20` at `B58D23`. It is used by both the bitmap helpers
  `B58604/B58844` and the motion tracker body `C5093C` (calls at `C51D61/C51E4B`).
- Retail `B0EC20` preserves pixel-coordinate vertices and builds shader
  projection constants. It uses native rectangle `29E05B4..29E05BA`, kind-zero
  target dimensions and 1280/720 raster factors. Its descriptor can select a
  custom projection branch. `B0EB54` issues `BF5F08(vertex,24)` at `B0EBF1`.
  Thus the generic primitive vertices cannot be treated as normalized device
  coordinates.
- HCEEK text `43D110` names `rasterizer/rasterizer_text.c`. Its font parser
  `4B4BA0` calls line draw `4B51C0`, which invokes glyph callback `43CEA0`.
  That callback forms foreground/shadow quads and sends them to `45DDC0`;
  `45DAC0/45DDF0` own the batched text shader pass. The text projection uses the
  native rectangle directly, unlike the bitmap projection above. Retail
  `CAA9D8` has the corresponding text pass setup (`CAA908` projection), and
  `CAAC4C` emits the 24-byte quad through `BF5F08` at `CAAC68`.
  HCEEK HUD main explicitly calls messaging `1F8C60` at `1F1FCB`; that function
  names `interface/hud_messaging.c` and calls Unicode text `43D360` at
  `1F96DB/1F972A/1F99F2/1F9A0C`. `43D360` uses the same glyph callback `43CEA0`.
  This confirms gameplay text reaches the separate raster text family.
- `BF5F08` is a general triangle-strip four-vertex submission. Other callers
  include the final world-output blit `B51A14`. Neither stride nor primitive
  function alone identifies gameplay HUD.

The native rectangle also has world, postprocess and screen-projection
consumers. No CE global rectangle, guessed curvature member or primitive
vertex structure is written by the layout implementation.

Source records are the UTF-16 Ghidra/PowerShell logs under
`out/ce-hud-layout-{kit-font,kit-raster-font,kit-text-consumer,retail-raster-font,retail-rectangle,retail-text-consumer}-20260915.txt`,
`out/ce-hud-layout-main-bytes-20260915.txt` and earlier
`out/ce-hud-{draw-consumers,quad-consumer,quad-draw,raster-quad,raster-dispatch}-20260915.txt`.
`HALOCE-HUD-LAYOUT-CONTRACTS.json` holds the independent optional main-scope
contract; it is merged into the central manifest/generated `hud_layout` group.
The pinned executable has one exact prefix match, six matching relatives and
two matching body witnesses. Record: `out/ce-hud-layout-contract-verification-20260915.json`.

## Deliberate VR-side implementation and H3 behavior being matched

Halo 3's working HUD exposes size, aspect trim and vertical height independently
of its authored reticle. `game.cpp::ComputeHudSafeFramePair` computes a symmetric
extent from eye zero's four FOV angles, corrects the native game aspect, applies
the user aspect trim, and bounds both scales to `[0.15,1]`. The CE pure helper
uses that same policy. Both native eyes use the same frozen settings/affine;
they do not receive independently scaled binocular HUDs. Positive height
raises the raster result by output pixels after scaling, matching H3's
positive-up native anchor-height convention.

`haloce_hud_layout.cpp` is an independent optional feature. Its `B12B08` scope
requires a fresh CE render owner and observed numeric viewport/scissor state
for the actual native context. In particular, the one observed root viewport
must exactly match the frozen native camera's viewport rectangle. A previous
render pass's viewport, missing observations, multiple root viewports or stale
ownership leaves layout stock. This conservative admission is not proof that
every live CE HUD pass has that shape; headset/native observation is pending.

The module applies a raster affine to both viewports and scissors, including
native setters occurring inside the scope. It retains the latest original
numeric state and restores that state on normal or exception exit. Crosshair
entry suspends the affine for the entire native call, including stock fallback;
balanced nested exits reapply gameplay framing. Prepared private capture can
copy the known numeric state and suppress observation of its own temporary
viewport changes. No render-hook COM getter, AddRef/Release, allocation, lock,
logging or signature scan is needed for this layout transaction.

The root-owned D3D integration observes SDK-defined setters and invalidates
before ClearState/command-list execution/context-state replacement. An active
scope restores known native state before such invalidation. It then remains
stock until a later frame has fresh observations. Other-title calls stop at
the active flag. Optional observation or hook failure cannot disarm CE's camera.

## Validation and remaining limits

`halomccvr_ce_hud_layout_tests` includes the production implementation and uses
a real D3D11 WARP context. It verifies affine/scissor output, latest-native-state
restoration, setter changes during the native scope, nested crosshair framing,
private capture state isolation, ownership/recenter rejection, command-state
invalidation and recovery, unknown/prior-pass state rejection, observer/title
isolation, structured-exception cleanup, optical aspect and empty scissors.
Release checks use explicit failures, not disabled assertions.

These checks verify the transaction and raster math. They do not execute MCC,
prove actual native HUD pixels in either graphics mode, or establish headset
acceptance. The shared native scope serves both modes only when their current
core exposes a matching fresh render context and raster state.

**Curvature is still unfinished.** The investigated CE HUD is projected flat;
no equivalent of H3's authored CHUD curvature record has been established.
`Tracking.hud.curvature` is retained for immutable configuration continuity,
but the affine does not claim to implement it. The cold feature log explicitly
reports `curvature=native-flat`. A deliberate curved HUD surface/compositor
would be a separate implementation, with independent pixel/lifetime evidence.
The September 15 continuation now explicitly requires a package. This limitation
must remain documented in that candidate; it is not accepted full HUD parity.

Anniversary's additional full-height-to-eye mapping and exact target/raster
restoration are recorded in `HALOCE-ANNIVERSARY-HUD-EVIDENCE-2026-09-15.md`.

## E-CE-HUD-LAYOUT-2: target routing is not a capture lifetime proof

The optional raster layout does not change render targets. A separate attempt
to remove COM queries from authored crosshair capture must account for these
additional native relationships:

- `B31D88` calls `AC7EF0` immediately before the gameplay HUD. That screen-effect
  function can alternate target kinds 1/2, then restores
  `B51670(29E0580,0,0,0)`. Bitmap and text drawing require `29E0580==1`.
- `B51670` sets a kind-one viewport from the native rectangle; other kinds use
  queried target dimensions. The adapter independently requires its observed
  viewport to match the frozen camera, avoiding a guessed inherited viewport.
- `AEB00` routes a target kind equal to `(graphicsMode!=0)` through `1DC2F0`.
  Other kinds 0..2 use `1DBE50` with the indexed wrapper at
  `2E3B910+8*kind` and the depth wrapper `(*2E3BDD8)+318`.
- `1DC2F0` pops the native target stack: context `*2E3BDE0` has a record pointer
  at `+8`, count at `+10`, and records of stride `0x48`. A nonempty stack calls
  its virtual `+F8` with the last record and decrements the count. Empty-stack
  handling sends a zero-wrapper descriptor through `1DBE50`.

Consequently Classic HUD kind one can carry a depth attachment, and Anniversary
kind one restores a saved native target descriptor. Neither mode is proven to
have one color target and a null depth view merely from its HUD function.
Classic's final output kind-zero proof is insufficient for restoring a borrowed
crosshair-phase RTV. Native wrapper/view ownership, current attachment identity,
and release boundaries remain to be established for a query-free capture change.
No borrowed target retention is implemented by this layout module.

Records: `out/ce-hud-layout-target-{ownership,routing,bind}-20260915.txt`.
