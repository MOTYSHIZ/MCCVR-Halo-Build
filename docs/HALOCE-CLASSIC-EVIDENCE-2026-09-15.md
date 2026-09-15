# CE Original / Classic render evidence — September 15, 2026

## Status and reference behavior

Halo 3's behavior to match is two independently rendered, correctly culled eyes,
headset rotation and physical leaning, a single consistent controller rig, and
the shared OpenXR/configuration/lifecycle behavior. Classic needs its native CE
render path; manufacturing an Anniversary view list does not render Classic.

This is **local implementation and offline evidence**, not a headset result.
The accepted pointer remains `4e01f28`. The three failed Anniversary candidates
are not evidence of working Classic VR. The user's package hold includes both
CE renderers, tracked hands/weapon/aim, HUD/reticle and shared input. CE physical
melee and world collision remain deferred. No game launch, installation, game
file modification or package was used for this work.

Inputs are the pinned HCEEK `halo_tag_test.exe` and MCC `halo1.dll` identified in
`HALOCE-EVIDENCE-MANIFEST.json`. Retail SHA-256 is
`0A12DC561780F449D3F4D0DF10BB8D3BC7BE7840A5BEB2B236F672EB6CD42E6C`;
kit SHA-256 is
`FC9E2B6193C6F6D9FF988278B0D39A983747F3FDBDECA7CAFADD28F1F0C53E73`.
No bindings come from another Halo engine or a console executable.

## E-CE-C1: native render scope and replay boundary

E-CE-1 established the HCEEK/native camera, window and render functions. Retail
`0xAC47C0(float,float)` constructs player windows and the final UI window at
`0x2E9FE80`, stride `0xAC`, then calls `0xBBCE28` at `0xAC4956`. Its float
arguments are saved from XMM0/XMM1 and forwarded on the stack. The outer
renderer derives an absolute timestamp from the existing native tick and
interpolation; it does not run the simulation update `0xAB0D10`.

`0xBBCE28` renders its normal player through the call at `0xBBCEE6`, then the
UI window, then `0xB2F8BC` and final output `0xAE0DF4`. A second call to
`0xAC47C0` therefore rebuilds the window cameras, visibility and native target
state before drawing its second eye. The Anniversary host frame renderer and
its worker completion are outside this replay boundary.

This boundary has render side effects. It increments the outer render frame
identifier, and `0xB11FE4 -> 0xAE0684` sets the absolute render timestamp and
increments native cache frame counters through `0x2DA0848/+0x38` and, when
enabled, `0x2EA8900/+0x38`. It also invokes the render reset helpers
`0xC11D50` and `0xBD9644`. These have **not** been described as side-effect-free
or rolled back by guessing the rest of the cache structures. Regenerating
render state for each eye is intentional. HCEEK outer renderer `0x4267B0`
confirms the same existing-tick/interpolation and window-render structure.

The runtime pins one player, initialized clock address and tick, title
generation, graphics-mode epoch, reference revision and one tracking sample.
A change rejects the current pair. Exact entry camera bytes must repeat for
the second eye. Main-game rendering is the only repeated function; simulation
update is never invoked by the adapter. Actual native replay across missions,
reflections, animated effects, paused state and map transitions still needs
headset testing. Offline evidence cannot certify every native render side
effect merely from the absence of a direct simulation-update call.

## E-CE-C2: both cameras and the actual view consumer

`0xBBCA64` constructs two native frusta: render camera at window `+4`, and
raster camera at `+0x58`. Each camera is `0x54` bytes. Native fog can lower the
render camera's far clip and correct a collapsed clip interval. The optional
reflection call is `0xBBCC77`; the normal class-1 call is
`0xBBCCAD -> 0xBBCF30`. The latter receives:

| Argument | Native source |
| --- | --- |
| player | window player index |
| render camera | window `+4`, RDX |
| render frustum | native stack frustum, R8 |
| raster camera | window `+0x58`, R9 |
| raster frustum | native stack frustum, first stack argument |
| view class | `1` for primary, `2` for reflection |
| reflection state | native byte, passed through |

`ClassicViewPair` stages both cameras for each eye from each camera's own stock
position/basis; alternate native cameras are not assumed to alias. It retains
opaque fields, windows and clip values. The shared cover projection maps the
rendered raster into the requested OpenXR eye FOV.

The native consumer hook requires one exact normal call per eye, the actual
window camera pointer identities, valid distinct frustum pointers, source
player, and the staged camera bytes. Only the native fog far-plane adjustment
is permitted. A pointer to an identical unrelated camera cannot claim an eye.
Reflections execute normally and cannot count as a primary eye.

