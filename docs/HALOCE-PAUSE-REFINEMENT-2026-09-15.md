# CE native pause presentation refinement - September 15, 2026

## Player behavior being matched

Halo 3's accepted behavior is a readable, head-locked native pause screen,
ordinary menu navigation, and stereo restoration after native resume. CE must
follow its own native state in Original and Anniversary, including keyboard
pause and selecting Resume with the menu cursor. A controller Start edge alone
cannot establish whether the game is still paused.

## Evidence and defect

The user tested source `a2b526a5d1a387a6b7cd4af1ed70856378b3cffc` and reports
that CE pauses but its menu is invisible. Preserved input is
`out/test-runs/a2b526a-ce-refinement-feedback-20260915/ce-user.log`:

- Steam, SteamVR/OpenXR 2.17.9, Oculus-family headset, panel 90 Hz. The log
  does not identify an exact headset model.
- At 14:11:50.010 CE publishes `gameplay -> paused`, but there is no shared
  pause-presentation request or transition; CE continues rendering eye pairs.
- At 14:12:06.271 the runtime changes from paused to loading.

Source review verifies the missing connection: CE's `Game_AutoVrTick` branch
read `player.nativePaused` only to publish the runtime mode. Unlike Halo 3,
Reach, and Halo 4, it never requested the shared head-locked pause screen.
The Y+B fallback admission also omitted CE. Finally, the compositor rejected
the native stock screen while CE's recent eye-ownership timer remained set,
even after a pause presentation had explicitly completed.

No new address is introduced. E-CE-FP-3 in
`HALOCE-FIRST-PERSON-EVIDENCE.md` establishes official HCEEK getter `0x5806E0`
and retail consumer `0xA994C0`: native game-time record byte `+2`, through
retail pointer `0x2E9FD68`. The existing generated `player_state` contract
verifies those instructions and their pointer. The initialized clock gate is
already part of CE's runtime ownership. The script `game_paused` override is
not the runtime source.

## Change

- Add a guarded native pause reader independent of local player, controlled
  unit, weapon, and optional turn-hook availability. It accepts only an
  initialized clock and boolean pause value, validates title/generation before
  and after reading, leaves output unchanged on failure, and participates in
  callback draining before the retained module can be released.
- Reconcile CE's native pause with the shared requested target after the same
  50 ms stable mismatch used by Halo 3. Compare the target, not the in-progress
  fade, so a request is not repeatedly restarted. Missing state is unknown,
  never evidence of resume. Native resume works without a second Start edge.
- Add CE to the existing Y+B Start fallback. When the native clock is readable,
  it remains the authority over presentation.
- Allow the native screen after pause presentation becomes active even while
  the CE gameplay-ownership timeout is pending. Ordinary failed stereo frames
  retain their existing isolation. Other-title stock-screen admission remains
  unchanged.
- Clear CE's pause state when leaving the title. Pausing does not remove the
  CE core, end the XR session, or change accepted-title lifecycle handling.

Existing tracking publication already disables CE per-eye work while pause is
requested or displayed. Both native renderer paths then execute their stock
rendering. The shared compositor's existing screen copy includes the MCC pause
menu; its stock navigation coordinates and the comfort transition are reused.

## Verification and limits

The Release `halomccvr_ce_controls_runtime_tests` executes the production
native pause reader against owned memory. It checks pause and resume, missing
player mapping, invalid booleans, uninitialized/inaccessible clock, stale
generation, another active title, optional turn failure, output preservation,
and callback cleanup after a native access exception.

The Release `halomccvr_ce_controls_tests` covers native-pause/stereo mismatch,
the 50 ms boundary, no repeated fade request, resume without a Start edge,
unknown-state recovery, generation replacement, and paused-screen admission
while keeping ordinary missing-eye frames isolated. `halomccvr_core_tests`
checks CE's chord admission together with every existing title. All three
suites passed on this refinement's local source.

The supplied log establishes CE's native pause observation; the new visual
connection still requires headset confirmation in both graphics modes.
It does not establish menu behavior in multiplayer modes that do not stop the
native simulation. No game launch or installation was performed. The accepted
build pointer remains unchanged.
