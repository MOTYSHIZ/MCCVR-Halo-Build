# All-title reload accessory and holster refinement

The shake description below records the original candidate. It is superseded
by NEEDLER-SINGLE-SHAKE-2026-09-16.md: one rapid out-and-back, no grip.

## Recovered request and scope

The preceding conversation was read directly from the local September 16
14:26 session transcript, including the full 14:26 request and 14:40 ALL-games
clarification. It ended at the magazine-catalogue update, with uncommitted
controls and extraction work above 46a6b61. That work is preserved and completed
in this candidate. The follow-up in the resumed conversation explicitly adds
automatic support for modded weapons across all games.

The user says the 46a6b61 gestures are great. This refines that implementation;
it is not a repair to a headset-rejected gesture candidate. Preserve the
CE vehicle steering/crosshair, CE Anniversary beam and H3 Cortana corrections,
both MCC editions, both CE/H2 graphics modes and all earlier standing/deferred
work. Accepted cumulative source remains d47a98c. Package only, no installation,
game launch, game-folder writes, publication or PR.

## Player behavior

The Halo 3 reference remains the accepted native controller-input boundary:
Halo owns ammo, inventory, automatic reload and animation timing. All six games
use that same interaction outcome through their configured native buttons.
The new visual is an optional shared VR accessory; it is not native animation
scrubbing or an ammo write. Both graphics modes of CE/H2 use their game's same
accessory, keeping the pouch/hand interaction independent of engine renderer.

- Enable **Manual Reload** in Weapon & Aim. A recognized detachable reload part
  appears at the support-side hip. Hold support grip there, move the part under
  the weapon hand, and release. This retains the existing reload gesture.
- The visible part moves with the support controller while held. Release away
  from the receiver to cancel; a new part remains available at the pouch.
- Separate sliders control magazine grab radius, insertion radius, holster
  radius and minimum slide distance. Existing saved body radius migrates to the
  holster radius when the new key is absent. Original defaults remain intact.
- **Holster slide / draw gesture** preserves the old draw. **Holster click
  gesture** switches once on a fresh weapon-grip click inside the zone. With
  both selected, click takes precedence and subsequent movement cannot repeat
  it. With neither selected, neither holster gesture runs.
- **Shake to reload needle weapons** is off by default. Away from the holster,
  hold weapon grip and make two deliberate up/down cycles, then release before
  another reload. Minimum stroke is adjustable, default 10 cm. The gesture needs
  four alternating strokes in 1.8 seconds. It recognizes each title's own
  Needler and Reach's Needle Rifle. It never guesses needle ammunition from an
  unfamiliar weapon's size, shape, name fragment or inherited game title.
- All controls mirror for left-handed use. Gestures remain single-weapon,
  on-foot, focused gameplay controls. Pause, death, vehicles, menus, tracking
  loss, recentering, title/generation/weapon changes and conflicting inputs
  cancel pending actions. Ordinary buttons remain available.

## Modded weapons: automatic detection and bounded fallback

This is live equipped-model detection, not a list of mod package names.
Recognized model identities use their catalogued part. A detected unfamiliar
held model gets a small **blue generic reload item**. This is mod-authored art,
not a claim to have extracted that custom weapon's magazine or identified its
ammo type. **Generic reload item for unknown weapons** can disable this visual
independently; it defaults on within Manual Reload. The native reload gesture
and holster controls remain available for custom weapons regardless of whether
their model can be read. Halo still decides whether the weapon can reload.

Per-title native evidence and detection:

| Title | Existing verified reader used for equipped-model identity |
| --- | --- |
| CE Original/Anniversary | Successfully applied first-person palette receipt; complete live animation-node names/parent fingerprint. Dynamic supported graphs can be unfamiliar to the stock catalogue. |
| H2 Classic/Anniversary | Successful final first-person packet's primary model tag and node count; live-verified compression header at +0x14/+0x18. Unknown models get a bounded bounds/count/tag fingerprint. |
| H3 / ODST | Primary interpolation source/generation and each title's loaded render-model checksum reader. Existing native body-remap/appended-weapon classifier admits unfamiliar held models while excluding body palettes. |
| Reach | HREK-proven render-model identity reader and existing live appended-held-range classifier. No retail-discovered new layout or cross-title node number. |
| H4 | Exact successful `CarryHeldModel` branch of the existing floating-hand skinning sequence, using its resolved model checksum/tag. No change to the palette or sequence decision. |

Unknown zero-checksum H3/ODST/Reach/H4 models use a marked native tag identity.
H2's two plasma rifle models have equal compression/count tuples; the catalogue
selector refuses ambiguity. They can only receive the explicitly generic item,
never another gun's authored magazine or needle classification. Invalid,
oversized, nonfinite or unreadable data cannot invent a model identity.
Receipts expire after 150 ms and must match title, generation and reference
space. Different unknown model identities cancel a carried interaction too.

