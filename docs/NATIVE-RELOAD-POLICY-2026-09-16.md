# Native reload policy candidate, 2026-09-16

## Scope and reference behavior

One requested behavioral refinement above accepted Alpha 0.4.1 runtime
`46c124b7208061bd33fe60a046e4f596686cf52d`: two independent optional controls beneath
Manual Reload, across all six engines and both editions. Starting HEAD `27f4bfb`
changed only release documentation above that runtime.

Halo 3 is the reference: manual magazine insertion or the existing recognized
Needler gesture requests the engine's reload, using its ammo eligibility and reserve
rules. The new options suppress the empty-trigger automatic request and skip the
identified first-person reload/ready playback and waits. Other engines use their
own proven layouts and dispatchers to match this intended player behavior. No new
headset result exists, including Halo 3; parity remains a testing requirement.

`manual_reload_disable_auto` and `manual_reload_skip_animations` both default false
and require `manual_reload`. Config load/save and F1 use these exact keys. Accepted
camera, input, gesture, accessory and holster code is preserved. CURRENT-STATE.md
does not advance. Deliver build/source ZIPs, without installation or publication.

## Evidence and reproduction

Official CE, H2EK, H3EK, H3ODSTEK, HREK and H4EK executables supplied the reload state,
message and animation semantics. Retail modules were then matched for each engine;
Reach behavior was established in HREK, not discovered from stripped retail code.
No console/archive/Reclaimer bindings were used. Local Ghidra work is preserved in
ignored `out/reload-policy`; it is not required by the shipped runtime.

The checked-in `NATIVE-RELOAD-BINDINGS-2026-09-16.json` records exact pinned retail
file SHA-256 values, hook RVAs and empty-trigger call RVAs. Reproduce and verify:

```powershell
python tools/re/generate_reload_policy_bindings.py --check
```

This requires the pinned private binaries in `out/deps/re-tools/inputs` and pefile
in `out/pydeps`. It verifies each already identified direct CALL target and each
unique signature, and compares both generated files. It does not discover or
substitute addresses. Runtime cold installation repeats unique-match checks at the
expected RVAs; zero/multiple/different matches leave the affected feature stock.
The auto CALL has a separate witness, including its instruction and surrounding
bytes. Runtime suppression additionally requires its exact return address.

Official kit anchors (absolute VAs; CE/H2 kits are 32-bit):

| Title | Reload start/request | Magazine state setter | First-person message dispatcher | Playback / specialized handler |
|---|---|---|---|---|
| CE | `008f3900` | same start function | `005cb290` | `005cb400` |
| H2 | `008a3350` | `008a2410` | `007087a8` | `00708bd7` |
| H3 | `140a8bce0` | `140a8b280` | `140968eb0` | `140969700` |
| ODST | auto predicate `140b0fa90` | `140b17230` | `1409cb4b0` | `1409cbd00` |
| Reach | `140df27d0` | `140df1670` | `1408cf0e0` | `1408cfcc0` |
| H4 | `140ea7fb0` | `140ea6ee0` | `14092d530` | `14092e1e0` |

Retail RVAs in binding order (Auto, State, Action, Play, Duration):

| Title | Hook entries | Empty-trigger CALL |
|---|---|---|
| CE | `b7814c`, none, `b27ba0`, `b285a8`, `b773e0` | `b759e5` |
| H2 | `8eadf0`, `8e9ed0`, `819d50`, `81a2e0`, `81c750` | `8e472a` |
| H3 | `35f040`, `36a394`, `2c1e60`, `2c2474`, `2c1cac` | `36588d` |
| ODST | `3a5208`, `3b0d08`, `2ea3fc`, `2eaa10`, `2ea21c` | `3abc11` |
| Reach | `4b6868`, `4c529c`, `2b0fe8`, `2b1910`, `2b0dc4` | `4bedf6` |
| H4 | `607e08`, `61a904`, `3b399c`, `3b439c`, `3b374c` | `6131f5` |

The automatic predicates are CE's weapon update, H2 `8e4570`, H3 `365760`, ODST
`3abae4`, Reach `4becbc`, and H4 `612ff8`. They admit empty-trigger reload calls;
the shared request functions also serve manual/continuation callers, which must
not be blocked. CE manual input bit 8 reaches `b7521e`; continuation reaches
`b786e3`/`b7883b`. H2's continuation caller `8ea76d` is likewise preserved.

## Data writes and retained native work

Only these signed 16-bit countdown fields can be written, after the original
state/start function executes once. Offsets are from the title's weapon datum;
magazine index is bounded to 0 or 1, state to 1..3 (CE only 1).

