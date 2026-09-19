# Current September 18 delivery status

REFINEMENT-RELEASE-NOTES-2026-09-18.md lists implemented, unfinished and queued/
deferred work; it supersedes stale partial-delivery statuses below. Matching ZIP
delivery is now authorized after zoom/CE-flare work. No install/launch/publication.

Circular zoom implemented H2both/H3/ODST/Reach/H4. CE separate lens unfinished
after native depth-alias proof; native zoom retained. CE Anniversary flare
suppression is an explicit default-off Picture toggle, not a proven root-cause
fix. See ZOOM-AND-CE-FLARES-2026-09-18.md. Release passes; packaging repeats all58
suites after commit. No headset acceptance or accepted-pointer change.

## Historical ledger follows

# September 18 cumulative refinement work (unaccepted WIP)

Request source: original local September 17 chat 01a0af94-4c5c-7612-8863-2e71cfae4562,
September 18 continuity chat, and current packaging instruction. Recovered 133
project sessions into ignored out/refinement-20260918/prior-user-requests.txt.
The original September 17 requests were read in full. Standing September 9-16
requirements remain in CONTINUATION-REFINEMENT-LIST.md; launcher warnings remain
excluded. Accepted baseline remains 1a9766c (Alpha 0.4.2), starting HEAD e1ae49d.

## Work ledger

Latest user scope correction: independent dual aim is H2 Classic/Anniversary
and H3 only. Stop the older ODST/Reach/H4 ordinary-campaign dual-wield expansion;
it is removed from pending work. The unshipped ODST controller experiment is
explicitly disabled, and no acquisition hooks were implemented. Preserve the
separate all-title barrel feature. Scope correction passes Release/all54 CTests.
This supersedes historical references to that expansion anywhere below.

All six title barrel adapters are now locally implemented; F1 Crosshair exposes
the default-off gun_barrel_aim option. CE adds its own native marker/target lease,
with3277 actual production firing/palette checks and364 lifecycle checks.
Release/all54 CTests/Reach gate/CE pinned manifest pass. Details and limitations
in BARREL-MUZZLE-EVIDENCE-2026-09-18.md. No native headset acceptance or ZIP.

H4 barrel adapter now passes Release/all53 CTests/Reach gate and its own pinned
verifier (14 witnesses, ten edges, five hook entries). Production fixture6005
checks; actual final weapon palette ownership, native firing ABI/target leases
and partial retirement exercised. See BARREL-MUZZLE-EVIDENCE-2026-09-18.md.
CE barrel, ordinary ODST/Reach/H4 acquisition and older outstanding scope remain.

Latest: visible hand positioning is locally complete (Release/all51 CTests/Reach
gate;190 production/config checks). See VISUAL-HAND-OFFSETS-2026-09-18.md.
Automatic vehicle smooth turning passes Release/all52 CTests/Reach gate; see
VEHICLE-SMOOTH-TURN-2026-09-18.md. Neither changes headset acceptance. Reach's
barrel adapter is also locally validated; H4/CE and the other standing open
requirements remain outstanding. No partial ZIP is authorized.

Latest user addition: responsive roomscale/body sliding is locally corrected;
see ROOMSCALE-DRIFT-2026-09-18.md. Native stopping travel and callback-thread
history defects are covered by regressions; Release/all49 CTests/Reach gate pass.
No headset claim; existing CE roomscale remains disabled. Barrel implementation
now covers H2/H3/ODST locally; Reach/H4/CE and all earlier open scope remain.

Latest additions: all-title vehicle input audit is locally validated (42 suites;
VEHICLE-INPUT-AUDIT-2026-09-18.md), with headset behavior still unconfirmed. Optional
flashlight input suppression is implemented and passes Release/all 41 CTests/
Reach gate; see FLASHLIGHT-INPUT-2026-09-18.md. Per-title button selections match
the user's MCC layout; native flashlight action bindings are not auto-detected.

1. Targeting/homing/reticle colors: CE continuous native acquisition now uses
   controller aim before the shot-only hooks. Local production fixtures, pinned
   contracts, 24 native cone cases, Release and all 40 CTests pass. Native
   target/range/team/visibility and HUD rules remain. See
   CE-CONTINUOUS-TARGETING-2026-09-18.md; brief-red-flash reproduction and headset
   homing/color confirmation remain outstanding.