This does not guarantee every imaginable custom rig: CE still requires its
existing safe hand/weapon graph binding, and each engine still requires its
current valid tracked weapon path. A custom model deliberately retaining a
stock identity can select that stock part. Arbitrary mod-created textures,
magazine topology and new ammunition classes are not automatically extracted.
Unknown weapons retain their normal reload button and pouch gesture, but do
not gain shake-to-reload without a recognized needle identity.

## Geometry and evidence

Official per-title source hashes, identities, node names and selected nodes are
in RELOAD-CATALOG-EVIDENCE-2026-09-16.json. The generated catalogue has 150 model
records, 42 visible parts, seven needle identities and 29,142 triangle vertices.
The 42 parts include 10 H3, 10 ODST, five Reach, seven H4, three CE and seven H2
models. Cross-title duplicate shapes still have their own title identity.

The tools read the official kits relocated to D:/SteamLibrary/steamapps/common,
export XML into ignored out/reload-kits, resolve first-person model references,
decode each format's compression and node maps, and retain triangles wholly
weighted to the selected reload node/subtree. They choose one authored
permutation per region and remove duplicate faces. Source names are not reused
as runtime offsets. Known detachable carbine cells, Spiker drums and Mauler
drums are explicitly selected per kit. Shotgun nodes named magazine are thin
receiver loading gates and are deliberately excluded; shotgun gestures remain.

The original research instruction to keep extracted art under out applied to
the unimplemented research output. The explicit user request is now realized
by the documented accessory design: only isolated reload-part triangle data
is embedded in the runtime/source, with mod-provided flat surface shading.
No complete game/kit model file, tag, texture, animation bank or editing-kit
executable is bundled. Derived Halo geometry remains artwork of its respective
owners (Microsoft, Bungie and 343 Industries); this does not relicense that
artwork under the project's source-code license. The generic blue item is
original mod-authored geometry.

## Renderer isolation and limitations

The accessory renders after the already completed world-eye resolves into
the shared XR projection images. It uses each submitted eye's actual pose,
FOV and image rectangle, including cropped asymmetric views. The unit is metres;
part/hand/pouch positions all use the same tracked reference space as the grab.
No additional engine hook or native palette write is installed for this feature.

A private deferred D3D11 context records its own shaders, buffers, self-depth
and pipeline state. Both eye command lists must exist and both world copies
must succeed before either optional draw is executed. ExecuteCommandList with
restore enabled preserves the game's full immediate-context state. Resource or
shader failure logs stock-gesture fallback and never changes eyeUploaded,
camera ownership, native HUD/crosshair, lifecycle or XR session state. Cold
worker logs identify model selection, unknown-model fallback and renderer errors.

The parts use simple grey shading, not native textures. They have self-depth,
but not native world/hand/gun occlusion; they can show through nearby scenery or
the native gun. The native gun retains its own magazine until Halo plays its
normal reload animation. The generic item is blue and does not adapt its shape
or dimensions to arbitrary modded art. These are explicit candidate limits,
not a claim of native physical magazine simulation.

## Validation and acceptance

The new GPU suite renders every selected mesh plus the generic item through
the production D3D11 renderer on WARP, checks actual pixel visibility, stereo
disparity, state restoration, staging without native pixel changes, projection
guards, all-title identities, unknown-model fallback, handedness and receipt
invalidation. Its rendered contact sheet was visually inspected locally.
The production-observer suite compiles the actual H2/H3/ODST/Reach observer
bodies against bounded native fixtures, including custom and zero-checksum
models, missing data, primary/secondary ownership and consumed body receipts.
Existing CE production receipt tests cover freshness and graph changes.

Existing gesture/config/input suites cover saved-key migration, separate radii,
click/slide combinations, cancellation, stroke timing, finite inputs and native
button transport. Cumulative Release, all 38 CTest suites and Reach gate pass locally: 2,545
accessory checks, 4,811 gesture checks, 608 production OpenXR/pad checks and
382 production native-observer checks. Packaging repeats the cumulative build,
tests and gate at the final committed identity. Exact results/artifact identities
belong in out/reload-accessories-current-handoff.json.

Headset testing is still required in every title, both CE/H2 renderers, both
handedness modes, stock and custom campaigns, with a Halo 3 regression pass.
Check mags at the hip, grip pickup/carry/drop/insert, shake and ordinary inputs,
weapons with no spare ammo, pause/death/vehicle transitions and switching titles.
Record edition, runtime, headset, source and log. Offline results do not advance
CURRENT-STATE.md or prove every custom weapon works.
