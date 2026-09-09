# WIP independent firing and support-grip exclusion

Halo 3 reference behavior: each equipped gun follows and fires along its owning
controller; the native engine owns ammo, reload, firing effects and damage.
This candidate is untested in a headset and does not advance CURRENT-STATE.md.

H3EK `A844A0` takes five arguments (weapon, barrel, data pointer, index, boolean).
Its pinned retail homolog `3683A0` calls the seven-argument firing-data helper
`3524B0` at `368B92` (return `368B97`). The helper receives the owner unit,
origin/direction, opaque marker argument, offset and two booleans. The outer
wrapper preserves RAX as well as forwarding all arguments, without assuming
that an otherwise unused retail return register is zero. These are different
ABIs from H2 and ODST, not copied bindings. Preserved decompiles:
`out/dual-h3-fire-kit-0xa844c6.c`, `out/dual-h3-fire-match.c`,
`out/dual-h3-firing-data-retail.c`.

H3EK's primary/secondary inventory selection agrees with retail `35A9A4`,
`3683A0` and inventory getter `356388`: role bytes `262/263`, four full handles
at `268`. Native owner getter `364E18` verifies owner-valid byte `15D` and
owner handle `168`. Table entries use H3's independently established salt,
kind and data pointer. Offline verifier `tools/verify-halo3-dual-bindings.py`
checks the pinned PE SHA-256, both unique hook entries, unwind extents, exact
call edge and inventory/owner instruction bytes. Existing melee selector
verifier also checks primary-role selection. These are static findings.

The optional H3 hook carries the firing weapon in a nested thread-local scope.
After the native helper returns, two current owned weapon handles must match a
coherent render-side publication and the local unit's complete salted identity.
Only then does the matching slot replace the native direction. Native origins
stay intact; the shot converges toward that controller's ray at the configured
reticle distance. Single weapons, remote units, vehicles, stale publications
and failed bindings retain stock behavior. Native spread after the helper is
preserved. Each snapshot expires at 100 ms and must match tracking epoch,
title generation and the current install. Visual gun offsets are excluded.
The render publication participates in callback quiescence before cleanup.
Faults are reported by the worker and disable only this optional feature.

Successful local secondary-slot presentation in H2, H3 and ODST now inhibits
the shared two-hand latch and aim calculation independently of collision or
physical melee. This evidence only inhibits support gripping; it is never
used as authority to fire or mutate inventory. It expires after 150 ms and
rejects foreign title generations. Dropping a secondary or changing handedness
while holding grip requires release before a new support grab.

Remaining work: verify H3 rays in a headset (both handedness modes, simultaneous
fire, pickup/drop, death/respawn and multiplayer); complete ODST/Reach/H4
independent directions and ordinary-campaign acquisition; complete anatomical
handedness in every renderer. Current handedness changes controller roles but
does not mirror meshes or swap anatomical arm chains. H2 packet review stopped
at `Halo2OwnFinalFirstPersonPackets` / `Halo2OwnDualFirstPersonPackets` in
`src/common/halo2_render_logic.h`: those still bind anatomical right to primary
and anatomical left to secondary/support. No unproven mesh rewrite was made.