`0xBBCF30` publishes the render camera to `0x29AF2C4` and copies its full
`0x18C` frustum to `0x29AF318` (three `0x80` chunks and twelve trailing bytes).
The runtime snapshots and restores these contiguous `0x1E0` bytes together
around the temporary eye camera, in addition to restoring the two window
cameras. This preserves prior native camera/frustum state exactly, including
opaque bytes, instead of reconstructing an assumed stock frustum. The earlier
`ClassicStockFrustum` helper remains dormant and is not used by this path.

First-person/HUD context is the saved **stock center**, tracking sample,
reference, world scale and positional setting captured once before either eye.
It is identical in both passes; it is not reconstructed from a temporary
global eye camera. The same context is published for bounded gameplay-thread
aim access, subject to root integration's revision/lifetime guards.

## E-CE-C3: completed output and exact source selection

World/postprocessing uses native kind 1/2 ping-pong surfaces, and the
postprocess helper `0xB0D40` can swap those surface slots and their cached
views. Capturing a fixed kind-1 wrapper before that swap is not final-output
proof.

`0xAE0DF4` selects target kind 0 via `0xB51670(0,0,0,0)` and invokes
`0xB51A14` at `0xAE0EC6`. `0xB51A14` samples kind 1 through `0xB519A0(0,1)`
and draws its full final quad through `0xBF5F08`. The hook captures **after**
that call, from the selected kind-0 output, before the next eye can overwrite
it. The later `0xBA19DC` is a conditional small native debug rectangle; it is
not the gameplay scene or a second eye source.

**Correction after the `be2140f` user result:** kind-0's wrapper is
`(*0x2E3C090)+0x500`, and its current cached RTV is `0x1B85E78`.
The former claim that kind 0 uses `0x2E3B910` was false; the native initializer
populates that array only for kinds 1..8. See E-CE-C5 below.
`0xAE0E0` proves the mapping from those nonzero native wrapper array entries
to per-kind SRV/RTV cache (stride `0x30`). The verified texture wrapper vtable is `0x17FB608`,
virtual `+0xD8` is `0x22B7B0`. Its selector chooses the actual surface variant;
the selected object has resource `+0xE0` and RTV array `+0xE8`.
The adapter runs the shared verified read-only selector and requires its RTV
to match the native cache. It does not assume the root wrapper's `+0xE0` is
the selected resource. Global immediate context `0x2EA2D30` must match the
backend context at `(*0x2E3BDE0)+0xCE0`.

Resource descriptors come from recorded creation/import metadata, never a COM
query in the hot hook. The two outputs must retain wrapper, selected object,
RTV, resource, resource-registry revision and context identity. Output rectangle,
native viewport and descriptor dimensions must agree exactly. Each completed
eye is copied immediately into owned GPU storage; source reuse cannot change
an already copied eye. Mode switch/recenter/resource change/incomplete pair
reject submission and preserve core ownership.

## E-CE-C4: HUD scope and remaining coverage

Gameplay HUD dispatcher `0xB12B08` is reached at `0xB32106` from `0xB31D88`,
near the end of `0xBBCF30`, after world/postprocessing and before
`0xAC8844/0xAE0DBC`. It reads current player `0x29AF2B8`, validates player
index `0..3` and the player datum, then dispatches authored HUD draws including
`0xC504C4`, `0xC5093C`, `0xB74830` and `0xC51EC8`.

`0xB321D8` is a separate global/UI-window callback called at `0xBBCCFA`; it
must not be relabeled gameplay HUD based on the nearby address. Authored HUD
extraction, reticle handling and the first-person feature each have separate
runtime ownership/evidence work. Capturing Classic's final world target alone
does not prove their desired VR presentation.

## E-CE-C5: failed kind-0 source and native output ownership correction

The September 15 `be2140fc440462c8ac1441186...` user log reports 610 Classic
outputs and 610 source misses, zero completed Classic pairs, and final failure
4 (`PairPreparation`). It is Steam, SteamVR/OpenXR 2.17.9, Oculus-family, 90 Hz.
The missing source prevented publication of the cold eye-cache request;
therefore the subsequent camera scope could not begin capture. It also rejected
the render context used to admit tracked first-person palettes. The log did not
contain a source substage, so the precise runtime pointer value was not logged.

Pinned initializer `0xAE410` proves the source route independently:

| Instruction | Native effect |
| --- | --- |
| `0xAE43E` | Load output owner from `0x2E3C090` |
| `0xAE455` | Load that owner's texture wrapper at `+0x500` |
| `0xAE462` | Call wrapper virtual `+0xD8` with all view indices zero |
| `0xAE489` | Publish the resulting RTV at `0x1B85E78` (kind 0) |
| `0xAE490` / `0xAE49A` | Select the same wrapper variant and read its resource at `+0xE0` |
| `0xAE4C3` / `0xAE4CD` | Initialize the following wrapper-array loop to kind 1 |
| `0xAE601` / `0xAE6CE..0xAE6D3` | Populate wrapper-array entries 1 through 8 only |

