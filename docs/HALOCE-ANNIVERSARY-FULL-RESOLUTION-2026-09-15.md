# CE Anniversary native full-resolution eyes

## Scope and reference behavior

The Halo 3 behavior being matched is two independently rendered eye images at
the configured render dimensions, with matching depth and a separately sized
HUD. The `58f71a4` CE feedback explicitly requires actual full native eye
resolution before packaging. Increasing only the VR copy/cache dimensions does
not satisfy that request. Original's accepted camera/hands remain the base.

This is E-CE-RES-1. All native addresses below refer to pinned `halo1.dll`,
SHA-256 `0A12DC561780F449D3F4D0DF10BB8D3BC7BE7840A5BEB2B236F672EB6CD42E6C`.
Contracts live in `HALOCE-ANNIVERSARY-RESOLUTION-CONTRACTS.json` and the main
evidence manifest; production verifies their generated loaded-image group.

## Native allocation chain

- `+2D44B0`, the resource pool's virtual `+8` initializer, reads render width
  and height from `[global +2E3BDD8]+118`, fields `+20/+24`. It retains separate
  native full/half/quarter sizes and creates named color, depth and output roles.
- `+20A9B0` registers each pool entry. `+1F3E90`, the root texture's virtual
  `+A8` initializer, creates two children through `+1F9D20` when the root's
  split flag and the renderer's native split mode are enabled. Native children
  receive root width and integer root height divided by two. The split rebuild
  path `+1F9BD0` uses the same child constructor.
- Color, main depth, reduced depth and reduced postprocess resources therefore
  all share this native split mechanism. Expanding their children to the full
  parent height doubles actual eye pixel height while preserving each resource's
  authored relative resolution. Left and right stay distinct native wrappers
  and textures. Width, formats, flags, selectors and backing identity stay native.
- Unsplit output roles `0x04000001` and `0x24000001` receive explicit render
  width and twice render height. They remain unsplit. Shadow resources, imported
  backbuffers and unrelated textures retain native dimensions and lookup roles.
- Native output `+45E2B0` obtains width/height through the selected source's
  native getters and sets the second destination Y to that source height. Thus
  its ordinary copies fill both full-height halves of the expanded output.

Preserved disassembly/decompilation: `out/ce-packed-pool-and-culling.txt`,
`out/ce-packed-target-dimensions.txt`, `out/ce-native-stereo-texture-shape.txt`,
`out/ce-full-resolution-child-pool-20260915.txt`,
`out/ce-full-resolution-resize-pool-20260915.txt`, and
`out/ce-native-two-view-investigation.txt`.

### Allocation-order correction found during final review

The first WIP membership check scanned only already counted pool entries. The
native initializer disproves that ordering assumption: `+20AC33` writes the new
root pointer into `pool[count]`, `+20ACF5` calls its initializer, `+20AD04` writes
the role, and `+20AD52` increments count. Both initial children are created
inside the initializer, before role/count publication. The former fixture made
children after initialization, so it missed this defect.

The production entry detour now lends a scoped receipt for exactly that current
native slot, role and pool. Child admission requires the live pool's current
count and root pointer to match that receipt. It never accepts an unregistered
arbitrary texture. Published entries retain their ordinary bounded membership
check. Nested entry calls and native exceptions restore the prior receipt and
callback count. Exact instruction witnesses guard the ordering in production.

The revised C++ fixture creates actual WARP children during entry initialization,
before publishing native role/count. Both initial color children and both initial
depth children must already have full height. This tests the real production
detours, including the ordering correction, rather than only subsequent resize.

## Lifecycle, camera, depth and HUD coherence

`+455170` calls native management `+455700` before camera preparation and work
submission. The optional management detour first runs the ordinary native
management function. If the live pool lacks the current full-resolution
receipt, it invalidates pending CE frame/list receipts and invokes native
release `+4F1AD0`, then native reconfigure `+4F11E0(0,0)`. Native release drains
its outstanding job through `+425B60` before retiring resources. Reconfigure
recreates the native pool and renderer through their ordinary lifecycle.
Steady frames do not reallocate. Failed rebuilds retain the core and back off
for one second; the cold log records failures and requested dimensions.

The root render settings and Original render allocations stay native. CE's
existing output observation discovers the new real color descriptors even
while a frame is rejected. Present prepares matching VR caches; subsequent
private camera construction uses those measured dimensions. Depth receipts
must match both eye-camera dimensions and must identify independent depth
resources/views before submission. A transitional mismatch drops that frame.
Neither this path nor the cache upsamples a half-height source into a claimed
full-resolution eye.

The expanded packed output is two eyes, not the authored HUD canvas. The HUD
layout reads its native canvas independently and supports both native half
height and full height eye mapping. The natural native callback still owns
its timing and cleanup; the already implemented packed-eye HUD capture keeps
source/resource/revision checks. Original and Anniversary continue to share
the same HUD size, aspect and height settings.

Lifecycle evidence is preserved in
`out/ce-full-resolution-managed-release-reconfigure-20260915.txt`,
`out/ce-full-resolution-managed-lifetime-20260915.txt`, and
`out/ce-full-resolution-wait-review-20260915.txt`. These are native code
observations, not a claim of a completed live resize/headset session.

## Verification and limits

`tools/re/test_ce_resolution_native.py` executes complete pinned native pool,
entry, root, child and split rebuild functions in Unicorn. Its 18 cases cover
32x17, 1920x1080 and 2640x2368; both backbuffer configurations; stock behavior;
the disproven published-only membership policy; and corrected pending-slot
admission. It executes 448,560 native instructions and confirms that initial
children occur before native role/count publication. The broken policy retains
half-height initial children; the corrected policy produces exact full-height
independent children and double-height outputs. Native flags and lookup roles
are preserved. Record: `out/ce-resolution-allocation-native-20260915.json`.

The verifier models wrapper allocation, driver creation, native strings,
auxiliary lookup assets, compatibility/backing queries and the argument changes
made by the detours. It does not execute a game process or a D3D driver.

Production WARP tests separately exercise actual color/depth texture allocation,
native entry ordering, independent eyes, packed roles, odd heights, resize,
nested management, failed-rebuild recovery, exceptions and Original allocation.
The existing render/HUD suites exercise actual GPU copies, depth receipts,
packed HUD mapping and frame rejection/recovery. Cumulative Release and all 22
CTest suites pass in `out/ce-resolution-contact-final-build-20260915.txt` and
`out/ce-resolution-contact-final-tests-20260915.txt`. Packaging repeats checks
for the final committed identity.

No local check establishes headset sharpness, native long-session resize,
graphics-switch/relaunch stability or performance. Both CE modes and the
shared-callsite Halo 3 regression still need the user's headset result. Accepted
source remains `4e01f28`; package-only delivery changes no game files.
