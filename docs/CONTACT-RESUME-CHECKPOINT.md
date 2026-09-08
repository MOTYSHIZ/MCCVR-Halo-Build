# Contact work resume checkpoint, 2026-09-07

Read this alongside `CONTACT-PASS-REQUEST-CHECKLIST.md` and
`PHYSICAL-CONTACT-MELEE-WORK.md`; neither the accepted pointer nor delivery scope
has changed. This compact checkpoint exists to survive interrupted compaction.

## Latest continuation: gesture connected; physical melee still unfinished

User explicitly reaffirmed: **package only when physical melee is finished
properly**. Their urgency/remaining-usage concern does not authorize shipping
an incomplete contact implementation. No new ZIP, commit, install, launch or
accepted-pointer change. Keep updates and evidence reads small.

The new `gesture_melee_bindings.inl` is included by game.cpp. Cold worker
`RefreshGestureMeleeBinding` pins the active module only during its read,
uniquely verifies two native patterns per title, resolves/bounds the state
array, and publishes generation/title-tagged atomic controller bindings. The
input hook reads only this cache (150 ms expiry). All five title-specific
mapping proofs are in `GESTURE-MELEE-BINDING-EVIDENCE.md`; do not redo discovery.
H2 uses typed binding records, the others action-remap arrays. H2's shoulder
and face-button numbering differs; each kit's actual XInput mask table was
checked. H2 axis/held bindings currently decline with a diagnostic.

`Game_GestureMeleeInput` preserves the old velocity latch/cooldown and returns
the actual button or LT/RT transport. `input.cpp` uses it; the old physical
button entry remains dormant. Speed history now runs with either toggle.
Both toggles checked deliberately gives True physical melee priority; the UI
states this, and no extra gesture button is injected. New title/layout/cache
availability seeds latches until each hand settles (prevents an already-fast
hand becoming a fresh attack after a mapping/toggle change). H4 is admitted
through its armed camera and has a shared telemetry slot. Gesture still needs
headset testing; no runtime acceptance claim.

Release build/core tests/Reach gate passed after gesture connection. The later
small change that seeds gesture latches on admission/reference changes still
needs a rebuild. `tools/verify-gesture-melee-bindings.py` passes: five named
melee identities, ten unique native patterns, five native state pointers, and
five independently checked kit XInput tables. Output:
`out/gesture-melee-native-bindings.json`, `out/contact-gesture-binding-verification.txt`.
Build/gate: `out/contact-gesture-build.txt`, `out/contact-gesture-gate.txt`.
The package consistency gate still checks the old input dispatcher; update it
to the new behavior before final packaging, without weakening proof checks.

Immediate physical-contact gap under review: current native builders select
the primary weapon's damage response for either hand. Packets currently carry
only frame/time/generation. H2's publication already knows full primary and
secondary weapon handles and includes both in its motion shape. H3/ODST/Reach
need complete weapon identity carried from their own palette/native evidence,
not a copied object offset. Their publishers append 14 gun points after hand
points using `LegacyBuildMappedWeaponBounds`; record that point boundary and
owning weapon for correct damage selection. Fully-unarmed native builders
can skip grid rays entirely. H4 additionally defers right-hand publication to
the held-weapon record, so a no-weapon frame needs a contact-only end-of-pair
hand publication without disturbing accepted world-collision ordering.

Reuse preserved selector proof: H3EK `A5DE20` / retail `35A9A4`.
Kit reads active inventory index unit+262, weapon array+268, first authored
response weapon+24C (damage+0C/effect+1C), fallback weapon+22C/+23C then
unit-definition+1B4. It returns immediately with no weapon, before that fallback.
These are **kit offsets, not verified retail layout bindings**. Files:
`out/contact-h3-damage-selector-kit-console.txt`,
`out/contact-h3-native-melee-match-console.txt` and the other preserved
`contact-h3-native-apply/impact-layout` dumps. Next inspect the retail selector
and choose a scoped, evidence-backed native damage-selection path; never
temporarily rewrite the live unit's active-weapon field.

Further verified H3 retail details (new files, reuse them):
`out/contact-h3-weapon-selector-retail-console.txt` shows35A9A4 matches the
kit's response offsets and fallback exactly. Its inventory getter356388 takes
**two arguments**: unit handle low index, signed-short inventory index. Do not
trust the decompiler's earlier zero-argument display at the caller. Native
356388 reads TLS38 header, entries+48, stride18, data+10 and weapon array+268;
index-1 returns-1. Dump: `out/contact-h3-equipped-weapon-retail-console.txt`.
Selector caller35AA26 and native combined builder35AF1D both use this getter.
The actual caller-selected index/secondary ownership still needs verification.

