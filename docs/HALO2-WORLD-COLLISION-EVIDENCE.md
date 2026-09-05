# Halo 2 world-collision evidence

## 1c08837 live battle-rifle diagnosis (2026-09-04)

The user accepts working hand contact and reports the SMG responds better than
the battle rifle. The supplied log proves the corrected tag-data slot installs,
but still records **zero** authored weapon bounds and 3,326 fallbacks. The SMG
response therefore does not prove the authored-volume reader works.

At the user's invitation, a read-only `ReadProcessMemory` check of the loaded
Halo 2 mission (PID 20764) located the native tag table and tag-data base through
the already-proven getter's two slots. No injection, game-memory writes, disk
patching, or EAC interaction was performed. Full render-model datum `FA361801`
has the following first 28 definition bytes:

```text
E6 2C 00 0F 0C 00 00 00 0F 10 1C 1D 00 00 00 00
00 00 00 00 01 00 00 00 64 96 2A 01
```

The loaded compression count/address are **+0x14/+0x18**, not +0x10/+0x14.
The old count was actually the preceding import-block address, zero in this
record. At byte-relative address `012A9664`, the six floats are
`(-0.1052100956, 0.1898390800, -0.0083235390, 0.0120922094,
-0.0138870794, 0.0852878466)`. They independently match the official H2EK
`fp_battle_rifle` XML's six rounded compression bounds. The model SID's length
byte is 15, also matching `fp_battle_rifle`. This is direct loaded-layout and
authored-content evidence; the earlier claimed +0x10 layout is disproven.

The failed layout is disabled in its own commit `b3fe7ea`, retaining its code.
The replacement reads the verified header, keeps count/address/finite/extent
guards, and reseeds on local-unit or held-weapon changes even when sample counts
are equal. Four-byte shifted, truncated, missing, or multiple records cannot
silently pass the new header regression tests. Fourteen bounds samples cover
the actual authored model, rather than an invented per-weapon length.

The read-only capture is preserved at
`out/h2-live-battle-rifle-proof.txt`, SHA-256
`0509DC79E90FFB8AC2AC4B874F55FED59B83BC77BC36BCFDD0F1B658892D76E0`.
Its last line deliberately prints the rejected old-field interpretation; its
MODE entries contain the corrected records. This proves the data read, not
headset-visible collision; Classic and Anniversary headset tests remain needed.

## ff6d1fd decoder failure and correction (2026-09-04)

The new Steam headset log still reports zero authored-bound publications; hand
collision remains accepted. Before diagnosing the compression block itself,
the native base-slot decoder must work. It did not: `graphGetAddress+20`
included the instruction's ModRM byte `05` in the displacement, and `+24`
was not the instruction end. The resulting RVA is `-0x1B299243`, outside the
module, so `g_halo2TagDataBaseSlot` was never armed and no bounds were read.
The previous claims that the byte-relative change had reached runtime bounds
were not supported by the log.

Pinned `halo2.dll` SHA-256
`DE65B4F4FDBF3F0A5EAB7431FE530DA17DD815599182DFD6AE9B7E21CF171946`
contains this exact native loaded-tag getter at `+0x79EEA0`:

```text
48 8B 05 89 5C E4 00 0F B7 C9 48 03 C9 48 63 44 C8 08
48 03 05 7F 5C E4 00 C3
```

The last instruction starts at `+0x12`, reads its signed displacement at
`+0x15`, and ends at `+0x19`. Its correct tag-data slot RVA is `0x015E4B38`.
The replacement validates the ADD opcode and RET, bounds-checks the result,
and logs Installed/StockFallback plus the resolved slot from the cold setup
path. It retains the previous decoder dormant. Unit tests use these exact
bytes and reject changed, truncated, and out-of-image cases.

H2EK's Classic consumer at `+0x2B6140` independently calls
`tag_get('mode', packet[0])`, confirming the packet supplies a render-model
tag. The existing guarded compression-block reader can now run after the
base binding succeeds. Actual bounds publication and gun contact in both
Classic and Anniversary remain headset tests; this correction does not claim
that those runtime results have already passed. Log identity and preservation
are recorded in `ALL-TITLE-WORLD-COLLISION-EVIDENCE.md`.

## Candidate scope

Halo 2 Classic/Anniversary hand collision, haptics, and physical melee were
accepted in the user's headset on 2026-09-03 from source `d35d078`. The
node-extrema weapon proxy was not: the gun still passed through geometry. This
successor keeps the accepted hand/melee transaction and replaces only that weak
weapon proxy with authored render-model compression bounds. Halo 4's accepted
implementation is unchanged.

The implementation is deliberately title-native. It does not copy a Halo 4
address, object layout, collision mask, render-model bound, or world scale.
Halo 3, ODST, and Reach are handled by their independently proven Gen3 paths in
`docs/ALL-TITLE-WORLD-COLLISION-EVIDENCE.md`. CE remains excluded.

## Pinned inputs

| Image | Bytes | SHA-256 |
|---|---:|---|
| Official H2EK `halo2_tag_test.exe` | 12,364,016 | `D0B71186D3948C48DDD02E2CCB88FA13E77E25A3D8F7FA60922F23A2A0073E36` |
| MCC `halo2.dll` | 15,807,960 | `DE65B4F4FDBF3F0A5EAB7431FE530DA17DD815599182DFD6AE9B7E21CF171946` |

