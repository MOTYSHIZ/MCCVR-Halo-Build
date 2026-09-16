# CE head/body direction and native grenade aim

Later September 16 vehicle refinement: the on-foot path documented below is
preserved. A separately verified seated branch now supplies controller-directed
facing/aiming/looking; see `HALOCE-VEHICLE-CONTROL-2026-09-16.md`. References
below to vehicles remaining stock describe this original on-foot transaction,
not the new vehicle branch.

Status: locally implemented and verified; no headset acceptance. This is one
independent optional feature within the user-authorized cumulative CE refinement
candidate. Accepted source remains `d47a98c`.

Halo 3 behavior being matched: physical head direction drives the player's
on-foot body/look orientation, controller direction drives aiming, and turning
the head does not apply movement yaw twice. CE uses its own native unit-control
packet and grenade consumer; no Halo 3 offsets or structures are reused.

## E-CE-UNIT-CONTROL-1: native packet and consumers

Inputs are the pinned HCEEK `halo_tag_test.exe`, SHA-256
`FC9E2B6193C6F6D9FF988278B0D39A983747F3FDBDECA7CAFADD28F1F0C53E73`, and
MCC `halo1.dll`, SHA-256
`0A12DC561780F449D3F4D0DF10BB8D3BC7BE7840A5BEB2B236F672EB6CD42E6C`.
They are read offline; this investigation never opens a game process or writes
an installed file.

HCEEK `0x8cfc50` initializes exactly `0x50` bytes of `control_data`;
`0x8cfcd0` contains the `units.c` field assertions. The recorded-animation
readers at `0x513b30`, `0x513be0` and `0x513c90` identify facing, aiming and
looking independently. HCEEK `0x8cf9d0` and its retail homologue `0xafe098`
copy the following fields into the unit without modifying the source packet:

| Packet | Meaning | Retail unit destination |
| --- | --- | --- |
| `+0x02` | native action flags | `+0x1d8` |
| `+0x0c` | throttle, forward/left/vertical | `+0x258` |
| `+0x18` | native scalar (preserved) | `+0x264` |
| `+0x1c` | desired facing | `+0x204` |
| `+0x28` | desired aiming | `+0x210` |
| `+0x34` | desired looking | `+0x234` |
| `+0x40` | native aim-assist targeting data | `+0x1e0` |

The hook is called only from the normal player update `0xad0720`, at the
verified return site `0xad0d5b`. The alternative native calls, including the
adjacent blocked-input branch, remain unchanged. The full packet is copied
privately. Native actions, weapon/grenade choices, scalar fields, animation,
aiming speed, zoom, client-update identity and aim-assist bytes are preserved.
HCEEK validates throttle **magnitude <= 3**, not per-axis bounds of one. The
body adapter leaves all throttle bytes intact. Movement uses the separate
native consumer described below; desired facing is not its movement basis.

HCEEK `0x8db7f0` creates the held grenade from the unit's current aiming vector
at `+0x21c`. Retail `0xb0a268` matches that operation, including the native
`left hand` marker and `throw` effect. Retail unit update `0xafbe54` consumes
desired aiming at `+0x210`; its zero-rate branch `0xafce44..0xafce6c` copies it
to current aiming `+0x21c`. Other branches retain the native aiming-rate and
animation interpolation. Actual retail grenade release `0xb0b204` supplies
that same `unit+0x21c` to the native throw-vector service at call `0xb0b397`.
The adapter changes the desired vector only: native grenade timing, counts,
marker origin, interpolation, random cone, inherited velocity, collision and
damage remain native. This is controller-directed native throwing, not a new
physical velocity-based throwing gesture.

Exact unique hook-entry signatures, packet copy witnesses and the normal
player-call edge are in `HALOCE-UNIT-CONTROL-CONTRACTS.json`, integrated into
the evidence manifest and generated loaded-image checks. Zero/multiple matches
leave only this feature stock.

## E-CE-MOVEMENT-BASIS-1: independent private movement basis

HCEEK biped-motion builder `0x8c5fc0` copies current body forward from
`unit+0x30` to the private movement record at `+0x14`. It selects current aim
from `unit+0x21c` through `0x8d5300` for record `+0x20` in the normal tag branch.
The motion consumer `0x8c82e0` uses current body forward for normal ground/air
motion and current aim in steep/custom motion branches. Therefore neither
desired head-facing nor desired controller aim can be assumed to be the
movement basis; native interpolation can make both differ from current state.

The retail homologue is `0xbb5d88`. Its two biped producers call at `0xbb2c38`
and `0xbb40ef`, with return sites `0xbb2c3d` and `0xbb40f4`. The verified
private record contains local unit at `+0`, body forward at `+0x14`, current
aim at `+0x20` and native speed-scaled forward/left/vertical motion at `+0x3c`.
The native formulas explicitly use
`worldX = forwardX * throttleX - forwardY * throttleY` and
`worldY = forwardY * throttleX + forwardX * throttleY`: positive throttle Y
is left. This is CE-native evidence, not a copied axis convention.

