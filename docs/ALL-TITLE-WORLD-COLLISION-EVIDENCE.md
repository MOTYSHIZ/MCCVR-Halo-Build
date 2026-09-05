# All-title world-collision evidence

## ff6d1fd headset result and replacement evidence (2026-09-04)

The user accepts hands in the tested titles except ODST in the loaded level;
Halo 4 remains the explicitly accepted hand/weapon reference. Visible gun
collision in the other titles is not accepted. Source `ff6d1fd9ad9c87dde5b423119ab8b41bfe1b9e6d`,
Steam, SteamVR/OpenXR 2.17.8, Oculus-family headset at 120 Hz (model not named
by the log). Supplied log SHA-256:
`93C34B1174E03F3666476D02C9716E33920483556DD3B9E9E33F36487AA8546C`.
The separately checked installed DLL matches the candidate manifest:
`16FD06E8E76C8EB4F2E70E1E5074C70AFE0C0597FF90E60039CFA11A6C1AEE2A`.
Evidence is preserved in `out/test-runs/ff6d1fd-collision-partial-20260904/`.
The deployment backups available on this machine predate collision work;
comparison therefore uses the preserved supplied `64b9545` log alongside them.

ODST previously recorded 602 native queries and seven left-hand contacts in
one window. The new level repeatedly records zero queries despite roughly
726 authored-volume publications per window, and records no contacts or
corrections. The old `callbacks` metric is an in-flight count, not a cumulative
call count: its zero value does not prove the hook was never called.

Two concrete errors are independently established:

- The alleged H3/ODST central schedulers at `+0x1FE5D4` / `+0x230770`
  are only point-to-point adapters. They subtract start from desired, then call
  the actual start/vector collision entries at `+0x1FD748` / `+0x22F80C`.
  Direct vector users bypass the former hooks. H3EK resolve at `+0x64CF20`
  independently calls its vector engine at `+0x652A10` via `+0x64D055`;
  H3ODSTEK resolve at `+0x69DFE0` independently calls `+0x6A3B30` via
  `+0x69E115`. The retail adapter calls at `+0x1FE63F` / `+0x2307DB`
  verify the corresponding edges. The replacement hooks each title's actual
  vector entry, with a separate unique full entry signature and the native
  eight-argument ABI. The existing accepted-position resolver remains native.
  Query work is bounded to 33 ms and protected by a nonblocking single-worker
  lease; native calls never wait. Runtime telemetry now counts engine calls,
  admitted ticks, rejected publications, and reseeds separately. The precise
  contribution of scheduling versus publication rejection in the failed ODST
  level remains a headset/log validation item.
- Catalog lookup succeeded, but weapon bounds were composed using combined
  body `solved[0]`. The visible-palette contract is instead
  `destination[0] = root * source[boneMap[0]]`. Existing official HREK proof
  is recorded in `REACH-SIGNATURE-EVIDENCE.md`; ODST's pinned mapper at
  `+0x2EDD10` independently reads `boneMap[i]`, scales its index by `0x34`,
  and composes that source with the call's root. The replacement remembers
  the actual weapon callback's mapped root index and source pointer together
  with its title-generation/tag/checksum identity, then uses the corresponding
  solved bone in the combined publication. A different source graph, invalid
  map index, unknown checksum, or identity older than 150 ms falls back to
  hands. Shape identity changes reseed the sweep, preventing a sweep between
  unrelated gun volumes. No guessed weapon node is introduced.

The failed combined-body-root behavior and H2 decoder-dependent bounds were
disabled in their own commit `2aa4e8d` before this replacement. Dormant code
is retained. H2's separately verified decoder correction is recorded in
`HALO2-WORLD-COLLISION-EVIDENCE.md`. The requested default melee threshold is
now 5.00 m/s, including generated configs; saved custom settings still load.
Halo 4 collision geometry, query scheduling, and contact response are unchanged.
Its physical-melee threshold initializer follows the new shared default.

