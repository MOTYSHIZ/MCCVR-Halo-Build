# CE tracked first person and controller shot evidence

September 15, 2026. Local implementation and consumer verification for the CE
candidate; no headset result or accepted-pointer change is claimed. Both CE graphics
modes and both MCC editions remain required; physical melee and world collision
wait until functional CE VR is confirmed in the headset.

## Player behavior being matched

Halo 3 is the reference: the weapon follows the selected main controller, each
physical hand has its own visible hand, support grip controls two-hand aim, the
shared mounting/scale settings apply, and releasing support grip restores the
independent hand. Arm IK and floating-hands presentation remain shared options.
The visible barrel trim changes the mesh only; controller aim and the reticle
keep the shared calibrated aim pose. Native weapon animation and shot spread
remain native. The engine retains the actual fire-origin policy, as on the
accepted existing-title line.

## Pinned inputs and reproducible records

The pinned CE retail DLL and official HCEEK identities are in
`HALOCE-EVIDENCE-MANIFEST.json`. This work reads only those local inputs and the
already-extracted official HCEEK tags. It uses no Reclaimer or console data.

- `HALOCE-FIRST-PERSON-CONTRACTS.json`: exact function prefixes, call edges,
  instruction witnesses and independent `first_person`, `first_person_aim`,
  `player_state` and `controls` verification groups. All current entry prefixes are unique in the pinned
  full mapped image. The original short graph-palette prefix matched both
  `0xC43C14` and `0xC6423C`; it was extended on an instruction boundary.
- `HALOCE-FIRST-PERSON-TAG-EVIDENCE.json`: SHA-256, node count and named root/
  wrist identities for all 12 extracted official first-person animation graphs.
- Local decompilation/disassembly: `out/ce-hands-kit-fp.txt`,
  `out/ce-hands-kit-palette-aim.txt`, `out/ce-hands-kit-nodes.txt`,
  `out/ce-hands-retail-fp-build-disasm.txt`, `out/ce-hands-retail-matrix.txt`,
  `out/ce-hands-kit-adjust-ray.txt`, `out/ce-hands-retail-shot-ray.txt`,
  `out/ce-fp-consumer-bodies.txt`, `out/ce-fp-assist-disasm.txt` and
  `out/ce-fp-player-ray-disasm.txt`.

## E-CE-FP-1: native first-person palette

The official HCEEK first-person builder at VA `0x005C9FC0` calls the animation
graph palette routine at `0x007D7940`. The latter reads the `antr` definition,
node block at `+0x68`, 64-byte node records and 52-byte output matrices; parent,
sibling and child are signed 16-bit fields at `+0x24`, `+0x20`, `+0x22`.
`model_animations.c` asserts the 64-node work queue bound. It composes each
authored animation transform through its native parent.

Pinned retail homologues are prepare `0xB29698` and graph palette `0xC43C14`.
Prepare indexes the native user record by `0x1E94` through pointer `0x2D9CD90`.
Its call at `0xB29D88` passes output `user+0x1088`; position/forward/up come from
native camera `0x29AF2C4`, `0x29AF2D0`, `0x29AF2DC`. The graph definition's
cached node address is `+0x6C`, resolved by the native tag virtual/mapped bases
at `0x2EA3410` and `0x2D9CE10`. These exact accesses are witnessed in contracts.

The optional hook calls the original builder first, accepts only its proven
prepare callsite and exact output pointer, and stages a complete private palette
before writing. It verifies the graph, generation, XR space/reference revision,
renderer epoch, controller poses, node counts/indices/hierarchy and finite
orthonormal matrices. No scanning, logging, allocation, COM or locks occur in
the palette hook. Exceptions/fallbacks are counted and reported by the cold poll.

Official graph fixtures establish both arm chains by names, not copied indices:
`frame l/r upperarm` -> `frame l/r forearm` -> `frame l/r wriste`.
The weapon is the direct right-wrist child named `frame gun`, `frame body`
(Needler), `frame pole` (flag), or `frame skull` (oddball). The rocket launcher
also contains a deeper `frame body`; accepting any matching name incorrectly
made that graph ambiguous. The direct-parent rule is verified across all 12
graphs. Tests consume their converted native node records as optional fixtures.

