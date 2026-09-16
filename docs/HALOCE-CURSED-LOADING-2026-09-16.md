# Cursed Halo loading diagnosis: missing CE Multiplayer content

**Resolved by the user's content installation.** The user installed CE Multiplayer,
confirms mod loading now works, and explicitly requests a new verified ZIP pair
that preserves d7dbfcb Original tracking/movement and Anniversary behavior.
No runtime, launcher, configuration or test source is changed for this handoff.
The initial hold and uncertainty below are historical investigation stages.

The user tested delivered source `d7dbfcbfdb4f204a72721c936d69e9f38ecda95e`.
Vanilla CE Anniversary and rotation are confirmed good within that report;
preserve them. Cursed Halo Again Quick Start (first campaign mission) still
stalls at the completed loading bar with menu music. The user reiterates that
these mods run Original CE, not Anniversary graphics. The loading worker's
internal name does not change that player-visible requirement.

Packaging is held until a satisfactory loading result. No new ZIP, installation,
game-folder write, game launch or publication was performed during diagnosis.
Accepted cumulative source remains d47a98c; this failed candidate is not promoted.

## Reproducible identity

Live Steam PID 9480 started September 16 at 01:26:56 local time. The supplied
log and preserved live log identify d7dbfcb, SteamVR/OpenXR 2.17.10, Oculus-family
headset at 90 Hz. The log does not name an exact headset model. Installed DLL
SHA-256 was independently read and matches the delivered candidate:
`462B71E611B50A3775BBBD83DC2B6FCD0DB4B83FC252572FD45DA95CA160F670`.

Evidence is preserved under ignored `out/ce-loading-20260916/`: `user-stall.log`,
`live-stall-initial.log`, `live-config.cfg`, `modules.json`, `native-state.json`,
thread contexts/stacks and offline unwind output. Earlier comparable CE stalls
are under `out/test-runs/d47a98c-ce-custom-campaign-failure/`. Existing deployment
backups predate CE support and cannot serve as a working CE loading comparison.

## Established so far

- Live CE PE timestamp/image size and the pinned clock-reader anchor match.
  Twelve 100 ms reads show the same native clock pointer, initialized byte zero
  and tick zero. The closed cold gate is observing the actual native state.
- No CE hooks, resolution work, camera heartbeat or completed stereo pairs have
  occurred. The cold gate reads native memory only and does not make the engine
  wait for VR. Bypassing it would not correct the observed file failure.
- The process CWD is the actual MCC installation root. The command line is the
  shipping executable plus `-WINDOWED -ResX=2912 -ResY=2100`.
- PSS thread/context capture succeeded using query/read rights, with no injected
  code, debugger attachment, target-memory writes or explicit thread suspension.
  Initial stack bytes were read afterward and can race running threads; repeated
  stable waiting stacks are corroboration, not a guarantee for active workers.
- Native CE thread 23888 has the same unwound loading stack in both captures:
  `4D96F3 -> 64CCDF -> 4726CE -> 47237B -> 476F71 -> 414B86 -> 412C15 ->
  3CCBAD -> 3CCABF -> 3BBD28 -> C259C -> C2D8B -> 882D9 -> 1243E3A ->
  48C09 -> 92DCC -> CBBE1` (all RVAs in the pinned halo1.dll).
- The native waiter at `4D96D0` waits on the resource worker object's event.
  The worker function `4D7D20..4D83CC` logs failed file operations but can repeat
  its read loop without any byte-count progress. Thread 6832's captured stack
  contains `CreateFile : error 0x2: The system cannot find the file specified.`
  and alternating ReadFile/GetOverlappedResult error 0x6 (invalid handle).
  Exact active filename/queue identity requires a coherent context and stack;
  the initial samples also contain beavercreek resource-path text, which alone
  must not be promoted to the currently requested filename.
- Installed Steam appinfo explicitly supplies `-no-eac` for its official
  "Mods and Limited Services" direct-shipping-executable launch option. Our
  launcher omits it. The MCC executable recognizes that argument. This is a
  verified launch difference, **not yet a proved cause of the resource stall**.
  Steam recognizes the current process as AppID 976730 and retrieves its 11
  subscribed Workshop entries, so global Steam/Workshop initialization is not
  simply absent.

## Captured failure and non-VR comparison

