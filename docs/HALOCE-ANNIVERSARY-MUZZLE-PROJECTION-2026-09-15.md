# CE Anniversary muzzle projection correction

September 15, 2026. Candidate evidence, not headset acceptance. The user reports
that `884de13` injects successfully in Original CE, with correct hands and muzzle
flashes. Anniversary freezes after switching graphics; its earlier visible
muzzle flash followed the gun but missed the gun's muzzle. The separate unsafe
Anniversary HUD replay rollback addresses the newly reached rendering path.
This document covers the independent optional muzzle projection correction.

## Halo 3 behavior being matched

An authored muzzle flash stays attached to the tracked gun and uses the same
eye projection as its visible muzzle. Native trigger timing, particle animation,
shot spread, inherited velocity and projectile-origin policy remain native.

## E-CE-FP-9: separate Anniversary particle projection

The mesh correction already selects the primary-eye world lens for Anniversary
GLT, ZFILL and SFX first-person geometry. It does not cover the separate native
particle shader. Original CE already redirects the particle/light-flare fixed
lens callsites, consistent with the user's positive Original result.

The actual Anniversary `particles.sdc` vertex shaders retain reflection:

| Buffer or field | Native layout | Meaning |
| --- | --- | --- |
| `CB_PASS_PARTICLES` | 201 float4 vectors | Complete particle constant block |
| Projection pair | `+0x00`, `+0x40` | World and fixed first-person projection |
| Emitter table | `+0x150`, size `0xB40` | Nine records, 20 float4 vectors each |
| Per-emitter selection | Record `+0xA0`, first lane | Greater than 0.5 selects fixed first-person lens |

Pinned native draw `0x6770D0` copies emitter `+0xE8` to that selector at
`0x6776CE..0x6776DC`. It zeros every new `0xB44` CPU batch at `0x67738F`, copies
the initialized batch at `0x6773FE`, then fills active emitter records. Unused
emitter slots are therefore zero, not arbitrary pool data. The complete emitter
table is copied into the constant block at `0x6786F3`.

Native emitter update `0x671BA0` supplies canonical zero/one values. Its tail
tests the emitter's parent object flag `0x10000000` and compares the parent's
name against the seven bytes `shells\0`. A flagged parent with a different
name produces one at `0x6727F6`; the other path produces zero at `0x6727D3`.
Thus native shell exclusion and world-emitter selection are preserved by the
copied selector correction. The producer, initialized unused slots and shader
consumer are all verified; zero/one admission does not assume initialized pool
memory.

Shader `0000-000` selects between the common world/FP matrices using the emitter
selector. Shader `0000-001` selects between the particle block's world/FP pair
using the same selector. The latter is a sprite shader, distinct from the model
particle shader. Clearing only the copied selector therefore corrects both
consumers; changing only one matrix pair would miss the other consumer.

Native `0x6770D0` explicitly constructs a private camera with vertical FOV bits
`0x425DA6D7`, computes horizontal FOV, and creates its second projection through
`0x2EBBE0` at `0x6784AA`. The supplied primary camera creates the first projection
at `0x67835F`. This is a concrete mismatch with the tracked gun's world lens.

### Attachment and camera-transform trace

The actual trigger-to-authored-marker route is documented separately in
[HALOCE-MUZZLE-TRIGGER-EVIDENCE-2026-09-15.md](HALOCE-MUZZLE-TRIGGER-EVIDENCE-2026-09-15.md).
The firing-effect selection reaches the native effect-location query and the
first-person palette already moved by the VR rig. `0x7D350` is a **flashlight**
query and is excluded from muzzle evidence.

Anniversary particle callback `0x497BB0` passes its supplied camera through
visibility/preparation `0x675F10`, collection `0x675C10`, and emitter matrix
preparation `0x6762D0`, before drawing. The matrix preparation uses the supplied
camera's `+0x00` and `+0x40` matrix blocks with the native emitter/object
transforms, then transposes into emitter `+0x48..+0xA4`. It does not read emitter
`+0xE8` or camera FOV `+0x14C/+0x150`. The fixed camera is constructed later
inside particle draw for projection only. The correction preserves all these
native matrix/particle calculations. This does not establish every custom
Anniversary effect's upstream asset conversion or guarantee perfect alignment
for every animation without a headset result.

## Production transaction

The new independent `first_person_particles` binding group verifies three
unique function prefixes, complete access/callsite witnesses and buffer-vtable
pointers against the pinned module. They are recorded in
`HALOCE-FIRST-PERSON-PARTICLE-CONTRACTS.json` and generated from the authoritative
manifest into `haloce_contracts.generated.h`.

Two optional detours cover particle draw `0x6770D0` and native constant-buffer
commit `0x202B10`. Only commit return address `0x678796` inside that particle
draw is admitted. The draw scope requires both the exact primary camera reader
and the current material-eye reader. Both are revalidated before submission;
auxiliary cameras, output intervals, nested masked frames, stale reference/
generation/space/serial and blocked controller presentation stay stock.

