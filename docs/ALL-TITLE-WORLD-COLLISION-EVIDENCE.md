# All-title world-collision evidence

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

## Physical melee and isolation

The shared VR input path maps Quest lower-right-grip to
`XINPUT_GAMEPAD_RIGHT_SHOULDER` for these titles. A threshold crossing from
either tracked controller emits a short pulse on that same verified route;
the configurable range is 0.30-5.00 m/s and the default remains 1.20 m/s.

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
- Release build and core tests: pass for behavior commit `4e92be7`.
- H3/ODST central scheduler identity: exactly one match in each pinned retail
  image and at the mapped RVA.
- Reach consistency gate: required before packaging.
- Halo 2 checksum-selected weapon bounds: pending headset test in both
  renderers.
- Halo 3, ODST, Reach refined world collision: pending headset tests.
- Halo 4 accepted world collision/physical melee: required regression test.
- Accepted-build pointer: unchanged until explicit headset acceptance.
