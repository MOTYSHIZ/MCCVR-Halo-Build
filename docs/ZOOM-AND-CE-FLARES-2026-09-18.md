# Zoom and CE Anniversary flare investigation

Target experience: H3 independently magnified gun-mounted world view with wide
headset views. Each adapter uses its own title's established native bindings.

Shared optics: square1024 output, circular antialiased aperture, premultiplied
alpha OpenXR composition, narrow rim, aspect-aware crop including portrait.
Every admitted frame renders the lens; no claim of meeting headset deadlines.
Resources allocate at cold preparation, not scope callbacks. Title/generation
changes and new render requests invalidate stale lens images.

## Title paths

- H3 retained native adapter; shared circular optics and cadence.
- ODST: own eight compact/derived blocks, native viewport/matrix/FP upload and
  render helpers. Camera blocks/FP interpolation receipts restored in finally.
- Reach: admitted inner player-view render only, never the outer frame-once
  renderer. Head cull covers the scope at the same origin; rays outside the
  head's representable perspective hemisphere refuse the optional image.
  Existing HREK-proven first-person admission predicate suppresses scope FP
  pixels only; passenger experiment remains disabled. Workspace, bounded view,
  camera owner, FP and48-byte wind state restore.
- H4: own observer/setup/wrapper with measured projection gain and readback.
  Exact FP caller hides its known120-transform input bank; argument7 is output
  count, not this bank's count. Native observer restoration and feature fault
  latch preserve the camera core.
- H2both: render-only pass after completed real eyes, title-native camera build
  and projection readback. Final target learned from successful real-eye copy.
  Classic restores camera spans/latch; Anniversary restores the exact post-right-
  eye context before existing native camera restoration. Optional failures leave
  stereo ownership intact. FP/HUD suppression retains native callback work.

Actual production inl fixtures: ODST257, Reach899, H43478, H2both226 checks.
Camera/resource refusal, projection, exceptions and state/flag cleanup covered.
No new GPU/headset acceptance.

## CE separate lens: negative result, disabled

Pinned CE SHA256:
0A12DC561780F449D3F4D0DF10BB8D3BC7BE7840A5BEB2B236F672EB6CD42E6C.
Native455A10 renders all primary depth before scenes. The per-view boundary sets
selector16 only for viewIndex==1; every other primary clears it. AD5F0 selects
one of two split children. Index2 therefore reuses left depth. Extended
`tools/re/test_ce_depth_targets_native.py` executes the actual selector, binder
and clear instructions. Third-view case makes scene0's last depth writer view2.
D3D endpoints and geometry ownership are modeled, not GPU/headset results.
Output: out/refinement-20260918/ce-third-view-depth-proof.json.

Replaying455A10 is also invalid:456F1D decrements worker completion (existing
pinned manifest evidence). No third view/frame replay is shipped. Independent
scope transaction/resources remain necessary. Classic's separate adapter also
remains unfinished. Both CE renderers keep native zoom; the menu states coverage.

## Flare report and optional workaround

Preserved exact-11.png in out/test-runs/flare-video-20260917 shows the vertical
white/cyan streak from an overhead light. Log18: accepted1a9766c, Steam,
VDXR1.0.10, Quest3 at72Hz. No video/log synchronization. Final counters:
visible42160, offscreen14, near-clipped0, unproven0, exceptions0. These do not
attribute the streak to a particular effect.

Already-verified native projection446F70/draw447EA0, callers45109B/44718A/4472EB
remain the feature boundary. Native size uses authored size/aspect and width
on both axes; source-centered halos retain alpha outside the normal envelope.
The existing near/offscreen guard stays. No arbitrary brightness/aspect clamp is
justified by footage alone.

User explicitly requires a toggle. ce_anniversary_disable_lens_flares defaults
false, persists both states and is under F1 > Picture. The tracking receipt
freezes the choice for both eyes. Only current-generation owned camera/effect/
caller/record draws are suppressed. No shared effect-data mutation, world shading
change or Classic effect change. Cold logs count userSuppressed. Foreign/stale/
retired/nested draws keep native execution; off restores existing guarded draws.
Production fixture covers both records, on/off, ownership rejection and cleanup;
config fixture covers default and persistence. Native projection emulation:
384cases,432calls,139602instructions, modeled final draw. The toggle is a targeted
workaround/attribution aid, not proof that the reported lighting issue is fixed.
