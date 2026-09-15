# Latest - September 15: tracked-view construction correction

User tested 6e31b25: 803 captured pairs and faster visible Anniversary switching,
but displaced/noclipped right eye and flat/head-attached left; Original remains
flat. Exact request/log preserved under out/test-runs/6e31b25-ce-partial-failed-20260915.
Prior delivery chat was read. This is NOT acceptance; do not resend 6e31b25.

After separate disable f63254b, a correction now stages each tracked eye BEFORE
native append builds its independent position metadata. Finalization retains
native per-eye fields and applies the primary native clip range to both eyes.
See E-CE-12 and HALOCE-CONSTRUCTION-2026-09-15.md for proof, tests and limits.
Release/eight suites, Reach gate, pinned/generated and mapped-PE checks pass.
The displaced-view cause is not fully isolated; no headset success claimed.

Package the correction without -Install with current notes, verify both ZIPs,
deliver build/source here, then WAIT for headset result/instructions. Read
out/ce-current-handoff.json for exact final identity. No installation, MCC launch,
game-folder modification or PR/publishing. Both editions remain supported.
Accepted 4e01f28 and all existing input/gesture/other-title work remain preserved.
Original stereo, CE independent tracked hands/weapons, controller aim, HUD/reticle,
locomotion and state/vehicle parity remain REQUIRED and unfinished. Physical
melee/world collision still await functional injection confirmation. All standing
and deferred items remain below; do not describe this candidate as full CE VR.

# Latest - September 15: 6e31b25 tested, partial progress but FAILED VR

Recovered prior delivery chat and new user log. Anniversary now captures 803
pairs (805 built, 6 drops) and switches without black VR, but right eye is
displaced/noclipped and left looks flat with head-attached gun. Classic remains
flat by current implementation. Do not redeliver 6e31b25 or call it accepted.
Full test record: out/test-runs/6e31b25-ce-partial-failed-20260915/.
Failed CE rendering disabled in a separate commit before correction; retain
all code, working input/graphics gesture and earlier capture/raster fixes.
Next: verify actual render-consumed cameras and per-eye source identity, then
correct stereo/6DoF. Both CE renderers and independent controller hands/aim,
HUD/crosshair remain required. Prior deferred tasks preserved. Physical melee
and world collision still await functional VR confirmation. Package only a
credible correction with build/source ZIPs; no install/launch/game writes/PR.
Accepted source remains 4e01f28; both MCC editions supported.

# LATEST - September 14 late: CE failure correction candidate

The actual e17a664 headset failure/report was recovered from the previous chat.
After the separate disable commit 736f0c5, CE primary-eye receipt matching now
handles auxiliary culling views, cameras rebuild for the proven eye-source raster,
and logs distinguish rejection reasons and camera positions. Release/eight suites,
Reach and pinned/mapped binding checks pass; headset success remains unproven.
See E-CE-11, HALOCE-REPAIR-2026-09-14.md and ACTIVE-WORK-CHECKPOINT.md. Package
and deliver the correction's build/source ZIPs then wait for its test result;
read out/ce-current-handoff.json for final artifact identity after packaging.
All standing/deferred requirements below are retained. No full CE completion
claim; CE melee/world collision remain deferred until functional VR confirmation.

# September 14 late: first CE candidate FAILED headset testing

e17a664 was delivered and tested. Input/graphics gesture work; Classic is flat
and Anniversary shows black VR plus mismatched stacked desktop views. User
provided a log and explicitly requested fixing stereo/6DoF. Read the NEW top
of ACTIVE-WORK-CHECKPOINT.md and preserved e17a664 failure report before work.
Do not treat the historical ready-to-package instructions below as current.
Failed rendering is disabled separately before correction; retain the code,
working input/gesture, both editions, accepted 4e01f28 and every task below.
CE melee/world collision remain deferred until functional injection confirmation.

# Historical pre-test continuation - September 14, 2026

