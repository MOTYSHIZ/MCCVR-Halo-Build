# Authored barrel origin and direction investigation

Requested behavior: a separate optional mode starts shots at the visible gun's
verified barrel and uses its direction, while retaining native spread, targeting,
ammunition, collision safety and local ownership. The all-title task is unfinished.
H2 and H3 now have locally validated implementations behind default-off `gun_barrel_aim`.
The F1 control awaits remaining title adapters; no package or headset result.

## H2 implementation and validation

Both H2 renderers publish actual final gun palette nodes, after carrier trim and
collision correction. Primary/secondary lookups are separate; the latter cannot
evict primary reload/alignment identity. Exact model node count, full unit/weapon,
generation, tracking epoch/serial/time, role and handedness travel with the ray.
Invalid committed gun palettes publish empty receipts instead of retaining an old
valid muzzle. Catalog now retains authored up as well as forward, preserving roll.

H2EK `867EB0` and retail `8D11A0` independently establish 0x70-byte marker records,
world matrix +38, forward +3C, up +54 and position +60. Retail firing's sole scoped
wrapper call is `8E4BAA -> 8D6570`, return `8E4BAF`; wrapper calls `8D11A0` at
`8D657E`. Only that caller, the actual full owned firing weapon, one native marker
and an unambiguous authored FP marker are admitted. Local matrix/scale/flags stay
native. Missing/multiple markers preserve normal firing for that shot.

The native unit helper is H2EK `87DC20` (older shorthand 47DC20 is an RVA), retail
`8F0F70`. Its separate projection/unit-aim/obstruction controls are proven in both.
The muzzle is first obstruction-clamped by its trampoline with private velocity
storage, no projection/offset/unit-aim substitution. This covers authored branches
which skip the later helper or restore marker position afterwards. Native helper
velocity accessors H2EK `86FB50/86FC20` read parent/physics or object velocities
and write caller outputs; the ordinary native call still supplies actual firing
velocity. Later origin helper and native assist camera use the clipped muzzle.
Native spread, projectile construction, homing decisions and ammunition remain
in the original firing call, which is never replayed after an exception.

Native acquisition runs on the captured native query thread with native observer
location/BSP data preserved, converging toward the muzzle ray. The later assist
camera uses the clipped muzzle itself. A separate 0x24-byte target lease nests
inside the independent-dual lease; both restore in reverse order with exact
generation/owner/storage/unchanged-byte checks. The marker hook has a separate
optional install/retirement transaction; its failure leaves independent aim live.

Release, all 47 CTests, Reach gate and both pinned H2 binding verifiers pass.
Production fixtures: 2,375 H2 shot/lifecycle checks, 506 muzzle transform/publisher
checks. Native engine services are stubbed in fixtures. See logs
`out/refinement-20260918/{build,tests,gate}-h2-muzzle.log`. No live/native projectile
or headset result is claimed. Four other title consumers remain to implement.

## Authored data

`tools/re/audit_weapon_muzzles.py` reads the existing official per-title XML
exports and extracted HCEEK tags. It found 22 H2, 27 H3, 27 ODST, 24 Reach and
39 H4 models, with respectively 33, 38, 38, 40 and 83 trigger/muzzle markers.
HCEEK adds 12 models and 72 authored markers of all kinds. The ignored detailed
audit is `out/refinement-20260918/authored-muzzle-audit.json`.

HCEEK's own descriptor `C49A20` names a 0x50-byte
`model_region_permutation_marker_block`. Fields at `C499B0/C0/E0/F0` prove name,
node index, quaternion and translation. Serialized offsets are name +0, signed
node +20, quaternion +24 and translation +34 (hex). The exporter maps each model
node by exact name into that model's animation graph and retains permutation
identity. The pinned kit hash is checked before parsing.

The title-specific native default selectors are:

- H2EK `89F580`: barrel 0/1 defaults `0F0000DB`/`110000DC`, with authored
  override at barrel +30. These are H2 length-bearing string IDs.
