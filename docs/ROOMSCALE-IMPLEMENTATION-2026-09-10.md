# Roomscale implementation - September 10, 2026

Scope was explicitly narrowed by the user to preserve controller gun aiming for
this package. Implement horizontal native-body follow with head-relative walking;
independent head-following body yaw is deferred. Both MCC editions and H2 renderers
remain supported. This document records source behavior, not headset acceptance.

## Reference behavior and title integration

H3's existing head-relative native walking, positional head view and controller
shot direction are the baseline to preserve. The new optional follow controller
uses those same systems in H3 and each title's existing equivalent. There is no
new native hook, character teleport, velocity write, or copied engine layout.

Roomscale_Camera samples the native camera origin BEFORE tracked translation,
using the same title-owned physical/world basis and metre scale as its existing
head/hand transform. It updates that title's horizontal tracking reference only
when native displacement actually advances an outstanding physical request.
H3/ODST use g_headPosRef; Reach uses the effective non-seat reference and publishes
it for hands; H4 updates its private reference; H2 publishes the reference through
its existing observer packet for both renderers. H2 retains its valid tracked
camera if the optional follow recomposition is refused.

Native XInput LX/LY receives the bounded follow demand only with an idle physical
and VR movement stick. Existing Game_MapMoveStick maps it from head space to
native heading. Native walking therefore owns body collision, ground, steps and
network simulation. Vertical tracking remains ordinary lean/crouch. Physical
movement during manual locomotion does not accrue an extra later walk. Body
follow has a 2 cm deadband and max 0.8 analog magnitude; no speed equality across
weapons/mods/game settings is claimed before headset testing.

## Admission evidence

- H3: existing unique vehicle binding, fresh on-foot publication, native local
  unit getter, full-handle unit reader and H3 seat word; player-controlled camera.
- ODST: existing native camera-thread vehicle sampler publishes positive unseated
  evidence only after a valid unit/seat read, independent of seat-art resolution.
  Existing ODST cinematic reader must report player control.
- Reach: existing HREK-proven native occupation sample, generation-matched OnFoot
  state and fresh player-controlled cinematic publication. Retail is not used to
  discover new behavior; no new Reach binding is introduced.
- H4: existing H4EK-matched FP record and salted native contact object reader;
  unparented local biped (+0x24), same established contact admission, and the
  existing cinematic reader. Optional contact-binding failure disables roomscale
  admission only; toggling physical melee/world contact off does not remove it.
- H2: existing uniquely proven native local-aim/seat observation plus fresh final
  first-person packets. H2 has no installed cinematic classification publisher;
  demanding one would silently block the entire feature. Its scene transitions,
  hidden hands and unarmed cases need explicit headset coverage. This is an
  intentional feature gate difference, not proof of complete cinematic coverage.

Prior evidence: HALO3-VEHICLE-EVIDENCE.md, ODST-VEHICLE-EVIDENCE.md,
REACH-VEHICLE-EVIDENCE.md, HALO2-SIGNATURE-EVIDENCE.md,
CONTACT-RESUME-CHECKPOINT.md and the corresponding existing native readers.

## Isolation and validation

Commands expire in 100 ms and are tagged by title/generation and input epoch.
Atomic command fields use a version check; simultaneous publishers refuse without
waiting. OpenXR must be focused with both position and orientation TRACKED and a
successful pose sample within 100 ms (VALID-only inferred tracking is insufficient).
Invalid/stale tracking, native occupation, menus, pause, D-pad gesture, title
change, teleport, large tracking jump and recenter cancel or re-seed follow demand.
The camera math retains finite-value and bounds checks. Hooks do bounded arithmetic
and atomic publication; Roomscale_Report logs counters on the existing cold worker.
No feature failure disarms the VR camera or session.

Release build and 3 CTest suites passed during development. Core tests cover
physical forward/right requests, observed-travel consumption, blocked movement,
no double translation, three scale/heading combinations, manual priority,
teleports, tracking jumps, new generations, config defaults/save-load, recenter,
stale resumption and nonfinite inputs. Existing snap/handedness/melee/lifecycle
regressions remain covered. Reach consistency and pinned legacy/H4 weapon-bounds
verifiers pass. Packaging repeats build/tests for its committed source.

Required acceptance: all titles and both H2 renderers in the headset, including
Halo 3 regression and both editions as available. Native analog response, camera
bob, stepping, walls, physical turns, simultaneous stick use, death/respawn,
vehicle/turret and cinematic transitions, controller aiming and hand/melee
coherence remain runtime questions. CURRENT-STATE.md is not advanced.

Packaging gate correction: C-H2-77 formerly rejected ANY address reference to the
dormant NativeHudAnchorBasisDetour. The preserved lifecycle work references it
only in the retirement/quiescence range list; no installation exists. The gate
now rejects passing that detour to MH_CreateHook, retaining the original rejected
hook prohibition while allowing safe teardown bookkeeping. No HUD behavior was
changed to satisfy the gate.
