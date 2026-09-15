# CE native controls WIP — 2026-09-15

The accepted Halo 3 reference is configured snap/smooth turning, HMD-owned
on-foot pitch and head-relative walking, with native controls retained when
VR does not own the current player. This document records local implementation
and evidence, not a headset acceptance result. No package is authorized by it.
Physical melee and world collision remain after functional CE headset testing.
The latest basic-VR priority also defers CE physical body following; ordinary
head-relative stick walking and configured snap/smooth turn remain enabled.

## Native boundary and state

The detailed official HCEEK and pinned retail state/control proofs are in
`HALOCE-FIRST-PERSON-EVIDENCE.md`, E-CE-FP-3. The generated `player_state`
binding group validates those native owners independently of the optional
first-person, shot and turn detours. A failed turn hook leaves verified native
state and head-relative movement available; a failed shot hook does not remove
the controls state reader. The old FP state API forwards to this module.

The optional `controls` hook targets retail `0xA99C1C`, the official
`0x58FB70`-matched input angular-delta updater. It admits only the return from
native call `0xA9965B` (`0xA99660`), after native director look suppression,
and only the actual input user mapped to output-zero's full player handle.
Other native calls/users keep their original arguments and do not reset the
local user's turn state. It passes the chosen yaw delta to the original engine
function and parks the raw pitch delta while the HMD owns the on-foot view.
The native updater still performs its yaw normalization and angular clamps;
the normal input-to-output copy still runs. It never directly writes a unit
position, camera global, input/output record, or another user's angles.

Admission requires the checked local unit/player/input/output identities,
on-foot parent, native first-person perspective zero, no native input/look
suppression, no native pause/cinematic flag, a fresh coherent camera/XR receipt,
and the shared user/presentation enable flag. Vehicle/attached users stay on
native controls. Other perspective values remain unnamed. Loss of admission
ends look ownership for this feature and preserves native arguments.

## Shared behavior

- Snap turning calls the existing `Halo3ConsumeSnapTurn` helper directly:
  activate beyond 0.6, rearm below 0.3, and consume a deflected takeover until
  the user centers the stick. It reads the same `turn_snap_deg` configuration.
- Smooth turning uses Halo 3's 0.15 deadzone, the same configured degree rate,
  and a high-resolution elapsed time capped at 100 ms. Right stick subtracts
  native yaw. A repeated or older XR serial is not consumed again. Generation,
  XR space, reference and renderer transitions reset the local turn state.
- `HaloCEControls_OwnsLookStick` additionally requires an admitted native call
  within 150 ms and rechecks the current native state and presentation flag.
  Shared input can therefore suppress raw RX/RY only while this path is live.
- `HaloCEControls_MapMoveStick` projects native camera and HMD headings onto
  CE's independently established world-Z-up plane, then rotates the stick into
  the native heading while preserving magnitude. Invalid/stale/vertical
  headings leave output untouched.
- `HaloCEControls_GetLocomotionFrame` returns the native center camera, HMD,
  reference, scale and checked local owner together for shared locomotion.
  The native camera position is not claimed to be the unit's collision origin.

The hook uses native simulation update timing. Unlike Halo 3's render-reference
turn author, CE's native updater turns the native body/camera heading itself.
The difference in interpolation and perceived snap/smooth timing still needs
headset testing; no extra independent render yaw has been layered onto it.

## Verification and remaining limits

`halomccvr_ce_controls_tests` uses always-active checks in Release. It exercises
all native admission flags, shared snap transitions, held ownership handover,
duplicate/older XR samples, invalid input, reference reset, smooth elapsed-time
accumulation/cap and head-relative movement/rejection.

`tools/re/test_ce_controls_native.py` verifies the pinned DLL SHA and executes
the actual `0xA99C1C` instructions in Unicorn: 32 yaw/pitch cases across all
four native input users pass, including wrap and clamp cases and unchanged
other users. Seven actual `0xBBB8D0` datum lookup cases pass for valid/stale
salts, invalid/empty indices and the native unsalted-index behavior (which the
runtime ownership reader rejects). Only the on-foot seat-description service
is stubbed with no parent/seat/constraint. These checks do not prove the entire
live input loop, native state-transition timing, stereo output or headset feel.

Shared body-following remains disabled for CE under the user's basic-VR scope.
The coherent locomotion receipt is retained for possible later refinement.
Ordinary head-relative stick movement does not consume tracking position or
write native body position. Classic, Anniversary and Halo 3 regression headset
results remain required before claiming parity or advancing the accepted pointer.

## Production state/turn review — September 15 basic-VR pass

The shared input path calls CE's live verified-state reader before selecting
gameplay stick treatment. It preserves plain menu navigation when native
input/look suppression, native pause/cinematic flags, missing controlled unit,
or shared menu/presentation state blocks movement. CE's own head-relative
mapping and turn hook require on-foot native first-person ownership; vehicles
keep native controls. The native controlled-unit `NONE` condition is the
official dead-player check described in E-CE-FP-3, not a guessed death enum.

`TurnDispatch` now retires callback ownership through native structured
exceptions with `__try/__finally`. Its predecessor used ordinary C++ RAII
around a native callback, which does not guarantee cleanup for those exceptions
under this project's exception model. An abnormal exit also revokes its last
look-ownership receipt and increments the cold-reported exception counter.
The original native exception still propagates. The exact native return-address guard remains at the outer hook,
and retirement also checks the separate dispatch/body code ranges.

`halomccvr_ce_controls_runtime_tests` executes the production reader and turn
dispatch against an explicitly owned fixture image. Native object/datum/weapon/
perspective services are small declared stubs; the pinned native instruction
test above separately validates native datum lookup and angular updates. The
fixture exercises input/output/player/backlink/weapon ownership, duplicate
mapping and unsalted rejection, each actual memory-backed admission flag,
dead/unarmed states, stock argument preservation, exact caller/user selection,
configured snap/smooth turn and duplicate-sample rejection, actual head-relative mapping,
stale-owner refusal, independent turn failure and native exception cleanup.
It does not execute a game process or establish native transition timing in a
headset.


## Integrated Anniversary publication correction - September 15

Source review found that only ClassicWindowBody published gameplayCamera; the
Anniversary controls/shot consumers could never obtain a current native center.
After successful Anniversary construction, BuilderHook now publishes the
unmodified primary append input with its frozen reference, scale and renderer
epoch. It removes the native Saber bridge world offset and forward bias before
publishing native CE coordinates (E-CE-FP-7). The optional gameplay_bridge group
verifies the producer and all six RIP-relative global operands and exceptional-camera branch witnesses before those
reads; a failed group leaves this feature stock and does not disarm VR.

The production runtime fixture covers missing/unverified publication, nonzero
native offsets/bias, fresh XR input with an unchanged native center, recenter,
old-reference rejection, recovery and renderer-switch invalidation. The native
bridge emulator separately executes 27 producer-to-compiled-inverse cases.
This fixes local admission and coordinate handling, not the unresolved old
Anniversary world-render appearance or untested headset controls.
