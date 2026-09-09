# Resume checkpoint — paused at user's request, September 8

**Historical checkpoint: the user subsequently resumed this work.** Current
implementation, validation and remaining requirements are recorded in
`MELEE-HANDEDNESS-DUAL-WIELD-2026-09-08.md`, including its September 9 section.
Do not interpret the old pause below as a new instruction to stop.

The user is running out of usage and explicitly asked to save a checkpoint and
wait for their resume instruction. Do not continue implementation, build,
package, publish, install or launch while paused. No background build or
analysis processes were running when this checkpoint was saved.

## Current scope and acceptance

- GitHub work is cancelled for this pass. A previous assistant already pushed
  `handoff/physical-gesture-melee-4e01f28` to remote `shadow` and created PR #3
  at `https://github.com/moistman42069/MCCVR-Halo-Build/pull/3`. No release was
  published. Do not perform further GitHub work until newly requested.
- Next deliverable is a build ZIP and matching source ZIP, package only using
  `tools/package-candidate.ps1` without `-Install`. No game-folder writes,
  installation or game launch. Wait for headset testing after delivery.
- Refine physical melee; finish independent dual wield in ALL supported titles:
  Halo 2 Classic/Anniversary, Halo 3, ODST, Reach and Halo 4, on both editions.
- User explicitly clarified that ODST/Reach/H4 dual wield must support acquiring
  and firing two weapons in ORDINARY campaigns too, not only modded campaigns.
- User explicitly clarified left-handed support means an option to move the
  MAIN weapon and aiming to the physical left hand. Right hand then supports
  the primary weapon or independently holds/fires the secondary.
- See `MELEE-HANDEDNESS-DUAL-WIELD-2026-09-08.md` for the ongoing scope ledger.

User headset-accepted baseline is now `4e01f28b3ec5f5f8f533ac66d94978509cbcea54`.
They tested all supported games and report responsive, precise true physical
melee with both hands. Exception: left hand failed to register an impact while
holding the SMG in ODST Mythic Overhaul. H2 dual-wield hand now renders fully,
but its secondary weapon lacks independent bullet trajectory. Gesture mode,
fully unarmed selection and secondary-weapon damage response are not separately
accepted. Preserve the working baseline; don't redesign it without evidence.

Acceptance is committed as `8c77f3c` and recorded at the top of CURRENT-STATE.md.
Baseline DLL SHA-256:
`BE25108E16209DDB5ECF82D39C1EAB7B6982F0B3AC50F7DB8966B20D3630B21F`.
Existing delivered ZIPs:
`out/candidates/HaloMCCVR-4e01f28-Physical-and-Gesture-Melee-Test.zip` and
`out/candidates/HaloMCCVR-4e01f28-Physical-and-Gesture-Melee-Source.zip`.

Supplied log is preserved at `out/test-runs/4e01f28-accepted-feedback/user.log`,
SHA-256 `BD269C3800377EEDC2369BEA174D3ACA4A0000AEC655B12E8C03176DA2977DB1`.
Steam, SteamVR/OpenXR 2.17.8, Oculus-family, panel120Hz/app60Hz. Prior user
identification was Quest3; this log does not independently name that model.
ODST has successful submissions from both hands, no contact fault, and repeated
missing authored weapon bounds. That does NOT prove the exact SMG miss cause.
Reach/H3 queue drops also appear in transition windows; no new failure diagnosis
has been established from them.

## Saved source changes (uncommitted, incomplete)

1. `src/dll/halo2_dual_wield_runtime.inl`: new optional native fire/aim scopes.
   H2EK49C960(weapon,barrel,projectile,predicted) calls47DC20(unit,...).
   Independently matched retail8E4940 calls8F0F70 at8E4FC8 (return8E4FCD).
   Scope stores the full firing-weapon handle, preserving nesting. The helper
   runs stock first and only changes an identified secondary weapon's result.
   Read-only checks use the already evidenced H2 user0 FP records, complete
   object salt and parent unit. Builds a secondary ray from H2's own mirrored
   left orientation and current observer publication, with finite checks,
   exact serial admission, isolated failure and quiescent cleanup.
2. `src/dll/halo2_observer_6dof.cpp`: includes this adapter, installs it as an
   optional feature after contact, removes it before dependencies, adds worker
   telemetry. The old `Halo2WeaponAimHelperDetour` is DORMANT and remains so.
   An initial commentary mistakenly blamed that old helper; corrected after
   tracing installs. Active H2 aiming writes shared native unit aim/targeting;
   the new secondary-only firing boundary is deliberate.
3. `src/common/weapon_hand_logic.h`: pure full-handle weapon-slot resolution
   and physical-hand selection. No tests added yet.
4. `src/common/config.h`, `config.cpp`, `src/dll/menu.cpp`: added default-off
   `left_handed` setting with load/save and F1 checkbox. THIS IS UI ONLY SO FAR.
   VR pose/input/haptics routing is not implemented. Do not ship as functional.
5. `tools/re/inspect_dual_wield.py`: offline PE inspection utility. Prints H2
   unique firing/helper signatures and raw kit dual-wield strings/RVAs.

Release build of the H2 adapter passed (`out/dual-h2-first-build.txt`). This was
BEFORE the later config/UI edits. No new tests, full gate or packaging have run.
No new runtime code is committed, delivered, installed or headset-accepted.

The last attempted apply_patch (VR role routing, input, tests) was REJECTED
entirely because the patch listed vr.cpp twice. It made NO edits. Confirmed
vr.cpp, vr.h, input.cpp and core_tests.cpp are still clean. Reapply deliberately
with one operation per file; do not assume that proposed implementation exists.