CE Anniversary now has a connected native two-view stereo/6DoF candidate with
shared OpenXR, owned eye storage, lifetime/raster/pose/recenter guards and eight
passing local suites. Package build/source ZIPs after final checks and wait for
the user's headset testing. See ACTIVE-WORK-CHECKPOINT.md, the September 14
bring-up section, E-CE-10 and HALOCE-CANDIDATE-2026-09-14.md. Not headset accepted.

Retain Classic stereo, CE controller aim/tracked weapons/hands, HUD/crosshair,
native state/vehicle integration, snap turning, head-relative walking and body
following as unfinished. The left-head-side graphics gesture is wired; Classic
is stock flat for now. CE melee/world collision still await injection confirmation.
All existing-title and deferred tasks below remain intact. Both editions,
accepted 4e01f28, package-only delivery, and no launch/publishing remain in force.

# Latest continuation - September 13 evening, 2026

CE owned GPU eye storage is now implemented and tested with actual D3D11 WARP
copies, source recycling/release, frame identity/recovery and resource lifetime
guards. Native hooks/source acquisition and OpenXR admission remain unfinished;
this is not enabled/testable CE VR. Resume from the evening section of
HALOCE-BRINGUP-2026-09-12.md and E-CE-8/9. User reiterated reusing the working
VR baseline across existing titles. Preserve every completed/deferred item
below, both editions and accepted `4e01f28`. No ZIP until credible comparable
6DoF; no installation, launch or publishing.

# Latest user override - September 13, 2026

Continue Halo CE VR. Do not package a ZIP until the implementation is reasonably
expected to function with 6DoF comparable to the other games. This supersedes
any earlier suggestion to package an injection-only or research milestone.
No installation, MCC launch, game-folder writes or publishing. Physical melee
and world collision remain deferred until the user confirms CE injection.

Later September 13 continuation implemented/tested the CE loaded-image binding
adapter and explicit preparation receipt logic. Runtime hooks/GPU capture and
OpenXR admission remain unwired, so this is still not a testable CE VR package.
Use the latest section of HALOCE-BRINGUP-2026-09-12.md and E-CE-7 for the exact
handoff; retain every completed and deferred item below. Accepted pointer stays
4e01f28 and packaging stays held for credible comparable 6DoF.

# Latest override — September 12 Halo CE Anniversary priority

Package the first credible CE test implementation promptly, per latest user
instruction; do not wait for full parity or the deferred refinements. Include
matching source and clear limitations. No install, MCC launch or GitHub writes.

CE staging: graphics toggle matches H2 (left hand at left side of head + movement
stick click). After user confirms VR injection, implement true physical melee
and world collision; do not claim or implement those as part of initial injection.
Existing CE VR mod may inform design, but its bindings are not MCC CE evidence.

User explicitly adds Halo 1 / CE Anniversary VR to current scope and prioritizes
stereo injection, 6DOF, HUD, crosshair and parity with the other titles. Vehicle
controls are manageable as-is: preserve/checkpoint that work and defer refinements.
The all-title zoom task is retained after the new CE priority. Older statements
excluding CE or requiring vehicle+zoom completion before any new scope are
superseded. See ACTIVE-WORK-CHECKPOINT.md and HALOCE-BRINGUP-2026-09-12.md.

# Historical override — September 11 weekly usage pause

User requested checkpoint and STOP until explicit resume. See the latest first
section of VEHICLE-ZOOM-PAUSED-2026-09-11.md. No vehicle/zoom implementation or
new tests exist yet; HEAD is still cfb22ed. Retain the clarified hand-directed
vehicle priority, positive H2 AI feedback, and all-title zoom requirement below.
GitHub release task is cancelled/completed manually by the user.

# Historical override — September 11 vehicle/zoom work resumed

Vehicle clarification: retain the observed main-gun-hand-directed steering/aim
in other games and make H2 follow that same behavior. Pointing the right/main
controller should direct the vehicle; raw-stick/wheel steering is not the
requested replacement default. All-title zoom remains the following priority.