- H3EK `A844A0`: default D4/D5, override +30 in the 0x134 barrel record.
- ODSTEK `B0FCB0`: default D5/D6, independently observed override +30.
- HREK `DE4290`: default 101/102, override +40 in the 0x184 barrel record.
- H4EK `E9E940`: default 165/166, override +40 in the 0x190 barrel record.
- HCEEK `8EFD90` reads the primary/secondary-trigger name pointer table at
  `C77704`. Its authored FP marker names use spaces, unlike later titles.

Pinned retail static name tables independently confirm H3 D4/D5, ODST D5/D6,
Reach 101/102 and H4 165/166. These are matching evidence after kit discovery,
not permission to copy IDs between engines. Exact exports retain explicit barrel
names; most ordinary weapons rely on their title's default selector.

`tools/re/generate_weapon_muzzles.py` produces 123 unambiguous marker records in
`weapon_muzzles.generated.h`, with source hashes and omissions in
`WEAPON-MUZZLE-CATALOG-2026-09-18.json`. Thirty-two requested model/barrel entries
have missing or distinct transforms and are omitted. Identical transforms across
permutations may be shared; distinct ones need a native selector. No arbitrary
first marker or generic hand offset is substituted. H2 uses its already-proven
model identity; CE uses exact animation-graph identity; other titles retain their
authored model checksums.

`weapon_muzzle.h` transforms a marker through an adapter-supplied committed
palette node, with finite, node, scale and orthogonality guards. It supports
reflected visible palettes. The receipt carries title, generation, full unit and
weapon handles, tracking epoch, serial, time, slot, barrel and handedness.
H2 now publishes/consumes it as above; the other title adapters remain pending.

## Native consumers and remaining implementation

H3EK firing calls marker resolver `A43170`, forwarding to `A3ABE0`. This resolver
can select the world object's model/nodes, with optional interpolated nodes;
it is not proof of the final visible first-person palette. Retail firing
`3683A0` then calls `3524B0`, which can replace direction, project origin, apply
offsets and collision-clamp it. Later player assist can replace direction again.
The new independent-dual transaction covers acquisition/assist, but still keeps
native firing origins. It is not the barrel feature.

CE also has later origin consumers. HCEEK `8EFD90` can restore marker origin
after modern/legacy unit adjustment for authored multi-marker/legacy flags.
Changing only the early helper is therefore insufficient for all CE weapons.
The accepted CE helpers preserve native velocity, projection and clipping; that
behavior must survive any optional muzzle override.

Reach's earlier origin experiments in REACH-SIGNATURE-EVIDENCE.md are historical,
not an active implementation. Current `ReachUnitAdjustBody` already relocates an
owned on-foot shot toward its published hand ray through native collision checks.
The barrel option must use the actual authored muzzle instead and still account
for downstream native origin, aim-assist and tracking branches. Read current code
before building on the older experiment narrative.

Next steps: publish exact committed per-title muzzle matrices and full weapon
identity; bind the outer firing/selected-barrel transactions; cover later origin
and direction consumers while retaining native collision and acquisition; add
the separate persisted/F1 toggle only with the implementation. Test overlap with
independent dual aim, native special weapons, both handedness modes and stale
ownership. No headset acceptance, ZIP, install or game launch is claimed.

## H3 implementation (supersedes earlier H3 investigation above)

H3EK A3ABE0 / pinned retail 343D74 establish the six-argument resolver,
0x70 stride and world basis/position +3C/+48/+54/+60. Firing call 36864C
returns 368651. Own FP lookup at 2C24AC/2C250F establishes TLS568, user2430,
slot11BC and weapon3C. Cold signatures and firing edge are uniquely verified
by tools/verify-halo3-muzzle-bindings.py against the pinned H3 PE.

Full owner/weapon is captured before and after native interpolation. A receipt
is published from final committed destination nodes only for the exact known
weapon model/count and unchanged owned FP slot. Auxiliary native obstruction
preflight uses private velocity output. Both it and acquisition reruns guard
against duplicate world-contact processing. Native targeting, later helper,
assist camera and nested exact-lifetime target restoration use the same clipped
muzzle. Optional marker-hook failure leaves independent dual aim available.

