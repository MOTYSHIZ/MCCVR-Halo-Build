# Reach direct-drive: locating the replicated angular control record (2026-09-19)

Handoff / plan doc for the direct-drive (`Actuation::WriteState`) rung of the Reach aim provider
(`src/dll/reach_aim_provider.{h,cpp}`). Findings are from a read-only review of the repo; **no game
process was opened and no installed file was written.** This doc does not change code -- it exists so
the direct-drive write has a target and a verification plan ready for the next co-op session.

## Status in one line

`AimCaps.can_write_state` for Reach is **false**, and it must stay false until the record below is
located AND co-op host-follow-verified. Actuation is the closed-loop stick loop
(`Game_ComputeAimStick`), which the provider already delegates to (no-op takeover, commit `10b3512`).
Everything here is the work that would let that flag flip.

## What is already established (don't re-derive)

- **Unit/player pointer path is fully resolved and guarded.** `playerUnitByOutputUser(0)` (native,
  signature-scanned; RVA `0x00053EF8` is a cross-check anchor only) -> datum handle ->
  `ReachVehicleObjectData(handle)` (`src/dll/game.cpp:24652`): `gs:[0x58]` TLS ->
  `TLS[*(base + 0x00C17B18)]` -> `+0x10` object collection (validate stride `@0x20 == 0x18`, count
  `@0x44`, entries `@0x50`) -> `entries[(u16)handle * 0x18]` -> salt `@0x00 == handle >> 16` -> **data
  ptr `@0x10`**. Seated uses `vehiclePlayerUnitByOutputUser(0)`; same collection. Arming gates on a
  stack of agreement checks (AOB + rel32 call-edge + TLS-index decode + descriptor fields), fails to
  `StockFallback` on any mismatch (`game.cpp:28071-28135`).
- **`unit+0x214` is the DERIVED aiming vector, not the wire record.** Three floats, x@0x214 / y@0x218
  / z@0x21C (`kReachUnitAimingVectorOffset`, `src/common/reach_vehicle_logic.h:751`). It is what HREK
  `weapons.cpp` reads to build the projectile forward (comment at `reach_vehicle_logic.h:748-750`).
  **Every use in the mod is a READ** (`ReachReadUnitAimingVector`, `game.cpp:24718-24741`; published as
  steering feedback `game.cpp:21831-21846`). Nothing writes it. `0x210` is unreferenced by any Reach
  path; `0x218`/`0x21C` are just y/z of the same vector.
- **The yaw reference is a camera-forward mirror**, not a unit field: `g_gameYawRef =
  atan2(fwd.y, fwd.x)` of the compact render-camera forward at `+0x0C`
  (`reach_render_logic.h:388`, `game.cpp:21804-21817`). `ReachReadYawReferencePair` just unpacks a
  packed 64-bit atomic of that (`game.cpp:18104`).

## The blueprint: CE already direct-drives this, and it is headset-proven

Halo CE is the shipped template to copy (`src/dll/haloce_unit_control.cpp`,
`docs/HALOCE-UNIT-CONTROL-2026-09-16.md`). The mechanism:

- The engine's per-player update `0xad0720` (`unit_control_player_update`) receives a `control_data`
  packet (`std::array<uint8_t,0x50>`) and copies its fields into the unit via `0xafe098`
  (`unit_control_set`), **without modifying the source packet**.
- The mod hooks that update (`UnitControlBody`, return site `0xad0d5b`), copies the packet privately,
  overwrites only the desired fields, and calls native -- so native validation, interpolation,
  grenade timing and replication all stay engine-owned.
- Packet -> unit field map (verified `HALOCE-UNIT-CONTROL-2026-09-16.md:33-40`):
  desired **facing** packet`+0x1c` -> unit`+0x204`; desired **aiming** packet`+0x28` -> unit`+0x210`;
  desired **looking** packet`+0x34` -> unit`+0x234`; throttle packet`+0x0c` left byte-identical.
- **The desired/current relationship is the key fact** (`HALOCE-UNIT-CONTROL-2026-09-16.md:54-57`):
  retail unit update `0xafbe54` consumes **desired aiming at unit`+0x210`**, and its zero-rate branch
  copies it to **current aiming unit`+0x21c`** -- which is what the firing/grenade path reads.