2. Native reload tail: six-title local implementation behind NEW default-off
   `manual_reload_shortened_animation`. Existing full animation disable is kept
   unchanged and takes precedence. Native animation seeks to its authored
   insertion; native timers retain the tail and transfer ammo at its end.
   Missing/ambiguous animation evidence retains the full native reload. Source
   review and headset timing/appearance confirmation remain distinct; see
   NATIVE-RELOAD-TAIL-2026-09-18.md and the validation entry below.
3. H4 Promethean item: fixed locally: recognized zero-geometry forerunner models now use a distinct
   orange reload token. It remains visible when unknown-weapon placeholders are
   disabled. Native eligibility, ammunition and reload requests remain unchanged.
4. Textures: implemented locally with authored surfaces for all 42 visible models;
   see the texture implementation/validation entry below.
5. Launcher warnings: excluded by explicit instruction.
6. Co-op crash: supplied log is older 35a4d096, Steam/SteamVR 2.17.9/Oculus-family
   at 72 Hz. It shows H2 Anniversary and shell activity, ending with a 1000ms
   Present stall and module unload. No exception address/stack. Firing and
   ownership audit completed below; do not claim a diagnosed crash from this log.
   Tester wording clarified by the user: "this will either be the crash from firing
   on Cairo Station, or a crash we got right after loading into Outskirts
   (cutscene finished, no textures loaded, game crashed)". Both are Halo 2.
   These are alternative identities for the supplied log, not proof of two
   captured exceptions. Host/client role is not established.
7. Checkpoint/first-person vehicle crash: ODST stale seat-storage restoration
   defect corrected and tested against replaced/inaccessible memory. H3 and Reach
   already re-resolve their own leases. Release/all 41 CTests pass. See
   VEHICLE-CHECKPOINT-LIFETIME-2026-09-18.md; reported crash attribution and other
   independent lifecycle paths remain unproven.
8. H2 vehicles: coherent observer vehicle reference now shared across view,
   controllers and steering, retaining native aim feedback separately. Seated
   throttle no longer undergoes head-relative walking rotation. Release/all 41
   CTests and Reach gate pass; see HALO2-VEHICLE-REFERENCE-2026-09-18.md. Native
   constraints and real vehicle/headset confirmation remain distinct.
9. CE camera follow: configured seated snap/smooth turn and optional actual hull
   yaw following now run through the verified native input-angle phase, retaining
   accepted native packet steering. Release/all 41 CTests/Reach gate and pinned
   manifest pass. See CE-VEHICLE-VIEW-2026-09-18.md; headset result remains due.
10. Walking: local candidate replaces per-axis deadzone floors with a radial
    mapping preserving the requested vector. A tiny lateral component previously
    jumped above 9000 independently. Numeric direction tests added. Native
    head-relative mappings remain; headset comparison still needed.
11. CE flare: preserved reviewed video/log; engine cause remains unproven.
12. Thumbrest D-pad: local exclusive XR publication and final XInput suppression,
    physical-pad suppression, per-control hold draining, roomscale/action cancellation.
13. Menu pointer: same exclusive policy in actual focused tracked menu context;
    private pointer trigger preserved and F1 chord retains priority. Local tests
    cover hold draining and physical-pad filtering; physical melee and gesture pulses also obey this mode. Pointer exclusivity
    additionally requires successful native delivery; D-pad suppresses pointer
    clicks. Full native transport/headset verification remains pending.
14. Independent dual trajectories: new default-off H2/H3 paths use native
    per-hand acquisition and cover later native assist/direct homing consumers.
    Old early-only experiments stay inert. Release/all 46 suites/Reach gate and
    pinned bindings pass; see DUAL-NATIVE-TARGETING-2026-09-18.md. Headset behavior
    and earlier ordinary ODST/Reach/H4 acquisition/aim scope remain outstanding.
15. Barrel origin/direction: all six authored marker catalogs and native adapters
    now implemented behind default-off `gun_barrel_aim`, exposed in F1 Crosshair.
    Release/all54 CTests/Reach gate and title-specific pinned bindings pass;
    see BARREL-MUZZLE-EVIDENCE-2026-09-18.md. No headset result or early ZIP.