## Immediate next work and unresolved design

- Review/verify the new H2 adapter before relying on it. In particular, confirm
  weapon type2 in the object entry, parent ownership proof, snapshot freshness
  and whether requiring contact snapshot serial==observer serial is too strict.
  Add meaningful salted-handle/role/ray tests and an offline binding verifier.
- Implement handedness coherently at the once-per-frame pose capture boundary.
  Proposed approach: primary/support controller roles, swapping pose validity,
  pose/velocity and trigger/grip transports, preserving physical sticks/face
  buttons, routing haptics back to their physical hand. Invalidate contact epoch,
  melee speed/latches and two-hand state on preference changes. Menu pointer
  should follow main hand; D-pad's physical-hand preference must stay physical.
  Merely swapping poses does NOT establish correct anatomical arm/hand art;
  review each title's solver and held-weapon ownership rather than claiming
  complete left-handed presentation from that alone.
- Existing VR capture: vr.cpp CaptureRightControllerPose around7520; publishes
  g_rightAimPose/g_leftAimPose around7624; reads pad actions around7694; calls
  UpdateTwoHandLatch around7738. CurrentAimPoseInputs around7199 feeds all title
  snapshots. Haptic paths in ApplyControllerHaptics around7510. D-pad physical
  selection is input.cpp around262. Existing getters take g_headCs, but contact
  and exact-title snapshots are bounded lock-free readers; keep hot hooks free
  of locks, allocation, scanning, logging and I/O.
- Prevent two-hand aiming from coupling dual weapons. Secondary-equipped
  identity must come from title-proven state, independent of world collision.
- H3/ODST shared solver is game.cpp DesiredWristWorld around5089 and
  RebuildInterpolatedFpPalette around5194-5830. Anatomical arm selection and gun
  attachment need deliberate handling for left-handed main-weapon mode.
- Do not assume H3 dual bullet direction is already independent: palette
  ownership exists, firing still needs direct verification per title.
- ODST contact publication is in odst_contact_melee_runtime.inl around293;
  game.cpp publishes actual wrist descendants around5950. Modded SMG miss
  remains undiagnosed. Avoid changing accepted collision/motion indiscriminately.
- All-title ordinary-campaign acquisition/firing is NOT implemented. Kit tags
  and strings are discovery leads only, not proven gameplay gates. Continue
  native pickup/action/weapon eligibility tracing, then verify in pinned retail.

## Reuse evidence; don't redo discovery

- `out/dual-h2-fire-kit.txt`, `out/dual-h2-fire-retail.txt`: complete native firing
  decompiles. Parameter1 is the weapon; H2EK type mask4, weapon parent bit+130
  and full owner+158 are also preserved in retail. H2 current primary/secondary
  FP record constants already live in halo2_render_logic.h (user array187C300,
  user stride20FC, weapon offset0C, slot stride1028, weapon handle+4).
- `out/dual-native-inspection.txt`: pinned hashes, two unique H2 patterns,
  raw kit strings including unanalysed ODST/Reach strings. Source utility above.
- `out/dual-{odst,reach,h4}-kit.txt`: initial Ghidra defined-string search.
  ODST and Reach had NO defined-string results; raw strings DO exist. Do not
  interpret empty Ghidra string lookup as missing dual-wield code.
- `out/dual-{odst,reach,h4}-pickup-refs.txt`: targeted existing xrefs. ODST/Reach
  refs are likewise sparse/empty in these partially analysed projects. Need
  raw RIP-relative/reference inspection or targeted analysis, not retail guesses.
- H4 kit `492130` is merely a UI/tutorial string table including dual_wield_hold
  and fire_both_weapons. It is NOT a proved gameplay gating predicate.
- Useful raw kit strings: ODST1360698(unit_get_current_secondary_weapon==NONE),
  1361AE0(secondary index!=NONE),1377C58(can be dual wielded),1386090(primary
  weapon or no secondary). Reach1A0F050(no secondary),1A107C8(secondary!=NONE),
  1A143B0(can dual wield),1A24820(can be dual wielded),1A4F4C0(primary/no secondary).
  H4 kit1D52F30(secondary!=NONE),1D5C9E0(can dual wield),1D49918(weapon flag).
  These are kit RVAs, NOT retail bindings.
- `docs/ODST-WEAPON-IK-EVIDENCE.md` around114: automag/SMG have authored dual
  animation sets and ODST renderer supports slot1. Enabling native campaign
  acquisition is explicitly separate and was never proved by that document.

Headless environment: JAVA_HOME is the resolved
`out/deps/re-tools/jdk21/jdk-21.0.12.1+1`. Executable:
`out/deps/re-tools/ghidra/ghidra_12.1.3_PUBLIC/support/analyzeHeadless.bat`.
Projects under `out/deps/re-tools/projects`: H2Aim(halo2_tag_test.exe,halo2.dll),
OdstCollision(halo3odst_tag_test.exe,halo3odst.dll),
ReachCollision(reach_tag_test.exe,haloreach.dll), H4Collision(halo4_tag_test.exe),
H4RetailContact(halo4.dll), H3Collision/H3Retail. Use -readOnly -noanalysis.
`tools/ghidra/DumpContactFunction.java` supports rva:, refs:, bytes:, find:.
`tools/re/QueryBsimFunction.java` emits candidates only; verify independently.
Python dependencies are in out/pydeps (`pefile`); set PYTHONPATH or add that
directory inside the utility. Stock Python has no pefile. No numpy/capstone
directory was found there. Keep outputs bounded; historical CURRENT-STATE.md
and rendered GitHub diff pages are very large. Do not load them wholesale.