H2EK supplied the semantics, ABI, result layout, caller mask, and object basis.
The stripped retail DLL was used only to map those already-understood
constructs and to prove unique signatures, in accordance with
`docs/EDITING-KIT-EVIDENCE.md`.

## Native collision transaction

H2EK `collisions.cpp` identifies the central vector test at kit RVA
`0x00103440`. Its six-argument ABI is:

`bool collision_test_vector(flags, start, delta, ignore1, ignore2, result)`

The result begins with type at `+0x00`, fraction at `+0x04`, position at
`+0x08`, and contacted object index at `+0x40`. The official H2EK melee caller
uses collision flags `0x2480000F`, ignores the local unit in the first slot,
and passes `-1` in the second slot. Independent instruction/data-flow mapping
places the retail homolog at `halo2.dll+0x0075B850`.

The production gate scans the complete entry identity and requires exactly one
match at that pinned RVA:

`48 8B C4 44 89 48 20 55 53 41 56 48 8D A8 A8 FB FF FF 48 81 EC 40 05 00 00 48 8B 9D 88 04 00 00 45 32 F6`

Zero, multiple, or moved matches disable only Halo 2 world collision and leave
the camera, stereo, first-person packets, aim, HUD, effects, and OpenXR active.
The stock call always runs first. Bounded mod probes run only from a witnessed
native collision-call engine context, at most once per 33 ms, and recursive
owned queries cannot start another tick.

## Visible hand and weapon volume

Both Classic and Anniversary consume the same final first-person packet
transaction already proven by the controller-hand implementation. After the
engine has composed the actual visible matrices, the collision publisher uses:

- one carrier root;
- minimum and maximum visible translations on X, Y, and Z;
- the engine-proven left/right hand subtrees; and
- the loaded weapon render model's one official compression record: eight
  oriented corners and six face centres, transformed by the visible gun root.

The `f5b3081` headset result proved the first retail address calculation was
invalid: both renderers logged zero authored-bound publications and 3,813
fallbacks. The H2EK render-model compression block is still at definition
`+0x10/+0x14`, but the candidate had treated its address as a word index and
multiplied it by four. The already verified generic loaded-tag resolver at
retail `+0x79EEA0` provides the decisive cache rule: it sign-extends the
stored address and adds it directly to the tag-data base, so the address is a
byte offset. The replacement uses that exact engine-proven calculation and
requires one sane authored record. Eight corners and six face centres are
transformed by the final visible gun root. A missing or invalid record falls
back to the accepted seven hand samples; it never copies a later-engine model
or invents a weapon length. A lock-free publication crosses to the native
collision callback. Results older than 150 ms, non-finite values, generation
mismatches, teleports, and excessive corrections are rejected. A valid
correction is applied only to the relevant controller carrier on a later
packet; stock packet production never waits.

## Dynamic objects

H2EK's four-argument local-velocity helper maps independently to retail
`halo2.dll+0x0090A000`:

`void object_set_velocity(object, forward, left, up)`

Its production identity is unique and pinned:

`83 F9 FF 0F 84 ?? ?? ?? ?? 48 8B C4 53 48 81 EC A0 00 00 00 44 0F 29 50 A8 44 0F 28 D3 44 0F 29 58 98 44 0F 28 DA 44 0F 29 60 88 44 0F 28 E1`

H2EK proves the object orientation basis at `+0x70` (forward) and `+0x7C`
(up), with left computed as `up x forward`. The existing signature-proven
Halo 2 datum accessor resolves the contacted object. The mod projects bounded
contact motion into that native local basis; it never writes object memory.
If the helper identity is absent, hand/weapon clamping and haptics remain live
while object nudging alone reports `StockFallback`.

## Input and failure isolation

The shared live VR input path maps Quest right lower grip/squeeze to
`XINPUT_GAMEPAD_RIGHT_SHOULDER` without a title branch. The accepted Halo 4
physical-melee route uses that same bit. No evidence supports substituting B,
X, or Y for Halo 2, so the Halo 2 swing pulse uses right-shoulder as requested
and stays disabled unless its collision transaction and final visible packet
are both active. Halo retains melee range, target selection, damage, lunge,
animation, sound, attribution, and networking.

Any fault in an owned probe disables only this optional collision transaction.
The thread-local recursion guard is restored in `__finally`; callbacks drain
before hook removal. No collision failure ends OpenXR, disarms Halo 2 camera
ownership, or suppresses stock collision.

## Verification status

- Prior `f5b3081` headset test: hand collision and physical melee remain
  effective, but authored weapon bounds were rejected (`0 / 3813`
  publications/fallbacks).
- Release build: passed for behavior commit `4e92be7`.
- Core tests: passed, including fraction/skin, finite-value, and visible
  extrema behavior. Runtime still gates the authored compression record for
  count, address, finite bounds, ordering, and maximum extent.
- Signature gates: required at runtime and at packaging.
- Headset acceptance: hands, haptics, and physical melee accepted; authored
  weapon bounds pending for both Halo 2 Classic and Anniversary.
- Required regression: Halo 4 world collision and physical melee must remain
  unchanged in a headset test before this can advance the accepted pointer.