H4 right-hand publication gap is now fixed in source: each frozen pair tracks
whether its right-hand contact packet was published. EndFloatingPair publishes
the authored hand-only contact volume when no held record published one. This
does not touch accepted world-collision publication ordering. BeginFloatingPair
resets the complete pair each time; the bounded queue still deduplicates eyes.
This fixes publication only; native unarmed damage selection remains open.
`out/contact-hand-fallback-build.txt` passes and also rebuilds the gesture
admission-seeding guard. Core tests/Reach gate are being run for this state.

## Recovered instructions

The previous conversation was recovered from local session
`01a077fc-83cb-7243-8989-4774f97af69e`. The user's latest scope is to finish
all-title true physical melee and separate gesture melee with active native
binding resolution, then deliver build/source ZIPs, wait for testing, then do
the H2 refinements and remaining checklist. Keep both hands/held guns,
independent world collision, threshold/default 5, and tracking/refresh-rate
guards. No install, launch, game-folder changes or PR.

HEAD remains `96e4f4e`, descending from accepted `ad7fbf5`. The uncommitted H2,
H3, ODST and Reach contact adapters and related refinements are intact. Gesture
settings/UI/persistence tests exist, but gesture input is not connected to a
verified active binding. The old H4 swing/button path still exists; it is not
true contact damage. No new ZIP has been produced or accepted.

## Exact interrupted operation and resumed evidence

The previous session ended after matching H4EK hit builder `E71F40` to pinned
retail `601B1C` through BSim. BSim similarity is a lead, not binding proof.
The resumed decompiles in `out/contact-h4-native-resume-console.txt` verify
the homologous 25-ray loop, native physics call and native selection/consumer
chain against the preserved H4EK dumps:

- Builder `E71F40` / `601B1C`: unit, native state, 0x50-byte output. Native
  grid calls `6020E5 -> 1C1D4C` (existing H4 physics raycast hook). A separate
  later ray call at `6026D4` must be accounted for, not mistaken for grid ray 26.
- Consumer `E717F0` / `60296C`: unit, native state, short simulation routing,
  float power, byte apply, output pointer, optional event pointer. Calls native
  direct apply `601120` at `602DD3` and `602E10`.
- Constructor `E730F0` / `603330`: unit, damage tag, damage owner, impact,
  optional impulse direction. Native impact full target is +1C, and fifth
  argument overrides the unit aim direction. Constructor calls originate from
  direct apply `601120`. Confirm all exact call sites before installing.
- Damage selector retail `6005E8` has native primary weapon selection and a
  unit-tag +3FC fallback when no weapon damage tag is selected. This does not
  yet establish a complete unarmed contact implementation: the ordinary hit
  builder skips its on-foot grid without a weapon.

These are offline observations, not headset results. Source adapter integration,
unique signatures/call-edge checks, exact simulation scheduling, tracking frame
publication, full salted-handle validation, and callback cleanup remain needed
for H4. Latest evidence: `out/contact-h4-damage-contract-console.txt`.

## Useful local resources

- Ghidra: `out/deps/re-tools/ghidra/ghidra_12.1.3_PUBLIC/support/analyzeHeadless.bat`
- Projects: `out/deps/re-tools/projects/H4Collision` (official kit) and
  `H4RetailContact` (analyzed pinned retail); use read-only/noanalysis for dumps.
- Generic dump: `tools/ghidra/DumpContactFunction.java`.
- `tools/verify-contact-melee-bindings.py` verifies H2/H3/ODST/Reach bindings.
- Existing H4 physics input/result structs start around game.cpp:31982;
  `Halo4FloatingPair` starts around 31931; world-contact detour around 32812.
- Native unit action path kit `F42DC0` is being inspected in
  `out/contact-h4-update-kit-console.txt` to identify a proper simulation hook.

Keep future checkpoint updates concise; avoid rereading the entire historical
CURRENT-STATE document or dumping entire conversation/tool histories.

## Latest source checkpoint (supersedes the earlier H4 pending-adapter note)