The follow-up PSS VA-clone capture preserved the active worker's heap string.
TID 6832 unwinds through the native error formatter to `halo1+4D8344`, with
restored RBX `0x7FF45C8499A8`. The counted string at that address has length 26
and contains exactly **`halo1\maps\beavercreek.map`** at +0xC. The native loader
passes this counted string to its file open. Initial adjacent PREBUILD text
was stale/intermediate data, not the exact final filename.

After the user restarted through Steam's standard Mods option without VR,
PID 29736 had `-no-eac` in its command line and no HaloMCCVR module. Its native
clock was also uninitialized at tick zero. TID 25864 has the identical main
loading-wait chain; TID 13432 is in the same native failed-read loop. Its saved
filename string at `0x7FF402505248` is again `halo1\maps\beavercreek.map`, with
the same missing-file and invalid-handle errors. `nonvr-consistent/` and
`nonvr-baseline/` preserve this comparison. This disproves a VR-only cause for
the captured missing-file stall and does not justify changing launcher flags
or CE loading hooks as its fix.

The installation had CE campaign maps and no CE multiplayer maps, including
beavercreek.map. The Cursed Workshop first-mission metadata names `a10`; the
native preload dependency does not mean the user selected a multiplayer map.
The [mod author's installation instructions](https://www.patreon.com/infernoplus/posts/cursed-halo-81524237)
explicitly require **both CE Campaign and CE Multiplayer**, including for
Game Pass. After the user downloaded that pack, beavercreek.map exists at the
requested path (14,047,972 bytes, installed September 16 at 01:45:29 local time).
The user confirms this was the missing dependency and loading succeeds.

PSS clone/context and stack capture are diagnostic evidence, not a replacement
for a debugger's complete unwind. Some active OS-formatter samples stop on an
unsupported/inconsistent frame; successful native-loop unwinds, counted heap
strings, repeated waiting stacks and the non-VR reproduction corroborate the
conclusion. The VR consistent metadata was reconstructed from retained raw
contexts after a later failed capture overwrote its metadata filename; the raw
context/stack/pointer files and completed original unwind output were preserved.
All snapshot and process handles were released. No game memory or files changed.

## User's post-install VR logs

Preserved under `out/test-runs/d7dbfcb-ce-multiplayer-installed-20260916/`:

| Log | SHA-256 |
| --- | --- |
| run-17196.log | `145AF308865000E7ADFFAA1673322D14B5B086F1A8F0CC38233D4ED5197C32B3` |
| run-30592.log | `EF9FCFC91E5750B0A9EAEE02CBFE3D9022AD276121A4637FF0F7777A13349B53` |

Both identify d7dbfcb, Steam, SteamVR/OpenXR 2.17.10, Oculus-family at 90 Hz,
2912x2100. The longer run completes **2,610 Classic stereo pairs, zero drops
and source misses**, 2,595 control applications, 59 aiming applications,
4,102 native movement applications and 5,012 authored crosshair captures.
No nonzero CE exception or feature-fault counter appears. The shorter run
completes 244 Classic pairs with one dropped frame, then vehicle/pause states.
Late heartbeat expiry/presentation gaps follow pause near the logs' ends;
they are not evidence that initial custom loading still failed.

The longer log's FP palette counter is 5,102 observed, zero transformed; contact
publication and queries are also zero. The unchanged first-person adapter can
decline unknown/ambiguous rig nodes or invalid context, and publishes contact
only after a successful palette commit. These logs do not identify the exact
decline reason or verify custom hand/gun transforms or physical contact. The
relevant FP source is unchanged from d47a98c through d7dbfcb. Do not claim
universal custom-rig support or invent a new movement regression from this.

Scoped acceptance covers the user's loading result and earlier Anniversary
confirmation. Cumulative accepted pointer remains d47a98c without fresh Halo 3
and all-title coverage. The new archive rebuild still needs the normal test
handoff; broader custom rigs, contact, transitions and Store coverage remain open.

## Delivery verification

An independent code review found no concrete new regression in head-relative
movement, controller/body separation, temporary native movement preservation,
controls retirement or cold admission. Eight relevant existing CTest suites and
native packet, heading, movement, turn and salted-datum checks pass. All 140
pinned native contracts and generated-header consistency pass. The package
script repeats cumulative Release, all CTests and the Reach gate; final linked
unwind/archive checks and exact identity are recorded in
`out/ce-community-current-handoff.json` after completion.

Only documentation and package metadata are updated. Keep all d7dbfcb runtime,
launcher, configuration and tests intact; there is no justified engine workaround
for game content that was missing from the installation.
