# CE Anniversary natural late HUD sequence - September 15, 2026

Halo 3 behavior being matched: the native gameplay HUD reaches both eyes and
keeps the shared framing controls and independent authored crosshair. This
document establishes CE's own native ownership and ordering. It does not
establish headset acceptance or complete safety of repeated gameplay drawing.

Pinned retail `halo1.dll` SHA-256:
`0A12DC561780F449D3F4D0DF10BB8D3BC7BE7840A5BEB2B236F672EB6CD42E6C`.
All addresses below are RVAs. Only the preserved image was read.

## Native ordering and the missing capture

The earlier HUD evidence correctly identified that synthetic list flag bit 0
does not select the *early per-eye* callback branch, which requires bit 1.
That finding is incomplete without the late branch. The same full frame
`455A10` runs another, natural HUD callback **after both output copies**:

1. The per-eye loop calls `45E2B0` at `456DD0`. Its first eye obtains the
   packed native destination and publishes `2E3D0D0` at `45E2EB`. Its transfer
   descriptors copy the first eye at Y=0 and the second at Y=source height
   (`45E35D`). Both transfers use the same destination wrapper.
2. After the eye loop, `456ECE` pushes the current descriptor. `456EEC` loads
   `2E3D0D0`, and `456EFA` calls `1DBE50` to bind that packed color with the
   parent's depth wrapper at `+318`. Thus the packed target is established
   before native worker cleanup and the later callback, not merely aliased
   by the callback's kind-1 globals.
3. At `4572A5..4572DC`, the late branch requires a positive view count,
   frame flag `0x10`, nonnull native owner `2E3CD78`, and list flag bit 1 clear.
   A synthetic list with flags=1 therefore **enters** this branch.
4. `4572F0` calls native preamble `4255A0`, which tail-dispatches `740B0`
   through `1C33FE0`. This is still inside the original `455A10` call and
   therefore inside the mod's `FrameBody` lifetime, before `cache.Finish`.
5. The ordinary non-upscaled branch calls `45E450` at `4572FA` only **after**
   that callback returns. `45E450` pushes the native target descriptor and
   binds `2E3D0D0` with the parent's depth wrapper at `+318`. It does not
   create or populate the packed color surface.
6. Native per-view overlays `4512D0` follow. With list bit 0 set, their index
   arguments are 0 and 1. `425660` follows at `457381`, and final composite
   `45E490` follows at `4573C7`. An upscaling path can composite before the
   HUD and use `2E3D0C8`; the ordinary packed resource identity must never be
   assumed for that case.

The existing mod captures individual world sources during step 1. Natural HUD
pixels written in step 4 consequently cannot appear in those copies without
an explicit later update. Calling `740B0` manually inside step 1 changes its
native lifetime and remains disabled after the earlier graphics-switch freeze.

## Native callback target and gameplay boundary

`740B0` saves target kinds 1 and 2. It loads the previous output `2E3D0C8` at
`7412A`, loads current output `2E3D0D0` at `74131`, chooses the current output
when nonnull at `7413B`, and publishes that choice as target kind 1 (`2E3B918`)
at `7413F`. The secondary target is acquired normally. It then owns the native
critical section, interface setup, player selection, and gameplay dispatch.
Cleanup `ADF50` restores both prior target kinds and releases the secondary.
The final full-height viewport call is at `7441D`.

`B31D88` calls gameplay main `B12B08` at `B32106`, between independent interface
calls `AC7EF0` and `AA2070`. The official HCEEK counterpart and native bitmap,
radar, status and text families are established in
`HALOCE-HUD-LAYOUT-EVIDENCE-2026-09-15.md`. The main reads the current player,
recomputes HUD resolution/scale and dispatches those drawing families. It owns
none of the outer callback's target-kind borrower or critical-section setup.
The native callback itself can dispatch two players, but that does **not**
prove that every descendant of a repeated same-player `B12B08` draw is free
of side effects. Keep any repetition inside the established natural scope,
restore its observed raster, and test actual mod routing separately.

The packed capture adapter must prove the actual gameplay target snapshot
matches **both** observed earlier copy destinations, including selected
resource, registry revision, context, descriptor and full packed bounds.
The wrapper-global relationship is a lead, not sufficient live resource proof.
Capture after the real callback returns must recheck those identities and the
frozen current-frame ownership. Refusal leaves the already captured world pair
available; it must never call the outer callback again or disable the core.

