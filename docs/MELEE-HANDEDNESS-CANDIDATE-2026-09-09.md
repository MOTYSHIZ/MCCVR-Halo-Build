# September 9 WIP test build ? read before testing

Requested early by the user because of usage limits. This bundles unfinished
work rather than claiming completion. The exact source commit and DLL/launcher
hashes are in CANDIDATE-MANIFEST.json. Both Steam and Microsoft Store editions
are supported; H2 Classic/Anniversary, H3, ODST, Reach and H4 are included.
Halo CE VR is not supported. No installation or game launch was performed.

The accepted physical-melee baseline remains 4e01f28. Nothing newly included
here has headset acceptance. Keep the accepted ZIP available for comparison.

## Implemented and possibly working ? headset testing required

| Feature | Included behavior | Limits / possible issues |
| --- | --- | --- |
| Left-handed main weapon | F1 > Weapon & Aim > Left-handed main weapon, default off. Main tracking/aim, trigger/grip, velocity and haptics move to the physical left hand; right becomes support/secondary. | Anatomical mesh/arm routing is unfinished: a right-looking hand can appear on the left, arms can cross, and hand/gun seating may be wrong. Physical sticks, buttons and D-pad preference keep their assignments. |
| H2 independent dual aim | Each verified owned weapon uses its own controller direction, in Classic and Anniversary. | Unconfirmed in headset. Missing/stale ownership or tracking retains native aim. Close-range alignment, simultaneous fire and swap/drop need testing. |
| H3 independent dual aim ? NEW | Optional native firing scope selects primary/secondary by full weapon and owner handles; each gets its own controller ray. | Unconfirmed. On-foot dual weapons only; single weapons/vehicles keep prior native behavior. Native origin stays in place and converges toward the hand ray at reticle distance, so close-range parallax is possible. Native spread remains. No independent second reticle has been added. |
| Support-grip exclusion ? NEW | Recent local secondary presentation in H2/H3/ODST prevents two-hand aiming from coupling the guns. Dropping a secondary or changing handedness while gripping requires release before a new support grab. | Presentation evidence expires after 150 ms; prolonged rendering gaps and transitions need testing. Reach/H4 dual ownership is unfinished. |
| Slider precision arrows | Every VR-menu slider has arrows stepping the last displayed digit: 0.01 for two decimals, 0.001 for three, 1 for integers/percentages. Dragging remains available. | Local native-ImGui click/drag/bounds tests pass; VR pointer usability still needs testing. |
| Trajectory / visual alignment controls | Crosshair exposes trajectory pitch/yaw/roll. Visual support-hand offsets no longer alter the two-controller aim line or grab zone. | Per-title alignment and gun-stock calibration need validation, particularly secondary guns and left-handed presentation. This is not a claim that all alignment problems are solved. |
| Physical-melee speed | Slider/config/runtime range is 0.30?10.00 m/s; default remains 5.00. | Existing saved settings remain in effect. |
| World-contact smoothing | Small release corrections decay, bounded to 10 mm and 120 ms; actual collision/damage still uses native contact. | May still jitter or feel sticky; headset improvement is unproven. |
| World-object melee targets | Removed the explicit biped-only target filter from the five native contact adapters; attacker remains a verified local biped. | Warthog/shield-generator damage is not proven. Native damageability, materials and authored responses still decide results. |
| H3 melee response selection | Optional physical-strike selection can use the secondary weapon's authored response or authored unarmed unit damage. Main armed strikes/manual melee keep native selection. | This is new and unaccepted. Custom tags with no usable response retain fallback behavior. Other titles' unarmed/secondary response selection is unfinished. |
| ODST secondary contact | Secondary palette has its own authored bounds and contact samples. | The Mythic Overhaul SMG left-hand miss is not confirmed fixed; unfamiliar custom models can use hand-only bounds. |
| H3 transition recovery | Worker retires camera hooks after two seconds without a camera sample, then requires normal load proof before reinstalling. | The multiplayer flat-mode report is NOT confirmed fixed. Recovery can take time while the normal gate waits. |

## Unfinished or unresolved

- Anatomically correct left/right hands and arms in all titles, including
  support-hand grip, weapon seating, and coherent role-dependent collision.
- Ordinary-campaign acquisition and firing of two weapons in ODST, Reach and
  Halo 4. This build does NOT enable it. Modded/native availability is unchanged.
- Independent dual firing/presentation for ODST, Reach and Halo 4. H2/H3's
  new direction paths also remain unaccepted until tested.
- Restore snap turning: no snap-turn fix was made in this checkpoint.
- Finish physical-melee response/target cases, world-contact refinement and
  all-title weapon/reticle alignment testing.
- Unexpected flat mode across all games: H3 recovery is implemented but
  unconfirmed; audit found an H4 repeated-eye-capture-failure latch that can
  keep the level flat. That H4 path was not changed. Other title transitions
  still need testing. The user's later instruction allowed handedness/dual
  work to proceed without treating this as resolved.
- Existing deferred reports (including H2 AI behavior and Reach effects)
  remain on the retained contact checklist; they were not fixed here.

## Suggested test order

1. With usual settings, briefly check H3 stereo, ordinary melee, single-weapon
   aim and pause/resume to catch a baseline regression.
2. Equip two guns in H2 and H3. Aim them apart, fire each and both together,
   then drop/swap a gun. Repeat H2 in Classic and Anniversary.
3. Enable Left-handed main weapon in F1. Check which physical trigger fires
   each gun and where each hand appears. Report functional routing and visual
   hand/arm problems separately. Release grip after changing handedness.
4. Test slider arrows, including 0.01 values and a melee value above 5. Move
   visual gun offsets and trajectory sliders separately.
5. Recheck ODST Mythic SMG contact, damageable world objects and sustained
   contact. Repeat the multiplayer session transition that previously went flat.

Send the matching log with the game/mission, edition, headset/runtime, setting
changes and result. A passing build or a positive result in one game does not
establish all-title completion. Wait for the user's next instruction after
this ZIP handoff; do not install, launch or publish anything automatically.

Resume details are in docs/ACTIVE-WORK-CHECKPOINT.md and
DUAL-WIELD-REFINEMENT-2026-09-09.md in the matching source ZIP.