The final palette moves wrist descendants with a rigid carrier delta, retaining
finger poses and each weapon part's authored relationship. The gun has its own
mask so the shared `experimental_hand_alignment` option can place anatomical
left/right hand geometry on matching physical controllers while the dominant
weapon remains on the selected primary hand. With that option off, the native
right-hand-authored mesh follows the semantic primary role, matching the shared
released behavior. The existing `IK_SolveTwoBone` math receives the CE-named
shoulder/elbow/wrist chains. Floating hands collapse only non-hand/non-weapon
nodes after solving, preserving graph node zero because the Saber bridge also
uses the first remapped model matrix as an object carrier. Native animation is
still evaluated; no graph state or
reload timer is rewritten. Authored reload finger/part transforms survive the
carrier operation; final visual quality during every animation is untested.

The arm solve matches Halo 3's shoulder-only anchor adjustment, 25/75 blend of
authored pole with outward/down bias and extension capped at 1.8 times authored
length. CE supplies its own measured chain lengths and named joints. Shared
`shoulder_back_m`, despite its name, is a native-world-unit setting in the
existing Halo 3 code and config documentation, as is `right_shoulder_drop`;
neither is converted from metres. Controller mount offsets retain their actual
metre semantics. Tests explicitly distinguish shoulder units from world scale
and check extension with the wrist beyond the authored arm's reach.

Shared configuration in the frozen `ControllerRig` supplies arm IK, floating
hands, shoulder leveling/back/drop, main/support scale, visible barrel Euler
trim, main forward/right/up offset and support forward offset. Support-hand
calibration uses the same explicit `BasisFromAngles(-gun_yaw, gun_pitch,
-gun_roll)` presentation equation as Halo 3. Shared XR code supplies
role-routed calibrated primary aim, support grip and the user's handedness.
The standalone helper never reads another title's configuration or engine data.

### Consumers: established and still open

Native `0xB275B8` consumes `user+0x1088` through `0xB2A12C`, builds native
model palettes through `0xC6350C`, and copies 0xD00-byte gun/hand palettes to
`0x1C384A0` and `0x1C350A0` (per-user stride 0xD00). Native model-marker
consumer `0x7D350` also reads the same graph palette with 0x34-byte stride.
These are real downstream consumers of the chosen write boundary.

Retail `0x7AC60` is the native-to-Saber first-person palette bridge. It chooses
the gun or hand palette by matching the supplied model tag against per-user
tags at `0x1B7AA88`/`0x1B7AA98`, selects the corresponding arrays above, and
copies `node_count * 0x34` bytes to skin data `+0x20C` through Saber object
`+0xA8`; the count is stored at skin data `+0xF14`. It also converts the first
model matrix through `0x77FB0` and writes the Saber object's transform
`+0x58..+0x94`. These exact native operations are preserved in
`out/ce-fp-saber-bridge.txt`. The bridge demonstrates why an arm-hiding filter
must retain the carrier matrix. The later E-CE-FP-5 verification below follows
this data through native packing/upload and actual skinned vertex shaders.

Follow-up tracing located its actual virtual update: `0x76170` creates the
skin subclass (vtable `0x17F1228`); virtual slot `+0x80` resolves to `0x8B5E0`.
This override walks the object's Saber bone chain at `+0x188`, maps each
bone's native index through model-record `+0x38`, and calls `0x77E70` on
`skin + 0x20C + native_index * 0x34`. It writes the bone matrix at `+0x70`,
combines the inverse/bind relation into `+0x78`, then marks derived data dirty.
This advances the chain beyond the copy; E-CE-FP-5 adds the bone-to-GPU
consumer check, while live draw visibility remains untested. Source trace:
`out/ce-skin-palette-virtual-20260915.txt`.

The initial investigation stopped before the Anniversary shader consumer.
E-CE-FP-5 now verifies the native material selector and actual skinned shaders;
this still does not establish live draw selection or headset pixels. In
particular, a previously investigated Anniversary particle shader has its own
first-person projection work; its `+0x150` constant data is reflected particle
emitter data, not the hand/weapon skin palette.
No claim that the current palette already renders correctly in both modes is
permitted until headset results support it.

## E-CE-FP-2: native controller shot and downstream aim assist

