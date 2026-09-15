# CE Anniversary scene visibility refresh - September 15, 2026

## Result and scope

Halo 3's reference behavior is a coherent world in both tracked eyes, with
ordinary visibility filtering preserved when entering and leaving VR.

Pinned native execution demonstrates a real cache dependency: adding a second
camera updates the scene regions immediately but does not itself rebuild static
objects' per-eye visibility bits. A scene initialized for one camera can therefore
continue rejecting region-bound geometry in the second view until a native dirty
request occurs. Passing the existing scene-camera callback a refresh request
rebuilds those bits using the independently evaluated camera positions.

This establishes the dependency and its correction under controlled inputs. It
does not establish that the user's failed frame followed this exact chronology,
or that all of its displaced/missing world rendering is corrected. The available
RenderDoc recordings retain the failed image in initial contents, but record mono
retry draw streams; see `HALOCE-RENDERDOC-EVIDENCE-2026-09-15.md`. A new headset
result, plus the required existing-title regression, remains necessary.

## Pinned native chain

Image: `out/deps/re-tools/inputs/halo1.dll`, SHA-256
`0A12DC561780F449D3F4D0DF10BB8D3BC7BE7840A5BEB2B236F672EB6CD42E6C`.
Addresses below are RVAs in that image. The production signatures, instruction
witnesses, call edges and scene vtable identity are recorded in
`HALOCE-EVIDENCE-MANIFEST.json`; no copied engine offset is used.

1. Scene constructor `0x545BD0` installs vtable `0x1819658`. Slots `+0x230`
   and `+0x238` point to `0x5434B0` and `0x543CB0`. This is distinct from the
   separately registered resource constructor `0x545A70` and its vtable.
2. Preparation `0x454E40` calls the scene-camera slot using the active list's
   primary and secondary camera positions, then the scene-update slot before
   geometry admission. The copied-list preparation branch supplies equivalent
   positions. Relative to the view-list base, these are `+0x70` and `+0x438`.
3. `0x5434B0(scene, refresh, primary, secondary)` copies supplied positions
   to scene `+0x168/+0x174`, calls base maintenance `0x64D450`, then calls
   region update `0x543550` with region masks `0x20/0x40`. A nonzero `refresh`
   finally ORs scene `+0x110` with dirty bit `0x100`.
4. `0x543550` records presence of both camera pointers in scene flag `0x1000`.
   It clears and recalculates the region `+0x1A8` membership bits separately
   for each camera. Authored-hidden region flag `0x800` is respected. Updating
   this membership does not itself request the static-object rebuild.
5. `0x543CB0` enters the native scene critical section, promotes pre-existing
   dirty flags `0x200/0x400` to `0x100`, and calls moving-object update
   `0x543830`. With a nonempty region list, it calls static refresh `0x543AF0`
   only when dirty. The native code consumes `0x100`, performs the refresh,
   emits its event and balances its lock nesting.
6. `0x543AF0` respects static-object exclusion flag `+0x2C & 0x20000`. For a
   region-bound object it clears eye bits `+0x8E & 6` and sets the hidden bit
   `1`; an unbound object instead starts admitted to both eye bits. It traverses
   the scene regions through `0x542F00`, which translates region bits
   `0x20/0x40` into object bits `2/4`. The refresh tail-calls `0x2B2170`, which
   publishes the static geometry pointer and model-bounds arrays.
7. Geometry admission `0x2D19A0` walks the actual view list. A view marked
   `0x100` requires object bit `2`; one marked `0x200` requires object bit `4`.
   Independent native geometric predicates run only after these visibility
   checks and can still reject either eye.

The earlier upstream region audit in `HALOCE-STEREO-AUDIT-2026-09-15.md` correctly
found both camera positions being evaluated. It did not establish the downstream
static-cache refresh. This evidence closes that specific gap without changing
camera construction, source-player identities, GPU query outcomes or geometric
culling predicates.

## Native execution verification

Run from the repository root:

```powershell
python tools/re/test_ce_scene_refresh_native.py --output out/ce-native-scene-refresh-verification-20260915.json
python tools/re/test_ce_visibility_native.py --output out/ce-native-named-visibility-final-verification-20260915.json
```