## Native callback cleanup and final source check

The callback's final backend call at `744AC` uses vtable slot `+28`.
The pinned pointer at `17F9D38` resolves that slot to `203C00`. This native
method calls context `ClearState` at `203C17`, clears cached color count
`backend+CF8` at `2043A4`, and clears cached depth view `backend+D20` at
`2043AA`. It retains the target descriptor at `backend+18`; constant/buffer
rebinding follows, ending with the tail dispatch at `20443E` to `1DBB60`.
Consequently a post-callback check that requires the previously bound RTV
cache to remain populated would reject the ordinary native sequence.

The final capture instead rechecks the saved descriptor, backend and context
identities and resolves the saved descriptor's native wrapper again. Its
selected surface and resource must still match the gameplay snapshot. It never
uses target-kind globals after their native restoration as ownership evidence.
The existing registry revision/texture descriptor and frozen-frame checks then
run immediately before copying. Changed source selection or context refuses
only the HUD update and keeps the previously captured world pair.

`test_ce_late_hud_sequence_native.py` now also executes the actual `203C00`
instructions. It confirms `ClearState` is reached, the descriptor stays exact,
and cached color count/depth both become zero. Only D3D and the constant/buffer
rebind endpoints are fixtures in this additional case. The production WARP
fixture mirrors that real clear/unbind epilogue, including raster-observation
invalidation, and covers post-gameplay source/context mutation and recovery.
The actual HUD detour wrapper uses explicit SEH cleanup for its callback count;
the fault fixture requires the original native exception to propagate with
the in-flight count restored, so retirement cannot be blocked by that callback.

## Packed scissor enforcement

Drawing both HUDs into one packed resource requires each eye's scissors to
remain inside its own half, including large vertical slider offsets and native
viewport resets. Clamp only the emitted packed scissor rectangles after the
gameplay affine and eye affine. Preserve authored observations, private reticle
raster bypass and exact saved-state restoration. Empty scissors stay empty.

The native state constructor `202D70` builds a `D3D11_RASTERIZER_DESC` at
`rbp+70`. Immediately before its device `CreateRasterizerState` call `2034F1`,
instruction `2034B6` writes 1 at `rbp+8C`, descriptor offset `1C`, which is
`ScissorEnable`. The state object stores the resulting rasterizer at `+58`.
Native backend `205BE0` loads that `+58` field at `205CA9` and calls context
`RSSetState` at `205CC7`. Thus the backend's ordinary constructed states honor
the bounded scissors. This is native state evidence, not an assumption that
D3D11 enables scissors by default. The optional contract pins these witnesses.

## Offline validation and limits

`tools/re/test_ce_late_hud_sequence_native.py` executes pinned instructions for
the two output descriptors, late branch, callback target-kind borrowing and
normal cleanup. Five cases pass: ordinary synthetic stereo, missing HUD frame
flag, list bit 1, uninitialized HUD and native two-player dispatch. The ordinary
case observes two packed copies to the same wrapper at Y=0/360 before exactly
one natural callback and one interface draw. Both saved wrappers and native
lock depth return to their initial values, and the final viewport is 720 high.
The same verifier executes 48 native state constructions with varied source
state values; every actual rasterizer descriptor has `ScissorEnable=1`. Only
the D3D creation endpoints, memset and security-cookie services are fixtures.

The D3D/backend services, target acquisition, player draw descendants and
interface draw are explicitly substituted fixture services. These tests do
not execute real HUD pixels, prove a same-player repeated draw safe, model
arbitrary native exceptions, or establish the prior freeze's cause. Existing
native target push/pop/binder evidence remains a separate test.

`HALOCE-LATE-HUD-SEQUENCE-CONTRACTS.json` supplies unique optional late-branch,
native scissor-state and backend-clear signatures, fifteen exact relative edges,
twenty instruction witnesses and the native clear-state vtable pointer for
runtime integration. Results: `out/ce-late-hud-sequence-bindings-20260915.json`
and `out/ce-late-hud-native-sequence-20260915.json`. All local verification
remains supporting evidence, with accepted source unchanged at `4e01f28`.
The continuation audit's native sequence/scissor/clear-state result is
`out/ce-late-hud-native-audit-20260915.json`.