Current Release/all48 CTests/Reach gate pass. Fixtures: H2 shot/lifecycle2520,
H3 shot/lifecycle3057, common/H2 publisher506, H3 publisher217. Tests include
single/dual shots, ownership changes, marker ABI/field preservation, collision
clamping, nested leases, optional/native exceptions and retirement failures.
Native services are stubbed; no headset/native projectile result is claimed.
Logs: out/refinement-20260918/{build,tests,gate}-h2-h3-muzzle.log.

## ODST implementation and own native proof

The optional adapter is implemented in odst_muzzle_{ownership,publication,shots,
lifecycle}.inl. Halo 3 behavior matched: a shot uses its exact committed visible
barrel, while native collision, acquisition, spread, ammo and homing continue.
No controller/body inventory mutation is used to fake the shot.

ODSTEK B0FCB0 maps to retail 3AF230 (five-argument outer fire); B122B0 maps to
3AE8A4 (six-argument downstream data helper). Earlier notes calling 3AE8A4 outer
fire were wrong. The actual outer marker edge is 3AF4C5 -> 37F514, returning
3AF4CA. ODSTEK ABC880 -> AB4020 proves its own six-argument resolver and 0x70
record, final world basis +3C/+48/+54, position +60. The record and ABI are
independently matched in ODST retail, not inherited from H3.

ODSTEK AD1120 -> retail 396B7C retains native obstruction/velocity with projection
and unit-aim flags disabled in the optional path. Native firing calls it at
3AEB26; later assist B128B3 -> 4549A0 maps to 3AED8A -> 1610D4. The assist camera
6A8A00 maps to 242340, called at 1611FF. These later consumers use the clipped
muzzle. Native downstream marker-origin restoration is also covered by replacing
the scoped marker before it is copied into firing data.

Acquisition ODSTEK 453C30 maps to 1604E0; ordinary call 110719, fallback 110792.
It calls native view helper 455420 / 160FA0 at 160642. That helper normally
projects from camera toward world-object centre. The optional query replaces
only its private origin/camera outputs after projection, retaining native query
direction and lead math. Lead helper 1609CC calls it again at 160A58.

ODST target records are 0x28 bytes. ODSTEK 41D850/41D860 and retail 139F3C/
13A01C prove object +8, marker +4; strengths +10/+14, lead +18..20, flags +24.
The initial draft's H3-like object +4 was caught in the native-record audit and
corrected before full validation, together with its fixture. Native B122B0 and
3AE994/3AEA59 prove unit targeting +228 and controlling parent +2B8. Exact full
owner and unchanged storage/bytes guard nested target restoration.

Own primary/secondary roles +276/+277, four handles +27C, and weapon owner flag
+155/full owner +160 are matched to ODSTEK ADE940/B122B0 and retail 39AB60/
3AB19C. Own FP TLS598, user4F38, slot2740, full weapon3C are witnessed at
2E9E65/2E9E8E. Palette receipts compare owner/weapon before and after native
interpolation and again at final destination publication.

Release, all49 CTests, Reach gate and pinned ODST verifier pass. The new production
fixture exercises actual ownership/publication/shot/lifecycle code with native
services stubbed: native ABI/returns, both slots, nested shots, stock fallbacks,
weapon/checkpoint replacement, optional/native exceptions, all six hook failures
and exact ingress retirement. No headset or native projectile result is claimed.
Logs: out/refinement-20260918/{build,tests,gate}-odst-muzzle.log. Remaining barrel
adapters: Reach, H4 and CE. Roomscale drift is the newly queued next task.
# Reach adapter, September 18

Halo 3 reference behavior: optional authored visible-gun muzzle drives native
shot origin and central direction, with native obstruction, acquisition, assist,
spread, ammunition, ballistic correction and weapon-specific tracking retained.
Reach now implements this through its own HREK-derived bindings. Local checks
pass; this is not a headset/native projectile acceptance result.

Pinned haloreach.dll SHA256:
738DD2D24EA3AEA12E1EE9AA4A61094BF116027D42004C35A19E5048608B0894.
Research preserved under out/reload-policy/reach-{muzzle,query,acquisition,assist,
owner}*.c/.txt and out/dual-reach-fire-kit-0xde4290.c.

