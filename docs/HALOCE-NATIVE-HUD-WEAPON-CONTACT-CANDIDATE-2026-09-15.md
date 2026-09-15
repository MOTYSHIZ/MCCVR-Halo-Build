# CE native reticles, Anniversary HUD and stock weapon contact

This candidate refines user-tested `22cb813`. It preserves the confirmed
working Original/Anniversary VR base, full eye resolution, muzzle alignment,
hands and Original HUD. Keep your existing `halomccvr.cfg`. The same build
supports Steam and Microsoft Store, alongside the other supported titles.

## Changes

- **Native reticles in both CE modes:** CE's bitmap renderer writes RGB while
  leaving capture alpha at zero. The mod previously counted the real artwork
  as blank. It now measures the native color pixels and converts the captured
  artwork to the transparency OpenXR needs. The actual weapon reticle replaces
  the temporary marker after a successful upload; native shape/color and the
  common aim, size and distance controls are retained.
- **Anniversary HUD:** in `22cb813`, the late native HUD binds a packed two-eye
  color target together with a native depth root only one eye tall. That combination can
  prevent actual drawing despite successful callback/copy counters. The new
  optional transaction prepares compatible native attachments, draws both eye
  HUDs and restores native state. The old unprepared adapter was disabled in
  its own commit; the previously failed manual outer callback stays disabled.
  The completed native trace includes the late return from split depth children
  to the root. Earlier builds used different target dimensions; this finding
  does not attribute every earlier missing-HUD report to the same cause.
- **All twelve stock CE weapons participate in contact:** physical bounds now
  include every positively weighted weapon vertex from the official models,
  through each live animated gun bone. This covers barrels, stocks, magazines
  and other gun parts beyond the old skeleton pivots. Fourteen stable surface
  samples supplement the hand/node samples, and every weapon sample enters
  world-contact and physical-strike sweeps. Physical motion drives strikes;
  animation alone must not cause melee.

The contact shapes are bounded envelopes, not triangle-by-triangle collision.
Both graphics modes deliberately use the stock CE physical shape; Anniversary's
replacement artwork and custom meshes do not have separately verified exact
surface coverage. Unrecognized animation graphs keep logged node contact.
Models with unchanged stock animation graphs use the stock physical shape even
if a mod replaces their artwork. Moving reload parts can enlarge the envelope
and include empty space between parts; check contact while reloading too.
Native melee targets bipeds and keeps the held weapon's native damage selection
for either hand. Other damage targets, a separate bare-support-hand damage
selector and CE body following remain deferred. Prior standing work is retained.

## Test in the headset

1. In **Original and Anniversary**, check each weapon's native reticle while
   aiming and changing weapons. Check reticle size/distance independently of
   the HUD controls.
2. In **Anniversary**, check ammo, shields and the rest of the HUD in both eyes;
   adjust HUD size/aspect/height. Confirm Original's HUD still looks right.
3. With **F1 > Body & Hands > World collision** and **True physical melee**
   enabled, touch walls with the barrel, stock and sides of several guns. Swing
   each hand/held weapon into an enemy. Check reloads, firing and weapon swaps.
   These settings are independent; existing saved settings are preserved.
4. Switch graphics both ways, pause/resume and fully quit/relaunch. Confirm
   image quality, muzzle placement and smoothness remain at the working base.
5. Briefly test **Halo 3** for the shared reticle/queue regression check.

Please include `HaloMCCVR.log` with the graphics mode, edition and headset used.
The new reticle alpha/RGB and weapon-envelope counters distinguish captured
art and weapon geometry from callback execution alone.

## Evidence and delivery

The native write-mask trace, actual GPU reticle upload tests, native HUD target
allocation/binding trace and official weapon geometry are recorded in:

- `HALOCE-NATIVE-RETICLE-RGB-2026-09-15.md`
- `HALOCE-NATIVE-HUD-ATTACHMENTS-2026-09-15.md`
- `HALOCE-WEAPON-MESH-EVIDENCE.json`
- `HALOCE-WEAPON-CONTACT-REFINEMENT-2026-09-15.md`
- `HALOCE-22CB813-TEST-2026-09-15.md`

The cumulative Release build, all 23 CTest suites and Reach consistency gate
pass. All 123 pinned contracts and 19 production binding groups pass. Native
HUD attachment verification covers four cases; compiled weapon verification
covers 48 poses / 84,720 vertex-pose checks. Native reticle verification executes
3,546 instructions, with production WARP pixel tests including sRGB output.
The final package repeats the required build/tests for its exact source commit.
Local checks do not establish headset acceptance. The cumulative accepted
pointer remains `4e01f28`.

Delivery is a new build ZIP plus matching source ZIP, with hashes and manifest.
No installation, game-folder changes or MCC launch is performed. After delivery,
work waits for your headset result and instructions.
