# CE camera comfort and spatial audio — September 16, 2026

Halo 3 behavior being matched: physical head rotation determines what the
player sees and hears; authored recoil/shake must not move the headset view.
These CE-only optional integrations do not own stereo, controls or OpenXR.
Accepted runtime remains d47a98c. No headset result is implied by this work.

## E-CE-ORIENTATION-1: native camera effects

HCEEK `halo_tag_test.exe` VA `0052D4F0` identifies `effects/player_effects.c`
through its `matrix` and local-user assertions. It updates per-user 52-byte
effect matrices, advances random shake, timers and cached interpolation state,
then returns a matrix. Preserved decompilation is
`out/ce-camera-shake-kit-20260916.txt`. This is distinct from the nearby
screen-flash function at `0052DFE0` and is not a screen-flash suppression hook.

The pinned MCC `halo1.dll` SHA-256 is
`0A12DC561780F449D3F4D0DF10BB8D3BC7BE7840A5BEB2B236F672EB6CD42E6C`.
Its homolog is RVA `0xBAC0CC`. Native observer-to-camera builder `0xAC450C`
calls it at `0xAC4618`, then composes its private returned matrix with the
observer basis and copies the result into both render and raster cameras.
The native identity is 13 floats: scale, forward, left, up, translation.

The optional detour always calls the original generator once. Only output
user zero, exact return address `0xAC461D`, current CE generation and a current
admitted on-foot gameplay/tracking receipt permit replacing the returned
matrix with identity. Native effect state still advances, so returning to
stock does not freeze an effect. Other callers/users, menus, vehicles,
cinematics, stale tracking and retirement retain original behavior. No weapon
damage, recoil spread, first-person animation, haptics or flash state is edited.

`test_ce_camera_effect_native.py` executes the actual pinned camera builder
and its matrix routines for rotational, positional and combined effects.
All six cases pass (1,710 native instructions). An explicit effect-generator
service fixture supplies the transform; its internal random sequence is not
emulated. This proves the output composition and identity cancellation, not
that every reported AR jitter source was that effect or that every custom
weapon is comfortable in the headset.

## E-CE-ORIENTATION-2: native spatial-audio listener

HCEEK VA `008826F0`, preserved in
`out/ce-orientation-player-effects-kit-20260916.txt`, identifies
`sound/sound_manager.c` and asserts its observer camera exists. It constructs
the listener from observer position, forward `+0x20`, up `+0x2C` and velocity
`+0x14`, then dispatches a listener packet through the selected sound backend.

MCC homolog RVA `0xB4CB80` follows the same chain. For each valid output user,
it builds a 52-byte matrix using `0xBA24EC`, converts observer velocity through
`0xBA31B0`, and packs a `0x80`-byte listener record. Packet fields are:

| Offset | Native meaning |
| --- | --- |
| `0x00` | position |
| `0x0C`, `0x18` | forward, up |
| `0x24` | listener-relative velocity |
| `0x30..0x47` | native environment metadata and pointer |
| `0x48` | matrix scale |
| `0x4C`, `0x58`, `0x64` | matrix forward, left (`up × forward`), up |
| `0x70` | matrix position |

The native backend selection table at `0x18988A8` points to `0x1B7B880`;
its callback slot `+0x28` points to entry `0xABCC54`. The producer calls that
callback at `0xB4CEFB`, returning to `0xB4CEFE`. The backend synchronously
consumes the matrix at packet `+0x48` through CE's native bridge `0x77E70`,
converts its origin/directions, and submits the listener to the audio interface
at `0xABCE1B`. It does not retain the packet pointer. The native helpers use
internal volatile-register preservation assumptions, so those common matrix
helpers are deliberately not hooked.

The optional backend-entry detour accepts only that exact caller, listener
zero and a current admitted on-foot gameplay receipt. It copies the complete
packet to bounded stack storage, derives head forward/up using the same
`BuildTrackingFrame` mapping as CE's eyes, and changes the orientation fields
and matrix basis. Position, environment, scale and opaque bytes are retained.
The backend maps packet velocity `(x,y,z)` to `(x,z,-y)` before its matrix
transform. The copy re-expresses that vector in the new basis so the final
native world velocity/Doppler handoff stays unchanged as the head turns.
Native observer and sound-manager globals are untouched. Vehicles, menus,
cinematics and invalid/stale ownership retain the original packet.

`test_ce_audio_listener_native.py` executes the pinned producer, its real
matrix/velocity routines, and the actual backend through the final audio
interface handoff. Four cardinal yaw/pitch/roll cases pass (4,440 native
instructions), including the backend's existing up-axis sign convention.
The production C++ test can emit the exact original/adapted packet pair for
that same native consumer to verify the real integration. The emitted pair
`out/ce-community-audio-production-packet-20260916.bin` also passes: native
listener position and world velocity stay equal while orientation changes.
The combined run executes 6,076 instructions; output is preserved in
`out/ce-community-audio-native-20260916.txt`. Clock, active-user,
liquid and audio-interface services are fixtures; there is no live audio
device or headset in this test. Environment policy after the pose handoff is
outside the emulation fixture and is passed unchanged by production.

## Installation, failure isolation and tests

`HALOCE-ORIENTATION-CONTRACTS.json` supplies independent `camera_effect` and
`audio_listener` runtime groups: unique executable signatures, unwind entries,
caller/body witnesses, relative targets and relocation-aware backend-table
pointers. Each optional group verifies cold before its own hook installation.
Neither group's failure gates or tears down CE's camera core. Failed enables
retain their cleanup receipts; cleanup retries only that feature. Rejection
identity includes module address and generation, and a new generation retries.

Retirement first closes admission, disables hooks, proves detour/trampoline
quiescence and drains callbacks, then removes hooks before releasing the module.
Failed retirement retains resources and reports a one-time cold diagnostic.
Module-pin and partial-install cleanup failures also report cold diagnostics.
Native exceptions propagate while `__finally` releases callback accounting.
Periodic cold telemetry reports suppression, stock fallback, listener updates,
declined adaptations and exceptions. No hot hook logs, scans, allocates or locks.

The production runtime suite covers native effects executing exactly once,
local-only effect replacement, audio yaw/pitch/roll against actual `BuildEye`,
private-packet preservation, world-velocity preservation, invalid and foreign
ownership, exception recovery, real Windows unwind metadata and fault-injected
enable/removal/disable/quiescence/generation transitions. All 52 production
checks pass and the cumulative Release DLL compiles. The broader candidate
record contains final cumulative checks.

Required user checks after delivery: CE Classic and Anniversary head-turn
audio orientation and sustained AR/other-weapon firing comfort, transitions
back to stock/cinematics and recovery, plus the requested Halo 3 regression.
Existing Steam and Store support is retained; neither edition gains new
headset acceptance from offline evidence.
