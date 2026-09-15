# CE stock weapon surface contact

## Behavior and failure reproduced in source

Halo 3's reference behavior is contact and physical melee from the held gun's
authored extent, with the same tracked pose and independent physical hands.
The 22cb813 user confirms smooth Original/Anniversary VR and working hand
contact, but gun meshes pass through the world. Its preserved log reports
11,746 world queries, 176 contacts, 575 corrections and three right-hand
physical melee applications. The native adapters are running; the old CE
publication contains skeleton pivots only. A barrel or stock outside those
pivots never reaches a collision or melee query.

This refinement appends a posed weapon envelope to the existing independent
hand frames. Camera ownership, muzzle effects, native damage selection and
both renderer pipelines remain unchanged.

## Primary geometry evidence

`tools/re/verify_ce_weapon_mesh.py` reads the extracted official HCEEK models
and matching first-person animation graphs. It pins the official executable
through the existing mesh descriptor verifier; no third-party tag layouts,
other-title offsets or guessed weapon lengths are used. Per-file hashes,
complete graph identities, names and numerical bounds are in
`HALOCE-WEAPON-MESH-EVIDENCE.json`.

| Stock item | Weapon vertices | Weighted model nodes |
| --- | ---: | ---: |
| Assault rifle | 1,713 | 5 |
| Oddball | 1,239 | 1 |
| Flag | 143 | 1 |
| Flamethrower | 2,179 | 3 |
| Needler | 2,366 | 17 |
| Pistol | 1,728 | 7 |
| Plasma pistol | 982 | 3 |
| Plasma rifle | 1,957 | 5 |
| Plasma cannon | 2,127 | 3 |
| Rocket launcher | 3,080 | 5 |
| Shotgun | 1,392 | 4 |
| Sniper rifle | 2,274 | 4 |
| **Total** | **21,180** | **58** |

The kit's own descriptors establish 0x9C model nodes, 0x30 geometry headers,
0x84 part headers and 0x44 uncompressed vertices. Vertex bone indices are
resolved through the actual local node table when the model flag selects it.
All positive influences participate; a zero-weight or missing second influence
does not invent a node. Model names are matched uniquely to the corresponding
animation graph names, then checked as descendants of its verified weapon
root. This excludes hand nodes from the weapon envelope. The plasma cannon
model has 41 skeleton nodes but only three have positive weapon weights.

Every weighted vertex is transformed into each influencing node's rest-local
space and enclosed in that node's bounds. Runtime transforms all eight corners
of every bound through that node's current tracked matrix. Their union is
expressed in the current gun-root space. Positive skin weights sum to one, so
each skinned vertex is a convex combination of points inside these posed node
boxes and is enclosed by the resulting gun-local box. This includes needles,
magazines, moving barrels and every other positively weighted stock part.

The native palette-to-render relationship was already established by E-CE-FP-1
and the actual-mesh floating-hand verification. The additional read-only retail
capture `out/ce-contact-model-runtime-native.txt` shows `0xC6350C` composing
each 0x34-byte supplied model matrix with the model node's inverse-bind matrix
at `node + 0x68`, using 0x9C-byte nodes. The catalogue uses the equivalent
rest-local coordinates; no new native runtime address or hook is introduced.

## Identity, transport and cost

The runtime hashes every ordered graph node's terminated name and parent
index, including the count, and matches the complete catalogue identity and
weapon-root index. Trailing bytes after a name's NUL are not semantic. An
unknown graph or invalid bounds keeps the existing node contact and increments
the cold `node-only-fallback` counter. The optional identity never rejects a
working hand binding or camera. Catalogue identity, availability and graph
handle form contact shape identity, forcing motion reseeding across swaps or
fallback transitions.

