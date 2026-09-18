# Refinement audit candidate — unaccepted, scope incomplete

Based on accepted Alpha 0.4.2 source `1a9766c`. One build supports Steam and
Microsoft Store and retains all six titles. This is a test candidate, not a
completed implementation of the full September refinement list.

## Changes included

- Thumbrest D-pad suppresses other XR/gamepad actions, roomscale input,
  generated actions, and native physical/gesture melee. Consumed button, grip,
  trigger and stick holds must return to rest before resuming normal input.
- The optional native menu pointer suppresses other controller inputs while
  successfully delivering the pointer. F1 retains priority. A failed pointer
  leaves ordinary menu controls available; thumbrest D-pad prevents pointer clicks.
- Walking stick conversion preserves the vector's direction instead of pushing
  each small axis independently above the game's deadzone.
- Known H4 Promethean weapons without an extracted magazine receive an orange
  reload item, including when unfamiliar-weapon placeholders are disabled.
  Native reload eligibility, ammunition and reserve rules remain in charge.
- `Hide HUD completely` in F1's HUD category (`hide_hud`, default off) suppresses
  the scoped native gameplay HUD and VR reticle. Menus remain available.
- H2 aim, shot, collision/contact and reload ownership reject invalid tables and
  recycled datum handles. A faulted native aim-assist call is not replayed.
  ODST seated personal-weapon ownership also requires the full salted handle.

## Still unresolved

Targeting/homing/native reticle colors; playing only the native reload's final
cocking/chambering tail; proper magazine textures; H2 vehicle steering and CE
driving camera follow; the CE white/cyan flare; independent dual trajectories and
actual barrel-origin trajectories. Their existing runtime behavior is retained.
The H2 fixed-reference steering experiment is preserved but disabled because it
does not yet agree with the rendered camera and reticle.

The Cairo Station firing / Outskirts post-cutscene loading crash report is H2.
Ownership hardening is not proof that either it or the first-person vehicle
checkpoint crash is fixed. The supplied log contains no exception address/stack.
The Windows launcher warnings work remains explicitly excluded. All standing
scope is retained in `docs/CONTINUATION-REFINEMENT-LIST.md` in the source ZIP.

## Validation and testing

Packaging requires the Release build, all 40 CTest suites and the Reach
consistency gate. Added checks cover recycled H2 handles, invalid table/storage
rejection, radial input and hold draining, actual D3D11 rendering of the
Promethean item, and CE HUD scope cleanup after a native callback fault.
Passing these does not establish headset or co-op acceptance.

Check walking diagonals, hold/release behavior in both input modes, Promethean
reload presentation, and HUD hide/show during gameplay and pause in each title.
Compare Halo 3 with the accepted build. Crash verification needs the H2 Cairo
firing and Outskirts loading cases plus the original vehicle checkpoint case.
Record edition, runtime and headset with results. The accepted-build pointer
remains unchanged. This package was not installed and MCC was not launched.