`tools/verify-world-collision-bindings.py` verifies pinned file hashes, both
unique vector signatures, each editing-kit and retail call edge, and the H2
tag-base decoder. Its output is preserved at
`out/collision-bindings-verified.json`. Core tests cover mapped-root rejection
and the exact H2 instruction bytes, including truncated/mismatched instructions
and an out-of-module result. Headset acceptance remains pending for ODST hands,
guns across H2/H3/ODST/Reach, and the Halo 3/Halo 4 regressions.

The first replacement package (`2bdd2c2`) passed build/tests/gate but was not
installed: the installer still required schema 35, five titles, and the old
H2-92/H4-58 candidate IDs. The installer now validates the current schema 41,
six title entries, current candidate IDs, and this candidate's collision
bindings. Source/HEAD equality, exact hashes, closed-game checks, backups,
all-present-edition installation and config preservation remain mandatory.

## Scope and player-visible target

This candidate extends the default-off `world_collision` and nested
`physical_melee` controls to Halo 3, ODST, and Reach. Halo 2 Classic/Anniversary
remain on their H2EK-proven path and Halo 4 remains on its headset-accepted
Stage 6/9 path. Combat Evolved is deliberately excluded.

The Halo 3 behavior being matched is its existing final-visible-palette
controller ownership: the same right/left hand and held-weapon geometry that
the player sees supplies collision samples, a valid engine contact corrects the
controller carrier, and a qualified OpenXR swing requests the same virtual
right-shoulder melee action already produced by Quest lower grip. ODST and
Reach match that player experience through their own title-native bindings.
Halo retains target selection, range, lunge, damage, animation, audio, impulse,
attribution, and networking.

## Pinned inputs

| Image | Bytes | SHA-256 |
|---|---:|---|
| Official H3EK `halo3_tag_test.exe` | 27,525,168 | `59A78F2C96034D7CEB5D710505B2B36813AA141FC81A083E3F952973DBCE4602` |
| MCC `halo3.dll` | 11,127,768 | `B209D8454B12DC77E54CCD2C9924EC8D44B8619D21CF98E36FFAF601E67EFB63` |
| Official H3ODSTEK `atlas_tag_test.exe` | 28,510,448 | `354EC94158AECCE3E9D0F6463023AD5FA6D2AFE49B390E6067EBC17465C63C2D` |
| MCC `halo3odst.dll` | 11,496,920 | `5BB20976EFDFD9E1CE59C589339804725FEC239021027C8D65B2733EAB94829A` |
| Official HREK `reach_tag_test.exe` | 36,935,408 | `CBDD8448A87A433B0DFFC0DE47D06DB7A18B4BF868B96B057135DAA86790ABA8` |
| MCC `haloreach.dll` | 13,229,016 | `738DD2D24EA3AEA12E1EE9AA4A61094BF116027D42004C35A19E5048608B0894` |

Editing-kit images supplied semantics, argument meaning, result behavior, and
render-model layouts. Retail images were used only to find and uniquely verify
their homologues.

## Native collision wrappers

H3EK `halo3_tag_test.exe+0x64CF20` and H3ODSTEK
`atlas_tag_test.exe+0x69DFE0` expose the same five-argument helper:

`bool resolve(start, desired, accepted, ignoreA, ignoreB)`

It runs the engine collision transaction, returns `desired` when clear, and on
contact publishes the hit position with a one-percent skin. Independent BSim
mapping places the Halo 3 retail homolog at `halo3.dll+0x1FFD18` (similarity
0.4442146591, significance 26.4361105). The same complete entry identity is
unique in ODST at `halo3odst.dll+0x231EC4`:

`48 8B C4 48 89 58 08 48 89 70 10 48 89 78 18 55 41 56 41 57 48 8D 68 A9 48 81 EC C0 00 00 00 48 8B 05 ?? ?? ?? ?? 41 83 CF FF 48 89 45 BF 49 8B F8 0F BA 6D BF 09`

