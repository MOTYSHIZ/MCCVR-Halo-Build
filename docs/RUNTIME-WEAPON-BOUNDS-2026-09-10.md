# Runtime equipped-model bounds and physical melee

The runtime weapon-bounds/melee-coverage implementation pass is complete locally
for H3, ODST, Reach and H4. H2 retains its separately live-verified runtime reader
in both renderers. New code builds and all three CTest targets pass. Headset
validation is pending; this does not accept secondary damage selection, arbitrary
custom mesh topology or every item on the wider physical-melee refinement list.

The behavior being matched is the accepted hand/gun world contact and native
contact-melee path: swept visible samples select the object actually touched;
Halo supplies damage, authority and effects. This change supplies missing gun
geometry to that same path, including when world collision is off and physical
melee remains on. It adds no engine hook or damage call.

## Halo 4 native proof

Official H4EK halo4_tag_test.exe SHA-256 is
B7468DB9FD160B035C329540EE0B0D47BCF609E1BA6E85AE4F204B70661113A6.
The source-named compression assertions lead to function 1572A0. Its model
lookup requests group mode, reads the compression block at model +0x80 with
52-byte elements, and branches on flags & 5. Ordinary data uses interleaved
position min/max at +4..+1B. The optimized union uses extents XYZ at +4/+8/+C
and minima XYZ at +14/+18/+1C. Full evidence is preserved in
out/weapon-runtime-h4-functions.txt.

Independently verified retail halo4.dll SHA-256:
7C53E7D5BC9848545A1B70E2768242479336FBA1B7630D7AB955F7FD0C34FA84.
Retail renderer 344534 resolves the submitted model through the proven tag
table and selects its geometry at model +0x64 (34459C adds 0x19 packed dwords).
344651 reads geometry +0x20, therefore model +0x84, as the compression block's
packed address. It decodes that address through group-base table 496A180 and
calls 387AB8. This native shader-constant writer reads min/max XYZ at the
documented offsets and uploads minima and extents. The geometry consumer in
3472F0 independently indexes compression records with 13 packed dwords (52
bytes). See out/weapon-runtime-h4-{retail,resolver,model-geometry}*.txt.

Three unique instruction windows are verified cold before enabling the optional
reader. Failure retains the previous catalog path locally. The offline verifier
tools/verify-halo4-runtime-weapon-bounds.py passes uniqueness, decoder call edge,
packed table and official-kit identity checks. No live process was inspected.

## Implementation and limits

The admitted same-frame held-model record passes its exact descriptor to the
reader. Bounded reads decode up to 64 compression records, union their authored
bounds, and recheck checksum/block identity before committing. Unknown checksums
and custom models can provide geometry without a catalog entry. Shape identity
includes the model tag, checksum and decoded dimensions so weapon/model changes
reseed both world collision and melee rather than sweeping an old shape into
the new one. Refusals are counted and cold-reported; malformed/unreadable data
retains catalog geometry when recognized, otherwise hand-only contact.

The existing 14-sample oriented-bounds proxy is preserved. This is not exact
triangle collision, a newly authored physics body, or proof of animated/custom
multi-root mesh coverage. No guessed dimension or cross-title layout is used.

The supplied log has 1,752 native-melee telemetry windows with zero fault
windows, 23,740 queries, 199 contacts, 17/66 left/right submissions, five native
rejections and zero queue drops. It does not show wholesale failure of native
damage. The missing model catalog entries are the strong coverage lead.

Release build, all three CTest targets, Reach consistency check and diff check
pass. A regression specifically verifies weapon-only contact with a stationary
wrist, one damage submission, duplicate-eye suppression and safe reseeding on
geometry change. Tests also cover raw/optimized decode and invalid data.
Logs: out/continuation-20260910-h4-melee-bounds-{build,tests}.txt.
No package, installation, game launch or accepted-pointer update.

## H3, ODST and Reach native proof and implementation

H3EK 617843, ODSTEK 8D7B81 and HREK 6ED9FE independently access the model's
render_geometry.meshes count at +0x68, identifying geometry at +0x64. Their
compression functions H3EK 6F5250, ODSTEK 7535B0 and HREK 3FF3A0 assert
geometry->compression_infos.count at geometry +0x10. H3 and ODST explicitly
request 44-byte records; HREK requests 52-byte records. All use position flag
bit 0 and interleaved float min/max XYZ +4/+8, +C/+10 and +14/+18. H4's optimized
union is NOT used to interpret any of these three titles.

Retail H3 2B5820 resolves the model using the title's tag table and data-base
slot 1FCF4C8, loads model +0x78, and calls decoder 28E8E0 at 2B58C9. ODST
2DD78C independently resolves the model using data-base slot 2022AA8, reads
model +0x78 and calls 2B9B70 at 2DD7F0. Both native decoders upload the same
independently identified position minima/extents. Reach 29A00B..29A035 passes
model +0x64 to its geometry renderer; 2560A3..2560D1 reads geometry +0x14,
indexes records by 13 packed dwords and resolves through group-base table
4E39F20, then calls 284FDC. That decoder reads all six position bounds. This
matches the HREK layout; retail was used to match, not invent, Reach behavior.

Primary output: out/weapon-runtime-{h3,odst}-kit-compression.txt,
out/weapon-runtime-reach-kit-compression.txt, and corresponding model, render
and decoder outputs. Official kit hashes and native call edges are pinned by
tools/verify-legacy-runtime-weapon-bounds.py. Retail hashes:

- H3: B209D8454B12DC77E54CCD2C9924EC8D44B8619D21CF98E36FFAF601E67EFB63
- ODST: 5BB20976EFDFD9E1CE59C589339804725FEC239021027C8D65B2733EAB94829A
- Reach: 738DD2D24EA3AEA12E1EE9AA4A61094BF116027D42004C35A19E5048608B0894

Each title checks three unique native windows cold. Missing/moved/ambiguous
proof disables only this reader and logs catalog/hand fallback. Teardown clears
the proof. No hooks or engine damage calls are added. Up to 64 records are
read safely, unioned, checked for finite/order/overflow and revalidated against
the model descriptor, checksum and compression header before publication.

Unknown weapons are admitted only from the actual native appended held graph.
H3/ODST learn a complete body remap containing both native wrists in the same
source/generation/slot, then require the held model's root at that body's end,
with every other mapped node before camera-control and outside the body.
Reach uses its already proven body-layout boundary. Optional unmapped nodes
are allowed. A partial/malformed body map cannot classify an unknown gun. The
recognized catalog remains a fallback. Observation snapshots use an atomic
sequence to reject mixed root/tag/source reads. Shape identity includes tag,
root, checksum and dimensions, preventing weapon changes from generating hits.

The existing Halo3PublishContactHand, OdstPublishContactHand and
ReachPublishContactHand all call the same mapped-bound builder before their
native contact-melee queues. This also runs with world collision off. Regression
tests now exercise H3/ODST/Reach raw records through the weapon-only rotation
scene, one damage application, duplicate-eye suppression and changed-shape
reseeding. They also reject malformed records, arm/camera remaps and oversized
source graphs. Logs: out/continuation-20260910-legacy-bounds-melee-{build,tests}.txt.

Scope limits remain deliberate: 14-point oriented bounds, native supported FP
graphs (legacy contact currently at most 64 source nodes), bounded records and
existing extent guards. H2's accepted reader remains one compression record.
This broadens ordinary/modded weapon coverage without promising exact triangle
contact, arbitrary huge rigs or all animated appendages. No headset result was
available for the new readers, including the ODST Mythic SMG report.
