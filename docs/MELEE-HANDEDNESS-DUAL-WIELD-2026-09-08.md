# Next candidate: melee, handedness, dual wield and transition recovery

**Latest September 9 interruption:** user explicitly requested a WIP build ZIP
and matching source due to usage limits. Package this checkpoint, list all
limits, then wait. See ACTIVE-WORK-CHECKPOINT.md and the updated candidate notes.
New H3 independent firing and H2/H3/ODST support-grip exclusion are implemented
and locally tested, still headset-unaccepted. Anatomical left-hand meshes,
ordinary ODST/Reach/H4 dual wield, snap-turn restoration and complete flat-mode
recovery remain unfinished. This supersedes the packaging hold below only for
this explicit WIP handoff; it does not advance the accepted pointer.

**September 9 user delivery constraint:** continue implementation; do not run
packaging or create another ZIP until left-hand support, all-title dual wield,
physical melee and world-contact smoothing/refinement are implemented. The
`11eb89e` test milestone was delivered before this instruction and remains
unaccepted. Its partial scope must not become the next stopping point. Local
edits, evidence analysis, builds/tests and commits remain authorized; no game
installation, launch or GitHub publishing is authorized.

Additional September 9 instructions: raise the physical-melee slider ceiling
to 10 m/s, retaining default 5; investigate/fix exclusion of damageable world
objects (Warthog, shield-generator examples); add arrows to both sides of every
VR-menu slider. The user clarified each click means one unit of the last
displayed digit (0.01 for two decimals, 0.001 for three, 1 for integer values).
These join the full pending scope and do not authorize an intermediate ZIP.

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

- The old H2 Halo2WeaponAimHelperDetour is dormant; its target is not installed.
  The live native unit-aim path supplies primary aim. E-H2-37 proves a native
  firing boundary suitable for independently identifying the secondary:
  H2EK 49C960 -> 47DC20; retail 8E4940 -> 8F0F70. Inspect weapon ownership at
  this caller before adding independent direction selection.
- Supplied ODST log has successful submissions from both hands, no contact
  fault, and repeated missing authored weapon bounds in the modded run. It
  does not identify the exact missed SMG strike or prove its cause.
- Remaining native unarmed/secondary damage-response limitations are not
  disproved by the accepted ordinary melee result. Preserve working behavior
  while investigating them.

This document records scope and observed code, not completion or new acceptance.

## Multiplayer feedback and added scope (September 8)

User resumed the paused work and reports true physical melee works well in
multiplayer with a friend. Both supplied logs identify source 4e01f28 and Steam.
Preserved under out/test-runs/4e01f28-multiplayer-feedback:

- user.log (Downloads/HaloMCCVR (3).log), SHA-256
  B4A393E49D143BF9F5F58DA72B739A051EA86FED90F714598D7A66A915B2C9AF;
  SteamVR/OpenXR 2.17.8, Oculus-family, panel 120 Hz.
- friend.log (Downloads/HaloMCCVR (4).log), SHA-256
  EAB24EFB7CDDA8D2EF1C9203721F46C7215046F0BC2CC112BDBF93175C0A2134;
  SteamVR/OpenXR in Meta compatibility mode 2.17.8, Oculus-family, panel 72 Hz.

Additional requirements: recover VR after game/level/session transitions;
put trajectory adjustment in Crosshair settings; keep visual gun positioning
independent of reticle/bullet aim, including gun-stock calibration.

At 10:35:34.999 the user loses stereo. No later camera installation, retirement
or load-gate rearm is logged; loading/unsupported (later paused) alternates while
old contact/collision telemetry remains enabled. The friend loses stereo at
07:35:35.448, records a lost mapping and new load-gate observation, then gets a
fresh camera hook and stereo at 07:37:43.679/07:37:44.701. This supports a stuck
old hook epoch in the user's run; it does not prove the exact loader event.

Source-confirmed recovery gap: PlayerViewLivenessGate::Observe returns true
forever after opening, and the Halo 3 worker formerly required title/generation
or gate closure to retire hooks. There was no camera-heartbeat timeout in that
worker. A new 2-second stale/no-first-camera retirement path now resets the
normal gate; it never authorizes timed installation. Core tests replay a frozen
load followed by a real camera tick and exercise the recovery grace interval.
Initial Release build and core tests passed; no new headset acceptance.

Crosshair now exposes the existing independent aim pitch/yaw/roll settings,
retaining their config keys for compatibility. Visual hand offsets no longer
feed the two-handed aiming line. New left-handed pose/input/haptic routing is
implemented but requires further presentation review and validation. Anatomical
mesh mirroring is not established by role routing alone. Ordinary-campaign
dual acquisition in ODST/Reach/H4 is still unfinished; do not advertise it.

## September 9 continuation: reviewed source and exact remaining work

The user resumed again and requires continuity through interruptions. This
section supersedes the old paused checkpoint's implementation-status claims.
The requested final scope above is unchanged; nothing is newly accepted.

Implemented in the worktree:

- H2's optional full-weapon firing scope identifies both equipped weapon roles
  and supplies independent controller directions only while both owned weapons
  are verified. The single-weapon native aim path remains the baseline.
- Review caught a repeat of H2 C-H2-60's disproven frame-order constraint: the
  new scope compared the immutable observer camera publication's serial with a
  separately read latest VR sample. It now keeps the observer's camera and
  controller together, uses the current sample only for matching tracking epoch
  and a bounded 100 ms age, and rejects stale title/tracking generations.
  Tests cover adjacent-frame ordering, stale time and tracking reset.
