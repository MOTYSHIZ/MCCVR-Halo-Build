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

## UPDATE 2026-09-19 (later): disassembly ran, candidate found, read-only probe shipped

Disassembled retail `haloreach.dll` (`G:\SteamLibrary\...\haloreach\haloreach.dll`, 12.6 MB) with the
repo's `tools/pedis.py` (capstone; `--module` override) + a scripted `.pdata`-bounded analysis. HREK
(`reach_tag_test.exe`) is not installed on this machine, but the unit **struct offset** is identical
between HREK and retail, so the retail binary answers it directly. Findings:

- **Reach's unit aim family is three (desired, current) vec3 pairs, each 0xC apart** (the Blam control
  model, matching CE): facing `0x1e4`/`0x1f0`, **aiming `0x208`/`0x214`**, looking `0x22c`/`0x238`. A
  spawn/reset broadcaster (retail `0x47D900`) initialises all six from one source vector, proving they
  are one family in one struct.
- **`unit+0x208` = DESIRED aiming; `unit+0x214` = CURRENT aiming** (current already established). Proven
  by the integrator **retail `0x480280`**, which `movsd`-reads `[r14+0x208]` and `movsd`-writes
  `[r14+0x214]` on the same base, and `lea`s both as vec3 pointers into its helpers -- the
  desired->current consumer, the exact analog of CE's `0xafbe54` (desired `+0x210` -> current `+0x21c`).
- So the direct-drive target (the thing to write) is **`unit+0x208`**, with `unit+0x214` as its
  value-agreement reference.

**Shipped a READ-ONLY probe to confirm this SOLO in headset (no co-op, no write):** config
`reach_aim_probe=1` (default 0), `reach_desired_aim_offset=0x208` (config-settable, hex ok). On the
engine thread it reads `unit+0x208` (candidate desired) and `unit+0x214` (known current) each ~0.5 s
and logs their agreement. Expected if `0x208` is right: both are unit-length, `dot`≈1.0 / `ang`≈0 at
rest, and the desired **leads** (nonzero `ang`) while you turn. A non-unit `des` vector prints
`offset likely wrong` -- then sweep `reach_desired_aim_offset` (try `0x1e4`, `0x22c`). This probe is
the value-agreement guard the write will gate on; confirming the offset read-only is the precondition
for ever writing. Code: `ReachAimProbe_OnEngineTick` in `src/dll/game.cpp` (guarded, SEH, throttled).

**The write is still the co-op step.** Once the probe confirms `0x208` in headset, the WriteState rung
writes the VR desired-aim vector to `unit+0x208` and the co-op host-follow test decides whether that
replicates (a client write that the host/other clients see) or is only a local mirror the packet path
re-stamps -- in which case the CE-style control-packet hook is the next RE. `can_write_state` stays
false until that test passes.

## UPDATE 2026-09-19 (evening): the unit aim vectors are DERIVED -- 0x208 write DISPROVEN

Built a guarded write test (`reach_direct_drive`, default 0) and ran it in-headset. Result:
**writing `unit+0x208` does NOT drive aim or the shot.** A per-tick persistence readback settled the
mechanism: the field reads back each frame equal to the current aim (`unit+0x214`), NOT the value I
wrote the previous tick -- `persistErr ~= 44-47deg` every frame (the exact 45deg offset I wrote is gone
by the next tick). **The game re-stamps `0x208` to match `0x214` every frame; it is a derived OUTPUT,
not a write target.** (An earlier swing-and-hold test with the stick SUPPRESSED made the shot look
pinned, but that was the suppressed stick freezing aim -- a confound the no-suppression "live offset"
test ruled out: no shot offset ever appeared.)

By strong inference the whole aim-vector family (`0x1e4/0x1f0`, `0x208/0x214`, `0x22c/0x238`, all set
together by the init broadcast at retail `0x47D900`) is derived and re-stamped from the control input
each frame. `unit+0x214` is itself "the derived aim vector" per `REACH-VEHICLE-EVIDENCE.md`, so writing
it from this hook would be re-stamped too; the integrator `0x480280` (reads 0x208 -> writes 0x214) is
therefore NOT the governing on-foot path.

**Conclusion: no unit-aim-vector write can drive aim.** The drive point is UPSTREAM -- the
player-control / desired-angle record the game integrates into the aim (also the field that
replicates, which is what direct drive wants anyway). That is candidate #1 below (HREK
`0x1E770F/0x1E7934`). Next RE: disassemble what WRITES `unit+0x214` on foot and trace its source back
to that control record -- and mind the thread, the write must land where the sim reads it, not from
the render hook. The mechanism test stays behind `reach_direct_drive` (default 0) as a diagnostic.

