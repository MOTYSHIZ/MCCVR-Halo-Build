# CE native HUD and crosshair evidence

## E-CE-HUD-1: native crosshair boundary (September 15, 2026)

Official HCEEK `halo_tag_test.exe` (manifest-pinned SHA) establishes:

- HUD main `1F1C30`, invoked by interface draw `1CD9B0`, selects the local
  player and native HUD resolution/scale, then draws gameplay HUD components.
- Weapon interface `1FF6C0` names `hud_render_weapon_interface` and asserts
  `user_index == render.local_player_index` in `interface/hud_weapon.c`.
- It calls `1FEAB0` (`crosshairs_draw`) with player, equipped weapon, weapon-HUD
  tag and the 0x20 weapon state. This also handles the unarmed HUD tag.
- Crosshair draw checks native visibility state and authored weapon-HUD
  hierarchy, then draws the selected bitmap overlays. It is separate from
  ammo, health and motion tracker. It is not a general HUD-disable predicate.

Pinned MCC CE homologs match the same call tree, state branches and authored
tag relationships: main `B12B08`, weapon interface `B3ED24`, crosshair `B3FFA0`.
The two calls `B3EEE9` and `B3EF55` cover unarmed/equipped authored crosshairs;
the 4-argument Windows x64 boundary uses ECX/EDX/R8D/R9. Main HUD computes the
same 480/960 source scale and dispatches the same separate component families.
Native bitmap overlay drawing occurs within this crosshair function.

Source records: `out/ce-full-hud-kit-{functions,main,dispatch}.txt`,
`out/ce-full-hud-retail-{dispatch,weapon}.txt`,
`out/ce-full-crosshair-retail-disasm.txt`, and the Classic agent's
`out/ce-classic-output-source.txt`. Signature/call witnesses are included in
the optional `hud` group of HALOCE-EVIDENCE-MANIFEST.json. A failed HUD binding
must not reject camera or first-person bindings.

These findings establish a native scope, not captured-pixel success. The
prepared shared reticle redirect still requires CE-specific target/state
validation, complete eye-frame ownership, and visible headset verification.
Gameplay HUD size/aspect/height now have the separately verified native-scope
raster implementation in `HALOCE-HUD-LAYOUT-EVIDENCE-2026-09-15.md`; private
crosshair target restoration is established in
`HALOCE-HUD-TARGET-EVIDENCE-2026-09-15.md`. Visible Classic/Anniversary output
and full HUD/reticle parity remain unaccepted.

## E-CE-HUD-2: scope and capture failure isolation (September 15, 2026)

Halo 3's reference behavior changes gameplay HUD framing independently of the
reticle. The CE native crosshair boundary must therefore suspend the gameplay
HUD affine even when authored capture resources or target bindings are absent.
The prior WIP installed that boundary only after private resource preparation
succeeded, while HUD layout installed independently. Inspection establishes
that a resource failure could then scale/raise the stock reticle with the HUD.
No claim is made that this failure occurred in a user log.

`haloce_hud.cpp` now installs the independently verified native boundary first.
Its normal stock path still calls the original CE draw with layout suspended.
Private target contracts/resources prepare separately; a refusal retains that
boundary and the readable HUD, with a cold log naming the capture fallback.
If the native boundary itself cannot install, gameplay layout remains stock
so it cannot move an unisolated stock reticle. Neither failure affects camera
ownership. Resource-not-ready remains retryable without reinstalling the hook.

Production-body tests explicitly reject authored/discard preparation while
retaining native scope ownership, verify stock drawing inside the suspension,
then recover capture when resources are ready. The WARP HUD layout fixture
verifies stock framing with a missing boundary and recovery afterward. Release
HUD transaction, raster layout, target restoration and redirect suites all pass.
These tests establish isolation and state restoration; they do not prove native
gameplay pixels or headset appearance. Curvature remains native-flat and is
outside the user's current basic VR priority.