HCEEK trigger `0x008EFD90` calls modern adjustment `0x008CD510` or legacy
adjustment `0x008CD400`. The modern helper optionally replaces direction from
unit `+0x21C`, projects the muzzle onto that ray, applies a native offset,
clamps its origin through native collision and obtains inherited velocity.
The legacy helper uses the same facing/projection policy and projects inherited
velocity onto the final direction. Its upright perpendicular basis independently
uses CE world Z-up. HCEEK unit origin `0x008D54A0` distinguishes attached
objects through payload `+0xD8`.

Retail trigger `0xB7A374` calls modern `0xB00880` at `0xB7A791` or legacy
`0xB00740` at `0xB7A853`. The corresponding modern ABI has seven arguments,
including offset and two booleans; legacy has six. Only those return addresses
are admitted. A private controller direction is passed to the original helper
with its final `use unit facing` boolean cleared. The native origin/project/
collision/offset/inherited-velocity behavior executes unchanged. The helper's
direction result then returns to the trigger's own buffer before native spread.

Crucially, those two hooks alone are insufficient: player assist `0xB67B00`
later obtains a director ray and can overwrite the adjusted shot direction at
`0xB67E9E`. Its query at `0xB67BDF` calls `0xB14F14` with local unit, origin
output and direction output. The third hook admits only return `0xB67BE4`, runs
the native query, then routes its direction through the same controller pose.
It preserves the native perspective return, origin and complete downstream
assist/target/spread algorithms. Other director-ray callers are unchanged.

Local ownership uses full handles: native slot-zero equipped weapon at user
`+8` must pass object type mask 4 in `0xB389A4`; `0xB77814` resolves its parent
and validates unit type mask 3. That owner must equal the shot/query's supplied
unit, and the unit's native parent `+0xD8` must be invalid for on-foot admission.
The native object lookup validates the salted handle through `0xBBB8D0` before
resolving its payload. NPC, remote, stale, seated or mismatched shooters stay
stock. This is an on-foot weapon ownership check, not complete death/menu/
cinematic/vehicle state proof.

Hands and aim have separate binding groups and installation results. An aim
binding failure leaves hands available, and a hand binding failure can still
leave controller aim available. Neither feature can revoke CE stereo ownership.
Teardown disables and drains native detours before releasing module lifetime.

## E-CE-FP-3: narrow local state and native control facts

`HaloCEFirstPerson_GetLocalPlayerState` reads the output-zero full player handle
from native mapping `0x2EA2D90` `+0xB8`, validates it through native
`0xBBB8D0` against the player table `0x1C40480` (record stride `0xC20`), and
reads the player's controlled unit `+0x64`. Official `0x595290`, identified by
the explicit `any_player_is_dead` caller, tests this same controlled-unit field
for `NONE`. The API exposes `hasControlledUnit`; a missing unit is not silently
turned into an on-foot player. It rejects unsalted/index-only handles even
though the native generic datum getter permits them.

It matches that player to exactly one native input-user mapping at `+4`, checks
input control `+0x10` (stride `0x58`) and output control `+0x170` agree with
the full unit handle, then validates the unit and its player backlink `+0x1F8`.
It exposes parent handle `+0xD8`, optional native FP equipped weapon and the
native first-person visibility flag (user byte zero, mask `0x01`). Thus an
unarmed local player can still have a proven control state. A present weapon
must have this same unit as native owner. All native pointers are reacquired
within each checked call and generation is revalidated before publication.

Native leaf `0xB14EA4` accesses the inline per-user director array at
`0x2D9B960`, stride `0xF8`, reads callback `+0x10`, scalar `+0x0C` and cached
int16 `+0x5C`, and returns/caches a value from zero through three. Native
first-person dispatch `0xB27510` calls prepare only for result zero. The API
therefore exposes the raw value and `nativePreparesFirstPerson`, with only zero
named by its proven use. This is not the Classic/Anniversary graphics-mode
reader. The callback identities alone do not prove vehicle/cinematic/death
enum names.

Official input updater `0x5908F0` and matched retail `0xA9915C` provide direct
control-admission facts. The relevant per-input-user director byte `+0x58`
blocks the native angle update. Byte `+0x59`, or the native global condition
`int32(0x1C34FC8) == 0 && int32(0x1B85760) != -1`, zeros the native control
result (while retaining a native allowed-button mask). The API conservatively
exposes these as `nativeLookBlocked` and `nativeInputBlocked`; it does not
invent a more specific menu/cutscene name from the bytes.

