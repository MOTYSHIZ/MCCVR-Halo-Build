# Next candidate: melee, handedness and all-title dual wield

Accepted starting point: 4e01f28. See CURRENT-STATE.md for the exact artifact
and the September 8 headset result, including the ODST Mythic Overhaul SMG
left-hand contact exception. Preserve that result as the regression baseline.

The user explicitly requests one next build/source ZIP containing refinements
to physical melee, optional left-handed main weapon/aim, and independent dual
wield in ALL supported titles: H2, H3, ODST, Reach and H4, on both editions.
For ODST, Reach and H4 this includes acquiring and firing two weapons in
ordinary campaigns, not only compatibility with mods that provide two weapons.
CE is not a supported VR title. GitHub publishing is cancelled for this pass;
publication waits for a satisfactory tested ZIP. No installation or launch.

Halo 3 reference behavior: each equipped weapon follows its owning controller;
each trigger fires its own weapon along that controller's aim. The native
engine remains responsible for ammo, reloads, damage, pickups and simulation.
Optional failure must leave the working camera and other features running.

## Evidence and work in progress

- H2 current helper Halo2WeaponAimHelperDetour always uses the primary reticle
  for every local-unit firing call. Its local-unit guard is insufficient to
  select the secondary ray. E-H2-37 already proves the native firing caller:
  H2EK 49C960 -> 47DC20; retail 8E4940 -> 8F0F70. Inspect weapon ownership at
  this caller before adding independent direction selection.
- Supplied ODST log has successful submissions from both hands, no contact
  fault, and repeated missing authored weapon bounds in the modded run. It
  does not identify the exact missed SMG strike or prove its cause.
- Remaining native unarmed/secondary damage-response limitations are not
  disproved by the accepted ordinary melee result. Preserve working behavior
  while investigating them.

This document records scope and observed code, not completion or new acceptance.
