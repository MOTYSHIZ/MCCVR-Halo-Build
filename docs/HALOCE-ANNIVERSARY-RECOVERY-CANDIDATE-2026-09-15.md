# CE Anniversary recovery and muzzle alignment candidate

This candidate follows the `884de13` headset report: Original enters VR with
correct hands and working muzzle flashes, but switching to Anniversary freezes.
The reported Anniversary flash also follows the weapon with a visible offset.
Both Steam and Microsoft Store remain supported. Keep your existing config.

## Anniversary rendering

The manually replayed Anniversary HUD callback is disabled. The failed run
reached the newly enabled native replay once, reported unverified cleanup,
and completed no Anniversary stereo pair. Earlier working Anniversary runs
refused the replay before native entry. The callback changes native target and
render state beyond the adapter's proven cleanup contract.

This rollback preserves the corrected Original startup, hand geometry, weapon
scale, controller aim and effects. It also retains Anniversary camera, tracked
hand and weapon-lens work. Anniversary HUD visibility remains unresolved: this
candidate does not re-enable the unsafe replay. The precise stalled native
instruction is unknown because the supplied log has no crash address or stack.

## Anniversary muzzle flashes

Anniversary particles previously selected a fixed first-person lens while the
tracked weapon used the current VR eye's lens. The new optional particle
adapter selects that same eye lens for first-person emitters. It changes only
their projection selectors in a private copy of the native upload buffer.
The authored effect attachment, particle motion, gun placement and native shot
origin remain intact. Invalid or unowned draws use the stock particle path.

The native firing-effect and first-person marker chain is traced in
`HALOCE-MUZZLE-TRIGGER-EVIDENCE-2026-09-15.md` and
`HALOCE-ANNIVERSARY-MUZZLE-PROJECTION-2026-09-15.md` in the source ZIP.
Both the actual mesh-particle and sprite-particle
vertex shaders pass 144 WARP draws spanning both eyes, three scales and several
emitter slots. This verifies the lens mismatch and correction with real native
shader code; the final visible muzzle alignment still needs your headset test.

## Validation and test

The production rendering regression completes two successive Anniversary
stereo pairs with the failed HUD callback left present but disabled. It verifies
both eyes' pixels, native target/raster state and retained camera ownership.
The Original startup regression also passes. These tests use native callback
fixtures; they do not reproduce the exact missing native crash stack.

The native constant uploader also passes 15 offline cases covering full and
ranged uploads, bounded buffer sizes and exact source preservation. Production
hook tests cover invalid ownership, nested draws, native exceptions and teardown.
The cumulative Release build, all 20 CTest suites, 104 pinned native contracts,
16 mapped-image binding groups and Reach consistency check pass. Packaging
repeats the required build/tests for the exact committed source. Broader
transition stability and every weapon/effect combination remain to test.

The accepted source remains `4e01f28`; local checks are not headset acceptance.
Other-title work and all standing/deferred requests are preserved. CE melee,
world collision and physical body following remain outside this candidate.

1. Enter CE in **Original** and confirm its view, hands and muzzle flashes.
2. Switch to **Anniversary**, move both hands, fire while moving the gun, and
   check the flash at the muzzle from several angles. Check both headset eyes.
3. Switch back and forth, then pause/resume and Save & Quit.

Deliver both build and matching source ZIPs, then wait for the user's test.