So for CE: **desired aiming (`+0x210`, the control input written from the replicated packet) ->
integrated by the sim -> current aiming (`+0x21c`, derived).** Halo 2 has the sibling reference
(`player_control.cpp +0x72C30` converts local-player desired angles into the view vector,
`src/common/halo2_render_logic.h:63`).

## The analogy for Reach

Reach's `unit+0x214` is the analog of CE's **current** aim (`+0x21c`) -- the derived, downstream side.
**The record we need to write is the *desired* side**, one step upstream, which is the thing that
replicates. It has NOT been located. Reach's struct layout is NOT CE's, so the CE offsets
(`+0x210`/packet`+0x28`) are analogies to guide the search, **never values to assume**.

## Candidates, ranked (all require RE; none located yet)

1. **Reach's player-control desired-angle record (most likely replication-authoritative).** In Blam
   MP the player action input replicates and the host integrates it. The documented Reach entry
   points are the HREK **desired-angle path `0x1E770F` / `0x1E7934`**
   (`docs/REACH-VEHICLE-EVIDENCE.md:914-915,1247`: "the desired-angle consumer at 0x1E7934 ...
   subtracts its result from the target"). Reverse `0x1E7934` to find the per-output-user yaw/pitch
   struct. This is the field "on the wire."
2. **Reach unit *desired aiming* vector (control input the sim integrates into `+0x214`).** By the CE
   analogy a desired-aim vector should sit at a unit offset written by the unit-adjust / desired-angle
   consumer (`0xD67EE0` / `0x1E7934`, `REACH-VEHICLE-EVIDENCE.md:912-919`). Offset must come from RE.
3. **Follow CE wholesale.** Find Reach's `unit_control_player_update` equivalent, hook it, overwrite
   the desired facing/aiming fields in the copied packet. This drives the record the engine itself
   replicates and validates -- the shipped, proven pattern.

## Concrete next RE steps (headless; no game needed to start)

- Disassemble `0x1E7934` (desired-angle consumer) and `0xD67EE0` (unit-adjust body) in `haloreach.dll`
  (pinned identity in `docs/REACH-SIGNATURE-EVIDENCE.md`) to find the desired-aim source struct/offset.
- Locate Reach's control-packet update (the `0xad0720` analog) and its packet layout; identify the
  desired-facing / desired-aiming field offsets.
- Express whatever is found as a signature + agreement-gated resolve, never a bare constant
  (mirror the pointer resolver's discipline, `game.cpp:28071-28135`).

## Validation plan (the gate before `can_write_state = true`)

1. **Arming discipline** -- same as the existing pointer resolver: unique AOB + rel32 call-edge +
   TLS-index decode + descriptor-field agreement before the write path arms; fail to the stick loop
   on any mismatch. All reads/writes behind SEH + finite/length guards.
2. **Value-agreement against `unit+0x214`** (the derived vector we already read): a true *desired* aim
   should EQUAL `+0x214` when the player is not turning, and LEAD it by the turn rate when turning
   (the CE `+0x210` -> `+0x21c` relationship). Cross-check against the camera-forward yaw (`+0x0C`).
   "Does it move?" is not enough -- the neighbouring field moves too.
3. **Decisive: co-op host-follow test.** Write the candidate on a CLIENT and confirm the host / other
   clients see the unit's facing change. That is what proves the field is replication-authoritative
   and not a local mirror. Split-screen cannot show this -- it needs a second machine.

## Replication-safety gate (standing principle)

Only build direct drive on the replicated control-record layer. If RE shows the located Reach field
is a **local mirror** and not wire-authoritative (the co-op test fails to propagate), do NOT adopt it
as the direct-drive foundation -- keep the stick loop, which already replicates correctly because the
engine integrates it. Direct drive's whole objective is to be upstream of replication (responsive,
multiplayer-correct, controller-sensitivity-independent); a field that fails host-follow delivers none
of that and silently desyncs co-op.

## Read next

`docs/REACH-VEHICLE-EVIDENCE.md` (unit-adjust + desired-angle path, `0x1E770F`/`0x1E7934`,
`0xD67EE0`), `docs/HALOCE-UNIT-CONTROL-2026-09-16.md` + `src/common/haloce_unit_control_logic.h` (the
direct-drive model + desired/current offset relationship), `src/dll/reach_aim_provider.{h,cpp}`
(goal/status), `src/common/reach_vehicle_logic.h` + `reach_render_logic.h` (offsets/RVAs),
`docs/REACH-SIGNATURE-EVIDENCE.md` (module identity + arming discipline).
