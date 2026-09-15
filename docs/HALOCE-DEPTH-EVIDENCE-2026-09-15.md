# CE Anniversary interpass depth ownership

## E-CE-DEPTH-1: native boundaries and admission

Halo 3's reference behavior is a complete world render for each eye, with
that eye's own camera and depth. CE's native Anniversary frame is organized
differently: `0x455A10` runs both primary depth views before subsequent
depth-derived passes and the per-eye scene/shading/output loop. The old CE
camera/capture receipts did not establish independent depth ownership.

Pinned input SHA-256:
`0A12DC561780F449D3F4D0DF10BB8D3BC7BE7840A5BEB2B236F672EB6CD42E6C`.
All addresses below are RVAs. No game process was opened or launched.

- Frame entry acquires `__PC_Z_BUFFER__` with pool key `0x8800001` and
  publishes its native wrapper at backend configuration `+0x318`.
- `0x457510`, called at `0x4562F8`, binds this depth and clears depth/stencil.
  `0x4577B0`, called at `0x456324`, draws the native depth mesh batch. Its
  eye argument is the fourth register argument, R9D. Return `0x456329`
  precedes the next depth view; the new hook observes only this callsite.
- Native selector `0xAD5F0` chooses existing wrapper children `+0xA8/+0xB0`
  using native selector bits 15/16 and configuration bit 9. Root/selected
  wrappers use vtable `0x17FB608`; resource is `+0xE0`, writable DSV `+0x108`.
- Native binder `0x205E40` caches its descriptor at immediate backend `+0x18`
  and selected DSV at `+0xD20`, then calls the D3D OM setter. Backend vtable
  is `0x17F9D10`; D3D context is `+0xCE0` and matches global `0x2EA2D30`.
  Full binder evidence remains in `HALOCE-HUD-TARGET-EVIDENCE-2026-09-15.md`.
- The depth pool's native creation flags are `0x1000271`, including split
  flag 4. `0x1F3E90` creates separate `_SPLIT_1` and `_SPLIT_2` children
  when configuration bit 9 permits it. Thus shared depth is **not** established
  as the failed headset run's actual configuration or cause.

After each primary native depth mesh pass, the adapter requires the native
descriptor and cached writable DSV to agree with the selected root/child,
resource and immediate context. Immutable creation metadata must match the
tracked eye raster and ordinary single-sample depth shape. Both eyes must
have distinct resource and DSV identities; different DSVs alone are inadequate.
Camera upload at scene and shading must still resolve the same root, selected
surface, resource, DSV, context, descriptor and resource revision. A later
auxiliary depth draw may not overwrite a completed primary depth resource.

Any missing, aliased, resized, rebound or retired receipt drops that frame.
Native rendering continues and the next clean frame can recover. Cold polling
reports `CE DEPTH` identities/mask/rejection reason. Hot observations perform
bounded reads and invoke the existing selector; no allocation, COM query,
logging, waiting or native resource mutation is added. Creation-time metadata
now includes depth-stencil textures as well as color render targets.

This is a required rendering guard, **not a claim that the visual failure is
fixed**. The three rejected enable flags remain false. It does not prove every
intermediate shader input's contents, actual GPU command execution, or visible
stereo/6DoF. Core CE packaging remains held.

## Reproducible verification

`tools/re/test_ce_depth_targets_native.py` executes the pinned selector,
target binder, clear and viewport instructions with D3D endpoints modeled.
Its seven cases distinguish independent textures from missing children,
disabled native selection, shared wrappers, shared DSVs and separate DSVs
backed by one texture. The modeled geometry ownership check establishes the
interpass hazard condition, not its occurrence in MCC gameplay.

The production runtime WARP fixture now uses real independent depth textures
and DSVs alongside the existing real color-copy paths. Cases cover native
binding mismatch, aliased views/resources, missing depth mesh completion,
opposite-eye selection at scene, recycled resource revision, auxiliary
overwrite and subsequent frame recovery. Classic's independent runtime
fixture remains a regression requirement. Synthetic native calls and cached
ownership fields are explicit test fixtures, not live GPU-binding evidence.

Records:

- Final Release build and all 17 CTest suites pass, including both production
  renderer fixtures. Generated, pinned-image and production mapped-PE contract
  checks and the Reach consistency gate pass. Records:
  `out/ce-depth-final-{build,tests,mapped-contracts,reach-gate}-20260915.txt`
  and `out/ce-depth-contract-verification-20260915.json`.
- `out/ce-core-depth-call-witnesses-20260915.txt`
- `out/ce-core-depth-routing-emulation-20260915.json`
- `out/ce-depth-{scene-boundaries,resource-lifecycle,pool-selection}-20260915.txt`
- `out/ce-packed-pool-and-culling.txt`
- `out/ce-native-stereo-texture-shape.txt` (the first requested function was
  decompiled; a later unrelated requested address failed)
- `docs/HALOCE-DEPTH-CONTRACTS.json`, merged into the core contract

Depth-derived passes also exist between the all-depth loop and final scene:
`0x4596A0` walks the views and calls `0x459A60`; a later view loop calls
`0x45C600`. Moving a depth clear or restoring depth only at final scene entry
would not cover all consumers. No such reorder or replay was introduced.
