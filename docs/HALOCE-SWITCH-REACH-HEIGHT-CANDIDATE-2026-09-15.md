# CE graphics switching and Reach HUD height

This candidate preserves the CE Original behavior you confirmed in `2cf002b`,
including the native reticle, gun tracking and muzzle alignment. It corrects
the diagnosed graphics-switch resource failure and adds Reach's missing HUD
height control. Keep your existing `halomccvr.cfg`. One build supports Steam
and Microsoft Store and retains all other supported titles.

## Changes

- **CE graphics switching:** the matching Windows crash dump identifies a
  native HUD draw using a shader resource that the full-resolution rebuild
  had disposed. The rebuild now preserves the native resource lifetime so
  native reconfiguration reloads those shaders. A failed reload retains its
  verified owner for retry; the HUD waits for its resources without disarming
  the camera or ending VR.
- **Anniversary HUD:** the corrected path retains full-resolution eye images
  and compatible late HUD targets for both eyes. Native target cleanup also
  handles the verified intermediate state left by a failed bind. The earlier
  failed replay enables remain disabled; the corrected path has its own enable.
- **Reach HUD height:** positive height raises the HUD, negative height lowers
  it and zero restores native placement. The setting moves the completed
  native HUD anchor once, including projected markers, while the captured
  aiming reticle stays on its separate aim ray. Size and width remain
  independent. Reach's documented curvature limitation is unchanged.

All implemented CE tracking, hands, native reticles, muzzles, world contact,
physical melee and gun contact remain present in both graphics modes. Gun
contact uses the existing fourteen samples derived from all twelve official
CE weapon models through their live bones, alongside hand/node contact.
These are conservative stock weapon envelopes; exact Anniversary replacement
or custom mesh surfaces are not separately established. Reload parts can
enlarge an envelope. Native melee retains biped targets and the held weapon's
damage selection for either hand. Other damage targets, a separate bare-hand
damage selector and CE body following remain deferred. No prior standing work
is removed or newly claimed as accepted.

## Headset checks

1. In Reach, move HUD height up and down, then reset it. Confirm the aiming
   reticle remains steady while the HUD moves.
2. In CE, switch Original to Anniversary and back repeatedly. Check ammo,
   shields and reticles in both eyes, then pause/resume and fully quit/relaunch.
3. In both CE modes, check gun/hand tracking, muzzle flashes and image quality.
   With World collision and True physical melee enabled, test hands and the
   barrel, stock and sides of several guns against walls and enemies. Include
   firing, reloads and weapon swaps; keep your existing configuration.
4. Briefly check Halo 3's reticle and contact behavior for the inherited shared
   changes that still need headset regression coverage.

Please retain `HaloMCCVR.log` and identify the graphics mode, edition and
headset when reporting the result. Local checks cannot establish headset
acceptance or guarantee every transition and long-session case.

## Evidence and delivery

The matching crash dump, native disposal/reload sequence, exact resource
ownership and retry rules are documented in
`HALOCE-RENDERER-RESOURCE-LIFETIME-EVIDENCE-2026-09-15.md`. The native instruction
tests reproduce the original empty-resource failure and separately verify the
reload gate and shader-cache loader, including cleanup on shader failure.
They do not emulate the complete driver initialization.

`REACH-HUD-HEIGHT-2026-09-15.md` records the official HREK discovery, retail
match, 48 actual native anchor cases and 175 production wrapper checks.
Production CE regressions cover native-resource readiness on the ordinary
fallback and cleanup after partial target preparation. The full Release build,
all 24 CTest suites, 126 pinned contracts, 19 production binding groups and
Reach gate pass. The package repeats the build, tests and gate for delivery;
the manifest and archive hashes identify the exact committed source.

Delivery consists of a build ZIP and matching source ZIP. No installation,
game-folder writes or MCC launch is performed. The cumulative accepted source
remains `4e01f28`; this candidate awaits your headset result and instructions.
