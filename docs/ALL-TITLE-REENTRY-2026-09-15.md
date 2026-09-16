# All-title re-entry and recovery, September 15, 2026

## User result and preserved candidate

The user tested source `558fb2c2237492c0458b5cee02c690be652285fe`
and explicitly accepts Halo CE Original and Anniversary VR: "perfect now for
both original and anniversary graphics. dont change anything there."
Preserve the CE camera, renderer, resolution, HUD, weapon, contact and graphics
switch implementation. This is scoped CE acceptance. It does not advance the
cumulative accepted source `4e01f28` or confirm Reach's new HUD height control:
the subsequent Reach session did not enter VR.

The user requests automatic VR re-entry across title switches and Force Inject
recovery for every supported title. Prepare a build/source pair only after the
correction is ready. Do not install, write game folders, launch MCC or publish a
PR. Both Steam and Microsoft Store remain supported.

Preserved run:

- `out/test-runs/558fb2c-ce-accepted-reach-reentry-20260915/user.log`
- Log SHA-256:
  `54D160D4167D225C988C16B315F1C06E9690E3C542A41A273F76D011A7238329`
- PID 16860; Steam; SteamVR/OpenXR 2.17.9; headset string
  `SteamVR/OpenXR : oculus`; 90 Hz panel. The log does not identify an exact
  headset model. Prior user identification is Quest 3.
- Matching former handoff copied to `candidate-handoff.json` beside the log.
  That manifest records packaged DLL SHA-256
  `531CF825601C4654CB883858AD2DC6570AD948B815746E1992C3F880D0F398B0`.
  The DLL actually installed by the user was not separately hashed; the log
  establishes the embedded source identity, not the installed file hash.

## What the new log proves

| Time | Observation | Implication |
| --- | --- | --- |
| 20:41:08-20:42:11 | CE generation 1 renders Original and Anniversary; the final counter has 2,350 completed pairs and 21 dropped frames. | The user's accepted CE run reaches both stereo paths. Frame drops do not disarm the core. |
| 20:42:12.556 | CE camera heartbeat expires; hooks remain for re-entry. | CE stops owning presentation at its level-exit boundary. |
| 20:42:15.077 | Title adapter selects Reach. | Initial title detection succeeds. |
| 20:42:33.482 | Reach's camera changes after loading-screen stillness; its level-load gate opens. | The gate is not permanently stuck and should not be bypassed. |
| 20:42:33.719 | Reach exact loaded-image preflight passes. | The pinned native identity/signature check succeeds. |
| 20:42:33.753-33.765 | Native pause and muzzle bindings resolve; authored crosshair resources are cold-prepared for generation 3. | The worker continues after preflight and general VR resource preparation is available. |
| 20:42:33-20:43:02 | No Reach display proof PASS/waiting, core installation, or arming line occurs. World collision remains uninstalled, presentation stays one layer and stereo stays off. | The missing step precedes display-proof construction and camera installation. |
| 20:43:05.915 | CE is installed again as generation 1. | The title module was not observed unloading/reloading across Reach; module residency and active title are different facts. |
| 20:43:10.948 | Adapter reports `halo3.dll,halo1.dll` resident. | Inactive CE residency is also explicit later in the run. |
| 20:43:31.784 | Halo 3 logs automatic head tracking/stereo ON. | The same process/session can still enter another title's VR. This is supporting log evidence, not a separate user headset regression verdict. |

The OpenXR session remains focused/visible and keeps submitting. There is no
runtime failure or session-loss event accounting for Reach's refusal.

## Comparison against preserved working runs

The immediately preceding user log is
`out/test-runs/2cf002b-ce-reach-feedback-20260915/HaloMCCVR-user.log`,
SHA-256 `BF9B95D51EC75A8F99BF040C8FF428D919F47F11BEE845C55F33F2CC85076E5E`.
It enters Reach before CE on the same Steam/OpenXR 2.17.9 configuration:
preflight passes at 19:44:16.217, authored resources are ready at 16.348,
display proof passes at 16.500, core installation completes at 18.442, and
arming completes at 18.922. That run verifies the missing sequence, not the
later untested HUD-height behavior.

