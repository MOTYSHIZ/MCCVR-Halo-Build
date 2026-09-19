# H2 coherent vehicle view and steering reference (unaccepted)

The target Halo 3 behavior is a stable room-to-world controller target, native
turn-rate/seat constraints, and a view/weapon/reticle that share its frame.
No Halo 3, ODST or Reach control tuning is changed by this correction.

## Defect

H2 built the desired controller ray from the tracked camera, which inherited
the native observer's changing heading. The servo then turned that same native
heading toward the desired ray. Holding a controller off-center could therefore
preserve the error as the native camera turned, rather than converge. An earlier
input-only frozen-reference experiment remains dormant because it would leave
the view and visible hands using another frame.

The observer now retains a reference for the exact local unit, direct parent,
root vehicle, seat, generation and tracking space. View Follow off retains its
entry heading; View Follow on adds only the root chassis heading change. Both
retain the current native camera position. The native observer basis remains
separate, unchanged feedback for the steering servo.

The tracked camera, both hand carriers, contact transform, firing-carrier helper,
and Classic pose rederivation consume that same published reference. Anniversary
continues using its exact matched tracked publication. Missing/invalid ownership
leaves the camera stock for this feature and withholds controller steering;
it does not disarm VR. Vehicle Motion off retains the previous manual-input view.
On-foot snap, movement, camera and controller behavior retain their existing path.

H2's seated left stick also bypasses head-relative walking rotation. Looking
around a cockpit must not rotate a native throttle/strafe command.

## Native evidence

Official H2EK `0042CCB0` names `unit->object.parent_object_index` and
`unit->unit.parent_seat_index` in `sound_manager.cpp`, reading `+14` and `+210`.
Retail `6E49B0` matches its per-user/native camera/parent/seat call graph. The
block at `6E4AD2` reads those members and resolves the parent through `8D7000`.
A unique cold signature proves that block independently of the existing native
seat predicate proof. The new path adds no hook and performs no native writes.

Root vehicle orientation uses H2's own `+70/+7C` forward/up, already used by the
verified local-velocity path in HALO2-WORLD-COLLISION-EVIDENCE.md. Read-only
decompilation of `90A000` confirms these vectors are its local/world basis.
Every object read uses the salted datum resolver and requires the appropriate
biped/vehicle type. The parent walk is bounded and rejects cycles. Records:
`out/h2-vehicle-seats-kit.txt`, `out/reload-policy/h2-vehicle-parent.c`,
`h2-vehicle-basis.c`. No other title's layout is copied.

## Verification

Release and all 41 CTests pass, as does Reach consistency. New production-math
checks compose the actual view and controller carrier while simulated native
feedback turns, using the actual XInput deadzone-floor mapping. A stationary
controller target converges at 60/90/120/144 Hz for either input direction and
either turn direction. Tests cover translation, hull-follow yaw wrap, seamless
follow toggling, salted occupation changes, tracking/generation changes, stale
gaps, invalid cameras and unchanged on-foot publication selection. The parent/
seat witness is unique in the pinned retail image. Logs are under
`out/refinement-20260918/{build,tests,gate}-h2-vehicle.log`.

These are model/source checks, not headset acceptance or real vehicle physics.
Native seat limits, aircraft handling, both graphics modes, enter/exit/checkpoint
transitions and Halo 3 regression still require headset testing. Full H2 vehicle
feature parity (for example a two-grip wheel) is not implied. Packaging remains
held while the rest of the requested refinement list is completed.
