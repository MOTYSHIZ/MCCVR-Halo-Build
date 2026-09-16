# CE controller-directed vehicle control

Status: one optional CE refinement prepared for headset testing on top of the
September 16 build. The accepted pointer is unchanged. Halo 3 behavior being
matched is controller-directed vehicle steering and aiming with independent
headset viewing. Original and Anniversary share CE's native gameplay control
packet; each graphics mode still needs a headset result.

## E-CE-VEHICLE-CONTROL-1: native control roles and vectors

Evidence inputs are the official HCEEK `halo_tag_test.exe`, SHA-256
`FC9E2B6193C6F6D9FF988278B0D39A983747F3FDBDECA7CAFADD28F1F0C53E73`, and
the pinned MCC `halo1.dll`, SHA-256
`0A12DC561780F449D3F4D0DF10BB8D3BC7BE7840A5BEB2B236F672EB6CD42E6C`.
The investigation reads offline copies only.

The existing `E-CE-UNIT-CONTROL-1` proof establishes the 0x50-byte player
packet and native writer: desired facing at packet +0x1c reaches unit +0x204,
desired aiming +0x28 reaches +0x210, and desired looking +0x34 reaches +0x234.
Native throttle +0x0c reaches unit +0x258. The existing exact normal-player
callsite guard continues to select the local player's packet.

Official HCEEK unit update `0x8dc380` forwards those desired vectors according
to native seat-control ownership. At `0x8dc59c` it selects the movement
occupant handle at unit +0x304. The following branch resolves that occupant,
retains native player/animation gates, copies its low six control bits, and
copies all three components of desired facing +0x204 and throttle +0x258
into the controlled unit. At `0x8dc62c`, the independent aiming occupant
handle +0x308 selects whose desired aiming +0x210 becomes the controlled
unit's desired aiming and looking. Its native action mask is 0x7c00. Missing
handles, native control-suppression and the engine's occupant selection are
retained. This is why changing desired aim alone would not steer a driver.

Retail `0xafbe54` is the matched unit update. The corresponding movement
branch begins at `0xafc245`, copies desired facing at `0xafc33d..0xafc378`
and throttle at `0xafc37b..0xafc3ad`. The aiming branch begins at `0xafc3bb`
and copies aim/look at `0xafc485..0xafc4e2`. The native unit suppression bit
0x800000 bypasses both transfers at `0xafc22c`. These exact instruction
sequences and the unique function entry are loaded-image witnesses in
`HALOCE-VEHICLE-CONTRACTS.json`.

Official vehicle physics `0x8e1b60` resolves object mask 2 and the `vehi`
tag, then uses desired facing +0x204 (including its Z component) to construct
the orientation and force response. Thus vehicle-facing preserves the
controller's full pitch; the on-foot horizontal-body projection is unsuitable
for this packet path. Native physics, acceleration, speed and collision stay
under the engine's control. The runtime does not assign driver/gunner roles
or write the vehicle object directly.

## Native seat identity and independent viewing

Official camera-info builder `0x8d53b0` identifies its output with the
`camera_info` assertion from `units.c`. It reads the controlled unit's parent
handle +0xd8 and signed seat index +0x2d0, obtains that parent's unit tag,
and selects the native seat block (count +0x2e4, stride 0x11c). The retail
homologue is `0xb04ee4`: parent read `0xb04f40`, signed seat read
`0xb04f83`, and seat-identity copy `0xb04fa8`. The new adapter reads only
parent and seat identity; it does not index tags or dereference a seat block.

CE object masks are title-native: official biped motion `0x8c5fc0` resolves
mask 1, whereas official vehicle physics `0x8e1b60` resolves mask 2 and its
`vehi` tag. The already verified retail object-try-get service `0xb389a4`
validates the local occupant as a biped and the salted parent as a vehicle.
Parent and nonnegative seat identity are rechecked with the current native
player/input/output mapping immediately before private-packet consumption.

`HALOCE-VEHICLE-CAMERA-2026-09-16.md` separately proves that perspective 1 is
CE's following camera and that this camera obtains direction from independent
player-control angles. The native driver/gunner transfer does not modify
those angles. Consequently the packet adapter can use the coherent existing
native camera/reference frame without feeding its new aim back into the base
camera. First-person seated and custom camera perspectives remain stock.

## Runtime scope and failure isolation

The vehicle path requires the verified vehicle group, current local biped,
salted vehicle parent, nonnegative seat, following-camera perspective 1,
matching generation and tracking/reference/renderer epochs, available
controller/pad data, unblocked native input/look, and no native pause,
cinematic or shared presentation block. It copies the native packet and
replaces only facing, aiming and looking with the tracked primary controller
direction. Native throttle, buttons and all other packet bytes are retained.
The configured primary controller is normally the right controller; the
existing left-handed primary selection remains honored.

Loss of admission or tracking submits the original packet once. Vehicle
proof failure disables only this vehicle branch and is reported outside the
hook. On-foot control, private biped movement basis, camera ownership and
OpenXR stay independent of that failure. No new hook is needed.

## Verification and limits

`python tools/re/test_ce_vehicle_control_native.py` passes 40 native
driver/gunner handoffs with 80 complete native packet writes. Five heading
and pitch combinations cover absent, driver-only, gunner-only and combined
roles, each with and without native control suppression. The real retail
instructions retain the selected role's throttle/action masks, transfer
the matching desired vectors, leave both occupants unchanged, and preserve
native player-control angles byte for byte.

The native test executes the complete packet writer and the named transfer
slice, with the existing biped update notification modeled as a service.
It does not execute complete vehicle physics, collision, network replication
or the rest of the simulation update. The companion camera test executes
four complete perspective calls and 432 packet-to-camera numeric cases.
Production C++ admission/fallback tests and cumulative build results belong
to the candidate record. These local checks are not headset acceptance.

Test a ground vehicle, a gunner seat and a flying vehicle in both Original
and Anniversary; turn the head independently while pointing the controller,
then exit and verify normal on-foot movement/aim. Required Halo 3 regression
coverage remains separate. No game installation or launch was performed.

Preserved inspection records under ignored `out/` include
`ce-vehicle-kit-transfer-final-20260916.txt`,
`ce-vehicle-kit-update-review-20260916.txt`, and the previously recorded
`ce-body-native-afbe54-20260916.txt`. Camera records are named in the
companion camera document.
