# CE continuous controller targeting (unaccepted candidate)

The Halo 3 behavior being matched is native weapon target acquisition along the
tracked weapon aim, retaining the engine's eligibility, range, team and visibility
rules. The reported CE Needler/plasma-pistol homing and reticle behavior must be
tested with Eye Patch off. This change does not alter skulls or weapon tags.

## E-CE-TARGET-20260918

Official HCEEK `halo_tag_test.exe` continuous player assist at VA `005486F0`
names `aim_assist.cpp` and its control/targeting outputs. Its six-argument query
at `00546B20` takes weapon parameters, origin, direction, unit, team and an
80-byte result. It obtains the origin's cluster, collects eligible candidates,
sorts 80-byte records, checks visibility, and returns the first visible result.
Weapon parameter helper `005493C0` supplies authored angles/ranges and zoom.

The pinned retail equivalents are `B67FA8`, `B683EC`, and `B68658`. Direct-input
acquisition `B68284` also calls `B683EC`. These are the query's only two direct
callers: `B680F8` and `B6839B`, returning at `B680FD` and `B683A0`. The new
generated contract proves the unique query entry and both call edges. Input
processing `A97CF4` invokes the two acquisition paths before firing. Both used
the native observer direction, independent of the existing shot-only hooks.

Existing shot hooks at `B00880`/`B00740` are called from trigger `B7A374`.
Downstream firing assist `B67B00` queries the director at `B14F14`, then uses
`B697BC` with the cached acquisition target. Correcting only those firing rays
does not correct the earlier search that populates that cache.

Native HUD updater `B3E7A8` reads player-control state (`2D8FE70`, user stride
`38`, fields `194`/`198`) for authored crosshair states; `B3FFA0` selects the
authored overlays/colors from those states. This is evidence to retain native
HUD behavior, not permission to force a red/green bitmap. The exact brief-red-
flash report has not been reproduced or independently attributed.

Read-only decompilation records are in `out/reload-policy/ce-target-kit*.c`,
`ce-continuous-target.c`, `ce-target-input.c`, `ce-target-acquire.c`,
`ce-targeting.c`, and `ce-hud-target-state.c`. Kit and retail identities remain
those in `HALOCE-EVIDENCE-MANIFEST.json`.

## Implementation and isolation

The optional query hook substitutes only a private direction vector from the
same tracked controller basis used by the firing hooks. It requires an exact
player callsite, current CE generation/context, full local unit identity,
controlled on-foot player, valid controller pose, and unblocked gameplay.
Other calls retain their original arguments. Native search executes once;
the adapter neither creates a target nor repeats a faulted native call.

Origin, weapon parameters, team, result buffer, target selection and native
camera are unchanged. Signature/install failure leaves just this feature stock
and logs the reason. Retirement includes the new hook and trampoline and waits
for callback quiescence. Worker logs report observed/applied/stock queries.

## Verification and limits

Release build and all 40 CTests pass after this change and the final CE reload
tag-pointer bounds edit. The production CE first-person fixture covers both
graphics modes, both handedness settings, both callsites, foreign/stale owners,
seated/paused/blocked/cinematic states, native fault propagation without replay,
and independent cleanup. Reach consistency and all pinned CE manifest checks
pass. Logs are `out/refinement-20260918/{build,tests,gate}-ce-target.log` and
`ce-target-evidence.json`.

`tools/re/test_ce_target_cone_native.py` executes 24 cases against the actual
retail cone helper `B6E60C`, reached through `B68BA0`: controller-direction
eligibility, off-axis refusal, front/behind tests, authored range and target
radius boundary. It does not simulate teams, visibility or a live world.

These results are not headset acceptance. Needler and charged plasma-pistol
tracking, neutral/enemy/friendly reticle behavior and brief color transitions
still require both CE graphics modes and a Halo 3 regression result. The full
standing refinement scope remains open; no ZIP was packaged or installed.
