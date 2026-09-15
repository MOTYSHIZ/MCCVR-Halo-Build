# CE visibility progress and refinement feedback - September 15, 2026

## Identity and user result

Source `a2b526a5d1a387a6b7cd4af1ed70856378b3cffc`, compiled September 15
14:01:20. Both supplied logs identify Steam, SteamVR/OpenXR 2.17.9,
`SteamVR/OpenXR : oculus`, panel 90 Hz. These logs do not name an exact headset
model. The package manifest records DLL SHA-256
`25D7954D59546B143EB9279E4C35B172EBF4A82F47703655D58D1A160DD70645`;
the installed DLL was not separately read in this session, so its hash is not
inferred from the log.

Preserved verbatim under `out/test-runs/a2b526a-ce-refinement-feedback-20260915/`:

| File | SHA-256 |
| --- | --- |
| `ce-user.log` | `744AB20866E8929D0423960FC3D65A55898C1428EAD0565F6751EC5F4E2F02EE` |
| `halo3-user.log` | `6B8C0FFB3197F7D50E65773A73C3E55B6B3C14F4D9D24955ED220CD5D7613EBA` |

The user confirms seeing both Original and Anniversary in VR. This supersedes
the earlier missing/displaced-world report as the current visible baseline;
it does not accept complete CE parity. Remaining explicit defects:

- Original gun and hands look distorted or improperly scaled.
- Switching graphics changes Anniversary's facing direction; stick pitch does
  not recover it.
- The game pauses, but its pause menu is invisible in the headset.
- Weapon alignment, bullet direction, muzzle effects and performance require
  refinement to the existing-title player experience.

The user requests autonomous cumulative CE corrections through a build ZIP and
matching source ZIP, with no confirmations. This explicitly authorizes the
combined CE refinement scope rather than another isolated diagnostic milestone.
No installation, game launch, game-folder changes, PR or publication is needed
or authorized by that package handoff. Other standing/deferred work is retained.
`CURRENT-STATE.md` remains at accepted `4e01f28`.

## Log findings

CE finishes with 2,704 captured pairs and 20 drops. Original's source bootstrap
now works, unlike preserved `be2140f` feedback, which recorded zero Original
pairs and 610 source misses. Repeated graphics switches retain core ownership.
There is no matching CE pre-install log in `out/deploy-backups` for these
package-only handoffs; the retained prior CE test log is the comparison source.
Anniversary's HUD replay still records zero draws and failure 1. Authored
crosshair preparation fails specifically at `hud_target_release`. The FP
palette applies, and Anniversary skin/projection hooks apply thousands of
times. Shot-adjustment calls remain zero; firing coverage is not established
by the user's report, so this alone does not prove the shot hook is inactive.

At 14:11:50.010 CE enters `Runtime mode: gameplay -> paused`, but there is no
corresponding compositor pause transition and eye pairs continue. The user
reports the same missing menu. Subsequent camera expiration occurs at
14:12:04.860 during the exit/loading tail; it is logged, not a silent teardown.

CE gameplay render-window p95 reaches 25-37 ms in several windows, exceeding
the 11.11 ms budget at 90 Hz. These whole-frame measurements do not isolate a
CPU or GPU cause and do not justify promising existing-title performance.
There are no recorded ordering failures, duplicate frames or heartbeat stalls
in those timing windows. Native scene refresh counts remain low across steady
stereo, consistent with transition-only refresh instead of a per-frame rebuild.

## Halo 3 regression and recovery

The user reports Halo 3 initially stayed flat, then recovery worked after
pausing. The log shows the level-load gate waiting for liveness, two accepted
manual retry requests plus one coalesced repeat, and installation after 120
consecutive changing camera samples. Recovery completes at 14:13:53.291.
Later native pause/resume flags produce the expected head-locked/stereo
presentation transitions. This confirms the reported recovery outcome; it
does not establish automatic injection after every transition as fixed.

Halo 3 was requested as a regression because earlier CE integration touched
shared OpenXR presentation, input and title lifecycle paths. A CE-only engine
correction does not by itself require repeating every Halo 3 test. Any new
shared-path change still retains the contract's headset regression requirement.
Current user priority remains CE; do not divert this package into H3 recovery
redesign or treat the recovery result as universal acceptance.