The official game-time getter `0x5806E0` reads its time-record byte `+2`;
retail `0xA994C0` consumes this from global `0x2E9FD68`. The script-named
`game_paused` variable is only a separate control of this state and is not used
as the runtime gate. The native record byte is exposed as `nativePaused`.
Official cinematic getter `0x511C10` reads cinematic-record byte `+0xA`;
retail `0xA994E0` accesses the matching record through global `0x2EA0208`.
It is exposed as `nativeCinematicFlag`. Both flags, both suppression branches,
valid local control ownership, on-foot parent state and native FP perspective
zero are required before substituting controller shot direction. These gates
remain independent of stereo ownership.

Official HCEEK `0x58F350` asserts `output.desired_angles` and returns its
two-float pointer from the player-control globals, output-user stride `0x38`,
yaw `+0x17C`, pitch `+0x180`. It validates yaw in `[0, 2*pi]` and pitch in
`[-1.4922565, 1.4922565]`. Retail leaf `0xA999CC` independently returns the
same offsets from retail global pointer `0x2D8FE70`. Retail `0xA999E8` also
passes those angles and the user reference object to `0xAD0304`, matching
the official `0x58F450` -> `0x595E80` reference-space conversion. A proven
getter does not alone establish the correct write phase or state gates for
snap turning. Official `0x58FB70` and retail `0xA99C1C` are the native
input-user angular-delta updater: input-user stride `0x58`, yaw `+0x1C`, pitch
`+0x20`, native yaw normalization and seat/pitch clamps. Retail `0xA9965B`
calls it only after the native look-suppression check. Retail `0xA9AB00`
matches controlled-unit identity and copies input control `+0x14..+0x44` to
the matching output control `+0x174..+0x1A4`. Consequently an external write
only to output desired angles could be overwritten by the native input copy.
The subsequent optional controls module now routes the shared turn delta at
that exact native callsite, leaving normalization and output copying native.
Its independent lifetime, input/presentation gates, tests and remaining limits
are recorded in `HALOCE-CONTROLS-EVIDENCE.md`. No direct desired-angle pointer
write is enabled.

## E-CE-FP-4: Anniversary bone scale at the native conversion boundary

The September 15 interrupted scale correction is integrated and its native
instruction check is complete. Native first-person skin update `0x8B5E0`
calls `0x77E70` at `0x8B97D`, with input `instance+0x20C+node*0x34` and
the destination native skin matrix. The unique prefix, exact call edge and
body witnesses are in `HALOCE-FIRST-PERSON-SKIN-CONTRACTS.json`, merged as the
independent `first_person_skin` group. The callback admits only return address
`0x8B982`; the separate object-carrier conversion stays native.

The actual converter reads basis vectors at `+4..+0x24` and position at
`+0x28..+0x30`, transforms to Saber axes, applies native position scale/world
offset, and writes a homogeneous 4x4 matrix. It never reads the separate
scale at input `+0`. Consequently floating-arm hiding and visual hand-scale
changes stored only in that field do not reach Anniversary skinning.
`ApplySaberFirstPersonScale` multiplies the converted 3x3 basis by the native
scale, preserving translation and the homogeneous column byte for byte.
Finite, scale-range and homogeneous-matrix guards reject invalid data without
writing. The optional conversion hook has independent installation, teardown
and cold failure reporting; its failure cannot revoke the camera core.

`tools/re/test_ce_skin_native.py` executes the entire pinned converter
`0x77E70..0x77FAD` against synthetic bones, with the mapped image read-only
and execution outside that function rejected. Its output passes through the
compiled production helper. Thirty cases cover three orientations, two world
locations and five scales, including the floating-arm scale. All 36 native
calls confirm that changing only native scale leaves the original conversion
unchanged; the production helper changes only the nine basis elements.
Result: `out/ce-native-skin-adapter-verification-20260915.json`.
Caller/converter witnesses remain in `out/ce-fp-skin-convert-caller-disasm-20260915.txt`
and `out/ce-fp-skin-key-disasm-20260915.txt`; an earlier disassembly range began
mid-instruction at `0x8B940`, so only the aligned callsite witnesses are used.

This section verifies conversion and scale arithmetic. The following section
adds native skin upload and material projection; visible mesh and headset
results remain required before describing Anniversary hands as confirmed working.

## E-CE-FP-5: native skinned shader consumer and per-eye lens selection