`halo4_contact_melee_runtime.inl` is now included in game.cpp, installed after
H4's existing physics hook, and removed with explicit callback/trampoline
quiescence before the H4 module/physics hook is released. Both hand queues run
on native UnitInstance::Update. Optional faults disable contact only and are
logged from the worker. No install, launch, ZIP or accepted-pointer change.

- H4EK `E6A1B0` is explicitly named `UnitInstance::Update` by its own profile
  string. Retail `5ECCB4` matches its handle at instance-0C, update counter
  +480, native state and call chain. Unit handle is captured before native
  update and revalidated afterwards, so a destroyed instance is not read again.
  The earlier BSim search for the larger `unit_update_weapons` function did
  not produce a credible match; no binding uses its weak result `2112BC`.
- H4EK datum_get `97290`: 64-bit stride +20, valid +31, extent +44, entries
  +50, full signed salt at entry+0. Retail try-get `43AD8` repeats the exact
  extent/stride/entry/salt checks. Native object getter `5DA400` calls it at
  `5DA42A`, checks kind+4, and returns data+10. Biped mask is 1, confirmed by
  the native unit-update caller. Contact verifies the table then uses this
  getter, never a low-index-only dereference.
- H4EK `51FE3D -> 13EAE0` guards the explicit `!game_is_playback()` assertion.
  `13EAE0` tests initialized game globals and game options+1CDE8. Its helper
  `13F3A0` returns globals+18. Retail getter `9C62C` checks initialized globals
  through `9BE04` then returns globals+1CE00 (18+1CDE8). The contact route
  rejects nonzero playback mode. `6FE10` in H4EK is **game_is_predicted**,
  not playback; its constant-false kit implementation was not copied.
- Native `9C5A0` reads initialized game globals+1C; native consumer routes
  mode4 as predicted. Predicted attempts are counted separately and are not
  headset/remote-host acceptance. Native host cooldown/direction remains open.
- Native melee filter pair: RIP load `601EE4 -> 2FFB098`. Physical dry queries
  use the native profile/filters, first obstruction and salted biped handle.
  Application reruns the native builder, allowing only its centre grid ray
  with the chosen segment and suppressing its later retarget ray. Final native
  damage constructor calls check exact attacker and target before supplying
  physical impulse direction. State EA chooses the first authored response:
  kit selector `E7AE50` / retail `6005E8`; fully unarmed remains open.
- `halo4_contact_melee_logic.h` derives physical poses from H4's accepted
  carrier mapping (including mirror/sign settings), separates actor yaw and
  translation, and invalidates recenter/settings/tracking discontinuities.
  It freezes the shared independent controller snapshot only when its serial
  matches the H4 eye pair. Authored hand/weapon volumes are also computed with
  world collision disabled, and their unblocked points feed these frames.
- H4's old automatic swing/button substitute is no longer returned from
  `Game_PhysicalMeleePulseActive`. The dormant implementation remains intact.
  Separate gesture input still needs active native binding resolution.

Release/core tests pass (`contact-h4-native-build.txt`,
`contact-h4-guard-build.txt`). Motion tests cover mirrored/unmirrored mapping,
60/72/90/120/144/240 Hz, three scales, actor movement and tracking/recenter.
All five title binding checks pass; H4 adds nine unique patterns and thirteen
calls in `out/contact-h4-native-bindings.json`. Reach consistency passes in
`out/contact-h4-native-gate.txt`. Use `powershell -NoProfile -ExecutionPolicy
Bypass -File tools/check-reach-fp-parity.ps1` on this host (process-only).

Still required before delivery: all-title unarmed/secondary-weapon selection
review, gesture binding resolution and simultaneous-mode deduplication, final
failure-isolation review, documentation/commit/packaging. Keep unrelated H2,
visibility and alignment edits preserved but separate from the melee candidate
according to the user's delivery ordering. Check the full request ledger.

Gesture lead: H4EK `prop_controller_actual_button_melee_attack` string exists
at `18BDC78`, but the offline RIP-reference search found no code caller; it is
not yet a binding. Prior evidence: `out/contact-gesture-h4-*.txt`. H4EK active
profile setter `954880` writes preset at profile+20; controller profile getter
`93CFE0` returns controller+8 in its ordinary case. Consumer/table still needed.

`DumpContactFunction.java` now caps caller output at 64 with an explicit note;
large datum functions previously printed thousands of unrelated callers. Query
particular caller sites separately when needed.
