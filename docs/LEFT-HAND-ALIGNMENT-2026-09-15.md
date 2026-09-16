# Opt-in left-handed hand alignment

The user reports that enabling **Fix Hand Alignment (Experimental)** places
the hands to the right of the gun in all games. The accepted baseline is
`7ff9697685e78389dad16126e4d1ec7188a3acbe`; its right-handed rendering, weapon
placement and CE haptics remain the reference. This work concerns the explicit
`left_handed && experimental_hand_alignment` presentation option. Headset
acceptance of the new correction is pending.

## CE Original and Anniversary: established mismatch

`BuildTrackedFirstPersonPalette` currently gives the anatomical primary left
wrist the native **support-left** wrist's rotation relative to the gun, while
the unchanged weapon uses the native **primary-right** wrist's carrier. Merely
exchanging controller roles does not transfer the primary grip to the opposite
anatomical hand. Resetting the left wrist's position to the controller hides
the positional part of that discrepancy in identity-basis tests.

The baseline production-helper fixture in
`out/ce-left-alignment-repro-20260915/result.json` changes only the native left
support wrist and descendants by 90 degrees. The rendered gun matrix and
primary-left wrist position remain identical, but the primary-left basis
changes. The existing CE test executable used for this reproduction has SHA-256
`51356F117F72EF7464913D3345E7BD35365EADFE1A7BC931FCF1227668230476`.
This establishes a source-carrier mismatch; it does not reconstruct the exact
weapon/animation visible in the user's headset.

Both Original and Anniversary consume this same staged native graph palette.
The native consumers and their separate projection/scale handling are recorded
in `HALOCE-FIRST-PERSON-EVIDENCE.md`, E-CE-FP-1, E-CE-FP-4 and E-CE-FP-8.
The contact adapter assigns anatomical left-hand nodes and the unchanged gun
to physical primary left, then applies one common contact translation. It does
not introduce a second lateral offset between those carriers.

## CE primary model evidence and reflection convention

The official HCEEK first-person hand model is
`out/ce-hands-hceek-tags/tags/characters/cyborg/fp/fp.gbxmodel`, SHA-256
`851B5EC0C61D04FAFD865F8B4DB7BA9D0E3912F6D085663E93C47C052261C6C9`.
Its node translations/quaternions are parsed using the primary kit's model
node definitions by `tools/re/verify_ce_floating_hand_mesh.py`. The pinned
`halo_tag_test.exe` SHA-256 is
`FC9E2B6193C6F6D9FF988278B0D39A983747F3FDBDECA7CAFADD28F1F0C53E73`.

Both native rest wrists have identity bases. Direct index/middle/ring/pinky/
thumb child positions are bilateral about **wrist-local Y**. For example,
left/right index bases are `(0.034655798, +0.004354420, 0.015151240)` and
`(0.034654669, -0.004354200, 0.015152049)`; thumb bases are
`(0.008536830, -0.004043370, 0.014679870)` and
`(0.008530330, +0.004006950, 0.014685949)`. The largest component residual among
the five mirrored digit bases is below `0.00004` native units. Exact parsed
landmarks/bases are preserved in
`out/ce-left-alignment-hand-landmarks-20260915.json`.

Thus hand-local `F = diag(1,-1,1)` is supported by CE's own model, independently
of any other title. Applying a reflection on both sides of a relative rotation
produces a proper rotation; reflecting a single matrix basis would instead
produce a negative determinant and is not an acceptable skinning transform.
The first-person hand model has no top-level semantic hand marker. The
third-person cyborg's different skeleton/markers are not first-person proof.

The gun-side reflection plane needs separate care. The official weapon model
roots and graph identities come from `HALOCE-WEAPON-MESH-EVIDENCE.json` and
`tools/re/verify_ce_weapon_mesh.py`. Their complete rest matrices and bounds are
preserved in `out/ce-left-alignment-weapon-axes-20260915.json`. Assault rifle,
pistol, plasma pistol/rifle, shotgun, sniper rifle, rocket launcher,
flamethrower and flag have identity rest root bases, with lateral model Y equal
to gun-local Y. Needler's root has a small authored rotation, Oddball's
`frame skull` maps local Y approximately to model Z, and plasma cannon's root
has a substantial authored rotation. A universal gun-local-Y reflection is
therefore **not** established for all CE weapons by the hand model alone.