Halo 3's behavior being matched is tracked hand/weapon geometry using the same
per-eye lens as the surrounding world. Native Anniversary first-person model
registration `0x7ADF0..0x7AE1A` sets model `+0x28` bit `0x10000000`. Material
writers read that bit and write a four-lane lens selector: GLT `0x264080`
writes constants `+0x170`, ZFILL `0x26FF00` writes `+0x20`, and SFX
`0x26E960` writes `+0x70`. The selector is exactly all ones for first-person
models and all zeroes otherwise. Unique prefixes, instruction witnesses and
material-vtable pointers are in `HALOCE-FIRST-PERSON-PROJECTION-CONTRACTS.json`.
ZFILL is a complete leaf function without a PE unwind-function entry; its
full instruction sequence and vtable pointer are independently verified.

The actual GLT shader reflection names the selector
`VS_REG_GLT_USE_FPWEAPON_PROJ_MATR`. Common constant-buffer offsets `+624`
and `+688` hold the world and separate first-person view-projection matrices;
`+800` is the camera position. GLT, ZFILL and SFX skinned variants select the
fixed first-person lens when their selector is one. The optional runtime
correction calls each original material writer first, then changes only the
proven four-lane selector to zero. It requires the first-person model flag
and a fresh receipt from a tracked palette that actually committed, with
matching title generation, renderer epoch, XR space and reference revision.
A new native prepare invalidates the old receipt before rebuilding stock data.
Blocked presentation, failed prepare, stale context or a noncanonical selector
leaves that feature stock. Skin and lens failure cannot revoke stereo ownership.

`tools/re/test_ce_fp_projection_native.py` executes the native registration
loop and all three complete native material writers in a read-only mapped
pinned image. Eighteen material cases cover stock flags and three registered
first-person flag combinations. The compiled production helper selects the
world lens and preserves every unrelated constant byte. Result:
`out/ce-fp-final-native-projection-20260915.json` (18 calls, 1,815 native
instructions). This uses synthetic material records with external-service
branches inactive; live material selection remains a separate test.

Native palette packing `0x2CF700` reads bone matrix `+0x70` and writes three
transposed float4 rows per bone. With input flag bit zero clear it also reads
the bind-matrix array at `+0x60` and multiplies each bind matrix through the
converted matrix using native `0xF9B80`. Native uploader `0x30E290` selects
the model's bone range and copies these 48-byte records into skin constant
buffer staging, marking that staging and renderer dirty.

`tools/re/test_ce_fp_shader_native.py` executes the pinned conversion, both
packing branches (including the real bind multiply), and the native upload.
Five independent carriers include rotations, distinct world locations, normal,
resized and floating-arm scales. The compiler cookie service and exact memcpy
service are explicit stubs; render arithmetic executes native instructions.
An independent matrix product verifies the bind branch. The resulting native
uploaded bytes are consumed by actual locally extracted CE Anniversary GLT,
ZFILL and SFX skinned vertex shaders on D3D11 WARP, with stream output read
back and checked against expected vertex positions/projections. Both camera
locations, both asymmetric eyes and stock/corrected selectors run for each
bind branch: **48 WARP draws**, 20 vertices each; **1,634 native instructions**
and two native upload copies. Result:
`out/ce-fp-final-native-shader-20260915.json`.

The proprietary shader fixtures remain ignored local files, never part of the
source or build package. Their SHA-256 identities are pinned in the verifier:
GLT `B8F433CF95831AD2F19A4B993EB407E870B9DE7598F99E0E3486B7435BAFFD86`,
ZFILL `9913BF9C2CD321399F1AC39C4FD5E9F6B85A566F9A1E91149847493A7AF35489`,
SFX `3D3EDD95C2712BE65D7D45CCC76020A7E8E9548F0D9191F0B49D4BF1DADF3CFF`.
This proves the tested native skin and projection consumers, with synthetic
records and vertices. It does not establish every live material variant,
animation, muzzle alignment or visible headset behavior.

## E-CE-FP-6: retirement and receipt regression

The shared native quiescence helper in `game.cpp` admits at most eight detour
ranges. Adding the third material writer brought the first-person feature to
nine hooks, so the previous single count-nine call always refused retirement.
The feature now disables all nine entries first, drains batches of eight and
one, and retains every trampoline and the native module until both succeed.
No callback entry is re-enabled between batches.