- The H2 hook transaction creates both hooks before enabling either and attempts
  immediate quiescent rollback on partial failure. Stock fallback stays local
  to dual aim. The telemetry now correctly describes both independent rays.
- ODST's previously documented slot-1 source graph now has its own authored
  weapon-bound cache, world-contact publication and contact-melee shape. A
  recent secondary palette owns the support-hand contact samples, preventing
  alternating primary/secondary palettes from reseeding its swing each time.
  This is not proof of the reported Mythic SMG miss's cause or its resolution.
- Left-handed pose, trigger/grip, velocity and haptic routing is implemented;
  physical sticks/buttons and the D-pad's selected physical hand stay physical.
  A change invalidates contact/aim state. The two-hand latch now tracks grip
  edges across invalidation so a held grip cannot become a new toggle on recovery.
  Anatomical left-hand mesh presentation still needs title-by-title review.
- Crosshair settings expose trajectory pitch/yaw/roll. Visual support-hand
  offsets no longer affect the two-controller aim line or grab location.
- H3 has worker-side stale-camera retirement with named logging and renewed
  normal load-gate proof before reinstallation. It does not install on a timer.

Validation completed: Release build, core tests and Reach consistency gate
after the grip-edge edit (`out/resume-handedness-final-build.txt` and
`out/resume-handedness-reach-gate.txt`). The pinned H2 module identity, unique
fire/helper entries and call edge pass the offline verifier in
`out/dual-native-inspection-verified.txt`. HEAD descends from accepted `4e01f28`;
the accepted pointer and game folders have not changed. That milestone was
packaged before the later September 9 instruction. Do not package again until
the full requested scope is implemented, as required at the top of this file.
Plain-language test/limitation notes: `MELEE-HANDEDNESS-CANDIDATE-2026-09-09.md`.

Evidence work retained, not runtime implementation:

- `out/dual-h4-eligibility-retail.txt` confirms the H4 kit `EA0CA0` predicate's
  homolog at retail `610774`: the weapon/unit tag lookup, unit bit 23 at `1D4`,
  and weapon bits 17/18 at `2A0` agree with the independently inspected H4 kit.
- Reach kit `DEBC40` has its previously verified retail homolog `4BCDB4`
  (`out/dual-reach-eligibility-retail.txt`), with Reach-specific fields.
  These predicates alone do not prove acquisition, presentation and firing.
- H3/ODST existing firing-data signatures are unique at retail `3524B0` and
  `396B7C`. Both accept unit, origin, direction, marker, offset and two boolean
  arguments; neither accepts the firing weapon. Therefore independent selection
  needs a verified outer weapon scope, not a guessed extra argument.
  Decompiles: `out/dual-{h3,odst}-firing-data-retail.txt`; caller matching is in
  `out/dual-{h3,odst}-fire-match.txt`. The corresponding large kit barrel fire
  decompiles were already saved as `out/dual-{h3,odst,reach,h4}-fire-kit.txt`.

Still required before claiming the whole request complete: ordinary-campaign
dual acquisition in ODST/Reach/H4; complete independent direction/presentation
for H3/ODST/Reach/H4; anatomical handedness review; damage-selection limitations
from the accepted report; exact final build/tests/gate, unique committed source,
build ZIP and matching source ZIP. Preserve the multiplayer transition report,
the original accepted melee behavior and all title/edition coverage. Delivery
remains package-only, followed by the user's headset testing.

## September 9 resumed audit after the slider instruction

Recovered HEAD `11eb89e` and preserved all unfinished worktree edits. The branch
descends from accepted `4e01f28`. No newer headset acceptance was recovered.

- All menu sliders now use a common left/right arrow control. Each click steps
  the last displayed digit; integer and displayed percentage values step by
  one. Bounds, disabled controls, label identities and slider dragging remain
  supported. The native ImGui input test exercises clicks, independent widget
  identity, endpoints and dragging without launching MCC.
- The melee threshold range is 0.30–10.00 m/s throughout configuration, UI,
  gesture routing and all five native contact adapters. Default stays 5.00;
  configuration tests cover a saved value of 9 and the upper clamp of 10.
- Pending contact presentation edits extend release smoothing across clear
  queries, bounded to 10 mm and 120 ms. Native collision, damage, pushes and
  haptics continue to use actual contact. Tests exercise cadence and scale
  changes. Runtime/headset improvement is not established.
- Pending damage-target edits remove the explicit biped-only target filter.
  The attacker remains a verified local biped; targets retain full-handle
  validation. Native damageability and authored responses remain authoritative.
  Review of constructor/consumer evidence is still in progress; a build does
  not establish Warthog or shield-generator damage.

Validation of the recovered worktree: cumulative Release build, both CTest
targets and Reach consistency gate pass. Logs are
`out/continuation-2026-09-09-build.txt` and
`out/continuation-2026-09-09-reach-gate.txt`. These are development checks, not a
candidate identity. No ZIP, installation, launch, publish or pointer advance.

Continue native per-weapon firing/ownership, ordinary-campaign acquisition,
anatomical handedness and melee response selection. Keep the original
`CONTACT-PASS-REQUEST-CHECKLIST.md` as the older requirements ledger rather than
silently treating its unresolved items as accepted. Do not mistake this audit
for completion or another request to pause.
