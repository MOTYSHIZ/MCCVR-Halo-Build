# Halo 2 vehicle-control audit

Status: controller-directed native steering/aim is implemented for both H2
Classic and Anniversary, but full current H3/ODST/Reach vehicle-control parity
is not complete or headset-confirmed. This is separate from first-person seats.

Verified in the current source, not inferred from earlier progress text:

- Halo2NativeAimUpdateDetour samples the verified signed parent-seat member
  only for the mapped local player's exact unit. When seated, it preserves the
  stock aim updater instead of overwriting the on-foot aim vectors.
- Halo2Observer6Dof_VehicleControlActive requires the native predicate proof,
  armed gameplay and a seated sample no older than 100 ms.
- Game_ComputeAimStick admits that seated H2 state into
  ComputeHalo2ControllerAimStick. The latter compares the tracked-controller
  sight line with the stock observer yaw/pitch and emits the native right-stick
  commands. It resets servo history on generation changes or inactive gaps.
- input.cpp applies those commands through the same virtual XInput path used
  by the other titles. Both H2 renderers share this simulation/input route.

Limits: current H3/ODST/Reach vehicle support also distinguishes seat and
vehicle steering policies. With hull-follow enabled, supported look-steered
drivers use the raw turn stick or optional two-grip wheel; other seats retain
controller aiming, and Reach has a separate hull-heading feedback path.
The H2 seat-only predicate/observer servo does not reproduce all of those
policies, has no two-grip wheel implementation, and is not evidence of working
first-person seat cameras. Do not call the complete vehicle experience matched.

Required headset coverage remains driving, turrets, enter/exit transitions,
both H2 renderers and Halo 3 regression. Current all-title Release/tests pass,
but the supplied latest playthrough is H4 and proves nothing about H2 driving.

The user's minor tank-exit reticle report is stored separately in
bug-reports/halo2-tank-exit-reticle.md and deferred as requested.