| Native meaning | HREK | Pinned retail |
|---|---|---|
| Outer fire, four arguments, void | DE4290 | 4C2710 |
| Marker wrapper/resolver, six arguments, signed short count | D47970 / D3B0E0 | 47044C; outer call4C2A9B |
| Unit adjust, ten arguments | D67EE0 | 484F24; call4C303A |
| Ordinary acquisition, six arguments | 4E2330 | 10E970; ordinary call5EF44 |
| Query view, six arguments | 4E3EE0 | 10FA74; query10EAC8, lead10EEEC, assist10F94F |
| Native assist wrapper/data | 4E32A0 / 4E3850 | 10FB94 / 10F40C |
| Assist direction/lead helper | 4E3600 | 10F8F4; data call10F583 |
| Inventory getter | D78C90 | 48B3D0 |
| Controlling parent traversal | D77E00 | 48375C |
| FP full-weapon slot lookup | 8D2670 / 8CEED0 | 2B1218 |

HREK and matching retail prove role bytes34A/34B, four full weapon handles350,
controlling parent390, effective-unit target2B8. Native acquisition zeroes five
qwords (0x28 / 40 bytes), object8/marker4, strengths10/14, lead18..20, flags24. Own
retail106F04 validates target object8. Weapon owner32C with flag1A9/fallback1B4
matches HREK reload ownership in retail4BE0B8. FP TLS6A0/user53A8/slot2978/full
weapon3C is independently verified by retail2B1218. Record70, world matrix38,
forward3C, side48, up54, position60 are native marker outputs.

The fourth unit-adjust argument is an OUTPUT velocity vector, despite legacy
core parameter name basisForward. HREK's D48D40 writes it. Private preflight uses
private velocity and forced native obstruction; the actual firing helper retains
its original output pointer/collision flag/simulation dword. The existing core
hook handles the owned shot before its accepted hand/vehicle fallback, without
installing a second hook at that address. Four optional hooks own outer fire,
acquisition, query-view and marker evaluation. Missing/ambiguous bindings leave
only barrel adaptation stock. Installation requires the shared core helper to
be enabled. Teardown drains optional firing scopes before retiring that helper.

Before/after interpolation captures full owner and weapon, then final committed
visible destination publishes checksum/node/slot/tracking generation/reference/
serial/time/handedness. Reach's existing proven renderer admits slot0 only; no
secondary renderer change is claimed here. Unknown models, non-single native
markers, stale receipts and failed ownership stay native. Query runs on the
captured native input thread and leases the native target record through the
outer fire scope. Later query/assist camera outputs use the same clamped muzzle;
native lead and spread remain. No native exception causes fire replay.

Release/all50 CTests/Reach gate pass; the production fixture has5,384 checks
with native services stubbed. Own pinned verifier checks14 unique witnesses,
eight call edges and native hook entry/unwind records. Tests include signed
marker counts, exact four/six/ten-argument forwarding, real velocity output,
nested leases, exceptions, stale/replaced data, native target updates, primary
palette publication, query TLS, and all four-hook partial lifecycle failures.
Native headset confirmation and Halo3 regression remain due. No accepted pointer
or installation change; H4/CE barrel adapters and all other standing scope remain.

## Halo 4 adapter, September 18

Halo 3 behavior matched: optional actual visible authored barrel drives the shot,
while native obstruction, target acquisition, assist, spread, ammunition and
homing remain. Five optional hooks use H4's own H4EK evidence, independently
matched to pinned halo4.dll SHA256
7C53E7D5BC9848545A1B70E2768242479336FBA1B7630D7AB955F7FD0C34FA84.
Research is preserved under out/reload-policy/h4-{muzzle,marker,fire,aim,assist,
acquisition,query,held-owner}*.c/.txt. No headset/native projectile result yet.