The fourteen stable samples are eight corners and six face centres in gun-root
space. They are added only to the physical hand owning the weapon, including
left-handed/anatomical modes. Existing native hand and weapon pivots stay in
their original order; the wrist remains point zero. Maximum frame size is
64 + 14 = 78 points, within the shared 80-point limit. The queued packet carries
an explicit fourteen-point receipt; an unknown graph's last fourteen pivots
can never be mistaken for mesh samples. Other titles leave this field zero.

World contact retains up to seven hand/node probes and additionally queries
every weapon corner and face centre. This bounds the held hand at 21 native
resolver calls per consumed simulation update, and the other hand at seven.
Equal-coordinate extrema ties cannot hide a gun face behind a hand pivot.
Motion/damage sees the same complete pointer-free frame. Its existing rigid
controller-motion policy evaluates the current geometry under both controller
poses, so animated bounds/reloads alone cannot create physical swing speed.

No file I/O, signature scan, lock, allocation or logging enters the palette
hook. All mesh parsing and bounds generation are offline. Runtime work is at
most 17 weighted-node boxes / 136 corner transforms, plus the fixed sample
conversion. User headset cadence remains the performance acceptance test.

## Deliberate graphics-mode and custom-model limits

Both Original and Anniversary use this **stock CE physical envelope** through
their shared tracked graph. The already-proved Saber bridge maps and copies
the same native model bones to Anniversary rendering. This intentionally gives
both modes the same gun-contact behavior, independent of graphics switching.
These official gbxmodels do not prove the exact surface extent of each Saber
replacement mesh. This is a conservative authored envelope, not triangle-exact
collision or a claim that the two renderers' art is identical.

The union also contains empty space between posed parts. Reload/ejection nodes
such as magazines and authored bullets can enlarge it when separated from the
gun body. Per-part visibility or attachment filtering has not been established;
silently dropping those named nodes would instead lose requested gun coverage.
The existing world resolver follows posed geometry and may respond to that
animation, whereas physical melee explicitly excludes animation-only speed.
Detached/hidden-part precision and the visible reload/contact result remain
headset validation limits of this conservative envelope.

Graph identity proves the stock skeleton mapping; it cannot detect a custom
mesh substituted behind an unchanged stock animation graph. Such a mod retains
that stock graph's envelope. New/custom graphs get explicit node-only fallback.
Automatic runtime custom-mesh extraction and exact Anniversary replacement
surface coverage remain open and must not be described as completed here.

Native melee still uses the independently proved explicit biped target and the
held weapon's native authored damage for either physical hand. Distinct bare
support-hand damage and vehicle/world-object damage remain separate open work.

## Verification

`halomccvr_ce_contact_tests` tests all catalogue bounds under animated/scaled
node matrices, unknown/invalid rejection without partial output, routing and
the existing transform/correction guards. Its `--weapon-mesh-fixture` entry
consumes the actual 12 graph records, checks both handedness modes, exact
appended sample reconstruction, node fallback, stationary-controller animated
geometry producing no melee, real controller motion and shape reseeding.

The Python verifier can invoke this compiled entry to skin every actual vertex
against the generated runtime samples in four poses per weapon (48 cases /
84,720 vertex-pose checks). `halomccvr_ce_contact_runtime_tests` additionally
checks all fourteen corner/face probes when they tie with hand extrema and
preserves the exact receipt through the real queue. Existing native ABI,
damage, duplicate, lifetime and fault-isolation checks remain in that suite.
The production contact targets compile and both CTest suites pass; records
are `out/ce-native-weapon-contact-build-20260915.txt` and
`out/ce-native-weapon-contact-tests-20260915.txt`. The compiled all-vertex
fixture passes **48 cases / 84,720 vertex-pose checks**, recorded in
`out/ce-weapon-mesh-compiled-20260915.json`; source descriptor/catalogue
verification is `out/ce-weapon-mesh-source-20260915.json`. These are isolated
geometry/transport/native-adapter fixtures, not a real collision world or
headset result. Final cumulative Release/package checks are recorded by the
candidate handoff; user testing remains required for visible contact, reload
behavior, damage and cadence in both CE graphics modes.
