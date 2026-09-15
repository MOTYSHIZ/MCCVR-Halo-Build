# CE native reticle RGB publication — September 15, 2026

## Player behavior and observed failure

Halo 3's reference is the weapon's native authored artwork, placed on controller
aim with the shared size, distance and stabilization settings. Gameplay HUD
size/aspect/height must remain independent of that art. The user confirms
`22cb813` has excellent, equally smooth Original/Anniversary VR and correct
muzzle effects, but both modes show the temporary generic reticle. See
`HALOCE-22CB813-TEST-2026-09-15.md` for the supplied run's identity.

The log shows thousands of completed CE native capture scopes, no capture
fallbacks, and `nativeOwned=1`; every authored upload window reports zero
uploads, `art 0`, `measuredArt=0`, and `heldArt=0`. This proves that the captured
art never passed the previous measurement gate. It does not itself prove that
the texture's RGB channels were blank: that gate examined only alpha.

## E-CE-RETICLE-RGB-1: native RGB-only bitmap output

All RVAs below refer to pinned `halo1.dll`, SHA-256
`0A12DC561780F449D3F4D0DF10BB8D3BC7BE7840A5BEB2B236F672EB6CD42E6C`.
The existing HCEEK/retail call-tree proof is in `HALOCE-HUD-EVIDENCE.md` and
`HALOCE-HUD-LAYOUT-EVIDENCE-2026-09-15.md`. Crosshair `B3FFA0` calls native bitmap
helper `B5859C`, which passes a null custom-material argument through `B58604`
to the normal `B58ADC -> B0EC20` bitmap draw path.

The actual blend consumer establishes a title-specific fact:

1. `B0EC20` changes native state `A8` to `7` (call at `B0ECEF`). Its normal
   bitmap path therefore selects RGB color writes. The separate custom-material
   branch can select `F`; that is not a license to assume normal bitmap alpha.
2. `B3D8C4` searches the actual 40-byte state-dispatch table. The `A8` row at
   `1B85B78` contains destination `1C532AC`, setter `B3E224`, reader `B3E228`,
   and dirty byte `1C53280`. The setter writes CL into that destination byte.
3. `B3DB28` passes the descriptor at `1C53288` to `5A5F0`. Offset `24` in that
   D3D11 blend descriptor is `RenderTarget[0].RenderTargetWriteMask`, so the
   changed byte is the actual SDK write mask, not an inferred engine enum.
4. On a cache miss, `5A5F0` passes the descriptor to D3D11 device vtable `+A0`
   (`CreateBlendState`). `B3DB28` binds the resulting state through context
   vtable `+118` (`OMSetBlendState`), then clears the dirty flag.

Consequently, the native crosshair draw can put visible RGB on the mod's
transparent target while leaving its cleared alpha at zero. CE's prior
alpha-only measurement must reject those pixels indefinitely. Simply accepting
RGB would still leave an invisible compositor image when the same-sized
texture uses `CopyResource`, so the upload path also needs alpha reconstruction.
This conclusion is independently established for CE; no Halo 4 native offset,
shader or blend constant is copied into CE.

Decompiler and exact instruction-byte records:

- `out/ce-reticle-color-mask-native-20260915.txt`
- `out/ce-reticle-state-apply-native-20260915.txt`
- `out/ce-reticle-native-blend-desc-20260915.txt`
- `out/ce-reticle-native-mask-bytes-20260915.txt`

## Implementation and isolation

CE now measures the greater of RGB ink and alpha ink. A fully blank sample is
still rejected. Only the authored-reticle upload call site admits CE's new
shader conversion, and it requires the exact 512-square source/destination.
The normal CE eye/menu upload keeps its existing copy policy, including for a
512-square image. Halo 3 and the other titles retain their existing policies.

The existing production shader derives alpha from the strongest color channel
and divides RGB by that alpha, making the colored native art usable by OpenXR's
straight-alpha composition. This reconstructs transparency from captured color;
it does not recover the native bitmap's original alpha. The GPU fixture checks
stored color/alpha reconstruction, including antialiased edges and native tint,
not identical blending over every possible headset background. Opaque black
ink cannot be recovered from a zero-alpha RGB-only capture.
The shader text is byte-identical to the previous implementation. CE also uses
the existing typed-RTV retry for typeless XR images, with the negotiated format.
`vr_blit.inl`, `vr_swapchain_rtv.inl`, and `vr_blit_shader.h` expose the same
production bodies to the pixel fixture rather than duplicating those decisions.

The procedural bootstrap remains available only while native art cannot be
measured and successfully published. Successful native publication replaces it.
Existing capture ownership, source preservation, blank rejection, weapon-key
handling, frame age, native restoration and failure-isolation guards remain.
No hook, native binding, render target, weapon tag or game configuration changes.
Cold CE presentation telemetry adds separate `lastAlpha` and `lastRGB` values.

## Validation and limits

`tools/re/test_ce_reticle_blend_native.py` executes 3,546 actual pinned native
instructions through the bitmap entry's mask selection, state-table byte write,
descriptor creation and blend bind. D3D endpoints, blend-cache storage,
compiler cookie and memset are fixture boundaries. It exports the resulting
264-byte D3D11 descriptor, whose actual write mask is seven.

`halomccvr_ce_reticle_pixels_tests` feeds that descriptor to D3D11 WARP, renders
synthetic hollow, colored artwork with antialiased edges, and executes the
production upload body and shader. The old route reproduces invisible artwork:
alpha ink zero, RGB ink 1,780. The new route publishes 27,536 visible pixels,
including 2,582 antialiased fragments. Its checks cover transparent background,
reconstruction of native RGB, blend-state restoration, pipeline/view refusal,
exact CE call-site admission, Halo 3's unchanged copy result, and Halo 4's
unchanged repaired result. A typeless XR image exercises the production typed
RTV retry and the negotiated SRGB shader branch; stored color/alpha matches the
UNORM result within two byte values.

The five CE HUD/reticle suites pass. Records:
`out/ce-reticle-native-rgb-validation-20260915.json`,
`out/ce-reticle-native-rgb-tests-20260915.txt`, and
`out/ce-reticle-pixels-srgb-build-20260915.txt`.

These checks reproduce the native channel-mask defect and verify corrected
pixel transport. They do not execute native weapon bitmap tags or establish
every weapon's live shape, crop, animation, graphics switching, or headset
appearance. The user's Original/Anniversary native-art result and the required
Halo 3 headset regression remain acceptance tests. Cumulative accepted source
stays `4e01f28`.

The fixture executes the production blit/view creation, not the surrounding
OpenXR acquire/wait/release calls. Their inherited non-strict path uses a
one-second wait and does not separately handle a positive timeout result;
this existing ownership edge is unverified and is not changed by the RGB fix.
The supplied CE run did not reach native-art upload and cannot test that edge.
