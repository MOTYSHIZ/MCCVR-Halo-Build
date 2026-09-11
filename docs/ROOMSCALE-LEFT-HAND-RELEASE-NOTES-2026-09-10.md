# Roomscale and left-hand update

Supports H2 Classic/Anniversary, H3, ODST, Reach and H4 on Steam and Microsoft
Store. Start with MANUAL-README.txt. Keep your existing config when updating.
This alpha package is locally checked; headset confirmation remains pending.

- Restores the requested MCCVR-d77c9dd left-hand positioning by default while
  preserving the newer weapon-collision, physical-melee and other refinements.
- Adds **Fix Hand Alignment (Experimental)** beneath Left-handed main weapon
  in F1 > Weapon & Aim. It is off by default and available only in left-handed
  mode. The newer anatomical correction can still misalign hands; switching
  it off restores the released presentation path.
- Corrects roomscale's H3-only admission gate so all supported titles can
  receive native movement commands. Also prevents nested controller-input
  wrappers from treating their own output as a physical stick and cancelling
  roomscale, which affected the shared input path including H3.
- Retains native character walking/collision, horizontal offset consumption,
  stick priority, tracking expiry and recenter/transition guards. Roomscale
  should move the body with physical steps, not merely move the view.
- Preserves controller aiming. Independent head-following native body rotation
  remains deferred. Vertical tracking does not issue a jump. H3's rejected
  dual-fire hooks stay disabled; broad dual-trajectory work remains unfinished.

Local checks cover Release build, three CTest suites, all-title roomscale
command transport, nested input wrappers, freshness/generation cancellation,
manual-stick priority and simulated follow at 60/90/120 Hz and three world
scales. Existing hand-transform, contact/melee, slider and lifecycle tests pass.
These checks do not reproduce Halo's native movement dynamics or headset view.

After installing, leave experimental alignment off initially. In F1 > Controls,
enable roomscale, recenter with F3 and test small physical steps with the stick
idle. Check stopping, normal stick movement, controller aim, hand/gun contact,
recenter, pause, death/respawn and vehicle/title transitions. Include H3 and
both H2 renderers. Send the log and identify edition, runtime and headset.

See the manual for current limits and troubleshooting. No installation,
game-file changes or publishing is performed by extracting this package.
