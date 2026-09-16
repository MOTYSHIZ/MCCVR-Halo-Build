# Reach native HUD height correction - 2026-09-15

Status: implemented and locally verified; headset acceptance remains pending.
The user reports source `2cf002b6b41dd69b2515469bde82cd2b1553d50d`
cannot raise/lower the Reach HUD, while every other title can. That log is
Steam, SteamVR/OpenXR 2.17.9, Oculus-family headset; the exact model is not
identified. The six Reach safe-frame slots are repeatedly read back correctly.
Size/aspect scanning is therefore not evidence of a height implementation.

The actual source had **no Reach consumer of `hud_vertical_offset`**. Halo 3
and ODST alone installed an anchor-basis height hook. This supersedes the
historical unlocated-anchor statement in `REACH-HANDOFF-2026-07-26.md` and
`RE-notes.md`; their earlier lack-of-implementation finding was accurate.

The Halo 3 behavior being matched is a live native HUD translation in virtual
screen pixels: positive height raises, negative lowers, zero retains the
native position. Scale/aspect and the captured controller-aim reticle remain
independent. No Halo 3 ABI or unverified title offset is reused.

## Official HREK discovery

Input `reach_tag_test.exe`, SHA-256
`CBDD8448A87A433B0DFFC0DE47D06DB7A18B4BF868B96B057135DAA86790ABA8`.
Start with the existing HREK widget draw `+0x922AF0`, then its native bitmap,
text and model draw routines. Their calls `+0x91B409`, `+0x91AD34` and
`+0x921B29` all reach the basis producer at `+0x91C490`.

The producer's unwind boundary is `[0x91C490,0x91D270)`, complete body hash
`36C30C21050AEEF3BB442C2624DAD1607A4F86AD5A45697EAF04DEF85391427B`.
Its assert record at `+0x18AA998` names `basis`; the adjacent file pointer
names `omaha/interface/chud/chud_draw.cpp`. The preceding assert checks
`g_chud_draw_globals.valid`. The official enum table at `+0x20875C0` starts
`parent`, `top left`, `top center`, `top right`, `center`, `bottom left`,
`bottom center`, `bottom right`, and continues through the object/weapon and
other authored anchor types.

The ABI is **six arguments**, not Halo 3's four:

```cpp
bool(userIndex, anchorType, placementFlags*, drawWidgetData*,
     basis*, nativeAnchorFlag*)
```

Incoming full `EDX` selects the anchor type. Argument five is loaded from
`[rbp+0x77]` into `RDI` at `+0x91C4C4`, while argument six is the native
single-byte native output flag. The producer initializes a 0x34-byte matrix
and writes X/Y/Z at `basis+0x28/+0x2C/+0x30`. For example, bottom-right writes
the virtual width and height to X and Y at `+0x91C5C2/+0x91C5C7`.

Anchor 0 is recursive: `+0x91C9C7` calls parent-basis helper `+0x91DCA0`,
which calls the anchor producer again. The parent helper and outer producer
compose native matrices. A correction on every recursion level would compound
height with authored transforms. Only the completed outermost basis is moved.

The sixth output is **not a world-unit flag**. The false-output tracked target
(15), weapon target (21), ghost reticule (22), hologram target (23) and airstrike
target (24) use projected XY. Their HREK branches call `+0x91D270`: it loads XY
from the projection record at `+0x18/+0x1C` (`+0x91D2D3`), stores them to
`basis+0x28/+0x2C` (`+0x91D3E7`) and sets origin Z to zero (`+0x91D3F6`). Its
optional edge rotation changes axes, not the XY origin. Other object anchors
resolve through `+0x91DB80 -> +0x925000 -> +0x92D980` before using that same
basis helper. The projection at `+0x92DF1C/+0x92DF30` multiplies normalized XY
by `+0x4F4D740/+0x4F4D744`, the same virtual width/height as screen anchors.
Therefore subtracting the pixel-height setting from the completed basis does
not move a marker hundreds of world units, and requiring a true sixth output
would leave part of the HUD unadjusted. The wrapper preserves that output
without assigning an unproven broader meaning to it.

## Pinned retail match and runtime admission