The required older deployment comparison is
`out/deploy-backups/16fd06e-steam-before-1c08837-20260905-015705566Z/HaloMCCVR.log`,
SHA-256 `93C34B1174E03F3666476D02C9716E33920483556DD3B9E9E33F36487AA8546C`.
Its embedded source is `ff6d1fd9ad9c87dde5b423119ab8b41bfe1b9e6d`, Steam,
SteamVR/OpenXR 2.17.8, Oculus-family headset, 120 Hz panel. Reach display proof
passes at 19:20:11.588, installs at 12.841 and arms at 13.287. The backup
directory's shorthand is not the embedded source identity.

## Source-backed cause and correction boundary

In tested `558fb2c`, `ReadReachDisplayAdmission` in `src/dll/vr.cpp` requires
the complete resident-module availability mask to equal the single Reach bit.
Both the Present snapshot publisher and the cold display worker use this
admission. When Reach is active but CE remains resident, the cold worker
returns before its display-proof diagnostic. `ReachCameraCore_Poll` then
silently waits for `VR_ReachDisplayReady`, even though its other ready checks
have succeeded. This is a reproducible policy defect that matches the log's
missing display/installation sequence.

`TitleAdapter_PollLoaded` already resolves an active title separately from
module residency using Present-caller/activity evidence. Its informational
"detected" line does not say only one DLL is resident. `TitleRuntimeState`
increments a title's module generation whenever its availability or base
changes; CE retaining generation 1 in the later installation supports the
resident-CE explanation. The precise complete mask at the first Reach
preflight is not printed in this log; it should not be invented as a measured
value.

CE's `Remove` intentionally retains native hooks and its module reference
while synthetic lists can still be consumed. The correction must preserve
that safety behavior. For the expected Halo 3 player experience, switching
titles should allow the newly selected title to prove its own live camera and
enter VR even while an inactive title still holds safe cleanup resources.

Correct Reach display admission to follow the coherent selected active title
and exact resident Reach generation/base, instead of requiring every other
DLL to unload. Retain the existing module-availability consistency checks,
Present/engine-swapchain identity, loaded-image preflight, frame/lifecycle
serials, resource shape/device/context checks and revocation on transition.
Do not relax the level-load gate or force a camera hook into an unproven
engine. Add cold diagnostics for admission rejection so another refusal is
attributable without logging in the render hooks.

The original Force Inject route is explicitly Halo-3-only in the launcher and
worker. The replacement retries each selected title's existing verified
lifecycle, retaining stale-generation, cleanup, load and tracking proofs.

## Production admission regression

The corrected production admission is shared from
`src/dll/reach_display_admission.inl` by Present, the cold worker and the
offline executable `tests/reach_display_admission_tests.cpp`. It admits a
coherent, selected, resident Reach module while other titles remain loaded.
The fixture exercises every resident-module combination and rejects other
selected titles, unknown/shell selection, concurrent availability/selection/
generation changes, a missing module, zero generation and stale publication
identity.

Using the original one-bit requirement, this same production fixture reports
32 failures in 526 checks. With only that requirement corrected, all 526
checks pass. Preserved outputs are `out/reentry-admission-before.txt` and
`out/reentry-admission-after.txt`; the build transcript is
`out/reentry-admission-build.log`. The reviewed diff retains every later
native Present, shape, device/context, epoch and frame-lifecycle proof. This
establishes the policy correction offline; it does not establish runtime
acceptance of the new candidate.

## CE activity selection with retained title modules

The existing `title_reentry_probe.cpp` activity table covered Halo 3, ODST,
Reach, Halo 4 and Halo 2, but omitted CE. Its Present-caller route could select
CE, so this omission did not prevent every return to CE (the supplied log
actually returns to CE). When Present is called through a host module and
several title DLLs remain resident, however, the activity route had no way to
select CE. This is a source-proven coverage gap, not a second measured cause of
the reported Reach failure.