HREK `reach_tag_test.exe+0x4166A0` proves the equivalent five-argument source
helper. Retail optimization specializes `ignoreB` to `NONE`, yielding the
four-argument homolog at `haloreach.dll+0x12C5D4` (BSim similarity
0.4246694684, significance 24.71684235). Its independent unique identity is:

`48 89 5C 24 20 55 56 57 41 56 41 57 48 8D 6C 24 D1 48 81 EC D0 00 00 00 48 8B 05 ?? ?? ?? ?? 48 33 C4 48 89 45 1F 41 83 CF FF 48 8D 45 AF 48 89 44 24 38 49 8B F8`

Reach therefore uses a dedicated four-argument detour. Copying Halo 3's ABI
would corrupt its call frame; this title-specific distinction is intentional.
Zero, multiple, or moved signature matches leave only world collision stock.

The `f5b3081` headset run proved that the valid H3/ODST wrapper entries above
are not active schedulers in retail gameplay: both titles published hundreds
of visible volumes per window while recording zero callbacks and zero native
queries. Following the official H3EK wrapper's call graph identifies its
eight-argument central vector test at `halo3_tag_test.exe+0x652A10`. The
verified retail homologues are `halo3.dll+0x1FE5D4` and
`halo3odst.dll+0x230770`; their complete 80-byte entry identity is byte-exact
and unique in each pinned retail image. The candidate hooks those active
central routines only to schedule the bounded work, while calling the
unhooked title wrapper above to preserve its accepted-position and skin
semantics. A thread-local owned-query guard prevents the wrapper's nested
central call from scheduling recursively.

## Visible volume and scheduling

Each title publishes from its already-proven final visible first-person palette:
seven fixed-semantic hand samples plus fourteen held-weapon samples. The
`f5b3081` headset log proved the first implementation's `+0x54/+0x58` reads
were wrong: those are editing-format tag-block fields, not retail cache
offsets, and all H3/ODST/Reach frames fell back to hand-only collision.

The replacement exports the official weapon `render_model` tags with each
title's own editing kit and records the exact compression-position bounds:
32 distinct H3/ODST checksum records and 29 Reach records. Retail's immutable
runtime-import checksum at loaded `render_model+0x08` selects the exact record.
Eight oriented corners and six face centres are transformed by the visible
model root. An unknown checksum fails open to hand-only collision; dimensions
are never inferred from another title.

Publications are lock-free. Native collision work runs only from a witnessed
engine collision callback, no faster than every 33 ms. The local player unit
is ignored. Results are generation-scoped, finite, no older than 150 ms,
bounded for teleport/drift/correction, and applied on a later visible solve.
Reach is deliberately different: its explicit prepared wrist targets bypass
the H3/ODST `DesiredWristWorld` consumer, which explains `f5b3081` recording
contacts but zero visible corrections. Reach now consumes the prior correction
at its outer-frame prepared-target boundary before the stereo pair begins.
Contact haptics use the accepted gentle 0.18 amplitude. No file I/O, logging,
allocation, lock, or signature scan occurs in palette or collision hot hooks.

### Held-model callback ordering (`64b9545` headset result)

The 2026-09-04 Steam / SteamVR 2.17.8 / Quest 3 run confirms that the refined
native contact schedulers and visible hand corrections work in Reach, Halo 3,
and ODST. It also isolates the remaining weapon failure to publication order,
not missing collision callbacks or insufficient correction strength:

- Halo 3 reports, for example, `355 published / 355 hand-only fallback` with
  five weapon contacts, then `363 / 363` with eighteen weapon contacts. A
  recognized held model and the combined body model alternate through the
  visible-palette callback, continuously changing the published sample count.
- A later Reach load similarly reports `119 / 119`, `117 / 117`, and
  `122 / 122`; hand contacts and visible corrections work, but the paired
  hand-only publication replaces every weapon-bearing packet before it can be
  consumed reliably.
