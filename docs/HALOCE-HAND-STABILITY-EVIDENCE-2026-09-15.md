# CE hand geometry and copied-skin stability

September 15, 2026; follow-up to the e524d21 headset report. The user confirms
good gun scale in both CE modes and supplies an Original screenshot showing
large pointed strips connected to the hands. Anniversary hands also flicker.
This candidate preserves the successful tracked gun/hand scale and lens work.
Halo 3's behavior being matched is independent floating hands without visible
arm strips, with each native hand/weapon draw retaining its complete scale.
No Halo 3 implementation changes are made by this first-person refinement.

## Official mesh evidence

Primary input: extracted official HCEEK
`tags/characters/cyborg/fp/fp.gbxmodel`, SHA-256
`851B5EC0C61D04FAFD865F8B4DB7BA9D0E3912F6D085663E93C47C052261C6C9`.
The official kit executable is the pinned
`FC9E2B6193C6F6D9FF988278B0D39A983747F3FDBDECA7CAFADD28F1F0C53E73`.
`tools/re/verify_ce_floating_hand_mesh.py` reads its own tag descriptors to
verify the layout; no Reclaimer or console definitions are used.

| Official kit descriptor | VA | Size |
| --- | --- | --- |
| gbxmodel | `0xC4A168` | `0xE8` |
| model node | `0xC49368` | `0x9C` |
| gbxmodel geometry | `0xC4993C` | `0x30` |
| gbxmodel geometry part | `0xC49724` | `0x84` |
| uncompressed vertex | `0xC49470` | `0x44` |

The vertex descriptors at `0xC49420..0xC49450` name two signed-short node
indices and two float weights. The part descriptors at
`0xC49868..0xC49898` describe the local-node count and 24-entry table.
Model flag bit 1 selects those local tables; interpreting a vertex index
directly as a graph index produces incorrect cross-hand assignments.

The reference hands have 37 model nodes and 1,495 vertices in four parts.
After resolving the actual local tables, 71 vertices blend a forearm with
the same side's wrist. No vertex has a nonzero root-node weight. Every
weighted vertex belongs to one arm/hand side; no positive blend crosses
between left and right. The model binds into the official first-person
animation graphs by name, independently of their weapon-node differences.

The e524d21 floating filter scales the four arm joints to `0.00001` but
puts their origins at the camera. Wrist and finger matrices remain at the
controllers. Arm-only vertices therefore collect at the camera while the
blended forearm/wrist vertices span from that point to the hand. Small
scale alone cannot remove those spanning triangles.

The bad camera-collapse branch was disabled separately in commit `2b46667`
and retained inert. Its replacement derives left/right arm masks from each
official named shoulder ancestor, excludes wrist descendants and the gun,
and places each collapsed chain at its own final tracked wrist. Classification
uses the CE graph hierarchy, never another title's node indices. Root,
wrist, finger and weapon matrices stay byte-identical to the same tracked
palette with floating hands disabled. Handedness, anatomical hand alignment
and planted support-grip routing happen before this visibility operation.
All validation completes before the helper writes the private staged palette.

## Actual-mesh regression

The verifier sends the official assault-rifle graph and a synthetic pose built
from the actual hand model's bind nodes into the compiled production palette
builder. It skins all 1,495 actual vertices using their real local-node tables,
bind-relative positions and weights. Fifteen cases cover normal/left-handed/
anatomical routing, planted two-hand support, and independent scales from
0.3 through 3.0. This is CPU geometry verification, not a game screenshot.

With scale one, the old filter places some mesh vertices approximately
0.7000 native units from their corresponding wrist. The correction keeps
the entire corresponding hand within 0.07961 native units and its arm-only
vertices within 0.000011 native units of the wrist. Every case preserves
the non-arm matrices exactly. The production C++ tests additionally verify
unchanged output on malformed masks and complete arm classification in all
12 official weapon graphs. Record:
`out/ce-floating-mesh-refinement-20260915.json`.

## Anniversary copied-bone scale ownership

The existing pinned native evidence in E-CE-FP-4 proves that first-person
skin update `0x8B5E0` calls the converter at `0x8B97D` with a bone from
its already-copied `instance+0x20C+node*0x34` array. The converter ignores
the source's separate scale field; our adapter restores it into the converted
3x3 basis. Translation and the homogeneous column remain native.

Previously the adapter rechecked the single latest tracked-palette receipt
for each bone. `PrepareHook` invalidates that global receipt before building
the next palette. If this occurs between conversions of the already-copied
array, some bones receive their scale and others keep the converter's scale
one. This can enlarge hidden arms or change hand geometry within one skin
palette. The supplied logs show scale fallbacks interspersed with successful
conversion throughout Anniversary gameplay, not only at title exit. Those
counters alone cannot establish the scheduling of an individual fallback.

The regression explicitly invalidates that global receipt between the third
and fourth conversion of an unchanged copied source. Old production code
fails at the fourth bone (`out/ce-stability-fp-lifetime-before.txt`). The
corrected adapter reads scale from the specific source it is converting and
keeps title/generation/feature-lifetime, finite-value, orthonormal-basis and
homogeneous-output guards. The old latest-receipt gate remains inert in the
function. Source scale one leaves the converted matrix byte-identical; native
stock matrices therefore retain their original output. The exact first-person
conversion callsite remains the only hook admission, and object-root/world
conversions remain native. Projection ownership is a separate feature.

The production regression tests scales `0.00001`, `0.3`, `1`, and `3` across
eight conversions each, including receipt invalidation midway through the
array. It also checks source-scale preservation across reference/renderer
churn, stock scale-one identity, and rejection after title/generation changes.
Corrected production code passes (`out/ce-stability-fp-lifetime-after.txt`).
The existing native converter test also passes all 30 scale/basis/location
cases (`out/ce-hand-stability-native-scale-20260915.json`). Native skin packing,
upload, and the extracted GLT/ZFILL/SFX shaders pass all 48 WARP draws with
the rebuilt production helper
(`out/ce-hand-stability-native-shader-20260915.json`). No new address or hook
is introduced by either hand correction.

## Limits

These results establish the corrected palette geometry and copied-bone
conversion behavior. The new candidate still needs the user's headset result
for Original arm appearance, Anniversary flicker, every weapon animation and
mode/title transitions. The official reference mesh is verified; arbitrary
modded hand meshes are not exhaustively covered. Acceptance remains at the
previous accepted pointer. Both MCC editions remain supported; no game
installation, game launch or game-file modification was performed.