The Classic adapter now follows the native kind-0 owner and requires the
selected texture/RTV to match the native cache and immutable resource record.
Owner identity is frozen across both eyes as well as wrapper, selected surface,
resource revision and context. A missing owner does not fall back to the
unrelated wrapper array. Source failures record a bounded substage; no COM
queries, logging, allocation or scanning are added to the render hook.

`tools/re/test_ce_classic_source_native.py` executes the actual pinned initializer
from `0xAE410` through its complete kind-0 publication, stopping before allocation
of kinds 1..8. Native virtual RTV access and variant selection execute too.
Twelve cases cover root/two native variants, absent/decoy array element zero,
and absent/replaced cached RTV. COM AddRef/Release/GetDesc are explicit fixtures.
All cases pass in `out/ce-classic-source-native-20260915.json`.

The production Classic WARP test now starts with no eye cache and an empty
wrapper-array element zero. The first stock output discovers the true source;
`HaloCE_PresentResources` then allocates storage, and the following frame retains
distinct left/right pixels. Missing owner, decoy array, mismatched RTV and
inter-eye owner replacement checks pass, including frame-drop recovery. The
old test pre-seeded both the incorrect array entry and the eye cache, hiding
this native-layout and bootstrap failure. Native contract verification includes
the initializer signature, root/cache operands and loop witnesses.

These checks establish the wrong source route and its local correction. They
do not establish in-headset Classic parity or acceptance of the native renderer.

## E-CE-C6: preexisting DXGI output metadata

The kind-0 wrapper can bypass both hooked native resource management functions.
Pinned `0x1EFE90` acquires buffer 0 through the output owner's swapchain at
`+0x4E8`, virtual `+0x48` (`GetBuffer`), using the 2D texture IID. It creates
the named wrapper `__PC_BACK_BUFFER__`, writes it to owner `+0x500` at
`0x1EFF5B`, and directly assigns the returned COM texture to wrapper `+0xE0`.
It does not call the native texture create/import functions at this boundary.
See `out/ce-classic-host-output-create-20260915.txt` and
`out/ce-classic-host-output-disasm-20260915.txt`. The supplied log's backbuffer
exists before CE hooks install, so a later native import notification cannot
be assumed to supply its descriptor.

The existing cold `SubmitPreparedFrame` already holds a successful DXGI
`GetBuffer(0)` reference and calls `GetDesc`. It now publishes that exact texture
identity and descriptor to the CE registry. The native Classic source still
must independently agree with its owner, selected wrapper and cached RTV; no
screen texture is relabeled a native source by its dimensions. Repeated
observations preserve an unchanged resource revision. Resize and VR detach
revoke this observation before the buffer is retired, and retain no COM
reference or texture pointer for dereferencing in a render hook.

The Classic runtime suite now uses a real WARP DXGI swapchain with a hidden
test window. It obtains the source with `GetBuffer` and supplies **no** manual
creation/import event. An initial frame correctly fails for unknown metadata;
the cold presentation observation admits the following source discovery, then
the normal Present resource preparation permits the independent eye pair.
Repeated observation, revocation and re-observation pass. A real DXGI
`ResizeBuffers` succeeds after releasing the test's own references, proving
the registry retained none; reacquiring and observing the replacement buffer
restores capture. This closes a bootstrap gap that the former manually
registered WARP texture did not exercise.

## Verification and its limits

`HALOCE-CLASSIC-CONTRACTS.json` records unique executable signatures, unwind
entries, relative operands, instruction witnesses and image pointers.
`tools/re/verify_ce_classic_evidence.py` verifies both pinned files and Classic
contracts. The resumed result is `out/ce-classic-contract-verification-resume.json`
(`PASS_OFFLINE_ONLY`). The central generated runtime contract includes these
Classic entries separately from Anniversary and optional features.

`halomccvr_ce_classic_view_pair_tests` checks independent camera staging,
preservation and lifecycle identity. `halomccvr_ce_classic_runtime_tests`
executes the production hook bodies with native-call fixtures and real D3D11
WARP textures. It verifies distinct retained eye pixels, same stock controller
context in both eyes, float ABI arguments, native-state restoration, and
recovery after missing/foreign/changed camera consumers, duplicate windows,
missing/resized output, changed tick/clock, mode switch, recenter and source
revision change. The focused runtime suite passes locally. The native-call
fixture does not execute CE engine code and cannot establish in-headset stereo
correctness, native GPU projection, animation quality or full title parity.

Preserved derivation includes `out/ce-classic-native-boundary.txt`,
`ce-classic-target-routing.txt`, `ce-classic-output-source.txt`,
`ce-classic-final-kind1.txt`, `ce-classic-kind0-mapping.txt`,
`ce-classic-replay-hud-resume.txt`, the E-CE-1 kit/native dumps and exact
`ce-retail-main-render.txt`/`ce-retail-window-disasm.txt` disassembly.