| Native meaning | H4EK | Pinned retail |
|---|---|---|
| Outer fire: four arguments, void | E977E0 | 6176B8 |
| Marker selector: ids165/166, barrel override | E9E940 | 610F6C |
| Marker core: seven arguments, signed short count | E39E10 | 5D5B74; call6179B2 |
| Native obstruction/velocity: nine arguments | E66CD0 | 5F3510; call617E90 |
| Ordinary target acquisition: six arguments | 585160 | 1DB840; callsA364C/A36AE |
| Query/assist view: six arguments | 586DC0 | 1DC9E4 |
| Native assist wrapper/internal/view builder | 586060/586640/5863D0 | 1DCB8C/1DC2F4/1DC86C |
| Inventory role/full handle | E67B60 | 5F9E20/5FA1C4 |
| Controlling parent traversal | E80C00 | 5F1ABC |
| FP record producer | 92A1F0 | 3B1B4C |

The marker output has its own verified70-byte record: world scale38,
forward3C/side48/up54/position60/flags6C. H4 marker core has a separate first-person
byte argument in addition to original-object and interpolation bytes. Native
unit adjustment always performs obstruction; its fourth argument is OUTPUT
velocity, and it has no Reach-style separate collision flag. View argument3 is
unused in both H4EK and retail. The adapter replaces private view origin, camera
point and direction after native view calculation. No null-vector requirement
is invented for the unused argument.

Native target record is0x28 (40) bytes, object8/marker4, strengths10/14, lead18..20,
flags24. Own query/target validators prove this layout. Local target is unit5A8,
controlling parent694. Inventory roles63A/63B index four handles640; weapon owner
624 has flag471/fallback480. Full salted object lookup reuses H4's independently
verified native seat-input memory reader. Native input query captures actual
view owner and input user; fallback flag1 cannot seed a firing context.
The private native acquisition leases that target only through the exact outer
fire scope. Nested restoration checks generation, owner, storage and unchanged
bytes; native exceptions never replay fire. Collision ownership excludes the
separate physical-contact ray redirect during the private query/preflight.

H4 FP TLS6A0/user5F48/slot2EC8 stores active bit2 at0, full unit4, full weapon6C
and held render tag74. H4EK producer calls92A5FB (Storm) and92A727 (held) map to
retail3B1E15/3B1F1D. The held fill uses weapon handle from cursor-30, not the unit
handle used by Storm. Native final consumer36F3A3 forwards record objectIndex
to skinner33D8B8 (return36F3C9). Publication therefore resolves the actual skinner
weapon against inventory, native FP slot and render tag. It prepares the authored
ray from the exact carried scratch palette and publishes only after the original
skinner returns normally, rechecking ownership. Failed carried palettes publish
empty receipts. Existing H4 renderer behavior is retained; no new secondary
rendering support is claimed by this adapter.

Release/all53 CTests/Reach gate pass. tools/verify-halo4-muzzle-bindings.py passes
14 unique own witnesses, ten native call edges and five unwind-covered hook
entries. Production fixture6005 checks includes native seven/nine/six/four-arg
forwarding, signed marker counts, nested leases, full weapon salt replacement,
palette tag mismatch, stale tracking, callback exceptions and every partial
five-hook creation/enable/disable/removal failure. Tests stub native services;
they do not establish in-game trajectory appearance. Native seat reader remains
available until optional hooks drain. Logs: out/refinement-20260918/
build-h4-muzzle-final.log, tests-h4-muzzle.log, gate-h4-muzzle.log.
CE barrel and the rest of the cumulative list remain before packaging.

## CE adapter, September 18: E-CE-MUZZLE-20260918

Halo3 reference behavior matched: actual authored visible barrel origin and
central direction, with native obstruction, target eligibility/acquisition,
homing/assist, spread and ammunition. Classic and Anniversary share the native
firing path. Renderer/reference epochs and committed graph identity are checked
separately before the visible palette can authorize a shot. No headset result.

