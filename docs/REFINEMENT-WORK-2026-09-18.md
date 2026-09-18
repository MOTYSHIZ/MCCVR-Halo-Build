# September 18 cumulative refinement work (unaccepted WIP)

Request source: original local September 17 chat 01a0af94-4c5c-7612-8863-2e71cfae4562,
September 18 continuity chat, and current packaging instruction. Recovered 133
project sessions into ignored out/refinement-20260918/prior-user-requests.txt.
The original September 17 requests were read in full. Standing September 9-16
requirements remain in CONTINUATION-REFINEMENT-LIST.md; launcher warnings remain
excluded. Accepted baseline remains 1a9766c (Alpha 0.4.2), starting HEAD e1ae49d.

## Work ledger

1. Targeting/homing/reticle colors: investigating CE native shot adjustment,
   player ray and unit-control publication; no proven causal fix yet.
2. Native reload tail: investigating existing six-title duration/state/playback
   bindings. Preserving ready animation alone does not satisfy the request.
3. H4 Promethean item: fixed locally: recognized zero-geometry forerunner models now use a distinct
   orange reload token. It remains visible when unknown-weapon placeholders are
   disabled. Native eligibility, ammunition and reload requests remain unchanged.
4. Textures: existing renderer uses only one flat grey color; material work pending.
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
7. Checkpoint/first-person vehicle crash: ownership/lifecycle audit pending.
8. H2 vehicles: source finding: controller target built from the current tracked
   camera, which includes the native heading the servo itself turns. Constant
   physical controller offset can maintain error as native yaw advances. The proposed frozen steering reference is retained but disabled: rendered
   camera and reticle still follow native heading, so enabling only a fixed
   steering target would make those disagree. Correction remains pending.
9. CE camera follow: existing E-CE-VEHICLE-CAMERA-1 proves following camera reads
   independent player-control yaw/pitch, not vehicle/unit aim. Correction pending.
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
14. Independent dual trajectories: existing H3 failed path remains disabled;
    H2 path/source audited, further correction/evidence pending.
15. Barrel origin/direction: per-title investigation pending; no guessed bindings.
16. Full HUD: local default-off hide_hud config/F1 control. H3/ODST/Reach/CE/H4
    gameplay GPU draw scopes execute native callbacks normally; H2 uses existing
    exact gameplay HUD/crosshair shader identities. VR reticle and H4 helmet obey
    option. Menus excluded. Needs additional runtime-fixture coverage and headset
    coverage; do not call full native HUD coverage proven yet.

## Verification so far

Release build and all 40 CTest suites passed after correcting new include and
config-parser errors. Logs: out/refinement-20260918/build.log and tests.log.
These validate only changes present at that run; final packaging reruns all checks.
No installation, game-folder writes, MCC launch, publication or accepted-pointer
change. Current work must continue through complete reviewed ZIP handoff.

## September 18 ownership and firing audit

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

## Incomplete scope must survive delivery

Items 1, 2, 4, 8, 9, 11, 14 and 15 are not implemented as requested. Items 6 and 7
have ownership hardening but no reproduced crash diagnosis. Walking, exclusive
input, Promethean presentation and HUD changes require headset results, including
Halo 3 regression coverage. Native texture extraction has only collected UV and
material evidence; the renderer still has no proper native textures. The existing
skip-animation policy still skips the whole reload and does not play the final
cocking/chambering tail. No barrel-origin option has been fabricated from grip
position. Do not mark this package as completion of the entire standing list.