- ODST records hundreds of native queries and working hand correction but zero
  authored-bound publications against 534 or more hand-only fallbacks. Its
  combined body callback consistently consumes the context before the later
  held-weapon callback.

Offline inspection of the pinned retail visible-palette functions independently
confirms that the low word of every callback's first argument is the submitted
render-model tag index. The existing loaded-tag resolver therefore can identify
the held model on the later callback even when that callback is not the one
which owns the combined publication.

Behavior commit `f1dc2bee254572299e789f5c1fdbb536d99f6cbb` makes each title
remember every exact catalog-recognized submitted weapon tag. The next combined
publication may use that identity only when it belongs to the same nonzero title
generation and was observed no more than 150 ms earlier. Before use, the loaded
tag's immutable checksum is read again and must still equal the remembered
checksum and an exact title catalog entry. A weapon change or unknown model
therefore expires safely to hand-only rather than borrowing stale geometry.
The hot path remains lock-free, allocation-free, and log-free.

Supplied runtime log SHA-256:
`F8D236E4FB0150020FC280E93C08AB2B07644C36D7ECE9F2FAF846DA3AD00227`.

## Physical melee and isolation

The shared VR input path maps Quest lower-right-grip to
`XINPUT_GAMEPAD_RIGHT_SHOULDER` for these titles. A threshold crossing from
either tracked controller emits a short pulse on that same verified route;
the configurable range is 0.30-5.00 m/s and the default remains 1.20 m/s.

The supplied Samsung Odyssey log identifies SteamVR/OpenXR 2.16.7 and
`/interaction_profiles/microsoft/motion_controller`, then records 122 enabled
physical-melee telemetry windows with thousands of velocity samples but a
maximum reported peak of exactly `0.00 m/s`. This proves the failure occurred
before threshold qualification or XInput emission; changing the melee button
would not fix it. Some WMR paths advertise the OpenXR linear-velocity-valid
bit while returning a permanent zero vector.

The shared capture therefore computes a second velocity from consecutive
tracked positions and their OpenXR predicted-display timestamps. It admits
only finite 1-100 ms samples at or below 20 m/s. A meaningful native velocity
of at least 0.01 m/s always wins; the derived value is used only when native
velocity is absent or effectively zero. This keeps the accepted Quest motion
path unchanged while giving WMR/Vive-style SteamVR bindings a deterministic
fallback. The emitted melee action remains virtual right shoulder, independent
of where a runtime maps that action on its physical controller.

Each title owns an independent optional feature state and exact hook. A failed
signature stays stock. A guarded runtime fault disables only that title's
collision/melee feature. It never tears down the camera, ends OpenXR, or blocks
another feature. ODST and Reach include the optional detour, trampoline, and
callback count in their verified quiescence scans; Halo 3 revokes and drains it
before generic title hooks are removed.

## Verification status

- Prior headset result (`f5b3081`): physical melee accepted across supported
  titles; H2 weapons, H3/ODST world collision, and Reach visible correction
  rejected as described above.
- `64b9545` headset result: hand collision and visible correction confirmed in
  Reach, Halo 3, and ODST; held-weapon publication ordering fault isolated as
  described above.
- Release build and core tests: pass for held-model behavior commit `f1dc2be`.
- Release build and core tests: pass for behavior commit `4e92be7`.
- H3/ODST central scheduler identity: exactly one match in each pinned retail
  image and at the mapped RVA.
- Reach consistency gate: required before packaging.
- Halo 2 byte-relative authored weapon bounds: pending headset test in both
  renderers.
- Halo 3, ODST, Reach stable held-weapon collision: pending headset tests.
- Samsung Odyssey pose-delta velocity fallback: pending headset test; the log
  must report fallback activation plus a nonzero peak and swing crossing.
- Halo 4 accepted world collision/physical melee: required regression test.
- Accepted-build pointer: unchanged until explicit headset acceptance.