User finished the GitHub release manually and cancelled that task. Resume from
verified cfb22ed plus the preserved investigation/checkpoint docs. Main priorities:
proper H2 Classic/Anniversary vehicle controls, then H3-style zoom boxes in H2
Classic/Anniversary, ODST, Reach and H4. Preserve current H2 AI: user now reports
responsive/attentive AI in both renderers, apparently fixed. Source/history check
is requested, but no AI behavior change absent a demonstrated need. Exact test
identity/log is not supplied and the accepted pointer remains unchanged.

# Historical override — September 11 vehicle/zoom work paused

User explicitly requested "save a checkpoint here and stop" and will say when
to resume. See [VEHICLE-ZOOM-PAUSED-2026-09-11.md](VEHICLE-ZOOM-PAUSED-2026-09-11.md).
The cfb22ed roomscale/left-hand ZIP handoff and separate GitHub cleanup are done.
On resume, prioritize H2 Classic/Anniversary vehicle-control parity, then H3-style
weapon-side zoom screens in H2 Classic/Anniversary, ODST, Reach and H4. No new ZIP
until BOTH additions are implemented and checked. Preserve current progress and
the remaining standing scope. Investigation only so far; neither addition is
complete. Older immediate-delivery instructions below have been fulfilled.

# Latest priority - recovered September 10 headset feedback

The roomscale package 644148a was tested: user reports improved collision/melee,
bad left-hand misalignment and roomscale body movement doing nothing, including H3.
Preserve collision/melee improvements and pause that work. Restore left-handed
presentation from the latest user GitHub release (MCCVR-d77c9dd) as default;
gate newer anatomical correction behind default-off Fix Hand Alignment
(Experimental), available only with left-handed mode enabled. Improve experimental
alignment if evidence permits, without blocking the stable fallback. Then diagnose
and fix real physical movement moving the native body across supported titles,
without drift, duplicate movement, height errors or breaking sticks/controller aim.
Read ROOMSCALE-LEFT-HAND-REFINEMENT-REQUEST-2026-09-10.md for the full recovered
user message and packaging requirements. After implementation/checks, deliver a
clean user-facing build ZIP plus matching source ZIP with accurate fresh/update
installation instructions. No install, game writes, launch, PR or publishing.
The older immediate-delivery handoff below is superseded.

Recovered starting point was clean HEAD 644148a; the previous chat only investigated
after receiving feedback. The refinement implementation is recorded below.
Downloaded reference: out/release-reference/moistman42069/ (release metadata and
source ZIP). Log: out/test-runs/644148a-roomscale-left-hand-feedback/user.log
SHA256: 8E1FF4F9567BDCC62F669E04EE7C511AA51364D036270841C5BB682C28FFE75A.
Steam / SteamVR OpenXR 2.17.9 / Oculus-family headset, panel 120 Hz. Exact headset
model not identified by this log. Accepted pointer stays 4e01f28: no cumulative
acceptance of this failed roomscale/alignment package.

## Refinement implementation/package resume point

Latest recovered request above is now implemented locally. Left-hand default
restoration and experimental toggle: d5bed1e; failed roomscale disabled first
in f868203. Corrected roomscale has all-title admission and one VR input merge
per nested native XInput poll. See LEFT-HAND-REFINEMENT-2026-09-10.md and
ROOMSCALE-REFINEMENT-2026-09-10.md for source evidence and validation limits.
Experimental anatomical correction remains unproven; default released placement
is the fallback. Weapon-bound and melee improvements preserved, further melee
work paused. User-friendly package notes are ROOMSCALE-LEFT-HAND-RELEASE-NOTES-2026-09-10.md.
Release build, 3 CTest suites (including actual roomscale transport fixture and
60/90/120 Hz simulation), and Reach gate pass. Packaging repeats checks at its
final committed identity; both ZIPs and hashes go under out/candidates. Deliver
the build and matching source ZIP here, then wait for testing/instructions.
Do not redeliver 644148a as the update. No headset acceptance of these fixes yet.


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
