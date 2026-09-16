# CE seated crosshair correction

Halo 3 behavior being matched: native crosshair artwork is presented along the
aiming controller's ray, independently of gameplay HUD framing and head look.
This candidate corrects CE's seated crosshair admission in Original and
Anniversary, preserving the controller vehicle steering in source `115778a`.
Keep your existing `halomccvr.cfg`. Steam and Microsoft Store remain supported.

## Supplied headset result and diagnosis

The user confirms source `115778af3f71f4665fe1ca1eae715884002a7405` steers/aims
with the right hand and reports no other regressions. The vehicle crosshair,
including a Warthog turret, remains centered on the face. The Banshee is a
requested use case, not a separately established test result.

Preserved log: `out/test-runs/115778a-ce-vehicle-crosshair-20260916/user.log`.
SHA-256: `63253416C940583C6C00D17CB863A9DB8907E63CD38B6B242443455171C764E8`.
It identifies Steam, SteamVR/OpenXR 2.17.10, Oculus-family hardware at 90 Hz;
the exact headset model is not named. This run shows Original rendering;
Anniversary HUD draw counters remain zero. No installed DLL hash was read in
this session; the source identity matches the prior delivered package record.

At 03:08:04.440 the runtime enters vehicle mode. Between 03:08:06.120 and
03:08:10.140, crosshair captures stay at 6,958 while context refusals rise
from 2,550 to 2,912. Vehicle control packets continue increasing, with zero
vehicle declines. The compositor reports `quad=0 aim=1 nativeOwned=0`, despite
previously measured/published artwork. At 03:08:12.176, after leaving vehicle
mode, captures resume. The main HUD layout continues drawing with zero stock
fallbacks and zero unavailable native resources. This supports a crosshair
admission defect; it does not prove every HUD element visually correct.

The actual `haloce_hud.cpp` context gate explicitly required `onFoot` and
`nativePreparesFirstPerson`. A normal following vehicle camera satisfies neither.
The refused hook calls native drawing unchanged, leaving its screen-centered
crosshair in the eye image, and revokes ownership of the controller quad.
This agrees with both the reported symptom and log, without assuming missing
capture resources or a failed vehicle-control path.

## Correction and evidence boundaries

The native crosshair boundary remains the already verified `B3FFA0` scope;
see `HALOCE-HUD-EVIDENCE.md`. No new native binding or hook is introduced.
On-foot admission retains its existing checks. Seated capture instead asks
the vehicle control feature for a read-only, current owner check: independently
verified vehicle bindings, local biped, salted vehicle parent, nonnegative
native seat, following perspective, current player/input mapping and matching
generation, renderer, tracking-space and reference. It checks the seat and owner
twice and holds the existing callback lifetime count while resolving native
objects. It neither submits control packets nor writes native data.

The owner proof is the one already used by the working steering adapter,
documented in `HALOCE-VEHICLE-CONTROL-2026-09-16.md` and
`HALOCE-VEHICLE-CAMERA-2026-09-16.md`. Pause, cinematics, blocked input, lost
tracking, unproved/changed seats and stale contexts leave only this optional
crosshair stock. No camera ownership, session, HUD layout or steering logic
changes. Native artwork retains its weapon/HUD key and existing bounded
capture/upload cadence. The shared compositor already places CE's captured
art on the primary controller ray; no shared compositor edit is needed.

Worker telemetry adds `vehicleCaptures` to the existing CE HUD line. The
existing compositor line distinguishes capture from submitted/visible art.
No logging, allocation, locks, I/O, COM or scanning is added to hot hooks.

This presents the requested controller direction, with the existing crosshair
distance and stabilization settings. It does not introduce a native barrel
origin or actual clamped-barrel reticle solver; CE still owns vehicle physics,
aim limits and projectile origins. Custom/first-person seated perspectives
remain native, as in the previous steering candidate.

## Local verification and headset check

The production HUD regression fixture fails with the old on-foot-only gate
and passes with the correction. It exercises immediate Original and delayed
Anniversary receipts, capture plus second-eye suppression, admission failure,
recovery and exit back to on-foot. The existing real D3D WARP coverage-source
tests remain in the same suite. The production vehicle fixture checks the
read-only API, 30 invalid/changing-owner cases and eight cross-context
mismatches, as well as the previous steering packet and movement checks.

The cumulative Release build and all 35 CTest suites pass, as do the Reach
consistency gate, generated-contract ledger check and production binding
verifier against the pinned offline `halo1.dll`. Final committed package
build/test and exact archive results belong in
`out/ce-vehicle-crosshair-current-handoff.json` after verification. Local
fixtures do not establish new headset acceptance. The cumulative accepted
pointer stays d47a98c; the successful 115778a steering feedback is scoped.

For headset testing, aim a Warthog turret and Banshee with the right controller,
then hold the controller steady while looking around. Check that the native
crosshair moves with hand aim and no centered duplicate remains. Check both
graphics modes, then exit the vehicle and verify the on-foot crosshair.
The new seated visual result, broader seat/custom-vehicle coverage, Store and
long-session behavior remain to be confirmed. No general HUD issue has been
established by this report.

Deliver matching build/source ZIPs only after the correction and verification,
then wait for testing. No installation, game launch, game-folder writes, PR or
publication. The existing CE Multiplayer content requirement for Cursed Halo
and all earlier scoped limitations remain in force.