`halomccvr_ce_first_person_runtime_tests` compiles the production adapter and
tests renderer/recenter/generation/tracking rejection, failed-prepare receipt
invalidation, all three exact selector offsets, blocked presentation and
unchanged-output fallback. Its native stack-service fixture enforces the real
eight-range maximum; refusal of the final batch preserves generation and
trampolines, and retry succeeds. This is local lifetime-contract verification,
not a claim that a real MCC title transition has been headset tested.

## E-CE-FP-7: Anniversary gameplay camera uses native coordinates

Native producer `0x7B480` applies the E-CE-3 world offset and forward bias:
`SaberPosition = 3.048 * Map(nativePosition) + worldOffset + SaberForward * bias`,
where `Map(x,y,z) = (x,z,-y)`. Pinned reads at `0x7B696`, `0x7B6A3` and
`0x7B5DA` establish offset globals `0x2B05118/1C/20`; read `0x7B5EF`
establishes forward bias `0x2E3B838`. The unique producer prefix and all four
relative operands are independently verified by the `gameplay_bridge` group
in `HALOCE-GAMEPLAY-BRIDGE-CONTRACTS.json`.

`NativeCameraFromSaber` only inverse-maps axes and divides the entire rendered
position by 3.048. That is deliberate for per-eye scene staging, which remains
in the renderer's offset coordinate space. Publishing that result directly as
the gameplay camera would put controller positions in offset coordinates.
`RecoverNativeCameraFromSaberBridge` additionally subtracts
`(MapInverse(worldOffset) + nativeForward * bias) / 3.048` for gameplay
publication. It preserves every field except position and rejects invalid
input without writing. The runtime publisher must independently verify the
native bridge bindings before reading those globals; this optional feature
must not become a camera-ownership requirement.

The raw native first-person builder is already different: its proven palette
call receives native camera position `0x29AF2C4`, forward `0x29AF2D0` and
up `0x29AF2DC` (E-CE-FP-1 contracts). `PrepareHook` reads that same native
camera before building tracked hand poses. It must not remove the Saber
offset again. The later native bone converter introduces the renderer's world
offset while converting each native palette position.

`tools/re/test_ce_gameplay_bridge_native.py` executes the pinned producer and
passes its actual camera bytes through both compiled production conversion
helpers. Twenty-seven cases span three native world positions, three pitched/
yawed bases, and zero/positive/negative bias with distinct offsets. All recover
the original native position/basis within 0.000245 native units at the largest
position; 12,960 native instructions execute. Record:
`out/ce-fp-final-gameplay-bridge-20260915.json`. `halomccvr_ce_render_tests`
also checks preserved fields, signed offsets/bias and invalid-input fallback.
These are synthetic native arithmetic checks, not headset acceptance.

## Verification performed and limits

- `halomccvr_ce_first_person_tests` passes the production pose/palette helper,
  independent hands, left-handed weapon routing, preserved weapon parts,
  articulated elbows, floating filter, visual scale/mount separation and
  invalid-data/unchanged-output checks. All 12 actual official graph fixtures
  pass the production binding routine, including Needler/rocket/flag/oddball.
  Earlier Release runs had standard `assert` checks compiled out; those runs
  are invalid evidence. The test now uses always-active `CHECK` failures. The
  Release target, CTest and all 12 official fixtures were rerun successfully
  after that correction. A deliberately missing fixture returns exit code
  one, confirming fixture reads and failure checks execute in Release.
- `tools/re/test_ce_shot_native.py` executes the pinned native modern and
  legacy machine instructions in Unicorn against isolated fake memory: 32
  combinations of direction, native-facing flag and origin-projection flag.
  Direction overwrite, origin projection and inherited-velocity results pass.
  External origin, collision, velocity and security services are explicit
  deterministic stubs; this test is ABI/math evidence, not live game proof.
- E-CE-FP-4/5 now verify native bone conversion, bind composition, skin upload
  and actual GLT/ZFILL/SFX vertex transformations and lens selection. Both
  first-person CTest suites pass after the E-CE-FP-6 retirement correction.
- These checks do not prove live muzzle/pixel alignment, native draw visibility,
  title-transition recovery in MCC or headset feel.
  Shared controller aiming/visual calibration is wired; title-specific final
  barrel alignment, broad reload/weapon/vehicle coverage and all target-title
  plus Halo 3 headset results remain required before acceptance.