CE now joins that read-only activity selection through its existing E-CE-2
native clock evidence in `HALOCE-RENDER-EVIDENCE.md`: singleton slot
`halo1+0x2E9FD68`, initialized byte at `+0`, and current signed tick at `+0xC`.
The probe derives its code anchor and singleton slot from the generated
contracts. Before reading the clock, it checks the current `halo1.dll` mapping,
readable ranges, pinned PE timestamp/image size, and the exact fixed bytes of
the uniquely verified `initialized_clock_render_gate` at `0xAC269C`, including
the RIP operand to that singleton. An audit of every CE hook-install target
confirms this gate remains unmodified: the Classic game-render hook is
`0xAC47C0`, and the other renderer/feature hooks target different functions.
This activity-probe change does not alter CE camera, renderer, HUD, resolution,
weapon or contact behavior. Separate recovery bookkeeping only clears failed
setup attempts when CE is uninstalled or inactive; its successful engine
implementation remains the accepted source.

An uninitialized/frozen clock never selects CE. A fresh initialized transition
or a change after observed stillness can select it; an already ticking clock
requires the existing 120-sample sustained-activity threshold. Invalid or
inaccessible clock data, replacement clock objects, non-forward sample time
and gaps over 250 ms clear CE's accumulated activity evidence. The mapping,
clock pointer and initialized flag are rechecked before publishing a sample.
Two simultaneously active engines cannot supply a new unique selection.
Adapter retention remains separate from runtime ownership: this probe grants
no lifecycle, heartbeat, camera arming or capabilities and changes no game
memory or module reference counts.

`tests/title_reentry_probe_tests.cpp` compiles the production resolver with
only module lookup replaced by a fixture. Real Windows committed-range and
protection checks exercise synthetic PE, anchor and clock memory. All **172
checks** pass, covering retained Halo 3 to CE selection without a Present hint,
frozen/uninitialized startup, sustained activity, stale/non-forward samples,
wrong module/PE/clock operand, invalid booleans and negative ticks, inaccessible
or truncated clocks, replacement objects, conflicting title activity and
fresh/expired/future Present hints. Build and CTest transcripts are preserved
as `out/reentry-probe-build.log` and `out/reentry-probe-tests.log`. These are
offline policy checks; final headset acceptance remains required.

## Manual recovery implementation and review

Launcher Force Inject and F1 recovery now select CE, Halo 2, Halo 3, ODST,
Reach or Halo 4 using one atomic title/module-generation token. A title or
module transition cancels the override; another title with the same numeric
generation cannot inherit it. The successful token remains scoped to its
title after completion, allowing explicit recovery to remain active with a
saved `auto_vr=false` setting in the branches that observe that setting.
The existing CE/Reach/Halo 4 automatic presentation policy is preserved.

Halo 3 and ODST retry through their existing retirement/load gates. Reach and
Halo 4 request their existing verified cleanup. Halo 2 requests independent
observer and Classic/Anniversary core retirement, retaining pending cleanup
across callback contention and transient range unavailability. CE's explicit
recovery retries a failed initial installation and recenters; a healthy CE
core and both renderer implementations remain intact.

Recovery completion skips its dispatch tick and requires both a fresh,
unambiguous runtime owner and the selected title's actual armed core. Pending
cleanup/recovery markers, including all three Halo 2 queues, block completion
even if an older lifecycle publication still says armed. Active stereo and
released pause presentation must also be observed. This corrects the reviewed
premature-completion case without treating a queued request as a successful
recovery.

Explicit retries can clear only matching **failed** cold-proof attempts for
Halo 2, Reach and Halo 4, after their corresponding cores are uninstalled.
Passing proofs remain cached so installed detour bytes cannot be mistaken for
an unsupported native image. The ordinary native load and image checks must
pass again. Reach's prior runtime failure latch is released only after its
camera cleanup finishes. No retry bypasses an OpenXR failure, native identity
verification or callback lifetime guard.

`python tools/test_manual_vr_cold_retry.py --build` extracts the three actual
production retry function bodies, records their hashes, and compiles them
against bounded state fakes. It passes 46 checks covering zero/foreign epochs,
matching failure reset, duplicate requests, current PASS preservation and an
unrelated old Halo 2 PASS while a new gate retries. Outputs are
`out/reentry-cold-retry-test/result.txt` and `source-hashes.json` in that
directory. This tests the production reset decisions; it does not execute a
native module scan, hook removal, driver or game process.

## Halo 2 Anniversary retirement retry regression

