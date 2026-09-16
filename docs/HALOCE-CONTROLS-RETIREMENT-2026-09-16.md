# CE controls retirement after camera interruption

The Halo 3 reference is recovery of tracked gameplay controls and the VR reticle
when the camera becomes ready again. A temporary camera gap must not leave CE's
independent native player-state reader permanently disabled.

## Reproduced compiled defect

Community report 13 uses runtime `d47a98c947dc60dd98d7259a29a7582d5f46df7f`,
Steam, SteamVR/OpenXR 2.17.9, Oculus-family at 90 Hz. The log does not identify
the exact headset model. Its controls telemetry ends at `21:00:04.518` with
`observed=10817 applied=10813 stock=4 exceptions=0`. The core then reports a
camera-heartbeat expiry at `21:00:06.035`. At `21:00:07.248` the camera is ready
again; first-person, shot, contact, HUD and comfort transactions reinstall,
but controls never report reinstalling or polling again. Runtime mode moves
through `unsupported`, the reticle reports no native ownership, and contact
publications stop. These are observed events, not proof of why the camera gap
began or that every reported control failure has this cause.

The existing Release DLL hashes to the accepted artifact:
`ADAB506E9E3BFB1E04DBBF767FDD907EFD414526863AB5C837FD65E7FAB95922`.
Its build's `haloce_controls.obj` disassembly shows `TurnHook` is a 9-byte leaf:

```text
mov r9, qword ptr [rsp]
jmp TurnDispatch
```

`HaloCEControls_Poll` calls `Remove` when the camera loses admission. `Remove`
sets `retiring=true`, clears both state and turn readiness, disables the turn
hook, and passes eight function ranges to `WaitForNativeDetourQuiescence`.
That helper requires Windows `RtlLookupFunctionEntry` metadata for every range.
The leaf `TurnHook` has none, so the helper rejects slot zero before it can
inspect threads. Every subsequent poll retries the same failing cleanup and
returns before reinstalling or reporting telemetry. No callback needs to be
stuck for this failure to persist.

Original compiled disassembly is preserved in
`out/ce-controls-d47a98c-leaf-retirement-20260916.txt`; the corrected Release
fixture disassembly is
`out/ce-controls-corrected-fixture-retirement-20260916.txt`.

## Scoped correction and verification

`TurnHook` now captures the actual native return address and owns the existing
callback-counting and structured-exception cleanup boundary directly. Its
arguments, native caller guard, turn math, state admission and original engine
call are unchanged. The older dispatch remains available for its existing
direct transaction fixture tests. No shared quiescence policy changes, native
offsets, camera ownership changes, or gameplay-heading writes are introduced.

The runtime fixture now uses real `RtlLookupFunctionEntry` for all eight
production retirement addresses. It only replaces the later thread-freeze
phase because its private services have no concurrent native game workers.
Before the correction, it failed with missing range slot zero on both cleanup
attempts and could not restore native state after reactivation. Afterward:

- all eight ranges resolve on both attempts;
- a pending callback preserves dependencies and refuses cleanup;
- a drained callback permits cleanup and same-generation state reactivation;
- the actual hook entry releases callback ownership and propagates native
  structured exceptions;
- both `halomccvr_ce_controls_tests` and
  `halomccvr_ce_controls_runtime_tests` pass in Release.

The corrected fixture hook begins with `sub rsp,28h`, calls `TurnBody`, and
performs its own cleanup before returning. The full candidate DLL and headset
result still require verification; a private fixture does not exercise a live
thread freeze or prove native camera transition timing.

## Remaining reports

Report 13 already has native controller-shot overrides and VR reticle ownership
before this interruption. Shot `stock` counts cannot establish a local firing
failure: the observation counter increments before local-shooter filtering, so
remote/NPC calls are included. Report 14 retains controls through its final
telemetry and records 528 applied shot overrides, 57 physical melee contacts
and 20 native melee applications, as well as repeated VR reticle ownership.
Neither log establishes that every visible reticle or shot was correct.

The accepted d47a98c baseline changes the local on-foot weapon shot direction
and head-relative walking, but has no CE-specific grenade-direction,
sound-listener orientation or camera-recoil suppression transaction. The
September 16 cumulative candidate adds these as separate native transactions;
they are not effects of this retirement correction. See the current candidate
notes for their individual evidence. Physical positional body following remains
deferred; body facing is a separate feature. The melee refinement retains
native collision and adds a documented VR reach policy rather than inventing
a native radius or sensitivity field; see `HALOCE-MELEE-REACH-2026-09-16.md`.

Headset reproduction should check fresh CE gameplay, rotate the head without
turning the stick, aim/fire away from the original heading, then cause an
ordinary level or graphics transition and repeat after the camera returns.
The recovered log must show controls resuming, with local shots and the VR
reticle working visibly. Grenade/audio direction, AR firing jitter, and melee
against Grunts/Jackals should be recorded separately, with graphics mode and
mission, since this correction supplies no evidence of resolving them.