Input `haloreach.dll`, SHA-256
`738DD2D24EA3AEA12E1EE9AA4A61094BF116027D42004C35A19E5048608B0894`.
The matched native producer is `[0x2DB54C,0x2DC1BE)`, complete body hash
`BFA80813CCBDDC580FC2444CE782AFDE54633B8C6AB08BDD3CFF8E024CB38889`.
Its own entry loads argument five into RDI and argument six into R14;
`+0x2DB65B/+0x2DB660` store native X/Y. The native callers independently match:

| Path | Caller | Target |
| --- | --- | --- |
| Bitmap | `+0x2DF2D5` | `+0x2DB54C` |
| Text | `+0x2DCA43` | `+0x2DB54C` |
| Model | `+0x2DEC72` | `+0x2DB54C` |
| Parent | `+0x2DA8BA` | `+0x2DB54C` |

The outer producer calls the parent helper at `+0x2DB707 -> +0x2DA818`.
The matched projected-basis helper is `[0x2DA9E8,0x2DAB4B)`, complete body hash
`7F0B521C622B4E562CD2CDDE5B2BF51DEB118F0521239803290D424141D9A46F`.
Its `+0x2DAA30` loads the projected XY pair, `+0x2DAB1A` stores it to basis
X/Y and `+0x2DAB29` sets Z to zero, matching the HREK-established layout.
The cold installer requires the pinned title preflight, exactly one executable
entry signature, exactly one semantic X/Y-store signature, the exact unwind
boundary and all four direct caller edges. Zero/multiple/malformed matches
leave only height stock and are logged. No camera-arming prerequisite is added.

The production wrapper forwards all six arguments unchanged, calls the native
producer once, preserves its return and other outputs, and subtracts the
configured finite `[-300,+300]` height from finite Y only after native success.
A thread-local recursion depth ensures one correction after parent composition.
The current title/generation, installed/armed state, stereo state and teardown
state are checked after the original call. Both normal CHUD phases are admitted;
height does not require the authored-capture per-eye TLS scope.

Reach's existing widget redirect marks a thread-local exclusion while native
reticle/suppressed widgets draw, restoring it in the existing SEH `finally`.
The reticle art therefore remains centered on its separate aiming ray. No
widget is skipped or reclassified. Invalid height/basis memory is a local
stock refusal; only counters change in hot code. The worker reports adjustment
and refusal counts. The optional hook joins Reach's existing frozen-thread,
callback, relay and trampoline teardown checks. A failed hook enable keeps the
disabled feature's bookkeeping until that verified cleanup.

## Verification and remaining acceptance

`tools/re/verify_reach_hud_height.py` verifies both complete binary identities,
function hashes, official assert/enum, caller edges, ABI instructions and the
unique runtime signatures. It then executes the **actual HREK and retail
producer instructions** in private Unicorn images for all seven screen anchors
and the five projected-target anchors listed above, at 1280x720 and 920x690:
**48 native cases pass**, including 20 false-output projected cases. All native
basis axes, X/Y origins, zero Z and native outputs agree. HREK's native tag-block
lookup, per-user lookup and cookie check are stubbed; the anchor math and retail
producer are executed, not replaced with a synthetic formula. Retail uses a
private, verified TLS fixture and no mocked functions. The verifier also checks
the HREK object-projection call edges and matching virtual dimension references.

`halomccvr_reach_hud_height_tests` includes the production six-argument wrapper:
**175 checks pass** for positive/negative/zero/end-point settings with both
native output-flag values, unchanged
other basis bytes/arguments/flags, native false result, captured reticle,
disabled/unarmed/stale/other-title/flat/teardown states, ownership revoked by
the native callback, NaN/infinite/out-of-range settings and native Y, null or
inaccessible basis, optional SEH isolation, propagated native exception,
drained callback/depth accounting, and nested parent composition without
double application. The Reach consistency gate passes.

Native verification output is preserved at
`out/reach-hud-height-native-20260915.json`. Full cumulative build/package
validation is recorded by the candidate handoff. No game was launched or
modified. Local evidence cannot establish headset acceptance: test live Reach
HUD height in both directions and reset, with crosshair aim stable; the normal
candidate headset regression still applies. Reach curvature remains the
previously documented load-time native limitation, independent of this fix.
