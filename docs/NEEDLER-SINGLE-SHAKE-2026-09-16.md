# Single-shake Needler reload correction

## User result and exact evidence

The user confirms cursor and surrounding behavior work on source
205ff20da3611fe231acc042be37063c5a3f5d67, but the optional Needler shake had no
effect. Requested scope: fix only shake to accept one rapid shake of the gun
hand without any grip requirement, package both archives and wait.

Log preserved at out/test-runs/205ff20-needler-shake-failure/user.log; SHA-256
D7456D340BAA2207715B7CBA80E0C1232CBB2A9B4351AF9CCE4A211E05FF2309.
The source matches the prior verified package. Edition Steam; runtime
SteamVR/OpenXR 2.17.10; headset reported as SteamVR/OpenXR : oculus, 90 Hz.
The exact headset model is not recorded. No installed DLL hash is available
from this pasted log, so no installed-byte claim is made. No deployment-backup
log contains this source or needleShake: this line was delivered package-only.

At 16:27:02.052 needleShake becomes 1; at 16:27:47.424 CE identity
55EA2D6F6C10C375 is explicitly recognized as needler with readiness=ready,
reload=1 and needleShake=1. Reload requests remain at 1 (the earlier magazine
reload), including later recognized Needler samples through 16:29:12.802.
Thus the feature was enabled and recognized during ready samples. The log
has no per-frame hand trajectory/grip trace and cannot establish those inputs.

Source proves the old trigger required armedP_ AND heldP_, four vertical
strokes, each at least 60 ms, within 1800 ms, and grip release to repeat. This
contradicts the requested interaction. It is disabled in separate commit
ebfc2e3 before correction; old code remains dormant.

## Behavior and scope

Halo 3 reference: request the configured native reload input; Halo retains
ammo, inventory, animation and automatic reload rules. The same shared VR
interaction uses each engine's already verified needle identity. No engine
hook, address, identity reader, camera or renderer changes.

SingleNeedleShake reads only head-relative primary-hand position. One rapid
outward stroke and return of the slider distance (6-20 cm, unchanged default
10 cm) suffice in any direction. Each detected leg must complete within 300 ms,
last at least 20 ms and average at least 0.5 m/s. These are recognition bounds,
not a required wait or a player-visible countdown. After completion it waits
for 150 ms of settled motion (at most 0.2 m/s) before rearming. The existing
500 ms action cooldown and 120 ms native input pulse are retained.

It does not require/consume grip, release support aim or acquire a body zone.
Pouch/holster transactions have priority; normal grips retain their behavior.
Manual Reload remains the parent option; shake stays off by default and
retains the saved distance. Recognized Needlers in all six games and Reach's
Needle Rifle qualify. Unknown mod models retain existing behavior, without
inferring ammunition type. Both editions use the same source and input path.

Existing readiness, title/generation/space/weapon, single-weapon, pose and
finite-value guards remain. The detector resets on blocked input, transitions,
configuration changes, stale/nonmonotonic time or tracking discontinuities;
steps over 25 cm or 8 m/s and sampling gaps over 100 ms are rejected. Whole-body
translation, off-hand motion, slow sweeps and small jitter cannot complete it.
No logging, allocation, locks, file I/O or engine writes are added to sampling.

## Verification and pending acceptance

Focused Release tests pass: 14,820 interaction checks and 4,945 accessory
checks. Coverage includes one shake with grip released or held, mirrored hands,
three sampling rates, four axes including diagonal, all seven needle identities,
stroke thresholds, every native reload binding, settled rearm, continuous-shake
suppression, interruption/cancellation and existing magazine/holster behavior.
The final package command runs cumulative Release, all CTest suites and the
Reach consistency gate; final results and exact archive identities are recorded
in out/needler-single-shake-current-handoff.json after archive verification.

Local tests do not prove physical feel or runtime acceptance. User headset
testing of CE and Halo 3 regression, other titles and both editions remains
pending. No accepted pointer advances; CURRENT-STATE.md stays untouched.
