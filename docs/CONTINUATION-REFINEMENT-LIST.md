# Latest scope override - September 10 roomscale handoff

The user requested roomscale plus updated build/source ZIPs and instructions.
They explicitly chose to preserve controller aiming for this package. Horizontal
physical body following and head-relative walking are implemented locally across
H2 Classic/Anniversary, H3, ODST, Reach and H4; headset validation remains pending.
Independent head-following native body yaw is deferred by this choice. The broader
roomscale requirement therefore remains partly open. Deliver this candidate and
wait for testing/new instructions. All other retained tasks below remain in scope;
H4 damage blackout and the minor H2 tank-exit reticle report remain deferred.

Older packaging holds and "no roomscale code" entries below are historical and
superseded. Use ACTIVE-WORK-CHECKPOINT.md for exact current status.

# Standing user refinement list — September 9, 2026

User approved this list and requested that it guide every future "continue",
including a new chat. Read ACTIVE-WORK-CHECKPOINT.md for the exact resume point;
read CURRENT-STATE.md for acceptance. Preserve unfinished source edits.

Current instruction: verify the existing stability/recovery fixes, state their
real limits, then finalize left-handed support across all supported titles.
Left-handed support and independent dual trajectory are the first feature
priorities, followed by the remaining list. Do not stop after a partial ZIP.
Local implementation/builds/tests are authorized. Packaging remains on hold
until requested; no installation, game-folder writes, MCC launch or publishing.
Support both Steam and Microsoft Store. Halo CE is outside current VR coverage.

## Approved requirements

LATEST STOP: weapon/melee coverage and snap-turn implementation passes finished
locally. Send the checklist, then wait for the user's instruction before starting
roomscale. No packaging, installation, game launch or publishing requested.

## Completion ledger (September 10)

"Complete locally" means implementation/build/tests, not headset acceptance.
The user's accepted-build pointer remains unchanged. Item numbering below maps
to the full approved requirements retained later in this document.

1. Partial: recovered stability/lifecycle guards audited and validated locally;
   manual F1/launcher recovery control present but H3-only. All-title recovery
   and runtime confirmation remain open.
2. Complete locally: anatomical left-handed support across supported titles.
3. Pending: independent dual-wield trajectories and broader dual acquisition.
4. Current bounds/melee-coverage pass complete locally. New runtime readers for
   H3/ODST/Reach/H4; H2 live-verified path retained. Wider melee requirements
   (unarmed/secondary damage, specific mods and damageable objects) need further
   acceptance/refinement; do not label every imaginable custom weapon proven.
5. Separate world-contact, physical-melee and experimental gesture controls
   implemented. Gesture behavior and broader headset compatibility remain to
   validate; this entire compound item is not fully accepted.
6. Implemented: 5 m/s default and 10 m/s maximum, preserving saved settings.
7. Pending further refinement: sustained/sliding contact and jitter.
8. Existing alignment/trajectory controls retained; full requested separation
   and stock calibration not marked complete by this pass.
9. Pending: per-title defaults and automatic per-weapon saved overrides.
10. Completed per explicit user instruction: slider precision arrows.
11. Complete locally: snap-turn handlers audited; missing H2 shared-renderer
    snap path implemented and regression tested. Headset tests still required.
12. Partial: H2 controller-directed vehicle loop exists in both renderers;
    full H3-style seat/wheel control parity and AI report not closed.
13. Pending: H3/H2A lower-edge/corner visibility refinements.
14. Pending: full requested all-title weapon-side zoom-window behavior.
15. Pending: H2/H4 first-person vehicles after vehicle-control parity work.
16. Retained/pending: versioned Reach and other-headset reports.
17. Deferred: H4 damage blackout, only revisit when requested.
18. Deferred: minor H2 tank-exit reticle report; screenshot saved.
19. Implemented locally: optional physical horizontal body following and
    head-relative walking. Awaiting headset validation; independent native body
    yaw following the head is deferred to preserve controller aiming this package.

## Full approved requirements

Latest priority override: weapon bounds AND physical melee, then all-title snap
turn verification/fixes, then optional true roomscale/body following. Vehicle
controls and first-person vehicles remain queued afterward. The requested
roomscale behavior includes physical position and head-direction following;
user confirmed that movement heading follows head direction too.

