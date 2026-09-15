# CE native world contact and physical melee

## E-CE-CONTACT-1: native ownership

Halo 3's accepted reference is independent contact for each tracked hand,
physical motion rather than animation as the strike trigger, native collision
and damage, separate world-collision and physical-melee controls, and optional
failure that preserves working camera and hands. CE uses its own routines to
provide those behaviors. No Halo 3 address, object offset or damage layout is
used as CE evidence.

The pinned HCEEK `halo_tag_test.exe` SHA-256 is
`FC9E2B6193C6F6D9FF988278B0D39A983747F3FDBDECA7CAFADD28F1F0C53E73`.
The pinned retail `halo1.dll` SHA-256 is
`0A12DC561780F449D3F4D0DF10BB8D3BC7BE7840A5BEB2B236F672EB6CD42E6C`.
These are the existing `HALOCE-EVIDENCE-MANIFEST.json` inputs. Addresses below
are RVAs, except where an official executable address is explicitly given.

| Semantic role | HCEEK RVA | Retail RVA | Evidence |
| --- | --- | --- | --- |
| Biped update, native melee dispatch | `0x4C4F10` | `0xBAF258` | Both updates call their own player melee helper with the biped handle and predicted target/material; both return the successful-update byte. |
| Segment collision query | `0x41EE60` | `0xB913FC` | Flags, start point, displacement, ignored owner and result buffer; initializes result type to NONE and fraction to one. |
| Known-good to desired point resolver | `0x41CC30` | `0xB93C8C` | Official `physics/collisions.c` assertions name `known_good_point`, `desired_point`, `good_point`, and the unused second ignored object. Retail keeps the first four arguments. |
| Explicit-target player melee | `0x4CF1B0` | `0xB0C388` | Owner, target and material arguments; explicit valid target bypasses the native forward target search. Weapon-authored damage falls back to unit-authored damage. |
| Native object damage | `0x3F4440` | `0xB9EA28` | Official `objects/damage.c` consumer and the matched retail event construction/consumer. |

The official resolver at executable address `0x81CC30` forms desired minus
known-good and calls its collision query. Its retail homologue calls
`0xB913FC` with flags `0x1000E9` and the ignored owner. A clear segment returns
the desired point. A valid blocking result backs away 0.01 native units from
the intersection toward the known-good point. An unresolved result returns
false without providing a new point. This is a point resolver, not a swept
weapon mesh or a new physics body.

The retail collision object's result producer `0xB9105C` independently proves
the object-hit layout: result type `3` at `+0`, fraction at `+0x14`, contact
point at `+0x18`, plane normal at `+0x24`, material at `+0x34`, and full object
handle at `+0x38`. Its writes at `0xB91271`, `0xB91210` and `0xB912E7` are
manifest witnesses. The wrapper provides the full 0x50-byte result storage.
The physical-strike adapter admits only native object hits that resolve as
bipeds, excluding the owner, with finite fraction/point/normal. It repeats the
chosen segment before dispatch and requires the same full target handle.

The official melee helper at executable address `0x8CF1B0` chooses the held
weapon's damage reference at `+0x3A0` and response at `+0x3B0`; missing weapon
damage falls back to the unit tag at `+0x294`. The matched retail helper makes
the same selection. It builds attribution from the owner, chooses native
responses, and calls damage for biped targets. Its distinct weapon response
can also apply a separate event to the owner. The helper clears owner state
`+0x269` as part of normal completion; this native side effect is retained.

Only the private target event at retail return address `0xB0C8A8` is corrected.
The scope requires exact owner attribution at event `+0x10` and the exact
target handle. Contact position `+0x20` and normalized swing direction `+0x38`
come from the repeated physical segment. The separate owner response at
`0xB0C955`, ordinary native damage and all other callers remain untouched.
Definition, scale, flags, attribution and native response selection stay
native-owned. This implementation uses the currently held native melee
definition for either physical hand; it does not claim a separately proved
bare-support-hand damage selector while armed.

## Scheduling and failure isolation

The shared Original/Anniversary first-person palette stages pointer-free
contact frames from the requested tracked pose before collision correction.
It queues them only after both the visible native matrix copy and its palette
receipt publish successfully. The receipt is the final rollback point: a
failed matrix copy or receipt must never queue an invisible strike.

The native biped-update detour calls the original update first, then consumes
the local on-foot owner's queue only after a successful update. Generation,
unit, graphics/reference epoch, current tracking ownership, pause/input state,
finite data and a 150 ms publication lifetime are checked. Duplicate eye
publications share one serial. Animation/locomotion cannot become physical
swing speed; each hand keeps its own impact latch. Native world correction uses
the root and at most six extrema selected from the actual CE hand/weapon nodes.
It retains the shared 0.75-metre response bound and contact-release smoothing.

Common scheduling/query bindings, world resolver bindings and melee/damage
bindings are verified separately. Missing world bindings preserve physical
melee; missing melee bindings preserve world contact. A native fault disables
only the affected contact feature and is reported by the cold polling log.
No contact failure disarms the CE camera or ends the OpenXR session. Hook
retirement waits for callbacks before releasing native code or resetting queues.

## Reproduction and limits

Official read-only captures are preserved under `out/ce-contact-kit-*`, notably
`functions.txt`, `collision-melee.txt`, `unit-melee.txt`,
`native-melee-dispatch.txt` and `damage.txt`. Matched retail decompilation and
instruction captures are under `out/ce-contact-retail-*`. Exact unique entries,
instruction witnesses and call targets are tracked in
`HALOCE-CONTACT-CONTRACTS.json` and the generated manifest/header.

`tools/re/test_ce_contact_native.py` executes unchanged retail resolver/melee
instructions in isolated Unicorn memory, with declared collision/tag/object,
damage-submission, response and security-cookie services. Function extents
come from the pinned PE exception directory: resolver
`[0xB93C8C,0xB93DC7)`, melee `[0xB0C388,0xB0C988)`. This includes the resolver's
actual final epilogue/RET rather than stopping before its register restoration.
`out/ce-resolution-contact-native-20260915.json` records passing clear/hit/
unresolved resolver cases and ten armed/unarmed/fallback/response/target/material
melee cases, totaling 2,605 native instructions. It verifies event fields,
damage choice, target-versus-owner callsites and the native completion side effect.

Production C++ tests separately exercise the native ABI adapter with declared
private-image services, physical-hand routing, independent toggles/latches,
generation/reference/serial refusal, duplicate suppression and SEH cleanup.
The first-person transaction test verifies that contact sees the committed
receipt and is not queued after rejected ownership or a failed native copy.
The palette math checks are in `HALOCE-CONTACT-PALETTE-EVIDENCE-2026-09-15.md`.

These checks do not execute a real collision world, native damage internals,
network authority or headset rendering. Contact samples are native node
positions, not full weapon mesh surfaces; complete/custom weapon coverage and
every physical hand's authored damage selection remain unproven. The user's
test is still required for both CE graphics modes and graphics switching.
No local result advances the accepted source pointer.