The buffer has a native `0x30`-byte descriptor: vtable at `+0`, dirty start/end
at `+8/+0xC`, capacity at `+0x10`, CPU backing pointer at `+0x18`, GPU resource
at `+0x28`. Admission checks the vtable, non-null backing/GPU, overflow-safe
pointer bounds, `0 <= start <= end <= capacity <= 4096`, and exactly 201 dirty
vectors. All nine selector lanes must be canonical zero/one before changing any.

Submission copies the **complete capacity** into a private bounded stack buffer
and duplicates the descriptor, preserving its GPU resource. Only the nine
selector lanes belonging to the current allocation can change. The native
commit receives this private descriptor. Its D3D11 fallback uploads the entire
backing buffer, so a 201-vector-only snapshot would have been incorrect when
capacity exceeds the dirty allocation. The D3D11.1 path instead uses native
dirty byte-box bounds. Both paths synchronously consume CPU data.

Native commit changes only dirty start/end on normal completion. The adapter
copies those two fields back only if the original descriptor still matches its
pre-call snapshot. Native exceptions propagate and discard the private copy.
Nested calls receive independent stack storage; no native emitter or backing
bytes, palette data, camera state or GPU resource ownership are changed.

The bound is a 64 KiB stack snapshot and copy per admitted particle batch. This
is a deliberate isolation cost, not a measured performance improvement. No heap
allocation, lock, COM query, file I/O, scanning or logging is added to the hooks.
Cold polling reports installation, observed draws, rejected eye scopes, admitted
emitter totals and upload fallback counts. A missing particle-stage eye receipt
is therefore visible even when no upload reaches the correction.
Optional installation/retirement failures keep particle projection stock and
cannot disarm stereo, hands or controller aim. All twelve first-person detours
are disabled before the two bounded quiescence batches permit teardown.

## Local verification and limits

- Production first-person runtime tests cover full-capacity/private source
  preservation, nonzero allocation offsets, nested auxiliary draws, native
  exceptions, a newer descriptor taking ownership before completion, revoked
  eye scopes, invalid selectors and malformed signed bounds. The existing
  actual core tests cover native camera-stage/auxiliary/nested/output scope
  ownership; a prepared frame alone cannot authorize this effect.
- Pure helper tests check all nine mixed selectors, unrelated bytes, invalid
  count/pointer, NaN/infinity and all-or-nothing rejection.
- `tools/re/test_ce_particle_commit_native.py` executes pinned native commit
  instructions in isolated Unicorn memory: 15 cases, 1,164 instructions,
  whole-buffer/partial-buffer capability paths, capacities 201/512/4096,
  several allocation offsets, mixed/all-one/all-zero selectors and clean
  no-upload state. Source descriptor/backing are mapped read-only. The external
  D3D services are explicit synchronous copy stubs; this is native CPU consumer
  verification, not a running-game test.
- `tools/re/test_ce_particle_shader_native.py` verifies the shader hashes and
  runs both actual native shaders on D3D11 WARP through the production selector
  helper. Each shader passes 72 draws spanning both eyes, three gun scales,
  three emitter indices, world/first-person ownership and stock/corrected lens.
  Old first-person output uses the fixed lens; corrected first-person output
  matches the same world projection as the gun. Unrelated constant bytes and
  world emitter selection stay unchanged.
- Cold pinned verification passes all 104 contracts and all 16 production
  mapped-image groups. The cumulative release/check/package record belongs to
  the candidate document; no accepted-build pointer advances here.

The reported freeze must still be retested after the separate replay rollback.
Anniversary entry/switching, firing near/far and moving/rotating/scaling the gun
remain headset acceptance checks. Native shot origin is not relocated by this
change. Broad weapon, reload and custom-effect coverage is not claimed.

### Preserved records

Native disassembly/decompilation:
`out/ce-muzzle-particle-draw-disasm-20260915.txt`,
`out/ce-muzzle-buffer-native-disasm-20260915.txt`,
`out/ce-muzzle-emitter-view-native-20260915.txt`,
`out/ce-muzzle-emitter-transform-native-20260915.txt`,
`out/ce-muzzle-emitter-pack-native-20260915.txt`,
`out/ce-muzzle-review-emitter-update-native.txt`,
`out/ce-muzzle-review-emitter-setter-disasm.txt`.
Native commit result: `out/ce-muzzle-particle-commit-native-20260915.json`.
Initial shader output: `out/ce-anniversary-recovery-particle-mesh.txt` and
`out/ce-anniversary-recovery-particle-sprite.txt`.

External shader fixture SHA-256:

- `particles/0000-000.dxbc`:
  `EAEAA5F4C5A00172CBA02951B3AC0A28DE5F12ED14A703C66E4EA590D57E68FC`.
- `particles/0000-001.dxbc`:
  `F6F30FAC1CEE4A32BDD7887E63771C3A465CC3728D610EB24C2FB4D843F73D36`.