Scene refresh result: `PASS_NATIVE_SCENE_REFRESH_AND_ADMISSION`, 10 cases,
829,175 native instructions, 10 balanced scene locks, 8 actual static refreshes
and 30 calls to the native geometry-admission function. Three fixture objects
represent an admitted region-bound object, an authored-hidden region object,
and a region-unbound object. Unrelated object flag `0x80` survives every case.
The native static-list producer's object pointers, counts and model bounds are
checked as well.

| Sequence / input | Object flags | Geometry masks | Static refreshes |
| --- | --- | --- | --- |
| Mono initialization with refresh | `82, 81, 86` | `1, 0, 3` | 1 |
| Add second camera without refresh | `82, 81, 86` | `1, 0, 3` | 0 |
| Same second camera with refresh | `86, 81, 86` | `3, 0, 3` | 1 |
| Keep two cameras without refresh | `86, 81, 86` | `3, 0, 3` | 0 |
| Return to mono with refresh | `82, 81, 86` | `1, 0, 3` | 1 |
| Only second camera belongs to region | `84, 81, 86` | `2, 0, 3` | 1 |
| Neither camera belongs to region | `81, 81, 86` | `0, 0, 3` | 1 |
| Both belong, second fails geometric test | `86, 81, 86` | `1, 0, 1` | 1 |
| Pre-existing native dirty `0x200` | `86, 81, 86` | `3, 0, 3` | 1 |
| Pre-existing native dirty `0x400` | `86, 81, 86` | `3, 0, 3` | 1 |

Object flags are hexadecimal; admission masks use bit 0 for the first fixture
view and bit 1 for the second. Both views remain in the admission fixture even
for the mono case to expose what a newly added second view would consume.

The scene-camera, region, scene-update, static-refresh, region-application and
static-list routines execute their pinned native bytes. Base maintenance and
moving-object update also execute, with empty unrelated resource and dynamic
object collections; this is not dynamic-object coverage. Region-volume results,
bounds/frustum results, OS lock imports, progress/event callbacks and the
security-cookie check are explicitly modeled. The full mapped image is made
read/execute-only after fixture imports/globals are initialized. Unexpected
native calls/instructions fail verification; no DLL entry point is executed.

On resume, the unfinished verifier failed because its allowlist ended before
the moving-object return epilogue and omitted the static refresh's native tail
call. Pinned disassembly established the actual epilogue end `0x543AEA` and
tail-call target `0x2B2170`. The verifier now executes those bytes; it does not
bypass either routine. Other allowlist endpoints were tightened to their returns.

The separate named-visibility verifier passes 48 producer and 108 application
cases, 7,638 native instructions and 72 modeled CRT comparisons. Its producer
is intentionally entered after name lookup at `0x53C49B` and stopped at
`0x53C4DF`; application `0x2E2160` executes completely. Mono visibility for
source player zero produces mask `3`, while synthetic split input requesting
only player zero produces mask `1`. That is a separate native mask semantic;
it is not evidence that an unobserved live producer caused the failed frame.
No named-visibility override follows from that fixture alone.

## Production integration boundary

`SceneCameraHook` requests the existing native refresh only when the previously
recorded two-camera bit differs from the supplied camera-pointer count. It
requires the verified scene vtable and camera-position pointers belonging to
the currently owned active or copied preparation list. Existing nonzero native
refresh arguments pass through unchanged. The original native routine performs
the update; the hook does not write object masks or bypass hidden regions.

The native previous/current comparison avoids a new lifetime cache and repeated
refreshes on stable frames. The owned return to a single camera also requests
refresh during stock fallback, including while XR arming is unavailable. Hook
installation/retirement received an independent source audit; the compiled
runtime fixtures check the callback's ownership guards and retirement behavior.
Offline native execution
does not establish callback ordering in the user's running scene or a headset
image. The accepted source pointer remains `4e01f28`.

Additional preserved native records: `out/ce-world-scene-camera-virtuals-20260915.txt`,
`out/ce-world-scene-visibility-masks-20260915.txt`,
`out/ce-scene-static-visibility-dirty-20260915.txt`,
`out/ce-scene-dirty-region-mutators-20260915.txt`, and
`out/ce-scene-refresh-verifier-instructions-20260915.txt`.
