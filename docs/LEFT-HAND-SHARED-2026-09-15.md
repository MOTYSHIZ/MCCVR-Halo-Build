# Halo 3, ODST, Reach and Halo 4 opt-in palm alignment

The Halo 3 behavior being preserved and matched is a main hand gripping the
actual gun, with engaged support retaining the authored weapon grip. The
accepted right-handed/default-off source remains `7ff9697`. The user explicitly
requests correcting the existing left-handed **Fix Hand Alignment** option in
all titles, in the same candidate as the D-pad controls. No headset acceptance
is claimed for this correction.

## Source defect and correction

The prior optional shared route swapped wrist origins and reconstructed a
controller-relative orientation even though the input wrists were already
solved against the weapon. That omits each hand's native wrist-to-palm offset
and can apply a different rotation from the final gun/support solve. The
replacement consumes those final solved wrists and each anatomical hand's own
semantic marker, as in the separately documented Halo 2 correction:

```
primaryPalm = solvedRightWrist * nativeRightPalm
supportPalm = solvedLeftWrist * nativeLeftPalm
desiredLeftWrist = primaryPalm * inverse(nativeLeftPalm)
desiredRightWrist = supportPalm * inverse(nativeRightPalm)
```

The full rotation, translation and destination role scale are retained.
Finger subtrees keep their existing wrist-local animation. This does not
author new trigger/finger animations. The native marker scale is an editor
radius, not skeletal scale; local marker frames have unit scale, just as the
previous H2/H4 semantic-marker solves did.

Halo 3/ODST preserve their native render-model remap and intersect each hand
mask with the body nodes before applying either delta. Both wrist mappings
must match the selected title's proven marker nodes. The existing shoulders,
arm IK and contact-context routing follow those staged hand targets. Reach
requires its own checksum/count identity and validates both marker nodes
through the current palette remap against its already-proven source layout.
Held nodes remain outside the hand masks. Both adapters stage the complete
hand result before publishing; invalid/unknown evidence leaves the existing
tracked palette intact and the worker reports optional presentation refusals.

Halo 4 retains the accepted 80-node Storm classifier and its separate gun
record transaction. It saves the original primary weapon delta BEFORE
exchanging palm targets. Free support consumes the existing semantic-marker
solve; engaged support consumes the existing rigid weapon solve. Neither mode
recomputes the gun from the newly assigned anatomical right wrist. The old
unused wrist-routing helper remains in place; no dormant cleanup is included.

The only runtime callers require the existing `handAlignment` snapshot, which
is `leftHanded && experimental_hand_alignment`. Normal right-handed and
default-off rendering do not enter the new route. Camera, rendering hooks,
native bindings, haptics and lifecycle ownership are unchanged.

## Independent official asset evidence

Official editing-kit exports are preserved under
`out/left-hand-shared-evidence/{h3,odst,reach,h4}` with original tag paths under
each `tags` directory. These came from each title's own official `tool.exe`
and tags (H3EK, H3ODSTEK, HREK and H4EK under
`D:/SteamLibrary/steamapps/common`). Reach evidence is HREK-derived; no retail
discovery or archived console data is used.

Exporter SHA-256 identities:

- H3EK: `1DCF51EC39BDF61B6A6AE40917908CF23A247BFEDB7A4BFF07124F508EE0F3B5`
- H3ODSTEK: `626361C1D7627F5EDA731ED5CBD512F03F84C820EDA1FA1EE376D40FD0B1430B`
- HREK: `7C6B868B5F5B9303AA9210F31A7CC3221A6FA419A6AB3F323C8E0831DFD0AFCD`
- H4EK: `5E0AD8D03EC4B1C7F4C0C2A18C92CEC9F92F3EB7E7F0CBB7376BA9D866E3A758`

`tools/re/verify_shared_hand_alignment.py` reads the exported nodes and marker
groups, verifies all model identities below, and compares independent
quaternion matrices with the actual compiled marker builders. Complete tag/XML
SHA-256 identities, local marker values and bilateral landmark measurements
are recorded in `LEFT-HAND-SHARED-EVIDENCE-2026-09-15.json`.

| Title/rig | Import checksum | Nodes | Right/left wrist | Marker policy |
| --- | ---: | ---: | --- | --- |
| H3 Chief FP | 504041493 | 37 | 6 / 5 | both native |
| H3 Elite FP arms | 269159697 | 31 | 6 / 5 | both native |
| H3 Dervish FP | 268439051 | 31 | 6 / 5 | native left, derived right |
| ODST Recon FP | 286525724 | 37 | 6 / 5 | both native |
| ODST ONI operator FP | 403178001 | 37 | 6 / 5 | both native |
| Reach Spartan FP | 404622103 | 47 | 11 / 14 | both native; left is identity |
| Reach Elite FP | 419566353 | 41 | 14 / 13 | native left_hand_elite, derived right |
| H4 Storm FP | 353173504 | 80 | 29 / 37 | both native; left on identity child 54 |

H3 Dervish and Reach Elite lack native right-hand markers. Each rig's OWN
identity wrist bases and bilateral index/pinky roots independently establish
wrist-local Y reflection. For these two rigs only, the missing right semantic
frame is deliberately derived as `F * left * F` for rotation and `F * p` for
translation, with `F = diag(1,-1,1)`. This produces a proper rotation. Small
authored digit asymmetries remain recorded; no claim of an unexported native
marker or a perfectly symmetric hand is made. The cold worker log explicitly
states this implementation difference. H3 Elite's existing native right marker
is used as authored, rather than substituting this derived convention.

H4's left marker belongs to `b_l_hand_marker_offset` (54), an identity child
of `b_l_hand` (37) in the official model, matching the accepted free-left
marker policy. Its right marker is directly on `b_r_hand` (29). The existing
runtime Storm classifier remains authoritative; the kit checksum is not a new
gate on otherwise working H4 rendering.

The existing loaded-tag readers already use the import checksum at descriptor
`+0x08`; no new hook/signature or guessed memory binding is introduced.

## Verification and remaining headset test

The Release build and all **33 CTest suites** pass, including the new shared
production-helper suite: **96 rig/scale/eye/support cases, 9,657 assertions**.
It checks complete semantic palm frames with nonidentity rotations, nonzero
offsets, unequal scales, translated/rotated/scaled roots, both eye offsets,
free/engaged support, unchanged appended guns, preserved local finger pose,
identity rejection and atomic late-finger failure. The primary-asset verifier
passes all eight rigs. Reach's consistency gate also passes.

The cumulative candidate retains the separately passing H2 packet suite
(829 assertions) and CE primary-asset fixture (12 stock graphs at three
scales, 36 cases / 180 compiled palette calls). Final packaging rebuilds and
reruns CTest and the Reach gate for the committed source.

These offline checks establish transform arithmetic and evidence consistency;
they do not establish visible grip fit or driver/runtime behavior. Required
headset coverage remains all titles, both CE/H2 graphics modes, single/dual
weapons, engaged/released support, nondefault scale and offsets, unchanged
default-off behavior, and a Halo 3 regression for shared input/presentation.
Both MCC editions remain supported. Deliver matching ZIPs without installation,
then wait for the user's testing. The accepted pointer stays `7ff9697`.
