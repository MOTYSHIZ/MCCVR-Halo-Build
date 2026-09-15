# CE Anniversary manual HUD replay rollback - September 15, 2026

Halo 3 behavior being preserved: optional HUD failure cannot take down a
working VR camera or prevent later frames. CE's native renderer and callback
ownership differ from Halo 3 and require their own evidence.

The user confirms Original injection, hands and native muzzle flashes work in
`884de13`; switching to Anniversary freezes. The supplied Steam / SteamVR
OpenXR 2.17.9 / Oculus-family / 90 Hz log records the graphics switch at
16:05:28.842, the half-height Anniversary cache preparation at 16:05:29.136,
camera heartbeat expiry at 16:05:29.688 and a Present stall at 16:05:30.182.
There are 452 Original pairs and 452 total pairs: no Anniversary pair completed.
The HUD replay ran once, recorded zero successful draws and failure 5,
`unverified-cleanup`, then all rendering counters stopped. The later static
`cache-begin` diagnostic does not establish where the native thread stopped.

Both supplied `e524d21` logs recorded only HUD ownership refusals (failure 11),
while the user confirmed both graphics modes entered VR. `884de13` corrected
that refusal and thus exposed the previously untested native callback replay.

The preserved pinned native trace `out/ce-native-hud-bridge-ultra.txt` shows
callback `halo1+0x740B0` taking a native critical section at `0x7420B`, modifying
the engine's target-kind wrappers and preparing/restoring native state around
the gameplay HUD. The callback is not merely a HUD draw. Our outer structured
exception handler verifies only its output pointer, target stack and numeric
raster; it cannot prove native callback state cleanup. Direct PE unwind
inspection also finds the callback's two cleanup funclets at `0x1723860` and
`0x1723870`, both targeting native guard cleanup `0x49A30`. Therefore the
ordinary call trace does not establish missing native lock cleanup. The supplied
log has no exception address or stack; failure 5 can represent a nonexception
restoration mismatch or an exception. Its precise cause is unproven.

The failed manual replay is disabled at installation as a separate rollback,
before a replacement candidate. The complete adapter remains dormant in source.
Its cold installation message states the HUD fallback and retained camera.
Original startup/hands, Anniversary frozen-eye/material receipts, stereo,
graphics-switch reference continuity and native effects are preserved.

This rollback restores the earlier absence of manually replayed Anniversary
HUD; it does not claim to fix HUD visibility. Later work must prove the complete
native lifetime or use a narrower established HUD boundary before enabling it.
Actual graphics-switch recovery still needs the user's new headset test.
Accepted source `4e01f28` remains unchanged.

## Regression scope

The production runtime fixture preserves the former enabled adapter test:
a callback that changes the native target stack and raises an exception records
failure 5, rejects the affected pair and leaves unknown stack entries alone.
This is an explicit native-service fixture, not execution of the real callback.

The new continuation case exercises shipping installation refusal, leaves that
same failing callback armed, and runs two more complete Anniversary frames.
It verifies that neither callback is entered, both fresh stereo world pairs
reach the real WARP captures, source colors and native target/raster remain
correct, and camera ownership stays armed. The preceding Original startup
interleaving regression remains part of the separate Classic runtime suite.
These checks establish the bounded rollback; headset recovery remains pending.
Focused Release builds and both runtime suites pass. Retained output is
`out/ce-anniversary-replay-rollback-tests.txt`; the PE unwind inspection is
`out/ce-anniversary-unwind-audit.txt`.