Any correction must preserve the existing actual rendered gun matrix, use
the relevant authored wrist relation rather than an invented grip offset,
and distinguish a deliberate mirrored grip from native left-handed animation.
Native left/right finger articulation remains authored for its existing hand;
rigid wrist relocation cannot supply a new authored left-handed trigger grip.
Both graphics modes, active support grip, reloads, nondefault weapon scale and
mount offsets still require headset validation.

## CE implemented correction

`tools/re/verify_ce_hand_alignment.py` independently rechecks the five hand
landmark pairs and all twelve weapon-model to animation-graph node/parent
mappings. It generates `src/common/haloce_hand_alignment.generated.h` from
the actual weapon rest matrices. For rest root `(s,R,t)`, model-lateral
`Y=0` becomes a gun-local plane with normal `R^T*(0,1,0)` and offset `t.y/s`;
both are normalized together. This retains the authored Needler/Oddball/plasma
cannon root conventions. No per-weapon tuning offset is invented.

This is a deliberate VR-side bilateral grip policy about the original model's
lateral plane. Asymmetric weapons, Anniversary replacement geometry and native
left/right finger animations are not claimed to be exact mirror images.

The lookup requires the complete ordered graph name/parent fingerprint, node
count and named gun index. An unknown/custom graph retains the existing
opt-in hand mount; the periodic CE alignment diagnostic reports
`unknownGraphPriorMount`. Only the optional correction is unavailable. The
working tracked palette and renderer remain available.

For source gun `G`, native right wrist `Wr` and the **unchanged actual rendered
gun** `Tg`, `BuildMirroredGripTarget` first computes `G^-1 * Wr`, including the
relative scale. Let that relative transform be `(q,Rw,p)`, the verified
gun-plane normal/offset be `(n,d)`, and `A=I-2*n*n^T`. The new anatomical-left
relative rotation is `A*Rw*F` and position is `p-2*n*(dot(n,p)+d)`. Composing
with `Tg` produces the target wrist; the wrist's local finger pose is then
carried rigidly as before. Since both `A` and `F` are reflections, their
combined rotation has positive determinant; native positive scales are kept.

Active two-hand support uses the same operation on the native **left** support
wrist to place the anatomical **right** support hand at the reflected authored
support relation. Releasing support grip restores the existing independently
tracked right-support mount. That free support mount is unchanged by this
correction. Hand IK and floating-arm collapse follow the resulting wrist using
their existing code.

The weapon-node loop is unchanged, using the same original right-wrist to
weapon-grip carrier as the accepted build. Right-handed mode and left-handed
mode with the correction disabled never enter the new grip operation.
Invalid transforms retain the builder's staged, write-last behavior. Haptics,
camera ownership, graphics switching, projection, renderer hooks and native
bindings are not changed by the CE alignment implementation.

## CE verification

Primary proof and generated-header consistency passed for all twelve official
weapon graphs and five bilateral digit landmarks. Exact parsed data is in
`out/ce-left-alignment-primary-proof-20260915.json`. The checked-in generator
recreates this evidence from the pinned primary assets.

`tests/haloce_first_person_tests.cpp` now includes a nonidentity gun/wrist
fixture with lateral/vertical grip offsets, different native scales, mount
translation and pitch/yaw/roll trim. Its assertions check that changing the
native support wrist no longer changes the anatomical primary grip, the
rendered weapon is byte-identical, active support follows its authored
reflected grip, wrist-local fingers retain their existing pose, and invalid
data cannot partially publish. It also checks all twelve plane reflections
are proper, positive-scale involutions and that the option has no effect in
right-handed mode.

The primary-asset generator's `--adapter-exe` mode drives the actual compiled
palette helper through all twelve complete official graphs at three mount
scales. Independent Python affine calculations check primary and active
support grips; byte comparisons check unchanged gun and right-handed output.
The compiled CE suite passes. The primary-asset fixture passes **36 stock
cases / 180 palette calls**, recorded in
`out/ce-left-alignment-compiled-proof-20260915.json`; executable SHA-256
`1C6CE2E31EF15F18AE3C2F993FEAC31E6CA7AF19CF99D72858C34AFBAB418781`.
These checks do not replace CE Original/Anniversary or Halo 3 headset
regression results, which remain pending. The final committed candidate also
reruns the cumulative build/test gate before creating archives.