16. Full HUD: local default-off hide_hud config/F1 control. H3/ODST/Reach/CE/H4
    gameplay GPU draw scopes execute native callbacks normally; H2 uses existing
    exact gameplay HUD/crosshair shader identities. VR reticle and H4 helmet obey
    option. Menus excluded. All seven D3D11 draw variants now covered; production
    fixture passes 86 checks and Release/CTest/Reach validation passes. See
    HUD-DRAW-COVERAGE-2026-09-18.md; headset coverage remains unverified.

## Verification so far

Release build and all 40 CTest suites passed after correcting new include and
config-parser errors. Logs: out/refinement-20260918/build.log and tests.log.
These validate only changes present at that run; final packaging reruns all checks.
No installation, game-folder writes, MCC launch, publication or accepted-pointer
change. Current work must continue through complete reviewed ZIP handoff.

## September 18 ownership and firing audit

Follow-up: H2 observer cleanup now checks disable/removal results and exact
detour/trampoline ingress as well as counters, retaining dependencies on failure.
Release/all 43 CTests/Reach gate pass; 14,285 production fixture assertions.
See HALO2-OBSERVER-RETIREMENT-2026-09-18.md. Original crash attribution remains
unproven. H3 diagnostic camera/wrist labels no longer claim to measure native
shot origin or muzzle position.

Halo 2 now validates datum table valid flag, capacity, stride, relative storage
bounds, and the full salt before resolving local players or objects in native aim,
shot ownership, collision/contact and reload ownership. Previously several paths
used the low 16-bit slot alone. Stale checkpoint handles now refuse optional VR
mutation; stock non-owned callbacks continue. H2EK datum_get and the previously
verified retail allocator 67AFA0/object creation 8DD4D0 supply these layouts;
see PHYSICAL-CONTACT-MELEE-WORK.md. Tests cover slot reuse, invalid tables,
capacity/type mismatch and relative-address overflow.

The H2 aim-assist exception path no longer invokes an already-started native
calculation a second time. ODST seated personal-weapon ownership now also compares
the complete salted handle instead of the low slot alone. Neither correction is
proof that the tester's Cairo/Outskirts or unspecified vehicle checkpoint crash
has been resolved. No fault stack was present in the supplied log.

Read-only retail audit matched H2 firing 8E4940, aim helper 8F0F70 and generic
network-event 8BD810 against the implemented ABI. The event has eight arguments;
no missing-argument defect was found. Retained output is under out/reload-policy.
H3's previously failed independent dual-aim path stays disabled.

Release and all 40 CTest suites passed the audit run (build-audit.log and
tests-audit.log under out/refinement-20260918). New coverage includes real D3D11
rendering of the Promethean token and CE HUD native-callback fault cleanup.
Final packaging repeats validation for the committed source. These checks are not
headset/co-op acceptance. CURRENT-STATE remains at Alpha 0.4.2.

## Historical incomplete scope at the rejected partial delivery

Items 1, 2, 4, 8, 9, 11, 14 and 15 are not implemented as requested. Items 6 and 7
have ownership hardening but no reproduced crash diagnosis. Walking, exclusive
input, Promethean presentation and HUD changes require headset results, including
Halo 3 regression coverage. Native texture extraction has only collected UV and
material evidence; the renderer still has no proper native textures. The existing
skip-animation policy still skips the whole reload and does not play the final
cocking/chambering tail. No barrel-origin option has been fabricated from grip
position. Do not mark this package as completion of the entire standing list.

# September 18 additional authorized refinements

Finish the magazine texture task already underway, then implement these additions
and continue ALL remaining standing items before packaging. User again explicitly
forbids stopping at a partial candidate.

17. Magazine insertion haptic: verify grab haptic and add a distinct pulse when
    releasing the magazine successfully into the insertion zone. A drop outside
    the zone must not claim insertion. Preserve native reload eligibility.
18. Gun-specific insertion placement: align the required insertion location with
    each held weapon's actual magazine/reload assembly, using verified authored
    geometry/transforms. Respect handedness and weapon alignment settings.
19. Optional per-gun alignment: changes to gun offsets can be saved for the
    specifically equipped weapon, with a per-gun alignment toggle and preserved
    global settings when disabled. Persist stable title/weapon identity and keep
    unfamiliar/ambiguous identities safe. Cover all existing alignment controls.
