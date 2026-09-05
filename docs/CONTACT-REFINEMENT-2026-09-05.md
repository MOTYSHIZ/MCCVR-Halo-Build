# Contact and aiming refinement candidate, 2026-09-05

Pending headset test. The accepted collision pointer remains `1c08837` in
`CURRENT-STATE.md`. The user explicitly requested working through the entire
refinement list before another build; this combined candidate records each
change separately. Halo 3's accepted sustained contact is the parity target;
the user also identifies Halo 4 as the best hand/weapon contact reference.

## Changes prepared

- Halo 2 reads the live-verified render-model compression block at count
  `+0x14`, relative address `+0x18`. The rejected preceding layout stays
  disabled. The loaded first-person battle rifle, full datum `FA361801`, has
  one record at `012A9664`. Its six bounds match the official H2EK model XML.
  The prior layout read zero records, explaining **zero authored-bound
  publications** even when the SMG's fallback geometry appeared responsive.
  This is a model-format correction, not a battle-rifle name special case.
  Unit/weapon changes reseed the solver even when sample count is unchanged.
  Full proof and bounds: `HALO2-WORLD-COLLISION-EVIDENCE.md`.
- Reach schedules bounded contact work from its native vector test, rather
  than the much less frequent endpoint resolver. Current accepted-build logs
  contain two-second windows with zero scheduler ticks despite continuous
  palette publications. The existing 33 ms budget, query lease, recursion
  suppression, callback draining and feature-local fallback remain in force.
- Reach publishes raw desired collision points by subtracting the **exact
  correction stored with that prepared stereo pair**. Previously its corrected
  visible pose fed back into the solver as a new desired pose. This creates a
  feedback mechanism consistent with intermittent release/re-contact; the
  headset result must establish whether this fixes the reported jitter.
- Reach's existing native firing-helper hook gains an on-foot candidate:
  completed controller-ray origin/direction, exact full local-unit identity,
  title generation, age at most 100 ms, native current on-foot check, finite
  values, and at most 1.5 m of native world clipping from the stock safe origin
  to the hand. The ray uses the actual camera base, even when the hands use a
  different body-following base. Stale, missing or failed proof preserves
  native output. Seated aiming retains its existing branch.
- Shared physical melee now logs polls, unavailable admission, missing
  velocity, peak speed, threshold crossings, native input pulses and cooldown
  blocks from the worker. Input sampling performs only atomic updates. Halo 4
  retains its existing telemetry. This is diagnostic work, **not direct
  hand/NPC damage**. The threshold default remains 5.00 m/s, and saved custom
  settings remain untouched.
- F1 resolution presets explicitly show current-session and next-launch
  resolution. These presets already apply on the next MCC launch. The UI now
  makes that behavior visible and corrects the stale melee-default text.
  This does not establish which settings menu the tester used or claim to
  fix the game's own graphics menu.

## Reach binding and consumer evidence

Official HREK `reach_tag_test.exe` SHA-256:
`CBDD8448A87A433B0DFFC0DE47D06DB7A18B4BF868B96B057135DAA86790ABA8`.
Pinned retail `haloreach.dll` SHA-256:
`738DD2D24EA3AEA12E1EE9AA4A61094BF116027D42004C35A19E5048608B0894`.

HREK resolver `4166A0` calls the eight-argument vector test `41B960` at
`41677C`: flags, mode, start, vector, three ignored objects, result. Retail
resolver `12C5D4` calls adapter `12AFCC` at `12C63D`; the adapter subtracts
start from endpoint and calls vector test `12969C` at `12B046`. The new vector
signature has exactly one match. `tools/verify-world-collision-bindings.py`
checks these image hashes, call edges and signature uniqueness. HREK discovery
preceded the retail match; no Halo 3 ABI or address was presumed.