Independent review found a concrete blocker in source `558fb2c`'s
`Halo2AnniversaryStereo` cleanup. `RemoveCore` accepted only `MH_OK` from each
hook disable. If entries were disabled but an active callback exceeded the
drain limit, the next call received `MH_ERROR_DISABLED` for the scene entry
and returned before finishing retirement. It also ignored removal failures
and assumed any remaining cleanup necessarily included the scene target.

The production cleanup now resides in `halo2_anniversary_cleanup.inl`, used
directly by the actual stereo source and the offline regression fixture. It
accepts verified disabled/absent entries, records `CleanupRequired` before
retirement, preserves exact remaining targets on partial removal failure and
checks all three detour/trampoline ranges with the existing native quiescence
helper before freeing records. Active callbacks and a failed quiescence check
both retain the originals and module identity. No renderer algorithm changed.

The complete production `Halo2AnniversaryStereo_Poll` is similarly shared from
`halo2_anniversary_poll.inl`. Pending cleanup must succeed before an otherwise
eligible retained module can reinstall or arm. The manual request stays
pending on incomplete cleanup or missing mapping proof; completion checks its
pending state separately from an old armed camera. Only actual title exit or
a changed generation clears the failed-install latch automatically; transient
loss of the active module range is insufficient.

`tests/halo2_anniversary_cleanup_tests.cpp` compiles both production functions
and supplies deterministic MinHook, wait and quiescence outcomes. It covers
callback timeout followed by already-disabled retry, a busy detour before its
callback increment, each of three partial-disable/removal failures, remaining
rebuild/UI-only records, immediate same-module return, pending manual recovery
and rejected-generation reset boundaries. These are management-policy tests;
the fixture does not execute native engine callbacks or the thread-freezing
implementation.

The original `RemoveCore` was extracted verbatim from
`558fb2c2237492c0458b5cee02c690be652285fe` into ignored
`out/reentry-h2a-baseline/original-cleanup.inl` and substituted into that same
fixture, retaining the current production poll. It fails **31 of 59 behavior
checks**, including the second removal after callback timeout. The corrected
production functions pass **192 checks**, including the extra checks reached
inside the newly required quiescence callback. Before/after output is preserved
in `out/reentry-h2a-cleanup-before.txt` and `out/reentry-h2a-cleanup-after.txt`;
the corrected build and CTest transcripts are
`out/reentry-h2a-cleanup-build.log` and `out/reentry-h2a-cleanup-tests.log`.

## Final local verification

Cumulative Release configuration and build pass, with no new compiler warnings.
All **28 CTest suites** pass, including the new production display-admission,
activity selection, recovery-policy and Anniversary cleanup/poll fixtures.
The Reach consistency gate also passes. Final transcripts:
`out/reentry-final-configure.log`, `out/reentry-final-build.log`,
`out/reentry-final-tests.log`, `out/reentry-final-reach-gate.log`.
The separate reproducible cold-proof test passes **46 checks** in
`out/reentry-cold-retry-final.log`.

A source comparison against `558fb2c` verifies every CE renderer, HUD, tracking
and contact file is unchanged. The stereo core differs only by the explicit
uninstalled recovery-latch reset and inactive rejected-attempt reset, plus its
API declaration; `out/reentry-ce-preservation.txt` records this check.
Shared lifecycle changes still need the user's CE/Halo 3 regression.

Package only after this verification, without `-Install`. The package script
repeats configure/build/tests/gate for the committed identity. Verify ZIP CRCs,
staged/manifest file hashes and sizes, DLL embedded source, release notes,
SHA-256 sidecar, and exact source ZIP equality with `git archive` before
handoff. The verified package identity belongs in
`out/all-title-current-handoff.json` and `out/ce-current-handoff.json` after
archive validation. Deliver both ZIPs, then wait for the user's test.

## Acceptance still required

Local regression/build checks do not establish headset success. Test CE to
Reach, reverse switches, repeated entry and Force Inject with each supported
title. Verify Reach HUD height once Reach is in VR. A Halo 3 regression and
confirmation that accepted CE Original/Anniversary behavior is preserved are
required for the shared recovery change. The new candidate remains unaccepted
until the user reports those results.
