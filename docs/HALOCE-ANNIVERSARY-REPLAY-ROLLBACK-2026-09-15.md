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
raster; it cannot prove native callback lock/state cleanup. A callback fault
can therefore survive our frame rejection with native state still owned.
The supplied log has no exception address or stack, so it does not prove
which callback operation failed or that a lock was the precise freeze cause.

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
