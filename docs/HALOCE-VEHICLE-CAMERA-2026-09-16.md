# CE vehicle following-camera direction

Status: offline native evidence for the September 16 controller vehicle
refinement. This is not a headset result and does not advance the accepted
pointer. Halo 3 parity means a controller-directed vehicle heading while the
headset retains independent viewing. CE may use its own native control packet
to achieve that behavior.

## E-CE-VEHICLE-CAMERA-1: separate camera angle source

The official HCEEK `halo_tag_test.exe` SHA-256 is
`FC9E2B6193C6F6D9FF988278B0D39A983747F3FDBDECA7CAFADD28F1F0C53E73`.
The matched pinned retail `halo1.dll` SHA-256 is
`0A12DC561780F449D3F4D0DF10BB8D3BC7BE7840A5BEB2B236F672EB6CD42E6C`.
All inspection and execution used local offline copies.

HCEEK `0x504aa0` identifies itself through `following_camera.c` assertions.
Its forward calculation calls `0x58f350` for the output user's raw desired
angles. That helper reads `player_control + 0x17c + user*0x38`; pitch is the
next float at `+0x180`. The camera adds its own free-look offsets `+0x10` and
`+0x14`, clamps pitch to +/-90 degrees, then calls `0x7bcb00` to form
`result->forward`. Its unit/seat helper `0x58f6b0` supplies camera information
and pivot/track selection; unit desired/current aim and body forward are not
inputs to this direction calculation.

The retail homologue is `0xc52ca4`. The full following-camera body retains
the corresponding camera-info, free-look, track-position and up-vector shape.
At `0xc52e94..0xc52eb3`, it reads the output user's two angles from the global
pointer at `0x2d8fe70` plus `0x17c + user*0x38`. The next instructions add
camera free-look and clamp pitch. `0xc52edb..0xc52f2b` computes
`(cos(yaw)*cos(pitch), sin(yaw)*cos(pitch), sin(pitch))` into result `+0x24`.
It does not substitute a unit or vehicle direction afterward.

The native perspective reader `0xb14ea4` compares the active director callback
with `0xc52ca4` at `0xb14ee4`. The matched branch returns **1** at `0xb14ef0`
and caches that value at director `+0x5c`. Therefore perspective 1 is a proven
following-camera identity. Other native perspectives are outside this proof.

`HALOCE-VEHICLE-CAMERA-CONTRACTS.json` records the unique following-camera
entry signature, the angle-read and forward-construction witnesses, the
player-control global operand, and the perspective-1 callback comparison.
These are read-only identity checks; no camera hook is required by this proof.

## Native execution

Run `python tools/re/test_ce_vehicle_camera_native.py`. The test verifies the
retail SHA, uses the existing isolated CE Unicorn fixture, and passes:

- Four complete native perspective calls, one for every output user, proving
  that the following-camera callback returns and caches perspective 1.
- 432 cases executing the complete native unit-control packet writer followed
  by the actual following-camera direction instructions. Cases cover all users,
  yaw wrap boundaries, positive/negative pitch, native pitch clamps and camera
  free-look offsets. Desired facing/aiming/looking and current unit/vehicle
  directions vary independently of native camera angles.
- Expected numeric camera forward in every case, byte-identical native
  player-control angles and unit/vehicle records across the camera slice, and
  no unit/vehicle memory reads by that direction slice.

The separate vehicle-control evidence must establish that the native steering
consumer uses the adapted packet and does not rewrite those native camera
angles. The camera proof alone is not a complete vehicle-control proof.

CRT trigonometry and the biped update notification are modeled services.
The executed camera slice excludes pivot/track positioning, collision,
interpolation and earlier player-control updates. The independently rotated
vehicle object is a sentinel for detecting accidental direction dependencies;
this test does not emulate vehicle physics or infer a new vehicle layout.
First-person seated cameras and custom perspectives are not covered. Original
and Anniversary headset vehicle tests, plus the required Halo 3 regression,
remain necessary before claiming player-experience parity.

Ignored inspection records are
`out/ce-vehicle-camera-kit-reference-20260916.txt` and
`out/ce-vehicle-following-retail-reference-20260916.txt`.