A second hook admits only these biped callers and the same current local
on-foot owner. Around the original native motion consumer, it replaces just
the private record's two orientation inputs with the original native camera's
horizontal forward and full forward. The native packet's throttle has already
passed the shipped head-relative XInput map. This supplies the matching basis
without rotating the input again and without depending on native body/aim
interpolation. The inputs are restored afterward, including on a native
exception. Native output velocity, slope response, acceleration and collision
results are retained. No live unit vector is overwritten by this second hook.

The two hooks form one optional transaction: body/controller direction is not
admitted unless the matching movement-consumer hook is also installed. Both
must retire safely before their module and trampolines are released. Unique
consumer/producer signatures and call/layout witnesses are recorded in
`HALOCE-MOVEMENT-BASIS-CONTRACTS.json`.

## No camera feedback

The physical heading must not turn the next base camera and then get applied
again by stereo tracking. HCEEK first-person camera `0x5041c0` obtains the
player-control vector from `0x58f450`, separate from the unit's desired vectors.
Retail first-person camera `0xc5263c` calls its homologue `0xa999e8` at
`0xc52677`. The reader uses the output user's separate native angles at
`player_control + 0x17c + user*0x38`, then `0xad0304` converts yaw/pitch into
forward. Its optional seat transform `0xb0584c` returns unchanged for an on-foot
unit (seat `-1`). The subsequent camera builder `0xc523c4` copies that supplied
forward to its camera output; parentless on-foot units retain it.

Neither hook writes those native angle records or mutates the
render context. The offline native test varies unit facing, looking and aiming
independently and runs the actual angle reader/converter/on-foot seat transform
for all four output users. Camera direction stays at each user's original
angles. This proves the native basis distinction; headset feel remains a test.

## Admission and failure isolation

The hook requires the exact current CE generation, matching local unit/player,
on-foot native first person, unblocked look/input, no pause/cinematic or VR
presentation overlay, and one coherent tracking/reference/renderer context.
It rechecks ownership and reference epochs before native consumption. Stale,
foreign, vehicle, blocked and malformed inputs use the original packet once.
Loss of controller tracking retains native aim while valid head/body control
continues. The native unit flag `0x100` remains a conservative stock fallback.

No hook logs, allocates, locks or scans. Counters are reported by the worker.
Each hook's SEH `finally` releases callback ownership even when native work
throws, and preserves the native exception. Partial installation immediately
retires the exact created hook; a failed disable/quiescence/removal keeps its
module and trampoline until a later successful cleanup. Rejection is scoped
to module base and generation. A generation replacement restores admission in
the same worker poll. Module-retention fallback and pending cleanup emit
worker-only diagnostics once per unchanged failure episode. None of these
paths disarms camera ownership or OpenXR.

## Verification and limits

- `halomccvr_ce_unit_control_runtime_tests` executes the production adapter and
  hook bodies: head/aim separation, byte preservation, left/right and diagonal
  movement, composition with the production XInput head-relative map and native
  movement basis, stale
  and blocked ownership, private-packet ABI, native exception unwind, real
  Windows unwind metadata for both hooks, temporary movement-input restoration,
  preserved native movement outputs, two-hook partial-install cleanup and
  generation recovery.
- `python tools/re/test_ce_unit_control_native.py` passes **16 full native
packet writes**, **16 desired/current-aim and grenade-release argument
  handoffs**, and **64 independent native camera headings** across four users.
  It also passes **20 native movement-consumer cases** for forward, left and
  diagonal input over five headings, preserving the input basis while native
  velocity output matches the expected CE sign convention. These execute the
  `0x20`-flag velocity-formation branch and stop before collision processing.
  The CRT sin/cos and biped update notification are modeled services. The
  aiming and grenade portions execute the named instruction slices, not the
  complete native animation or projectile simulation.
- Full cumulative build, CTest and pinned binding validation are recorded in
  the candidate notes. Local results do not demonstrate body animation,
  grenade travel, custom-map behavior or headset acceptance. Test both CE
  graphics modes, turning while moving diagonally, up/down controller throws,
  tracking recovery and vehicles/cinematics remaining native. Halo 3 regression
  remains required for the cumulative candidate.

Preserved supporting extraction files under ignored `out/` include
`ce-orientation-control-render-kit-20260916.txt`,
`ce-unit-vectors-camera-kit-20260916.txt`,
`ce-player-unit-control-kit-20260916.txt`,
`ce-grenade-unit-kit-20260916.txt`,
`ce-body-motion-camera-kit-20260916.txt`,
`ce-unit-control-retail-disasm-20260916.txt`,
`ce-body-native-afbe54-20260916.txt`,
`ce-body-native-camera-c52677-20260916.txt`,
`ce-unit-camera-build-retail-20260916.txt`,
`ce-unit-grenade-retail-20260916.txt`, and
`ce-unit-grenade-release-retail-20260916.txt`.
The independent movement review is preserved in
`ce-motion-consumer-retail-review-20260916.txt` and
`ce-motion-producers-retail-review-20260916.txt`.
