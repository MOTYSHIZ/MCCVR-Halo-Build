# CE firing effects and authored first-person markers

September 15, 2026. Read-only native and official HCEEK evidence collected while
reviewing the Anniversary muzzle projection candidate. This is attachment-path
evidence, not a headset acceptance result. Pinned input identities are in
`HALOCE-EVIDENCE-MANIFEST.json`; no Reclaimer or archived console inputs were used.

## Official firing-effect definition

The official HCEEK executable's field at VA `0x00C76368` names the trigger's
`firing effects` block and references definition `0x00C75FB0`. That definition
names `trigger_firing_effect_block`, gives its element size as `0x84`, and points
to fields at `0x00C75F10`. Two shot-count shorts and `0x20` padding precede the
normal firing, misfire and empty tag references. Field `0x00C75F40` explicitly
describes the normal reference as the effect used when a loaded weapon fires.

The extracted official assault-rifle weapon tag contains the reference path
`weapons\assault rifle\effects\fire bullet`. The corresponding official effect
contains authored location names `primary trigger` and `primary ejection`.
These strings support the authored-marker route below; a string search alone
does not establish the effect's complete serialized hierarchy or every weapon's
Anniversary emitter conversion.

## Native shot-to-marker route

All addresses here are RVAs in the pinned CE retail module.

1. `0xB78DE8` selects a weapon trigger at stride `0x114` from the weapon tag's
   trigger data. It reads the trigger's firing-effect count at `+0x108` and data
   address at `+0x10C`, then selects an element at stride `0x84`. The normal
   loaded-fire path reads the effect datum at element `+0x30`; the neighboring
   misfire and empty paths read `+0x40` and `+0x50`. These are the datum fields of
   the tag references described by the official definition above.
2. That function invokes the native projectile trigger `0xB7A374` on its firing
   path and subsequently calls `0xB79A40` with the selected effect datum. The
   latter checks the tag group: `effe` invokes `0xB230B0`, while a sound tag takes
   the native sound path. Firing-effect creation is therefore a sibling of the
   projectile work inside the trigger operation, not a guessed projectile hook.
3. `0xB230B0` creates the effect record, stores its attachment object at `+0x48`,
   resolves its first-person user at `+0x58`, and initializes location heads.
   It calls `0xB2420C` with world marker query `0xB3701C`; if the first-person user
   is valid, it also calls it with first-person marker query `0xB2804C`.
4. `0xB2420C` reads the effect tag's location count at `+0x28` and location data
   at `+0x2C`. For each `0x20`-byte authored marker name, it invokes the selected
   marker callback against the attachment object and supplies the returned
   marker records to `0xB24314`. It explicitly distinguishes callback
   `0xB2804C` when passing the first-person flag.
5. `0xB24314` stores the marker's local transform in a `0x3C`-byte effect-location
   record and encodes first-person node ownership with bit `0x8000`. The existing
   native consumer evidence for `0xB25EB0` and `0xB23898` shows that this signed
   node representation selects the first-person palette at user `+0x1088`.
   The existing official HCEEK marker homolog and retail `0xB2804C` evidence
   establish its use of the first-person node remap and palette.

This connects a weapon firing-effect selection to an authored marker and the
native first-person palette that the VR rig already replaces. It does not
identify a single universal hard-coded muzzle offset.

## Anniversary emitter selector ownership

The native emitter update at `0x671BA0` resolves the emitter's parent through
`emitter+0x20`. Its final branch at `0x672790` tests parent flag `0x10000000` at
`parent+0x28`. If set, it compares the parent's name at `parent+0x80` against the
seven bytes `shells\0` at module RVA `0x1827908`. A different name writes exactly
`1.0f` to `emitter+0xE8` at `0x6727F6`; an unset flag or the excluded name writes
exactly `0.0f` at `0x6727D3`. This directly establishes both the first-person
ownership meaning and canonical zero/one values of the selector copied by the
Anniversary particle draw. The update's final ownership branch changes only
that selector.

Unused emitter records do not supply uninitialized selector values: particle
draw `0x6770D0` zeros a complete `0xB44`-byte emitter table at `0x67738F`, copies
that initialized table at `0x6773FE`, then fills active records. The upload
copies its `0xB40`-byte emitter-record portion after the 21-vector header. This
supports validating all nine selector slots, including unused zeroed slots.

## Excluded lead and remaining scope

`0x7D350` explicitly queries the marker named `flashlight`, including its
fallback at `0xB37178`. Its use of palette `+0x1088` is evidence for the
flashlight path only. It must not be cited as muzzle-flash attachment proof.

The native chain above and the separate Anniversary particle upload/shader
evidence answer different questions. This chain establishes authored attachment
ownership; the upload review establishes which projection the particle shader
selects. They do not prove that every Anniversary effect has the same upstream
transform conversion, nor that the reported visual mismatch is completely
resolved in the headset. No acceptance pointer advances on this evidence.

## Preserved local inspection output

- `out/ce-muzzle-review-official-fields.txt`: official field pointers and tag
  string locations.
- `out/ce-muzzle-review-trigger-fire-event-native.txt`: `0xB78DE8` and `0xB79A40`.
- `out/ce-muzzle-review-trigger-effect-owner-native.txt`: `0xB230B0` and native
  weapon/attachment-owner helpers.
- `out/ce-muzzle-review-effect-routing-native.txt`: `0xB2420C` and related
  effect creation, including the distinct `0xB22FA8` entry.
- `out/ce-muzzle-review-effect-creation-native.txt`: `0xB24314`.
- `out/ce-refine-retail-fp-marker.txt` and
  `out/ce-refine-kit-fp-marker.txt`: existing native/official marker query proof.
- `out/ce-muzzle-review-effect-marker-native.txt`: excluded flashlight lead.
- `out/ce-muzzle-review-emitter-update-native.txt` and
  `out/ce-muzzle-review-emitter-setter-disasm.txt`: exact first-person selector
  producer and its native zero/one stores.
- `out/ce-muzzle-particle-draw-disasm-20260915.txt`: zeroed emitter-table setup
  and upload copy.

The retail decompilations use `tools/re/DumpFunctionAt.java` against the read-only
`CERetail` Ghidra project. Function-call argument recovery was checked against
the native call edges; untyped decompiler parameters are not independent ABI
proof.