| Meaning | HCEEK | Pinned retail |
|---|---|---|
| Outer fire: six arguments, void | 8EFD90 | B7A374 |
| Marker: four arguments, signed short count | 8037A0 | B3701C; returnB7A574 |
| Modern adjustment: seven retail arguments | 8CD510 | B00880; returnB7A796 |
| Legacy adjustment: six arguments | 8CD400 | B00740; returnB7A858 |
| Full continuous query: four arguments | 5486F0 | B67FA8; callA98617 |
| Direct continuous query: three arguments | 547FE0 | B68284; callA98923 |
| Downstream player firing assist | 548A20 | B67B00 |
| Director ray within assist | 500CB0 | B14F14; returnB67BE4 |

Own kit/retail marker output is6C bytes: node/pad0, local matrix4, world scale38,
forward3C/left48/up54/position60. CE has no later-title trailing flags. The
primary/secondary name table is kitC77704 / retail189A6D8; names contain spaces.
The actual outer marker scope must receive one native marker belonging to the
full local equipped weapon. Multiple/unknown markers stay native. Updating
the native marker before it is copied covers the later authored20/4020 origin
restoration paths. Modern native obstruction clamps the optional private ray;
actual native modern/legacy helper still produces its original velocity/speed
output. No native spread/projectile construction is replaced.

HCEEK5486F0 and547FE0 construct the complete0x10 (16)-byte native targeting record:
strength0, identifiers4/C, marker/model8/A. Retail B67FA8/B68284 agree; B698D0
validates the direct target atC. B7A374 and kit8EFD90 read unit target1E0 and
controlling parent308. The adapter uses the existing CE full-salt local-player
reader and native object_try_get, rejecting controlling parents. Both query
modes capture actual input-user/zoom on their native thread. During a shot,
native acquisition runs with its original mode and private outputs; the existing
target-query hook supplies that shot's clipped muzzle origin and direction.
The resulting full native targeting record is leased only through the original
outer firing callback. Restoration verifies generation, owner, re-resolved
storage and unchanged bytes. Nested leases restore in order; new native targets
win. Network-replicated firing data remains native (nonnull fifth argument).
Local ordinary/predicted native callers pass a null fifth argument; retail
B79759 andB7AF10 are preserved in ce-fire-call-sites.txt.

Continuous normal targeting also uses the committed primary barrel when the
option and receipt are valid. Missing receipts retain the existing controller
aim path. Scoped later assist receives the same clipped origin/direction while
retaining its native perspective result. Three existing aim hooks now pair
callback increments with explicit __finally cleanup, so a native SEH exception
cannot strand a retirement counter. Their original functions execute once.

CE source files are haloce_muzzle_state/shots/lifecycle.inl, integrated through
haloce_first_person.cpp and ControllerRig's gunBarrelAim snapshot. Native palette
publication follows the final rollback point, after contact and hand-only
offsets. It matches the full weapon captured before native preparation and
revalidates ownership, graph, renderer/reference epoch, handedness and100ms age.
The four optional hooks drain before dependent aim/query/core hooks are removed.
Failed proof/install/retirement isolates the feature and logs the reason.

The pinned CE manifest adds six own witnesses and nine call edges under the
muzzle group. Official kit and retail identities, uniqueness, unwind and edges
pass tools/re/verify_ce_render_evidence.py; generated contracts match the ledger.
Research: out/reload-policy/ce-muzzle-{fire,marker}-{kit,retail}.c,
ce-direct-query-final-kit.c and ce-{outer,direct}-query-calls.txt.
Release/all54 CTests/Reach gate pass. Actual production fixture3277 checks cover
both renderer epochs/handedness, native ABIs, multi-marker fallback, stale
tracking/reference, owned target leases, weapon/storage replacement, native
exceptions and callback release. Separate364-check fixture covers all four-hook
creation/enable/disable/removal failures and ingress drain. Native services are
stubbed; neither fixture establishes in-headset projectile appearance.

All six adapters now have the shared default-off "Aim from the visible gun
barrel" F1 Crosshair control. Other cumulative requirements remain outstanding;
no ZIP, install, launch or accepted-pointer update. Latest logs:
out/refinement-20260918/build-ce-muzzle-final.log, tests-ce-muzzle.log,
gate-ce-muzzle.log, ce-muzzle-evidence.json. The final lifecycle warning cleanup
was rebuilt in build-ce-muzzle-lifecycle-final.log before all54 tests passed.