September 10 priority override: finish all left-handed work first, then refine
physical melee/weapon world contact (including modded weapons), then complete
H2 vehicle controls before first-person vehicles. Remaining items, including
optional dual trajectory, stay in scope after these priorities. The user will
provide additional tasks individually. Preserve the new black-screen-on-damage
report; its cause is not established. This overrides the earlier dual-first order.

1. Finish current stability/recovery fixes: failed H3 dual-fire experiment,
   flat mode after level/game/multiplayer transitions, and the recovery control
   already in progress. Do not describe unconfirmed recovery as solved.
2. Optional left-handed support in H2 Classic/Anniversary, H3, ODST, Reach and
   H4: anatomically correct hands/arms, weapon seating, aim, trigger/grip,
   support grip, collision, melee and haptics.
3. Independent dual-wield bullet trajectory with a toggle: each gun follows
   and fires along its own controller, simultaneous fire, both handedness modes.
   Include acquiring/using two weapons in ordinary ODST/Reach/H4 campaigns.
4. Physical melee: both hands' punches and held-gun impacts, correct unarmed
   and secondary-weapon damage, ODST Mythic SMG left-hand miss, damageable world
   objects such as Warthogs and shield generators.
   September 10 addition: automatically derive contact/impact coverage from the
   actual equipped weapon model across every supported title, including modded
   weapons. Current reports describe weapons phasing through targets while only
   the hand registers. Investigate actual model/geometry data rather than a fixed
   stock-weapon catalog. Do not promise support for unreadable/invalid custom data.
5. Separate physical melee/world collision controls; separate gesture melee
   using the active game's melee binding; headset and refresh-rate compatibility.
6. Physical-melee speed ceiling 10 m/s, default 5, saved settings preserved.
7. Smoother sustained/sliding hand and gun world contact, reducing jitter,
   phasing and sticking, preserving impact response and contact haptics.
8. Visual gun positioning independent of reticle/bullet aim, including gun-stock
   calibration; trajectory adjustment under Crosshair.
9. Per-game alignment defaults plus saved per-weapon overrides, automatic
   equipped-weapon selection and dual-weapon ownership.
10. IMPLEMENTED / removed from pending work by the user on September 10:
    arrows on both sides of every VR-menu slider, stepping its last displayed
    digit (0.01, 0.001, or 1). Preserve existing controls; do not redo this item.
11. Restore snap turning.
12. H2 Classic/Anniversary enemy perception/aim, controller-directed vehicle
    controls, and correct weapon contact through weapon swaps.
13. H3/H2 Anniversary lower-edge/corner world visibility when looking up;
    preserve H2 Classic visibility.
14. H3-style weapon-side zoom windows in every supported title, including
    correct handedness, magnification, placement and zoom transitions.
15. First-person vehicles in H2 Classic/Anniversary and H4, appropriate seat
    views and adjustments, preserving H3/ODST/Reach behavior.
16. Retain version-specific reports: Reach doubled grass/effects, side-held
    close-range shot alignment, ineffective graphics settings, grainy image,
    Show body/contact/melee, and other-headset melee failures.
17. DEFERRED by explicit September 10 user steering: screen goes black then
    fades back after certain damage impacts, e.g. a Promethean Knight melee.
    Preserve the report, but disregard investigation and implementation until
    the user explicitly asks to revisit it. Cause remains unproven.
18. DEFERRED/minor, September 10: reported Halo 2 reticle issue after exiting
    a tank. Save the example in bug-reports/halo2-tank-exit-reticle.webp; do not
    investigate now. Edition, renderer, source build, runtime and headset were
    not supplied with this third-party report.
19. Optional true roomscale tracking: character body follows physical movement
    and head turning. Preserve the existing mode with the toggle off; establish
    engine-supported movement/collision behavior rather than moving only the view.

These are requested outcomes, not completion claims. Older detail/evidence:
CONTACT-PASS-REQUEST-CHECKLIST.md and MELEE-HANDEDNESS-DUAL-WIELD-2026-09-08.md.
Some baseline behavior is accepted; implementation alone does not accept any
new behavior. Update the checkpoint with actual progress and outstanding work.

User communication instruction (September 10): explicitly announce completion
of each section before proceeding, e.g. "left-hand support implementation
completed". Distinguish local implementation/build validation from headset
acceptance. Never announce a whole section complete while titles remain open.