The already verified unit-adjust hook is HREK `D67EE0`, retail `484F24`,
called by retail projectile creation at `4C303A`. Its returned direction is
consumed at `4C3086..4C309A`. At `4C30AC`, the native barrel's
"projectiles use weapon origin" bit selects an alternative origin:
normal on-foot camera-origin barrels retain the adjusted origin; a marker
origin barrel overwrites it at `4C30D8..4C30F1`. This candidate deliberately
does not patch that later branch or revive rejected marker experiments.
Therefore **all authored weapon origins are not claimed aligned**. Side-held
magnum shots are a requested headset test, not an accepted result.

Preserved analysis outputs under ignored `out/`: `reach-refine-kit-console.txt`,
`reach-vector-retail-console.txt`, and the retail disassembly reproduced from
the pinned input. Existing hook proof remains in `REACH-SIGNATURE-EVIDENCE.md`
and `REACH-VEHICLE-EVIDENCE.md` (R-V12 and R-V27).

## Melee investigation: verified limits and next work

The current shared implementation sends a 120 ms native right-shoulder input
after a velocity crossing, with 600 ms cooldown. It does not pass the striking
hand or contacted NPC to native damage. The old tester log predates shared
melee telemetry, so it cannot distinguish a missed threshold from native
target/range rejection. A threshold of five means **5 m/s minimum swing speed**;
it is not a damage multiplier or increased target sensitivity.

HREK's `magic_melee_attack` evaluator `69B4B0` reaches `E23980`, which constructs
action `0x2D` and calls `DFC7F0`. That is another animation request, not a
contact-damage shortcut. Its native damage handler `B33680`, through wrapper
`B33130`, requires authority, valid damage tags, unit state, timing and a valid
event payload. Player update `1A7E80` builds/dispatches this event. `E20D90`
computes melee geometry from authored melee definitions and actor state;
`E50ED0` requires playable animation and primary damage keyframes. These are
kit facts, **not verified retail bindings**. Analysis is preserved in
`out/reach-melee-{contact,native2,geometry,params}-console.txt`.

Independent H3 kit handler `295400` and H2 kit event functions
`447070/447200/446E90` likewise expose simulation-event validation, not a
portable damage API. Halo 4's separately documented action is `0x31`, already
demonstrating that action numbers cannot be shared. No guessed event structure,
retail address, NPC collision mask or damage tag ships in this candidate.

Next implementation work is a title-verified hand sweep that returns the
actual NPC and hit region, followed by that title's validated native damage
transaction with authority, weapon, cooldown and duplicate-hit protection.
The world-wall resolver's filters are not evidence of NPC contact. Until that
route is proven, native targeting remains a known limitation. New telemetry
makes the current transport independently diagnosable in each title.

## Doubled grass/effects and graphics report

This report belongs to tester build `f5b3081`, Reach, unknown mission; the user
has not observed it on accepted `1c08837`. Preserved logs identify Steam,
SteamVR/OpenXR 2.17.8 in Meta compatibility mode, Oculus-family, 90 Hz,
5242x3780. Much gameplay was near 90 samples/s; blaming it on low FPS is not
supported. Exact files/hashes are recorded in the accepted-baseline notes.

HREK `render_decorators` descriptor `2014FE0` references boolean `1EF7D50`;
reader `3E9D30` gates the decorator pass and iterates its native draw list.
This does not prove a second-eye fault. Existing Reach code already rebuilds
both compact/derived eye cameras and contains title-specific fog/rain handling.
No unverified rendering patch or blanket grass/effect disable is included.
Next evidence needed is the mission and a repeat on this candidate, ideally a
stereo capture identifying the affected grass/effect pass. This issue remains
unresolved, not silently classified as fixed.

## Validation and headset checklist

Release build, core tests, pinned binding verifier and Reach consistency gate
pass before packaging. The package repeats build/tests and records exact source
and file hashes. Tests cover the live H2 header, malformed records, side-hand
ray geometry, recycled units, title generations and stale/invalid poses.

Headset acceptance remains pending: H2 BR versus SMG and weapon swaps in both
renderers; Reach slow wall sliding, sustained haptics, weapon contact and
close-range side-held magnum shots; intentional melee swings with the new
diagnostic counters. Halo 3 regression and Halo 4 contact regression are
required. Record edition, runtime, headset and refresh rate with each result.