| Title | Magazine base | Stride | Countdown offsets within magazine |
|---|---:|---:|---|
| CE | `280` | `14` | `2` |
| H2 | `228` | `10` | `2,c` |
| H3 | `230` | `18` | `2,10,14` |
| ODST | `228` | `18` | `2,10,14` |
| Reach | `2c0` | `1a` | `2,10,14,16` |
| H4 | `5a4` | `1a` | `2,10,14,16` |

All table numbers are hexadecimal. State, loaded rounds, reserves, regeneration,
cooling and initial-duration copies are untouched. Kit magazine updates (CE
`008f6f00`, H3 `140a8b500`, Reach `140df1d10`, H4 `140ea7690`) establish that native
updates consume these countdowns and perform the real transfer/continuation.
CE update `b74e6c` calls finish `b78570` when the active timer is below 2. No
recursive update, repeated tick, direct ammo assignment or synthetic reserve is used.

Admission uses the current title generation, gameplay/VR state and a salted local
weapon owner. Existing independently verified object accessors provide full handle
and kind validation. Per-title ownership fields are CE's published player weapon;
H2 inventory bit `130&1`, kind `aa==2`, owner `158`; H3 valid-owner byte `15d`, owner
`168`; ODST `155`/`160`; Reach owner `32c`, fallback `1a9`/`1b4`; H4 owner `624`,
fallback `471`/`480`. H2 also validates the mapped player salt and local biped; H4
requests weapon type mask 4. Unknown ownership declines the optional behavior.

## Animation differences

The original first-person message dispatcher still runs: equip/reset work must not
be skipped. A nested thread-local scope suppresses its playback call, and a separate
scope shortens duration queries while setting native reload stages. Unrelated nested
messages clear scopes. Firing and non-local weapons remain stock. Query mode 2
(keyframe, including its -1 sentinel) stays native; only duration modes 0, 1 and 3
are shortened in H2 onward. CE has its own differently shaped duration API.

Reload action IDs: CE 9,10,18,19; other engines 7..12. Ready actions: CE 12;
H2/H3/ODST 20,21; Reach 23,24; H4 25,26. Kit message maps and their matched retail
maps prove these identities. Additional dispatcher reset cases are not evidence
that an action plays ready: H2/H3/ODST 24,25 map to no animation; Reach 27,28 and
H4 29,30 map to other strings and remain stock. No semantics are invented for them.

Native weapon-ready duration calls independently use CE `(animation=0, mode=10)`,
H2 string `05000024`, H3/ODST `26`, Reach `27`, H4 `71` (hex). These calls are
shortened only for the local weapon. Native tag-authored additional delays remain.
The effect is prompt completion on native simulation updates, not a same-instruction
ammo transfer or a promise of zero delay for every custom weapon.

CE playback takes three arguments; H2/H3/ODST/Reach take four. H3/ODST playback
returns a byte, preserved on all ordinary calls (unlike CE/H2/Reach void playback).
H4's five-argument dispatcher calls a specialized animation handler before its
fallback player. Handler `3b439c`/kit `14092e1e0` only handles strings `278`, `315`,
`5d`, `5e`; it can mutate animation for those strings. The admitted reload/ready
strings are different and normally return false without those mutations. Returning
true in the admitted scope skips the fallback playback; unscoped results are kept.
Native world weapon messages are retained. This is a deliberate H4-specific path.

## Failure isolation, lifecycle and validation limits

Separate auto/skip installation results, default-off option bits, retained module
handles, generation checks, callback counters and detour quiescence protect the
optional hooks. Title changes clear option admission before disabling/removing hooks
and releasing the pin. Cleanup retries are logged. Missing hooks do not disarm VR.
Guarded countdown access faults latch skip-animation stock fallback without disabling
auto suppression. Cold worker logs include install/fallback and five-second counters,
including stock-owner-guard rejections so failed ownership admission is observable.
No file I/O, logging, allocation, scanning or locks are added inside these detours.

The production-hook test uses fake native boundaries to verify six-title option
independence, exact caller selection, stock remote/manual calls, bounds/states,
every unchanged non-countdown byte, title generation changes, preserved dispatcher
initialization, ordinary animation return values, keyframe queries and retirement.
It also round-trips the real config serializer. This does not execute engine code or
prove the ownership adapters in a live session. The signature verifier checks the
pinned code, not runtime behavior. Release, all 40 CTest suites and the Reach gate
are required by packaging; final identities/logs live in ignored
`out/native-reload-policy-current-handoff.json` and `out/reload-policy`.

Headset validation is pending for all six titles, both graphics modes where present,
both editions, shell-loaded weapons, weapon switching, custom mods, and transitions.
Run the Halo 3 regression alongside target-title tests. Preserve this candidate as
unaccepted until the user reports results; do not advance CURRENT-STATE.md.
