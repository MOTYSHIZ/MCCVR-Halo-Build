# Visible hand positioning

Six neutral-zero sliders under Weapon & Aim persist separately for every title
profile. Each physical hand gets local X/Y/Z offsets, bounded to +/-0.20 metres.
They are independent of the existing per-gun alignment controls.

The Halo 3 behavior being matched is final hand-only mesh placement after the
tracked gun, contact and reload calculations. H3/ODST use their verified body
render identity and remap; Reach uses its own exact body layout; H4 classifies
its own 80 body nodes; both H2 renderers use the actual final hand packet; CE
uses its verified graph masks with gun descendants explicitly excluded.
Collapsed, cross-weighted arm anchors move by the same delta as their wrist.
No native hooks were added. Unknown layouts retain their existing presentation.

The controls name physical hands. Released left-handed role swapping routes
the physical-left setting to the right-hand mesh; anatomical alignment retains
anatomical left/right ownership. Gun palettes, shot origins, contact volumes and
reload insertion targets are computed before these presentation offsets.

Local validation: Release build, all 51 CTest suites and Reach consistency gate
pass. The production-adapter/config fixture passes 190 checks including all-zero
byte equality, three handedness modes, hidden anchors, weapon/root preservation,
invalid input and profile save/load. Logs are under out/refinement-20260918 with
hand-offsets in their names. These checks are not headset acceptance; both
target-title appearance and Halo 3 headset regression testing remain unverified.
