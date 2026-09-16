# CE controller-directed vehicle candidate

This candidate adds Halo 1 vehicle steering/aiming to the delivered and
published 8d96381 build, for both Original and Anniversary graphics. It keeps
the existing shared configuration and support for Steam and Microsoft Store.
Keep your current `halomccvr.cfg` when updating.

Halo 3 behavior being matched: the aiming controller names the vehicle's
steering/aim direction, the movement stick retains native throttle, and head
look remains independent. Native vehicle physics, seat restrictions and fire
controls remain owned by CE. The ordinary right-hand configuration uses the
right controller; existing handedness settings continue to select the aiming
hand.

The supplied 8d96381 report is preserved in
`out/test-runs/8d96381-ce-vehicle-report-20260916/`. It identifies Steam,
SteamVR/OpenXR 2.17.10 and an Oculus-family headset; the exact headset model is
not logged. The log records entry into vehicle mode, but the earlier CE
tracked-control adapters deliberately admit only on-foot users.

## Implementation and local evidence

CE's normal local-player control packet now supplies the tracked aiming-hand
direction to native facing, aiming and looking while seated in a verified
vehicle with its normal following camera. The engine forwards facing to its
selected driver role and aiming to its selected gunner role. It still owns
vehicle physics, seat limits, throttle and every button. First-person seated
and custom camera perspectives retain their original controls.

On-foot controls and biped movement keep their existing admission. A separate
vehicle binding check runs before hooks install; a mismatch leaves only the
new vehicle branch stock. Changed parent/seat, stale tracking, pause, cutscenes,
blocked input and lost controller tracking also retain the native packet.
The worker log reports `vehicleReady`, `vehicles` and `vehicleDeclined`.

Evidence: `HALOCE-VEHICLE-CONTROL-2026-09-16.md` and
`HALOCE-VEHICLE-CAMERA-2026-09-16.md`. Offline native checks pass 40 driver/gunner
handoffs with 80 complete packet writes, four native perspective calls and
432 packet-to-camera cases. They preserve independent native camera angles.
The production C++ fixture passes directional/byte-preservation checks, 32
vehicle admission/transition rejection cases, vehicle-proof failure isolation,
vehicle exit and both unchanged on-foot motion-consumer paths.

The cumulative Release build and all 35 CTest suites pass, along with the
Reach consistency gate. The generated native contracts match the ledger;
production loaded-image verification passes all 24 groups, including the new
vehicle group. Final package identity and archive hashes are recorded in
`out/ce-vehicle-current-handoff.json` after packaging and verification.

## Headset check

- Drive a Warthog or Ghost using the movement stick and point the right
  controller left/right. Hold it still and check that steering settles.
- Look around with your head while holding the controller steady.
- Check up/down aiming in a Banshee and a mounted gun, including native limits.
- Switch Original/Anniversary, then exit the vehicle and check the existing
  on-foot movement, turning and weapon aim.

Local build/native checks are supporting evidence, not headset acceptance.
The cumulative accepted source remains d47a98c. Broader custom-vehicle,
passenger-seat, multiplayer, Store and long-session coverage remains unproven.
The prior CE Multiplayer-content requirement for Cursed Halo remains in
effect; see `HALOCE-CURSED-LOADING-2026-09-16.md`.

Package only without `-Install`; deliver matching build and source ZIPs, then
wait for the user's test. No installation, game launch or new publication.