20. Reach grass stereo report: grass reportedly renders differently in each eye.
    Investigate after current texture work and magazine/alignment additions.
    User explicitly allows an inconclusive diagnosis for this item. Check native
    per-eye camera/culling/LOD and shared draw state using HREK-first evidence;
    do not apply a speculative change or claim reproduction without evidence.



## Texture implementation completed locally after partial-delivery rejection

All 42 authored visible magazine models now carry their official UV/material
assignments through extraction and rendering. H2/H3/ODST/Reach/H4 surfaces come
from each title's own official bitmap export. CE pixels are decoded only after
the official XML export's processed-data checksum, format and dimensions agree.
H4 authored albedo tint and H2 meter tint are retained. Diffuse/emissive image
regions are cropped into one BC1 atlas; no whole editing-kit files are shipped.
Lighting remains the existing simple VR accessory lighting, not a reproduction
of each native game's full material shader or dynamic weapon display.

The material strip boundaries remove two accidental cross-part triangles from
each H2/H3/ODST carbine assembly; other catalogue geometry stays the same.
Catalog identities and native reload eligibility are unchanged. GPU tests passed
34,230 checks, including 42 authored models and both token models in both eyes,
UV bounds, authored color sampling per title and restoration of native surface
and sampler bindings. Preview inspected at ignored
out/refinement-20260918/textured-magazines.png. Headset appearance remains to test.
Evidence: docs/RELOAD-SURFACE-EVIDENCE-2026-09-18.json; reproducible authoring tools
export_reload_surfaces.py, extract_reload_geometry.py, generate_reload_catalog.py.
The full task is still active; no new partial package is authorized.

## Local implementation of additions 17-19

Insertion haptics already existed: support .20 on grab, support .45 and primary
.20 on successful release. OpenXR routing swaps physical hand paths for left-handed
play. Negative and edge-repeat tests now explicitly protect this behavior.

Insertion anchors are the isolated assembly centre transformed into the held
root's bind-local coordinates. CE uses its verified graph name/parent mapping.
Other titles use their own model root's default position/rotation; kit XML inverse
matrix labels are shifted by the scale float, so they are not read literally.
Native adapters supply the visible held palette and title-specific XR conversion.
H4 contact coordinates are body-relative; its separate native position mapping
is sampled to obtain XR coordinates. The comparator and renderer share the held
magazine centre. User radius settings remain unchanged. Generation, identity,
tracking epoch, handedness, 100 ms age and exact prepared serial protect freshness.
Counter logging names authored and generic fallback requests. No native ammo writes.

Optional per_gun_alignment stores 19 alignment controls by known title and primary
weapon identity. Serialization preserves fallback values; HUD stays per-title.
Classic and Anniversary H2 profiles stay separate. F1 names the active gun or
explicit title fallback. Pause retains the admitted gun within its generation.
Identity observation works with either manual reload or per-gun alignment enabled;
transient native object/tag handles are never used as persistent profile keys.

Validation: full Release and 40 CTests passed in build-receiver-targets-fixed.log
and ctest-receiver-targets.log. Core tests cover save/load, switches, toggle,
unknown-model fallback, all title profiles, and H4 world/XR mapping. Interaction
suite: 15,912 checks including all 42 identities/both hands, receiver motion,
stale rejection, and insertion/drop haptics. Native observer suite: 459 checks.
Reach consistency check passed. Final counter logging still needs rebuild.
No headset acceptance or full-task completion claimed; remaining scope stays open.


## Additions 20?21: foliage and ODST camera startup

Reach decorator wind state replay now prevents the two native eye draws from
advancing wind twice; unique pinned bindings, optional fallback and fault cleanup
are verified. Broader kit audit found similar potential H3/ODST/H4 wind hazards,
not yet retail-verified fixes. See FOLIAGE-STEREO-EVIDENCE-2026-09-18.md.
ODST log (21) never armed because a legitimate 0.17 observer offset was rejected
by an exact-zero gate. Finite offsets now pass; all other guards remain. Checked
other titles and found no corresponding zero-offset gate. See
ODST-OBSERVER-OFFSET-2026-09-18.md. Release/all 40 CTests pass after both changes;
Reach consistency gate passed after the foliage change. These are unaccepted
local changes; all remaining standing scope and packaging remain outstanding.