## UPDATE 2026-09-19 (night): HREK kit-mining FOUND the drive point (player_control desired_angles)

Installed the Halo Reach Mod Tools (`N:\...\HREK\reach_tag_test.exe`, 35 MB, SHA-256 `CBDD8448...` --
note: differs from the older recon's HREK build, so old HREK RVAs are NOT trusted; anchored on strings
instead). No PDB, but the kit binary carries near-symbolic assert/source strings, which named the
drive point directly (this is why CE was fast and the blind Reach path was slow):

- Source: `c:\mcc\qfe1\reach\shared\engine\source\omaha\game\player_control.cpp`.
- **The replicated control input is `player_control->state.desired_angles` -- stored as ANGLES
  {yaw, pitch}, not a vector.** Offsets read straight from the desired-angles validator (retail-shared,
  since struct layouts match HREK): **yaw @ player_control + 0x94, pitch @ player_control + 0x98**
  (yaw is wrapped/clamped at the validator top; pitch @ +0x98 is the field formatted into the
  `player_control->state.desired_angles.pitch` assert). Adjacent state fields: `throttle`,
  `primary_trigger`, `action_context`. Per-player base is `rdi+rbx` in the validators (a
  `player_control_globals` array element).
- The `boost ... clamping to avoid crashing networking` assert confirms `player_control->state` is the
  **networked** control state -- so writing `desired_angles` is upstream of replication AND of the
  unit aim-vector derivation. That is exactly what direct drive wants, and it explains why writing the
  derived unit vectors (0x208/0x214) did nothing.
- Update function name: **`main_player_control_update`** -- the CE-style hook point (overwrite
  `desired_angles` in the control state, call native, like `haloce_unit_control` does with the packet).

**NEXT (retail): (1)** resolve the per-player `player_control` base on retail `haloreach.dll` (find
`player_control_globals` / how the update fn gets it), **(2)** signature-match `main_player_control_update`
on retail, **(3)** hook it and overwrite `desired_angles = {yaw,pitch}` from the VR aim, **(4)** co-op
host-follow to confirm replication. Offsets 0x94/0x98 transfer as-is. Method win: kit assert-string
mining collapsed the multi-hop static writer trace into a direct by-name lookup.

## UPDATE 2026-09-19 (late): retail mapping DONE (hook fn + player_control resolution)

Mapped the HREK finding onto retail `haloreach.dll` (retail has asserts stripped, so anchored on the
shared struct offsets + the build-invariant angle constants HREK uses -- 2pi `0x40C90FDB`, +-pi/2
`0x3FC90FDB/BFC90FDB`, +-pi). Result:

- **Retail control-update / hook function = `0x1E0834`** (in the retail player-control cluster
  `0x1DE000-0x1E2000`, the analog of HREK's `0x1E7xxx`). It computes `desired_angles` from the input
  context (arg1 = rcx) and writes them to the player_control (arg4 = r9): **`mov [r9+0x94], <yaw>` at
  `0x1E0934`** and **`mov [r9+0x98], <pitch>` at `0x1E0C66`** (float bits via a GP `mov`, which is why
  the movss-only scan missed it). 0 direct callers -> a registered top-level update (matches the name
  `main_player_control_update`).
- **player_control resolution is SOLVED by the hook itself: it arrives in `r9` (4th arg)** -- no need
  to find `player_control_globals`. Retail confirms the shared layout: **player_control + 0x94 = desired
  yaw, + 0x98 = desired pitch** (radians, world frame).
- **Unique retail signature** (survives an RVA rot): `44 8b 51 08 41 83 cf ff 4d 8b f1 44 0f 28 c2 8b
  da` -> matches once at fn+0x38 (`0x1E086C`); function start = match - 0x38. (The bare prologue is
  NOT unique -- it collides with `0xC82A2`.)

**Step 3 design (the build):** inline-hook `0x1E0834` (resolve via the AOB, guarded). In the detour,
call the trampoline (native computes desired_angles from input), then when direct-drive is enabled +
on foot + player_control valid, overwrite `[r9+0x94]=yaw`, `[r9+0x98]=pitch` with the VR aim. KEY
simplification: `desired_angles` is stored as ANGLES, and `Game_ComputeAimStick` ALREADY computes the
world `desiredYaw`/`desiredPitch` -- so we write those two scalars directly, with NO vector/frame
convention to get wrong (unlike the failed unit-vector writes). This is upstream of both the aim-vector
derivation AND replication, so it should drive aim and be multiplayer-correct. **Step 4:** co-op
host-follow to confirm replication. Method note: kit angle-constants (build-invariant) are what mapped
the kit function onto the stripped retail binary.

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
