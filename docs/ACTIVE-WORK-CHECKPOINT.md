# Current publication — accepted d47a98c all-campaign release

The user headset-ACCEPTS **d47a98c** and explicitly requests GitHub publication
on the existing moistman42069/MCCVR-Halo-Build repository. This supersedes the
older publication holds below. Publish Alpha 0.4.0 — All Campaigns in VR,
tag `MCC_VR_ALPHA_0.4.0`, preserving the exact tested DLL, launcher and config.
No rebuild, game installation or launch. Runtime source: `d47a98c947dc60dd98d7259a29a7582d5f46df7f`.
DLL SHA-256: `ADAB506E9E3BFB1E04DBBF767FDD907EFD414526863AB5C837FD65E7FAB95922`.
Use simple `Halo-MCC-VR.zip` containing only those three files plus README.txt;
README includes licenses. Publish complete `Halo-MCC-VR-Source.zip` from the
documentation-only release commit, with SHA256.txt and the updated title table,
CE Anniversary coverage, left-hand fix, D-pad settings, known issues and roadmap.

User log preserved at `out/test-runs/d47a98c-accepted-all-campaigns/user.log`,
SHA-256 `47EB056034C45F5AB3F29E6A7E3A82F59D35EC9254654142AEF0527474F05892`. Source matches d47a98c;
Steam, SteamVR/OpenXR 2.17.9, Oculus-family at 90 Hz; user identifies Quest 3.
All six titles appear. This is campaign smoke-test acceptance, not exhaustive
mission/optional-setting/Store verification. Preserve all earlier open limits.

# Current continuation - September 15/16: D-pad controls and left-hand alignment

Implementation is complete for CE, H2, H3, ODST, Reach and H4. Shared palm
routing consumes final solved grip frames and leaves weapon transforms intact;
per-title marker identity/remap guards remain. Read `LEFT-HAND-SHARED-2026-09-15.md`,
`LEFT-HAND-H2-2026-09-15.md` and `LEFT-HAND-ALIGNMENT-2026-09-15.md`.
Release, all 33 CTest suites, the Reach gate, all eight shared primary-asset
marker checks and CE's 36 compiled stock cases pass. New shared tests cover
96 rig/scale/eye/support combinations; H2 passes 829 packet assertions.
Complete final committed packaging/archive verification and deliver the NEW
pair. Exact handoff identity belongs in `out/dpad-alignment-current-handoff.json`.
After delivery WAIT for headset testing; do not resume earlier deferred work.

The user headset-ACCEPTS **7ff9697**, confirming the CE haptics fix worked on
the first try. Preserve this cumulative runtime. Current scope is two Controls
settings: a head-gesture radius slider (10-50 cm, existing 30 cm default) and
an optional OFF-by-default Quest 3 left thumb-rest D-pad checkbox. Holding the
physical left thumb rest uses the physical right stick for D-pad directions,
independent of weapon handedness. Existing head gesture/clicks remain available.

The user then expands this SAME candidate to correct `Fix Hand Alignment`
while left-handed mode is enabled: hands remain to the right of the gun, in
ALL games (including both CE and H2 graphics modes). Preserve the D-pad work;
do not package a D-pad-only candidate while this correction is in progress.
Use each title's authored grip/mount evidence, preserve actual gun transforms
and normal right-handed/default-off behavior, and verify nonidentity rotations,
offsets, scale and engaged support. No speculative fixed positional nudge.

Read `DPAD-CONTROLS-2026-09-15.md` for accepted log identity and verified Touch
binding evidence. Consume the alternate stick centrally before turn/scope/title
snapshots; failed optional binding must preserve the complete existing input
bindings. Preserve haptics, all title renderers, HUD, recovery and other controls.

Package matching build/source ZIPs after verification, using
`tools/package-candidate.ps1` without `-Install`. Deliver both and WAIT for
headset testing. No installation, game-folder writes, launch, PR or GitHub
publication. Both editions remain supported. Accepted source is **7ff9697**;
the new shared input behavior requires headset testing and Halo 3 regression.

# Historical continuation - September 15/16: CE controller haptics accepted

The user explicitly CANCELS the GitHub task and accepts the most recent
**5ac02f5** runtime as flawless apart from absent CE vibration. Preserve every
other behavior. Restore native gun/gameplay vibration to BOTH controllers in
CE Original and Anniversary through the same bridge used by Halo 3. No new
rendering, tracking, input, title-recovery or HUD behavior belongs in this task.

The supplied source-5ac02f5 Steam / SteamVR / Oculus-family log is preserved in
`out/test-runs/5ac02f5-ce-haptics-20260915/user.log`. CE already publishes the
needed gameplay modes and the common XInput hooks are installed. Both its
descriptor and armed runtime capability mask omit `TitleCapability_Haptics`,
so existing input/output policy clears game and contact vibration. Read
`CE-HAPTICS-2026-09-15.md` for the evidence and verification limits.

Prepare and verify this single behavior correction, then package the build ZIP
and matching source ZIP with `tools/package-candidate.ps1` WITHOUT `-Install`.
Deliver both in chat and WAIT for user testing/instructions. Do not install,
write game-folder files, launch MCC, open a PR or publish a release. The user
plans GitHub publication after haptics testing; it is not the current task.
Both editions stay supported. Accepted source stays **5ac02f5** until the user
accepts the new haptics candidate in the headset.

Implemented: exactly two CE Haptics capability grants, with all other runtime
source byte-identical to accepted 5ac02f5. Baseline regressions reproduce the
missing capability; corrected Release, all 29 CTest suites, the Reach gate and
339 production haptic fixture checks pass. Package the committed source and
verify both archives; final identity is `out/ce-haptics-current-handoff.json`.
Current candidate notes: `CE-HAPTICS-CANDIDATE-2026-09-15.md`.

# Historical continuation - September 15/16: cancelled publication request

The user explicitly accepts the delivered **5ac02f5** candidate as a good
baseline and requests preservation and GitHub publication as the first release
where all MCC campaigns are playable. Publish the exact existing build ZIP and
matching source ZIP; do not rebuild the runtime. Release title/tag:
`Halo MCC VR Alpha 0.4.0 — All Campaigns Release` /
`MCC_VR_ALPHA_0.4.0`.

Accepted identity: source
`5ac02f53a7896ffd6b8dff37ddc5bc4700890559`, DLL SHA-256
`55646EFF6AEF8FAD0C16E9FDA67685292637B97E0B6437B7DD670C4E4B840A63`,
build ZIP SHA-256
`1E75D939B0D65136AAFD29718EEE0BEAC269C44E4930E1E5C0C1D3E7DB958C2C`,
source ZIP SHA-256
`FB3563A2DE33CD5EFC1002B641FCA289FF8830D5849203621C259FAAD6B10A8A`.

Release documentation must clearly state three user-supplied limitations:

- campaign switching can occasionally crash; fully restart MCC and load the
  destination again;
- some titles need about seven seconds to initialize, so wait at least seven
  seconds after title/level entry before Force Inject;
- Halo CE Original/Anniversary graphics switching is unavailable during
  cinematics and remains planned refinement work.

Retain the complete implemented/pending breakdown in
`releases/0.4.0/RELEASE-NOTES.md`. The published release remains an Alpha
prerelease even though every campaign now has a playable VR path.

# Historical continuation - September 15 night: all-title re-entry and recovery

User-tested **558fb2c** is explicitly accepted for CE Original and Anniversary:
"perfect now ... dont change anything there." Preserve the accepted CE engine
implementation and all standing features. Reach subsequently failed to enter
VR, so its HUD-height fix still needs headset testing. Latest task: restore
automatic VR entry across title switches and make Force Inject work in every
supported title. This supersedes the previous CE-fix priority below.

Read `ALL-TITLE-REENTRY-2026-09-15.md` for the supplied log and comparisons.
Reach passed title detection, level-load proof and native preflight, then its
display admission required a one-bit resident-module mask. CE retaining its
safe native list/hook lifetime blocked that policy. Preserve CE retention;
follow coherent active selection while retaining all display and camera proofs.

Work through verification before packaging. Use `tools/package-candidate.ps1`
without `-Install`; deliver matching build/source ZIPs in chat, then WAIT for
user testing/instructions. No install, game-folder writes, launch, PR or
publication. Both editions remain supported. Cumulative accepted source stays
`4e01f28`; CE's scoped accepted source is `558fb2c`. Local checks do not prove
headset acceptance of the new shared lifecycle behavior. Required tests include
CE -> Reach, all-title re-entry/recovery, Reach HUD height, and Halo 3 regression.
All earlier standing/deferred scope and known limits remain preserved.

Implemented: selected-title Reach display admission, CE native-clock adapter
selection, all-title manual retry/completion, rejected-attempt rearm, failed
cold-proof retry and H2 cleanup continuation. CE engine implementation is
preserved. Final Release, all 28 CTest suites, Reach gate and 46 extracted
cold-retry checks pass. Package the committed source without `-Install`;
verify both archives before delivery. Exact final identity is recorded only
after validation in `out/all-title-current-handoff.json` and the compatibility
`out/ce-current-handoff.json`. Deliver the new pair, then WAIT.

# Current continuation - September 15 evening: CE graphics switch and Reach height

User-tested **2cf002b** confirms CE Original reticle, muzzle flash, gun tracking
and overall behavior are correct. Preserve that base and all implemented
contact/melee/weapon-envelope features. Original -> Anniversary crashes;
Reach HUD height has no effect. User requests autonomous correction through a
NEW build ZIP and matching source ZIP. Read `HALOCE-2CF002B-TEST-2026-09-15.md`.
All standing/deferred scope remains. No launch, installation, game-folder write,
PR or publication; package without `-Install`, deliver both ZIPs, then WAIT.

Matching crash dump PID26776 is preserved with its log under
`out/test-runs/2cf002b-ce-reach-feedback-20260915/`. Dump SHA-256
`98E8590634329CE53B2872807D483CC913B36EB4E06FF38CCDB8B594C4305863`.
Native HUD `halo1+B0EBC1` reads null shader `hud_meters` at slot `1B7D1F0`.
All 138 native effect slots are empty and native initialized flag `2EA2D5C`
is zero. The exact dumped mod branches show ordinary HUD fallback, before
prepared target binding or eye replay. Do not attribute this crash to binder
mutation. The prepared HUD enable was disabled separately in **8b6fd06**;
retain its code. Native backend disposal `82170` clears the shader-ready flag,
and reinitializer `80DD0` only restores effects when that flag remains set.
The correction preserves the previously active native lifetime across the
management-owned rebuild, validates all 138 restored effects and retains exact
module/generation/backend intent for failed-reload retries. Previously inactive
owners cannot inherit readiness. Ordinary HUD fallback now requires its own
module/generation and native HUD resources even without an armed camera.
Offline stack and native traces are under `out/ce-crash-*` and
`out/ce-switch-unwound-stack-20260915.txt`. Read
`HALOCE-RENDERER-RESOURCE-LIFETIME-EVIDENCE-2026-09-15.md`.

Reach's absent height consumer is implemented through its own HREK-proven
six-argument native anchor basis, isolated from captured reticles and camera
ownership. Read `REACH-HUD-HEIGHT-2026-09-15.md` and its native verifier.
An independent native target-binder audit also reproduced a partial-failure
cleanup defect: dimensions are zeroed before attachment validation. The
production guard now accepts only that exact owned intermediate descriptor;
new production regressions fail before correction and pass afterward. This
is not the observed shader crash cause. The separately enabled
`kCeAnniversaryNativeReadyHudTargetsEnabled` retains the full-resolution late
HUD for both eyes; all three older replay enables remain disabled.

Final cumulative Release, all **24 CTest suites**, all **126 pinned contracts**,
generated contracts, **19 production binding groups** and the Reach gate pass.
CE production tests cover failed reload/retry, successful pool with missing
effects, prior-zero isolation, SEH cleanup, stale generation/backend and
caller-bound fallback readiness. Native lifecycle verification passes five
cases; it executes the reload gate and cache loader separately, not complete
driver initialization. Native target binding passes ten cases. Reach passes
48 native HREK/retail cases and 175 production wrapper checks. Preserved CE
weapon geometry, visibility, material and WARP particle checks also pass.
Final reports use `out/ce-switch-reach-final-*`,
`out/ce-resolution-lifecycle-native-20260915.json`,
`out/ce-hud-target-prepared-final-20260915.json` and
`out/reach-hud-height-native-20260915.json`.

Release notes: `HALOCE-SWITCH-REACH-HEIGHT-CANDIDATE-2026-09-15.md`.
Package the committed candidate without `-Install`; the script repeats build,
tests and Reach gate. Verify both ZIPs, their sidecar hashes, embedded identity
and exact source archive before updating the handoff. Deliver both, then WAIT
for the user's headset testing/instructions. No further autonomous deployment.

Accepted cumulative source stays `4e01f28`. Local checks never establish
headset acceptance. Exact new archive identity belongs in
`out/ce-current-handoff.json` only after complete archive/source/hash validation.
Do not redeliver the old `2cf002b` pair. Existing exact-Saber/custom weapon
surface, native melee target/selector and CE body-following limitations remain;
all previous standing scope is retained. Test Reach height, CE switches both
ways, Anniversary HUD and both modes' contact, plus the Halo 3 regression.

# Historical continuation - September 15: native CE HUD/reticles and weapon surfaces

Latest user test is **22cb813**, preserved under
`out/test-runs/22cb813-ce-feedback-20260915/`. Read
`HALOCE-22CB813-TEST-2026-09-15.md`. Both Original and Anniversary inject/run
smoothly with equal image quality and correct muzzle flashes. Original HUD
works. Preserve this feature-confirmed base. Remaining requested work is actual
native reticles in BOTH CE modes, visible Anniversary HUD, and world contact /
physical strikes using every stock gun's mesh surfaces as well as the hands.
Halo 2/Halo 4 are precedents for the reticle fix, not newly reported failures.

The candidate now corrects CE's independently proven RGB-only reticle writes
with visible-color measurement and authored-upload alpha reconstruction. Native
allocation, parent assignment and late split-selector clearing prove the current
Anniversary depth root is H while its packed color is 2H. The prepared HUD
transaction detaches only incompatible depth, then restores verified native
target/raster state with partial-bind, SEH, lifetime and foreign-target guards.
Real WARP draws replace the old fixture's direct texture painting. Earlier
pre-full-resolution rootH/packedH attachments were compatible; this finding does
not explain every historical missing-HUD report. Preserve the working camera,
resolution, hands, muzzle path, all other titles, both editions and deferred
scope. Cumulative accepted source stays `4e01f28`; this partial CE result does
not accept the remaining features or replace the required Halo 3 regression.

The failed unprepared late HUD adapter was disabled separately in `48cf9b0`,
with its code retained. Read `HALOCE-22CB813-HUD-ROLLBACK-2026-09-15.md`.
The replacement must use its own explicit prepared-path enable; never re-enable
the earlier manual callback replay or mistake the rollback for a test handoff.

All twelve official CE weapon models now supply bounds from 21,180 vertices
across 58 positively weighted nodes. Live posed bones generate fourteen gun
samples, each included in world and physical-melee queries. Compiled production
geometry passes 48 cases / 84,720 vertex-pose checks. This is a deliberate stock
CE physical envelope for both modes, not exact Saber replacement/custom mesh
surfaces. Same-graph custom models retain stock bounds; unknown graphs retain
logged node contact. Detached/reload parts can enlarge the conservative envelope.
Animation alone cannot trigger melee. Existing native biped damage selection,
bare support-hand selector limits, other damage targets and body-following
deferrals remain. The shared reticle/packet changes require a Halo 3 headset
regression alongside both CE modes.

Evidence: `HALOCE-NATIVE-RETICLE-RGB-2026-09-15.md`,
`HALOCE-NATIVE-HUD-ATTACHMENTS-2026-09-15.md`,
`HALOCE-WEAPON-CONTACT-REFINEMENT-2026-09-15.md`. Release notes:
`HALOCE-NATIVE-HUD-WEAPON-CONTACT-CANDIDATE-2026-09-15.md`.

Pre-package cumulative Release and all 23 CTest suites pass. All 123 pinned
contracts, generated contracts and 19 production binding groups pass, along
with native HUD late-root attachment verification (four cases), 48 compiled
weapon poses, native reticle instructions/WARP pixels and the Reach gate.
Final records use `out/ce-native-hud-contact-*`,
`out/ce-native-hud-attachment-verification-20260915.json`,
`out/ce-weapon-mesh-compiled-20260915.json`, and
`out/ce-reticle-native-rgb-validation-20260915.json`. The final committed
package repeats the required build/tests before archive verification.

Complete this refinement, package with `tools/package-candidate.ps1` WITHOUT
`-Install`, deliver NEW build and matching source ZIPs in chat, then WAIT for
headset testing/instructions. No installation, game-folder writes, MCC launch,
PR or publication. The previous 22cb813 artifact pair is evidence, not the next
delivery. Exact new package identity belongs in `out/ce-current-handoff.json`
only after archive/source/hash verification.

# Historical continuation - September 15: full-resolution CE refinement and contact

Latest user test is **58f71a4**, with both supplied logs preserved in
`out/test-runs/58f71a4-ce-feedback-20260915/`. Read
`HALOCE-58F71A4-TEST-2026-09-15.md`. Original is an excellent working base;
preserve its camera/hands and fix its missing crosshair. Anniversary needs a
gun-directed crosshair, working HUD controls, hand/weapon flicker correction,
second-launch black-screen correction, and actual full native eye resolution.
The user explicitly rejected deferring resolution: **fix it, then package**.
Do not treat that as permission to stop at a limitation or to ship stretched
half-height pixels. The same request adds CE world collision and physical melee
in both graphics modes, superseding every older CE contact exclusion below.

The interrupted root conversation was recovered from
`rollout-2026-09-15T18-21-11-01a0a728-e8cb-7161-af8d-75b555a15dba.jsonl`,
including the user's quoted reticle/contact update. Its source is preserved.
Current candidate includes reticle receipt/blank-bootstrap fixes; reproduced
material-reader and tracking-publication contention corrections; non-evicting
resource metadata and release-token fixes; full-height HUD canvas mapping;
independently HCEEK-proven native CE collision/melee and palette contact work.
Native full-resolution color/depth allocation and managed reallocation are now
connected. The native pool creates children BEFORE publishing its entry count
and usage; a scoped pending-slot receipt fixes that reproduced allocation-order
defect. The production WARP fixture follows this actual order. Native allocation
verification passes 18 cases / 448,560 instructions, including the failed old
membership policy and odd heights. Contact queues only after the palette's final
receipt succeeds, after every rollback point. No headset fix is yet accepted.
Do not deliver the old 58f71a4 ZIP again.

Package with `tools/package-candidate.ps1` WITHOUT `-Install` only after full
resolution and the requested candidate are implemented and validated. Deliver
NEW build and matching source ZIPs in chat, then WAIT for headset testing.
No installation, MCC launch, game-folder writes, PR or publication. Both
editions and all standing/deferred scope remain supported/preserved. Accepted
source stays `4e01f28`. Native contacts, both CE modes/switching/relaunch, and
the shared-callsite Halo 3 regression require user headset results.

Pre-package cumulative Release and all 22 CTest suites pass; final packaging
repeats these for the committed source. Native material dispatch passes 36
cases and the actual GLT/ZFILL/SFX shaders pass 48 WARP draws. Native contact
passes three resolver and ten melee cases / 2,605 instructions. All 123 pinned
contracts, generated contracts and 19 mapped production binding groups pass,
along with native HUD sequence and Reach consistency. Final records use
`out/ce-resolution-contact-*`, `out/ce-resolution-stability-final-*` and
`out/ce-resolution-allocation-native-20260915.json`. Exact archive/source/file
verification updates `out/ce-current-handoff.json` only after packaging.

Candidate notes: `HALOCE-RESOLUTION-RETICLE-CONTACT-CANDIDATE-2026-09-15.md`.
Native resolution: `HALOCE-ANNIVERSARY-FULL-RESOLUTION-2026-09-15.md`.
Native contact details: `HALOCE-NATIVE-CONTACT-EVIDENCE-2026-09-15.md`.
CE contact samples are hand/weapon nodes; complete/custom mesh coverage remains
open. Melee targets bipeds and retains native held-weapon damage selection for
either physical hand. Vehicle/world-object damage and a distinct bare support
hand damage selector remain open. CE body following remains deferred. Metadata
reset can overlap shared publishers outside the core drain; this is recorded
as an unproven limitation, not a proven explanation of the user's second launch.

# Historical continuation - September 15: level horizon, Anniversary hands and HUD

The user resumed at the quoted right-eye omission / worker handoff update.
The exact interrupted root chat was recovered from
`rollout-2026-09-15T17-03-33-01a0a6e1-d50a-71f0-a36b-a68ad49bc6b1.jsonl`.
The missing production submission hook is now wired before native workers,
with exact copied-list ownership, old active-ticket revocation, and atomic
reset/retirement invalidation. Native hidden-weapon policy remains intact.
See `HALOCE-FIRST-PERSON-VISIBILITY-2026-09-15.md`. A reproduced native SEH
callback-count leak was corrected with explicit cleanup in both new visibility
hooks and the natural HUD callback. The production tests exercise those wrappers.

HUD capture now revalidates its saved source after the native callback's real
ClearState epilogue, without requiring the cleared render-target cache to remain
bound. Native cleanup instruction execution and WARP ClearState/source-change
regressions cover this addition. The outer native callback still runs once.

Recovered the actual prior root chat through the user's supplied quotation from
`rollout-2026-09-15T16-56-23-01a0a6db-4442-78b0-bb6a-5abd27da2dac.jsonl`.
The newest headset result is **fba9ee6**, not 884de13. Original injection,
hands/weapon scale and HUD are good. BOTH modes recenter with an unwanted
vertical world angle. Anniversary world renders again but its right-eye hands
are missing, left-eye weapon scale is wrong and HUD absent. The user explicitly
requires correcting all three areas before a NEW build AND matching source ZIP.
Read `HALOCE-FBA9EE6-TEST-2026-09-15.md` for the exact preserved input/log.

Package `tools/package-candidate.ps1` WITHOUT `-Install`, deliver both ZIPs in
chat, then WAIT for testing/instructions. No install, game launch, game-folder
writes, PR or publication. Both CE graphics modes and both MCC editions remain
supported. All standing/deferred requests remain retained; focus this candidate
on CE. Accepted source remains `4e01f28`.

The interrupted recenter WIP is preserved at
`out/checkpoints/20260915-170506-ce-horizon-hud-resume`. The level tracking-frame
correction now covers eyes, controller palette/shot direction and head-relative
movement. H3's actual yaw-only recenter behavior was checked in source. See
`HALOCE-LEVEL-RECENTER-2026-09-15.md`.

Native material writers run before render-eye TLS exists. Their old gate was
reproduced failing; the material-worker policy correction passes production
and native-dispatch/shader checks. See
`HALOCE-ANNIVERSARY-MATERIAL-WORKER-2026-09-15.md`. Native FP source-player
exclusion separately hides synthetic view 1; its correction and exact native
evidence are part of this continuation, not a generic world-visibility override.

The HUD runs NATURALLY late in the same frame, after both early eye copies.
The normal callback remains native-owned and runs once. The gameplay draw is
framed into both packed eye regions; live target/revision/frozen-frame proof
precedes copying their final pixels into the eye cache. Failed optional work
keeps the world pair. The previous manually invoked callback remains disabled.
Read `HALOCE-LATE-HUD-NATIVE-SEQUENCE-2026-09-15.md`; native sequence/scissor
proof and production WARP routing/guard/recovery tests are preserved.

Candidate notes: `HALOCE-HORIZON-HANDS-HUD-CANDIDATE-2026-09-15.md`.
Final cumulative Release and all 20 CTest suites pass, including worker
handoff/reuse, both visibility hook exceptions, real HUD ClearState and its
callback exception. All 111 pinned contracts, generated contracts, 17 production
mapped-image groups and Reach consistency pass. The native material dispatcher
passes 36 cases; the actual three skinned shaders pass 48 WARP draws. Native
visibility passes 32 producer/consumer plus six copy/submission cases; native
HUD passes five sequence cases, 48 scissor constructions and its ClearState
epilogue. Records are under `out/ce-horizon-resume-*20260915.*`, with dedicated
visibility/HUD native records linked in their evidence docs. Packaging repeats
required checks for the final committed identity. Exact NEW
build/source archives and hashes belong in `out/ce-current-handoff.json` only
after full archive/source verification. Do not redeliver its old fba9ee6 pair.
No local test advances headset acceptance. Both CE modes, graphics switching,
native HUD side effects and the CE shared-callsite Halo 3 regression still need
the user's headset test. No CE body-following/melee/collision expansion.

# Historical continuation - September 15: Anniversary freeze and muzzle alignment

Latest user tested `884de13`: Original injects successfully, hands look correct,
and Original muzzle flashes work. Preserve these positive headset results.
Switching to Anniversary freezes rendering. Anniversary muzzle flashes were
also reported moving with the gun but missing its actual muzzle. Current scope
is to restore Anniversary VR and trace/correct its native muzzle-flash path.
The user explicitly requests continued autonomous work until a NEW build ZIP
and matching source ZIP are ready, then wait for headset testing/instructions.

Package with `tools/package-candidate.ps1` WITHOUT `-Install`. No installation,
game launch, game-folder changes, PR or publication. Both editions and every
standing/deferred request remain supported/preserved. Accepted source remains
`4e01f28`; partial Original success does not accept failed Anniversary behavior.

The failed manual Anniversary HUD replay was disabled separately in `03814d4`.
Two consecutive Anniversary WARP frame captures pass with the same failing
callback left armed but never entered. Original's runtime regression passes.
Anniversary HUD visibility remains unresolved; do not re-enable this callback
without complete native-state/lifetime evidence. Read
`HALOCE-ANNIVERSARY-REPLAY-ROLLBACK-2026-09-15.md` for the failure evidence and
limits: a native lock leak is NOT established by the log or ordinary call trace.

The Anniversary muzzle correction is implemented in the optional first-person
particle upload feature. Native firing effects resolve authored marker names
through the same tracked first-person palette. The particle emitter view
transform uses the actual current camera; a separate shader selector chose the
fixed first-person lens while the gun chose the VR world lens. The correction
changes only those selectors in a complete bounded private upload snapshot.
It preserves native emitter/backing data, attachment transforms, shell choice,
shot origin and world particles. Exact current-eye and primary-stage ownership
are both required; rejected/auxiliary/nested draws keep their native path.
Read `HALOCE-ANNIVERSARY-MUZZLE-PROJECTION-2026-09-15.md` and
`HALOCE-MUZZLE-TRIGGER-EVIDENCE-2026-09-15.md`. The flashlight-only `0x7D350`
route is explicitly not muzzle evidence. Cold counters distinguish observed
particle draws, rejected eye ownership and refused upload correction.

Pre-package cumulative Release, all 20 CTest suites and Reach consistency pass.
Both actual particle shaders pass 144 WARP draws; the native uploader passes
15 cases. All 104 pinned contracts, generated contracts and 16 production
mapped-image groups pass. Results are under `out/ce-anniversary-recovery-*` and
`out/ce-muzzle-particle-commit-native-20260915.json`. Packaging repeats required
checks after the final commit; only a verified new build/source archive pair
may update `out/ce-current-handoff.json`. Delivery notes are
`HALOCE-ANNIVERSARY-RECOVERY-CANDIDATE-2026-09-15.md`. The headset still determines
whether graphics switching and visible muzzle alignment are fixed. Broader
transition stability, all weapons/effect variants and frame rate are unproven.

New input is preserved under
`out/test-runs/884de13-ce-anniversary-feedback-20260915/`; read
`HALOCE-884DE13-TEST-2026-09-15.md` for exact identity and observations. Compare
the previous preserved logs before drawing conclusions. Retain successful
Original startup, hands, scale, HUD, aim and effects. Do not redeliver the old
`out/ce-current-handoff.json` pointer until a new package is verified.

# Historical continuation - September 15: CE hands, HUD and stability

Latest user tested `e524d21`: BOTH CE modes inject, gun scale is excellent in
both, and Original HUD works. Preserve these results. Current concrete defects:
Original stretched arm triangles, Original startup flicker until an Anniversary
roundtrip, Anniversary hand flicker and missing HUD, and head-locked MCC shell
after CE exit with reported possible transition crashes. Halo 3 is explicitly
left alone. The suggested extra task was withdrawn; focus CE/Anniversary.

User explicitly requests autonomous cumulative refinement until a NEW build ZIP
AND matching source ZIP are ready. Package with `tools/package-candidate.ps1`
without `-Install`, deliver both in chat, then WAIT for testing/instructions.
No install, game launch, game-folder writes, PR or publication. Both editions
remain supported. Prior package holds and older exact-delivery pointers below
are historical. Accepted cumulative source remains `4e01f28`.

Supplied logs and screenshot are preserved in
`out/test-runs/e524d21-ce-refinement-feedback-20260915/`; exact identity/results
are in `HALOCE-E524D21-TEST-2026-09-15.md`. Steam / SteamVR OpenXR 2.17.9 /
Oculus-family / 90 Hz; exact headset model unspecified. Both logs show Present
stalls with worker logging still alive, no fault address or crash stack. Do not
claim all transition crashes fixed. The later run reaches Halo 3 gameplay.

The bad camera-origin floating-arm collapse was disabled separately in
`2b46667`. Replacement uses CE's named arm chains and final corresponding
wrists; official mesh proof covers 1,495 vertices and 71 blended forearm/wrist
vertices. Anniversary skin conversion now honors each copied source bone's
scale independently of another preparation's global receipt. Original's stock
Anniversary worker no longer claims the camera-reference guard; the production
interleaving regression failed old code and passes corrected stereo/pixels.
Anniversary HUD now retains its frozen in-flight eye receipt after the next
private worker reuses its source list. CE pause ownership ends at presentation
loss even when the CE module stays resident. Each has a reproduced regression.
Anniversary's independent primary-eye lens correction now passes depth, scene,
shading and nested-frame/exception regressions. The HUD eye reader alone does
NOT fix FP projection; the separate stage reader owns this optional feature.

Read the dedicated HAND-STABILITY, ORIGINAL-STARTUP, HUD-FRAME-OWNERSHIP,
ANNIVERSARY-LENS-OWNERSHIP and EXIT-PRESENTATION evidence plus
`HALOCE-STABILITY-CANDIDATE-2026-09-15.md`. Cumulative Release and all 20 CTest
suites pass, including final nested-frame cases; Reach consistency, generated
contracts, 101 pinned contracts, 15 production mapped-image groups, Classic/HUD
fragments, all 12 official weapon graphs, 15 official-mesh cases, 30 native skin
cases and 48 native shader draws pass. Records: `out/ce-stability-*` and the
dedicated hand evidence records. Packaging repeats required checks after final
commit. `out/ce-current-handoff.json` records exact new build/source archives and
hashes only after archive/source verification; never redeliver its old e524d21
pointer. Local checks never imply headset acceptance. Both CE modes and the CE exit shared callsite need
headset testing, with Halo 3 regression required by the shared-code contract.

No new CE melee/world collision/body-following feature is introduced. All other
title work and EVERY standing/deferred request in the continuation list remain
preserved. Keep successful gun scale, native aim/origin/effects, Original HUD,
graphics-switch reference continuity, scene refresh and desktop mirror.

# Historical continuation - September 15: CE camera, weapons and pause refinement

Latest user tested `a2b526a`: BOTH Original and Anniversary now visible in VR.
Keep that successful scene/output baseline. New concrete defects are Original
gun/hand distortion, facing jumps when switching graphics, and an invisible
native pause menu. User explicitly authorizes cumulative CE refinement through
build ZIP AND matching source ZIP, autonomously with no approvals. Package
without `-Install`, deliver both in chat, then WAIT for testing/instructions.
No install, launch, game-folder writes, PR or publishing. This supersedes older
packaging holds and isolated-diagnostic scope; both editions remain supported.

Both supplied logs are preserved under
`out/test-runs/a2b526a-ce-refinement-feedback-20260915/`; exact hashes and
feedback in `HALOCE-A2B526A-TEST-2026-09-15.md`. Steam, SteamVR/OpenXR 2.17.9,
Oculus-family, 90 Hz. Halo 3 initially flat, manual recovery completed after
native load-gate admission; pause/resume worked. Record this positive recovery
result without claiming universal automatic injection. Halo 3 regression was
requested because preceding CE work touched shared VR/input/lifecycle code.
Current priority remains CE, not a new Halo 3 recovery redesign.

Current refinements: preserve the HMD reference across graphics switches while
invalidating old receipts; independent native CE pause state drives shared 2D
pause presentation and the Y+B shortcut; Original tracked first-person models
and effects preserve the native world eye lens; cold HUD target proof precedes
the overlapping core release hook. Optional Anniversary HUD replay diagnostics
now identify the rejecting guard. Read the dedicated CAMERA, PAUSE, HUD,
FIRST-PERSON refinement evidence and `HALOCE-REFINEMENT-CANDIDATE-2026-09-15.md`.

The camera-switch regression failed against old behavior and passes corrected.
Cumulative Release and all 20 CTest suites pass, as do Reach consistency,
101 pinned contracts/15 production mapped-image groups, 30 native Original
lens cases, native Anniversary shader/skin upload, eight target-restoration
cases, 27 native gameplay-camera conversions, 10 native scene refresh cases
and all 12 official weapon-graph fixtures. Primary-eye projection ownership,
reflection exclusion and native exception cleanup pass. Package identity and
exact records belong under `out/ce-refinement-*` and
`out/ce-current-handoff.json` after packaging. Packaging repeats required
build/checks for the committed source. Do not
infer headset acceptance from these checks. Anniversary native HUD replay,
every weapon/animation/muzzle effect, multiplayer menus and 90 Hz parity remain
unconfirmed. Prior `be2140f` includes 52 successful shot adjustments and user
confirmation of controller bullets; a2b526a has no shot-hook calls and cannot
establish firing coverage. Preserve the verified aim path and native origin.

The four failed CE enables stay false; scene-refresh correction stays enabled.
Accepted `4e01f28` remains unchanged. CE body following, physical melee/world
collision, all-title zoom, dual trajectory, vehicles, alignment and EVERY
standing/deferred request below and in the refinement list are retained.

# Historical continuation - September 15: CE scene-refresh correction and ZIP handoff

Recovered the actual prior root chat through the quoted Original final-output /
Anniversary scene-cache update. Pre-edit WIP preserved under
`out/checkpoints/20260915-135529-ce-final-resume`. Resume source was `b272077`.
The current user says continue and forget nothing; the recovered prior request
explicitly requires build/source ZIP delivery without confirmations. Package
without `-Install`, deliver both ZIPs, then WAIT for headset testing/instructions.
No game launch, installation, game-folder changes, PR or publishing.

The native static-scene visibility omission is now reproduced offline: adding
a second camera updates region membership but retains first-eye-only static
object masks until the native refresh request is made. The scoped scene-camera
hook requests that refresh only on transitions into/out of two-camera mode;
stable stereo does not rebuild each frame. Native authored hidden regions and
independent geometric rejection remain active. Both active/copied preparation
branches, stock return during retirement, and foreign scene/camera guards are
covered. Independent hook-lifetime audit found no new defect; all 14 hooks drain
in supported 8+6 quiescence batches. See
`HALOCE-SCENE-REFRESH-EVIDENCE-2026-09-15.md` and the pinned native verifier.

Original's final-output capture and actual dimension-changing DXGI resize
recovery pass the production WARP fixture. Kind-zero source/Present bootstrap,
optional Anniversary HUD isolation, working hands/aim and the committed
single-eye desktop mirror are retained. The four failed CE enables remain FALSE;
`kCeSceneVisibilityBaseVrEnabled` enables this corrected cumulative candidate.
No further camera transform or global visibility override was introduced.

Cumulative Release and all 20 CTest suites pass. Generated contracts, all 99
pinned contracts, Classic/HUD fragments, 14 production mapped-image binding
groups and Reach consistency pass. Records: `out/ce-delivery-*20260915.*`.
Packaging repeats build/checks for the exact committed identity. Final build ZIP,
matching source ZIP, DLL/archive hashes and source commit are recorded in
`out/ce-current-handoff.json` after successful packaging. Candidate notes are
`HALOCE-VR-CORRECTION-CANDIDATE-2026-09-15.md`; the package now uses these notes.

The native omission is established; the user's displaced/missing Anniversary
world is NOT headset-confirmed fixed. No new capture, screenshot or runtime
test was requested or performed. Both CE modes and Halo 3 regression need the
user's test. The unusual support-hand angle remains unconfirmed. Accepted
`4e01f28`, both editions, all prior title work and EVERY standing/deferred task
are preserved. CE body following/melee/world collision, H2 vehicles, all-title
zoom, dual trajectory, alignment and other listed refinements remain retained.
Do not advance `CURRENT-STATE.md` from offline evidence or redeliver `be2140f`.

# Historical instruction - September 15: autonomous CE correction and ZIP delivery

The user explicitly requests continuation through a build ZIP with matching
source ZIP for BOTH CE Original and Anniversary, without being present to
approve anything. This supersedes earlier packaging holds. Use existing
evidence; do not take or request more screenshots of the reported failure.
No confirmation requests, installation, game launch, game-folder writes, PR
or publishing. Run `tools/package-candidate.ps1` without `-Install`.

Resume HEAD is `b272077`, which includes the verified single-eye desktop mirror.
The retained RenderDoc analysis and extraction/replay tools are preserved WIP.
Offline work now checks native geometry/view-role admission, Classic's corrected
source bootstrap and final capture integration. A final package must describe
the actual correction and verification honestly; local tests cannot establish
headset acceptance. Accepted `4e01f28` and all standing/deferred work remain.

# Exact continuation - September 15: screenshot reviewed, capture retry diagnosed

The user resumed the last chat at its recording-analysis/single-view desktop
update, reaffirmed NO ZIP until CE works properly, and explicitly reminded us
to inspect the screenshot already supplied. Root read the exact prior exchange
and visually inspected `C:/Users/Shadow/Pictures/3.PNG`, matching the preserved
`out/test-runs/be2140f-ce-partial-failed-20260915/3.PNG`: normal upper world/gun,
below-world lower image, reticle across the split. The prior explicit report
that one HEADSET eye shows the bad view remains authoritative. Do not ask for
the same symptom confirmation or screenshot again.

Both existing RenderDoc files are now analyzed. They retain the broken stacked
image in INITIAL backbuffer contents, but their recorded draw streams are mono
retries. The retained overlay and RenderDoc log prove frames 2767/3211 failed
with `Uncapped Map()/Unmap()`; saved frames are 2768/3212. CE heartbeat expiration
and detach also occur during each capture window. Earlier statements that the
files contain only flat evidence were incomplete. Read
`HALOCE-RENDERDOC-EVIDENCE-2026-09-15.md` and the reusable extraction scripts.

Retained GPU constants include both eye origins, matching the pre-timeout mod
log. Dominant world matrices are bit-identical across eyes, and camera origins
at CB0 +0x240/+0x320 agree. The much smaller second-eye object-transform set is
a lead for native geometry/visibility admission, not a proven cause. Current
independent audits cover culling/list visibility and explicit secondary-view
versus native stereo role selection. No new camera transform is justified yet.

The interrupted desktop-mirror edit is now implemented and its production WARP
fixture passes, including actual pixels, crop/gamma, source preservation,
dynamic shader state, release/recovery and failure isolation. Review caught and
fixed a reserved HLSL identifier and a transient linked-shader restore crash.
See `HALOCE-DESKTOP-MIRROR-2026-09-15.md`. Cumulative Release build, all 20 CTest
suites and the Reach consistency gate pass; records are
`out/ce-mirror-final-{release,ctest,reach-gate}-20260915.txt`.

HEAD at this resume is `0783d59`; pre-edit WIP is preserved at
`out/checkpoints/20260915-125635-ce-capture-mirror-resume`. Corrections from
`eacd81b`, working hands/aim, both editions and every standing/deferred task
remain preserved. All four rejected CE core enables remain FALSE. The accepted
pointer stays `4e01f28`. No ZIP, installation, game-folder writes or publishing.
The previously authorized ONE RenderDoc launch already happened. MCC was no
longer running when checked during this continuation; another launch requires
a new explicit request. Offline replay now works through a headless helper;
do not repeat the rejected RenderDoc preference change or identical F12 trial.

# Historical live diagnostic authorization - September 15, 12:40 local

The user explicitly authorized ONE diagnostic launch of the existing installed
Steam be2140f through RenderDoc, with the user loading the failing Anniversary
scene. This exception is already granted; do not ask again or launch a second
copy. Launch succeeded: MCC PID 26064, launcher PID 7844. Both RenderDoc and the
unchanged be2140f mod are verified loaded in MCC. No source fixes were installed.
The pending user step is reproducing the same displaced eye and pressing F12
once to capture, then reporting whether the original symptom persisted.
Capture prefix: `out/ce-renderdoc-evidence/ce-steam-be2140f`.
Read `out/ce-renderdoc-evidence/README.md` for commands/settings. Actual capture
file/reproduction remains pending; do not treat launch as acceptance.
Source corrections are committed as eacd81b; all four rejected enables stay
false and no build/source ZIP is authorized yet. Accepted source is unchanged.
Capture/export of an isolated WARP fixture passed, including raw constants and
immediate/deferred target/copy identities (`fixture-export-verification.json`).
GUI GPU replay remains unverified. A rejected RenderDoc preference-change
attempt made no change; a separate command-line export route succeeded.

# Latest user result - September 15: be2140f partial progress, FAILED core VR

Current correction work is preserved in
`docs/HALOCE-CORRECTION-HOLD-2026-09-15.md`. Classic's native kind-zero
source/late DXGI metadata bootstrap and Anniversary's HUD flag/optional-failure
isolation are corrected in source and locally verified. Final Release build
and all 19 CTest suites pass. Native pinned/mapped bindings, generated contracts
and Reach gate pass. These results do not fix or validate the Anniversary
world mismatch. The user explicitly confirmed it affects one headset eye.
All four rejected CE core enables remain false; no ZIP or deployment.
Actual GPU draw/target/constant capture is the remaining evidence requirement.
Only isolated capture-tool preparation is underway; MCC is closed and has
not been launched. Installed Steam be2140f DLL hash independently matches.

The delivered integrated candidate has now been TESTED. Anniversary hands track
and shots follow the controller, with an unusual left-hand angle; split/displaced
world rendering persists. Classic still does not enter usable VR. New log:
out/test-runs/be2140f-ce-partial-failed-20260915/HaloMCCVR-user.log, screenshot 3.PNG.
Read docs/HALOCE-BE2140F-TEST-2026-09-15.md. Do not redeliver or accept be2140f.

LATEST REQUEST OVERRIDES older package-ready instructions: resume basic VR for
BOTH CE Classic and Anniversary; no ZIP until evidence supports confidence in
the existing-title baseline. Preserve working hands/aim and all deferred scope.
No install, MCC launch, game-folder writes or PR. Accepted 4e01f28 is unchanged.
The failed integrated enable is disabled separately before correction; retain
all code. Classic logs zero pairs/610 source misses; Anniversary HUD has zero
replays/4890 fallbacks. World consumer admission alone did not solve displacement.
No manual compaction. Recover available prior chats and continue from actual code.

# Package-ready integrated CE candidate - September 15, 2026

Latest user instruction is to continue until a ZIP is packaged; it supersedes
all older package holds below. Deliver the build ZIP AND matching source ZIP
in chat, then wait for headset results. Never install, launch MCC, change game
folders, open a PR or publish. Accepted 4e01f28 is unchanged; both editions and
all prior/deferred work remain supported/preserved.

Recovered exact prior root chat through 15:11 UTC HUD update. Pre-edit recovery
copy: out/checkpoints/20260915-111814-ce-package-resume. This continuation
finishes the interrupted base CE integration: Classic native eyes; Anniversary
camera/depth admission, full-height HUD-to-eye replay and restoration; native
FP skin/GLT/ZFILL/SFX projection; motion-blur configuration; hands, aim, native
controls and state guards. A new integrated enable flag is on; the three failed
camera-only experiment flags stay false. Their verified camera construction is
reused with the new consumers/features, not presented as a new transform fix.

New audit fixes: Anniversary previously never published its gameplay center,
so its control/shot path could not admit. Publication now removes proven Saber
world-offset/forward-bias terms with separately verified normal-camera branch
guards, and rejects free/external cameras. FP retirement now drains nine hooks
in supported 8+1 batches. HUD callback stack, target, viewport/scissor and
full-height/half-height mapping are restored and verified.

Final pre-commit Release build and 19/19 CTest suites pass, alongside Reach gate,
generated contract check and full pinned/mapped native bindings. Actual native
camera, depth, HUD, controls/shot, blur, skin/material and shader verification
records are under out/ce-package-* and out/ce-fp-final-*. Actual shader WARP:
48 draws; gameplay bridge: 27 native->compiled inverse cases. Packaging repeats
Release/checks for the exact committed identity. Read out/ce-current-handoff.json
for the finalized package/source paths, hashes and source commit after packaging.
Candidate notes: docs/HALOCE-BASE-VR-CANDIDATE-2026-09-15.md.

The old b9662cd displaced-right/flat-left Anniversary world-render failure is
NOT established fixed. Camera/projection/viewport audit found no supported new
transform correction; this is an integrated headset test, not runtime acceptance.
Actual live materials, animation/muzzle/HUD behavior, broad vehicle/state parity,
and Halo 3 regression remain unconfirmed. CE roomscale/body following, physical
melee/world collision, H2 vehicles, all-title zoom and EVERY standing/deferred
requirement below are preserved for later. No manual compaction was initiated.

# Latest user instruction - September 15: continue through package delivery

The user explicitly requests continuing the recovered CE/Anniversary work and
DO NOT stop until a build ZIP is packaged. This supersedes the older packaging
hold below. Deliver BOTH build and matching source ZIPs in chat; run packaging
without -Install, then wait for headset testing. No install, game-folder writes,
MCC launch or PR/publishing. Accepted 4e01f28 remains unchanged.

Recovered the exact prior root chat through its September 15 15:11 UTC HUD
full-desktop/half-height-eye update. Its motion-blur, native FP shader and HUD
replay work is preserved with all earlier Classic, input, hands and depth work.
Finish and verify the base CE VR candidate, Anniversary first; roomscale,
physical melee/world collision, H2 vehicle refinements and all-title zoom remain
deferred and retained. Never claim local tests prove the unresolved headset
world-render failure is fixed. Do not initiate manual context compaction.

# Exact continuation - September 15: native skin verified, depth admission guarded

Recovered the actual 10:00 root chat and its three interrupted agent traces,
including the unfinished skin-converter verification and native depth audit.
Pre-edit copy: `out/checkpoints/20260915-102015-ce-exact-resume` (70 WIP files).
HEAD remains 7d34ab7; accepted 4e01f28 remains unchanged. Previous HUD/controls,
Classic, hands and deferred roomscale WIP is preserved. No install, launch,
game-folder writes, package, PR or publishing occurred.

Completed `tools/re/test_ce_skin_native.py`: 30 fixtures execute the pinned
native bone converter, then the compiled production scale helper. Native
conversion omits NodeMatrix's separate scale; the adapter applies it to the
nine basis elements while preserving translation/homogeneous bytes. See
E-CE-FP-4 and `out/ce-native-skin-adapter-verification-20260915.json`.

Added Anniversary per-eye depth ownership admission at the proven native
depth-mesh boundary. Immutable depth texture metadata, native selected DSV,
binding cache, camera raster and interpass resource revision must agree;
primary eyes cannot share texture/DSV identities and auxiliary depth cannot
overwrite a completed primary. Invalid ownership drops only that frame.
See `docs/HALOCE-DEPTH-EVIDENCE-2026-09-15.md` and its contract. Native split
depth allocation exists; aliasing is NOT established as the headset failure's
cause. All three rejected stereo enable flags remain false.

After these edits: Release build, all 17 CTest suites, generated-contract
check, pinned-image contract verification, production mapped-PE verification
and Reach gate pass. Final records: `out/ce-depth-final-*20260915.txt` and
`out/ce-depth-contract-verification-20260915.json`. WARP cases include aliasing,
missing/bad binding, interpass recycling, auxiliary overwrite and recovery.
These local checks are not headset acceptance. The displaced-right/flat-left
Anniversary failure and actual FP material projection selection remain open.
Core CE package hold and all standing/deferred requirements below still apply.

# Current priority - September 15: finish basic CE VR, Anniversary first

Actual previous root chat was read through its last roomscale-wiring update.
User explicitly corrects that priority: finish basic VR comparable to the other
games (stereo/6DoF, native HUD/reticle, independent tracked hands, weapon and
controller aim, standard input). Anniversary is first; Classic remains supported.
Roomscale, world collision, physical melee and other experimental refinements
are deferred. Preserve their work without spending this continuation on them.
No ZIP until the core CE implementation is credibly ready for headset testing.
No install, MCC launch, game-folder writes, PR or publishing. Accepted 4e01f28
remains unchanged. The b9662cd world-render failure is still unresolved at this
resume, and its three rejected enable flags remain false.

# Current resume - September 15 morning: full CE implementation remains WIP

Recovered the actual prior root chat and all three interrupted agent traces.
User reaffirms: no ZIP until confident both Original and Anniversary provide
VR comparable to the other titles; the user then headset-tests it, and only
AFTER functional confirmation do CE physical melee/world collision begin.
No install, game launch, game-folder writes, PR or publishing. Accepted 4e01f28
is unchanged. Preserve every standing/deferred item below.

HEAD at recovery: 7d34ab7, the separate disable of failed b9662cd. Preserved
pre-edit WIP at out/checkpoints/20260915-085454-ce-full-resume. The old session
ended mid-integration, not at a package-ready point. All three rejected
Anniversary enable flags remain FALSE; never re-enable them as a correction.

Current source work: Classic native render/capture adapter plus production
WARP failure/recovery tests; CE independent hand/weapon palette and guarded
native shot/aim-assist adapter; optional authored-crosshair transaction;
shared immutable controller/config publication, nonrender stock-camera receipt,
reference/renderer/generation freshness, and feature lifecycle wiring. Contracts
are now split by optional feature group; full mapped-PE checks exposed/fixed
duplicate first-person and HUD prefixes. Shared-renderer regression protects
Classic completed eyes from a stock Anniversary worker. New native camera
emulation checks execute pinned instructions and production C++ staging.

These are LOCAL validations, not native visible VR success. Anniversary's
same displaced-right/flat-left headset failure remains unresolved. Native
HUD size/aspect/curvature/vertical layout, actual Anniversary FP consumer,
locomotion/snap/body-following and state/vehicle parity still require work.
Read latest CE evidence docs and current git diff; active independent work
covers native shader upload, native HUD geometry, and first-person consumers.
Do not package a diagnostic/camera-only/single-renderer milestone. Full records
remain under out/ce-full-resume-*, out/ce-hud-*, out/ce-classic-* and new pinned
native camera/shot/tag verifiers. Exact final checks must be repeated after
active edits settle; do not infer current test status from historical output.

# Latest user override - September 15: b9662cd FAILED; full CE package hold

User confirms b9662cd has the SAME displaced right eye / flat left appearance.
Log records 557 builds, 555 captured pairs and 6 drops, coherent origins and clips.
Those diagnostics did not establish correct native rendering. Preserved report/log:
out/test-runs/b9662cd-ce-failed-20260915/. Do not redeliver or accept this build.

NEW packaging instruction supersedes every incremental handoff below: no ZIP
until BOTH CE Original/Classic and Anniversary implement proper stereo/6DoF,
two independently tracked visible hands, dominant-hand weapon and controller aim,
shared input/graphics switching, HUD/reticle and comparable existing-title behavior.
Continue implementation and substantive verification; no camera-only or one-renderer
milestone package. Local tests cannot establish flawless headset behavior.
Both editions and ALL standing/deferred work remain required. Physical melee/world
collision remain deferred until functional CE injection confirmation. Accepted
pointer remains 4e01f28. No install, MCC launch, game-folder changes or publishing.

b9662cd tracked-view experiment is disabled in its own commit before replacement;
retain code and evidence. Parallel work now covers actual Anniversary render/source
consumers, native Classic render path, and CE hands/weapon/aim bindings. Root handles
shared integration, HUD/input/lifecycle and end-to-end checks. Read current git state
and latest chat on resume; the prior package-ready paragraphs below are HISTORICAL.

# Latest - September 15: tracked-view construction correction

User tested 6e31b25: 803 captured pairs and faster visible Anniversary switching,
but displaced/noclipped right eye and flat/head-attached left; Original remains
flat. Exact request/log preserved under out/test-runs/6e31b25-ce-partial-failed-20260915.
Prior delivery chat was read. This is NOT acceptance; do not resend 6e31b25.

After separate disable f63254b, a correction now stages each tracked eye BEFORE
native append builds its independent position metadata. Finalization retains
native per-eye fields and applies the primary native clip range to both eyes.
See E-CE-12 and HALOCE-CONSTRUCTION-2026-09-15.md for proof, tests and limits.
Release/eight suites, Reach gate, pinned/generated and mapped-PE checks pass.
The displaced-view cause is not fully isolated; no headset success claimed.

Package the correction without -Install with current notes, verify both ZIPs,
deliver build/source here, then WAIT for headset result/instructions. Read
out/ce-current-handoff.json for exact final identity. No installation, MCC launch,
game-folder modification or PR/publishing. Both editions remain supported.
Accepted 4e01f28 and all existing input/gesture/other-title work remain preserved.
Original stereo, CE independent tracked hands/weapons, controller aim, HUD/reticle,
locomotion and state/vehicle parity remain REQUIRED and unfinished. Physical
melee/world collision still await functional injection confirmation. All standing
and deferred items remain below; do not describe this candidate as full CE VR.

# Latest - September 15: 6e31b25 tested, partial progress but FAILED VR

Recovered prior delivery chat and new user log. Anniversary now captures 803
pairs (805 built, 6 drops) and switches without black VR, but right eye is
displaced/noclipped and left looks flat with head-attached gun. Classic remains
flat by current implementation. Do not redeliver 6e31b25 or call it accepted.
Full test record: out/test-runs/6e31b25-ce-partial-failed-20260915/.
Failed CE rendering disabled in a separate commit before correction; retain
all code, working input/graphics gesture and earlier capture/raster fixes.
Next: verify actual render-consumed cameras and per-eye source identity, then
correct stereo/6DoF. Both CE renderers and independent controller hands/aim,
HUD/crosshair remain required. Prior deferred tasks preserved. Physical melee
and world collision still await functional VR confirmation. Package only a
credible correction with build/source ZIPs; no install/launch/game writes/PR.
Accepted source remains 4e01f28; both MCC editions supported.

# LATEST - September 14 late: correction after the ALREADY TESTED CE failure

Recovered the actual prior chat and attachment after the initial resume wrongly
treated the pre-test checkpoint as current. e17a664 was already tested/failed;
do not ask for its first test or resend it as a new update. Full failure details
are preserved immediately below. Failure-disable commit is `736f0c5`.

The follow-up correction now retains exact primary-eye identity through native
auxiliary culling views (total count 2..50), selects actual source texture raster
before camera/projection rebuild, and records explicit frame rejection reasons,
counts, dimensions, copy mask and both camera positions. E-CE-11 records native
proof and limits. Tests reproduced rejection with the old total-count guard;
the corrected pair and production WARP fixtures pass, including a full-height
stock camera/half-height source and copied lists with auxiliary views. All eight
Release suites, Reach gate, generated contract, pinned SHA/witness and mapped PE
checks pass. Logs/evidence: `out/ce-repair-*`, `out/ce-failure-*`.

Package the new correction without -Install, using HALOCE-REPAIR-2026-09-14.md
as notes; deliver BOTH build/source ZIPs, then wait for the new result. The exact
delivery record is written to ignored `out/ce-current-handoff.json` after archive
verification; read it and the latest project conversation on the next resume.
The old log did not record enough detail to prove which guard caused every drop,
and the displaced-view cause remains unconfirmed. This is a correction candidate,
not headset acceptance or full CE parity. No install/launch/game writes/publishing.
Keep both editions, accepted 4e01f28, working input/gesture and every deferred
item below. CE melee/collision still await functional VR injection confirmation.

# September 14 late headset failure, recovered from previous chat

The e17a664 build/source ZIPs were ALREADY DELIVERED AND TESTED. The previous
chat received the user's log and instruction to fix CE at 2026-09-15 03:07 UTC.
Its last action was reading the CE builder/capture code before interruption;
no fix or failure-disable commit had been made. Do not redeliver that candidate
or ask for its first test. On continuity-sensitive resumes, check the latest
project conversation as well as this file; older saved notes missed that reply.

User confirms input and left-head-side graphics gesture work. Classic is flat;
Anniversary is black in the headset, with vertically stacked desktop views,
one displaced/noclipped elsewhere and one near the normal camera. Fix the actual
stereo/6DoF failure, using the existing titles and CE-specific native evidence.
Preserved full report and log: out/test-runs/e17a664-ce-anniversary-failed-20260914/.
Log SHA256 6D283B2FD811A5A587AD5F0EB192F6505C60EAF8FBCE18CF62B5BB7B133EDF57.
Steam / SteamVR OpenXR 2.17.9 / Oculus-family headset / 90 Hz; exact model and
mission not supplied. Log source is e17a664c1160b750f29dabcb35da3b0709c968eb.
Runtime: 734 prepared frames, zero completed pairs, 734 dropped; staging=0;
cache allocated 2912x1050 format=90 while backbuffer is 2912x2100. Descriptor
miss counter stays zero. These facts do not yet isolate the failing guard.

Failed CE rendering is disabled in its own commit before the next experiment;
code remains intact. Shared input/graphics gesture remain. Next: trace native
camera/raster preparation and exact capture rejection, add a regression for the
actual mismatch, then build/check a corrected candidate if evidence supports it.
No install, launch, game-folder writes or publishing. Do not advance accepted
4e01f28. Both editions and ALL standing/deferred tasks remain. Physical melee/
world collision stay deferred: functional CE VR injection has not been confirmed.

# Historical pre-test continuation - September 14, 2026: CE Anniversary candidate

The runtime WIP beyond a6a507a was recovered and preserved under
out/checkpoints/20260914-215646-ce-finish-resume. CE Anniversary native two-view
preparation/capture is now connected to shared OpenXR and title admission.
Early texture metadata, lifetime/raster guards, exact prepared poses, recenter
revisions, stale-frame rejection and scoped retirement are implemented. Eight
Release suites pass, including actual WARP pixels through production scopes.
Read the NEW September 14 bring-up section and E-CE-10 for evidence/limits.

The first connected Anniversary stereo/6DoF candidate is ready for package-only
testing after final checks. Deliver both build/source ZIPs, then WAIT for the
user's headset result/instructions. Package with tools/package-candidate.ps1
without -Install. Current notes: HALOCE-CANDIDATE-2026-09-14.md. No install,
game-folder writes, MCC launch, PR or publishing. No headset success is claimed;
accepted pointer remains 4e01f28. Both editions remain supported.

Unfinished CE work is explicitly retained: Classic stereo, controller aim and
tracked weapons/hands, separate HUD/crosshair, native state/vehicle integration,
snap turning, head-relative walking and roomscale body following. The graphics
gesture is wired; Classic currently returns to stock flat presentation. Physical
melee/world collision await injection confirmation. ALL standing/deferred tasks,
including H2 vehicles and all-title zoom, remain preserved below. No full CE
parity/completion claim. Earlier no-runtime-hook entries below are historical.

# Latest continuation - September 13 evening, 2026

CE continuation from clean `73c9cd2`: owned D3D11 eye-cache component implemented,
with real WARP pixel-copy/recycling/release tests, exact frame/resource identity,
and guarded submission borrowing/retirement. Read the NEW evening section of
HALOCE-BRINGUP-2026-09-12.md and E-CE-8/9 in HALOCE-RENDER-EVIDENCE.md.
Seven Release tests and Reach consistency pass. Native scheduler/source
descriptor evidence extended; loaded-image contracts now check 24 entries.
Actual CE hooks, live source acquisition and OpenXR/title admission remain
unwired. No functioning CE VR or headset result is claimed.

User explicitly reminded us to reuse all existing games as the working VR
baseline. Preserve shared tracking/input/recenter/frame submission behavior,
translating CE-specific native details through evidence. Do not redo completed
camera math, bindings, receipt or GPU-cache fixtures. All standing/deferred
tasks remain retained. Packaging is held for credible comparable 6DoF; no
install, MCC launch, game-folder writes or publishing. Both editions remain
required; accepted pointer remains `4e01f28`.

# Latest user override - September 13, 2026

Continue Halo CE VR. Do not package a ZIP until the implementation is reasonably
expected to function with 6DoF comparable to the other games. This supersedes
any earlier suggestion to package an injection-only or research milestone.
No installation, MCC launch, game-folder writes or publishing. Physical melee
and world collision remain deferred until the user confirms CE injection.

September 13 later continuation: CE loaded-image verifier/native camera rebuild
adapter and explicit active/copied-list receipt logic are now implemented and
tested. Read the NEW later-continuation section of HALOCE-BRINGUP-2026-09-12.md
and E-CE-7 in HALOCE-RENDER-EVIDENCE.md. `0x4556B0` signals completion; it is
NOT a worker wait. Native scope/scheduling, GPU descriptor/lifetime/capture,
OpenXR/title admission and Classic/controller/HUD integration remain unfinished.
Six Release tests, mapped pinned-PE verification, SHA/witness verification and
Reach consistency pass. No hooks/injection, native game code execution, ZIP,
install or launch occurred. Preserve both editions, deferred tasks and accepted
pointer 4e01f28. Pre-edit WIP backup: out/checkpoints/20260913-155919-ce-runtime-resume.

Earlier September 13 continuation: CE private two-view preparation and native transfer
shape guards now implemented/tested. The previous full-frame replay direction is
not safe: CE consumes worker completion once per frame. E-CE-5/6 trace its native
two-view builder before culling and exact D3D surface handoff/variant selection.
Read the new continuation section of HALOCE-BRINGUP-2026-09-12.md and
HALOCE-RENDER-EVIDENCE.md before runtime integration. No CE runtime .cpp or
OpenXR/title admission is wired yet; no claim of functioning injection. Release,
five tests and Reach consistency pass; packaging remains held. Accepted pointer
stays 4e01f28. Do not repeat completed offline investigation or package scaffolding.

# ACTIVE — September 12, 2026: Halo CE Anniversary VR bring-up

LATEST delivery instruction: package a test ZIP as soon as the CE implementation
reaches a state reasonably expected to work. Do not wait for complete CE parity
or deferred vehicle/zoom/melee/collision work. Deliver matching source ZIP and
accurate implemented/unverified limits; no installation or MCC launch.

Latest CE steering: graphics switching must match H2's gesture (physical left
hand beside the left side of the head, click movement stick). Confirm VR injection
first; true physical melee and world collision are explicitly deferred until the
user confirms it. User permits using the existing CE VR mod as a reference while
acknowledging its different implementation. Verify MCC-native bindings independently.

User resumed and explicitly reprioritized: current vehicle controls are manageable;
checkpoint/defer that investigation. Focus now on Halo 1 / Combat Evolved
Anniversary matching the other supported titles: stereo injection, 6DOF, HUD,
crosshair and the same overall VR experience, translating proven workflows through
CE-specific evidence. This explicitly adds CE to scope, superseding older CE
exclusions. All-title zoom and H2 vehicle refinements remain deferred, preserved
in VEHICLE-ZOOM-PAUSED-2026-09-11.md; their old combined packaging hold does not
require completing them before CE work. No new packaging instruction yet.

Verified source at resume: cfb22eda5c968fc10b1083b56817a710855bd546, with only
the three preserved checkpoint documents modified/untracked. No vehicle changes
were implemented. CE registry entry exists but grants zero capabilities; no
CE-specific adapter/render/evidence files found on initial source search.
Read HALOCE-BRINGUP-2026-09-12.md for current CE progress and exact blockers.
Preserve existing title behavior, both MCC editions, and accepted pointer 4e01f28.
No installation, MCC launch, game-file modification or GitHub action is authorized.

# Historical pause — September 11, 2026: weekly usage checkpoint

User explicitly requested saving this checkpoint and stopping; wait until they
say to resume. Latest exact stopping point is the new first section in
[VEHICLE-ZOOM-PAUSED-2026-09-11.md](VEHICLE-ZOOM-PAUSED-2026-09-11.md).
Verified HEAD remains cfb22eda5c968fc10b1083b56817a710855bd546. This session
changed checkpoint documents only: NO vehicle/zoom source edits or tests yet.
The proposed stable seated heading reference and convergence test were announced
but NOT written. GitHub release task was cancelled because user uploaded it.
Preserve main-gun-hand-directed vehicle controls; do not replace with wheel/stick.
H2 AI feedback and source/history explanation are recorded in the detailed pause.

# Historical resume — September 11, 2026: Halo 2 vehicles and all-title zoom

LATEST vehicle clarification: user observes the other games following the right
controller/main gun hand wherever it points and explicitly wants that retained.
Match that controller-directed steering/aim in H2. Do not substitute raw-stick
or wheel steering as the requested default, or change the other titles' working
behavior. Existing optional controls are not a reason to change this priority.

Latest user instruction: disregard GitHub release work (user uploaded it
manually), resume the MOST RECENT progress, prioritize H2 Classic/Anniversary
vehicle controls matching the other titles, then H3's zoom box in the remaining
supported titles. Verified starting state: HEAD cfb22ed; only the existing
checkpoint/standing-list edits and VEHICLE-ZOOM-PAUSED-2026-09-11.md were dirty.
No post-delivery feature implementation existed. Resume the investigation in
that document; preserve the delivered build's changes. No GitHub actions.

User reports H2 AI is now more responsive/attentive in both renderers and appears
fixed. Preserve its current behavior; inspect existing source/history only unless
new evidence makes a change necessary. No new log or exact test identity supplied;
this is positive user feedback, not cumulative build acceptance. Vehicle controls
and zoom remain the priorities. Packaging stays held until both are implemented
and checked, then deliver build/source ZIPs without installation or game launch.

# Historical pause — September 11, 2026: exact delivered-build continuation

User requested a checkpoint and STOP; wait for their explicit resume instruction.
Read [VEHICLE-ZOOM-PAUSED-2026-09-11.md](VEHICLE-ZOOM-PAUSED-2026-09-11.md)
for the recovered prior-chat sequence and exact investigation stopping point.
The cfb22ed build/source ZIPs were ALREADY DELIVERED, followed by the separate
GitHub root cleanup c57160d. HEAD remains cfb22ed. No feature source changes were
made after that delivery; this session performed read-only investigation only.
Next requested work is H2 Classic/Anniversary vehicle-control parity and H3-style
zoom screens in the other supported titles. Do NOT package until BOTH additions
are implemented and checked. Preserve all delivered progress. The older
roomscale delivery instructions below are historical, not outstanding work.

# Latest priority - recovered September 10 headset feedback

The roomscale package 644148a was tested: user reports improved collision/melee,
bad left-hand misalignment and roomscale body movement doing nothing, including H3.
Preserve collision/melee improvements and pause that work. Restore left-handed
presentation from the latest user GitHub release (MCCVR-d77c9dd) as default;
gate newer anatomical correction behind default-off Fix Hand Alignment
(Experimental), available only with left-handed mode enabled. Improve experimental
alignment if evidence permits, without blocking the stable fallback. Then diagnose
and fix real physical movement moving the native body across supported titles,
without drift, duplicate movement, height errors or breaking sticks/controller aim.
Read ROOMSCALE-LEFT-HAND-REFINEMENT-REQUEST-2026-09-10.md for the full recovered
user message and packaging requirements. After implementation/checks, deliver a
clean user-facing build ZIP plus matching source ZIP with accurate fresh/update
installation instructions. No install, game writes, launch, PR or publishing.
The older immediate-delivery handoff below is superseded.

Recovered starting point was clean HEAD 644148a; the previous chat only investigated
after receiving feedback. The refinement implementation is recorded below.
Downloaded reference: out/release-reference/moistman42069/ (release metadata and
source ZIP). Log: out/test-runs/644148a-roomscale-left-hand-feedback/user.log
SHA256: 8E1FF4F9567BDCC62F669E04EE7C511AA51364D036270841C5BB682C28FFE75A.
Steam / SteamVR OpenXR 2.17.9 / Oculus-family headset, panel 120 Hz. Exact headset
model not identified by this log. Accepted pointer stays 4e01f28: no cumulative
acceptance of this failed roomscale/alignment package.

## Refinement implementation/package resume point

Latest recovered request above is now implemented locally. Left-hand default
restoration and experimental toggle: d5bed1e; failed roomscale disabled first
in f868203. Corrected roomscale has all-title admission and one VR input merge
per nested native XInput poll. See LEFT-HAND-REFINEMENT-2026-09-10.md and
ROOMSCALE-REFINEMENT-2026-09-10.md for source evidence and validation limits.
Experimental anatomical correction remains unproven; default released placement
is the fallback. Weapon-bound and melee improvements preserved, further melee
work paused. User-friendly package notes are ROOMSCALE-LEFT-HAND-RELEASE-NOTES-2026-09-10.md.
Release build, 3 CTest suites (including actual roomscale transport fixture and
60/90/120 Hz simulation), and Reach gate pass. Packaging repeats checks at its
final committed identity; both ZIPs and hashes go under out/candidates. Deliver
the build and matching source ZIP here, then wait for testing/instructions.
Do not redeliver 644148a as the update. No headset acceptance of these fixes yet.


# Active MCCVR work checkpoint - September 10, 2026

## Current roomscale/package handoff (supersedes every older stop/hold below)

User resumed with: "get roomscale working on all games and package an updated
build with updated install instructions and details within it". During this chat
user chose **Preserve controller aiming for this package** when told H3/H4 native
body yaw follows gun aim. Thus horizontal roomscale body movement and head-relative
walking are this package's scope; independent head-following body yaw is deferred,
not completed. User then said continue/forget nothing.

Implementation: src/common/roomscale_logic.h and src/dll/roomscale.{h,cpp}, config
roomscale_movement (default off), F1 Controls, native XInput walking and all five
title camera integrations (both H2 renderers). Native travel consumes horizontal
tracking reference only; no new native hook, teleport, velocity or guessed field
write. Positive native on-foot admission, manual-stick priority, 100 ms command
expiry, generation/input-epoch checks and tracking-jump resets. See
ROOMSCALE-IMPLEMENTATION-2026-09-10.md and ROOMSCALE-CANDIDATE-2026-09-10.md.

Preserved cumulative local anatomical handedness, equipped-model bounds/melee,
snap turning, slider arrows, recovery/lifecycle and vehicle guards. H3 dual firing
remains disabled by 9569690. Prior worktree backup, including the half-written
roomscale helpers recovered at chat start: out/checkpoints/20260910-roomscale-resume-161058.

Release build, 3 CTest suites, Reach consistency and pinned legacy/H4 weapon-bound
verifiers pass locally; packaging repeats build/tests at the committed identity.
Package command now writes build ZIP, matching git source ZIP and SHA256 sidecar.
Run tools/package-candidate.ps1 WITHOUT -Install. Updated MANUAL-README.txt says
KEEP saved config and accurately describes titles, recovery and unresolved work.
Do not ship the stale September 9 melee notes as current candidate instructions.

Delivery: attach BOTH ZIPs in chat, then WAIT for user testing/instructions.
No install/game-folder writes, launch, PR or publishing. Do not advance
CURRENT-STATE.md; accepted source remains 4e01f28. No headset result for the new
roomscale/snap/handedness/bounds work. Exact package commit/hashes are in its
manifest/ZIP filenames under out/candidates; the latest roomscale package is the
handoff, not an accepted pointer. Further refinements remain in the standing list.

## Historical checkpoint entries (preserved for continuity)

# Active MCCVR work checkpoint — September 9, 2026

## Latest continuation instruction (supersedes the historical WIP hold below)

LATEST STOP/HANDOFF: user asked to finish snap turning, resend the full goals
list with completed items marked, THEN WAIT for their instruction before the
next task (roomscale movement). Do not start roomscale automatically. No ZIPs
requested. Weapon-bounds/melee coverage and snap-turn implementation passes
are complete locally. Read SNAP-TURN-STATUS-2026-09-10.md: H2's missing snap
path was added for both renderers; H3/ODST/Reach/H4 handlers and handoff guards
audited. Release/3 CTest pass; headset validation remains pending. Full status
ledger is in CONTINUATION-REFINEMENT-LIST.md. Preserve all uncommitted WIP.

Latest September 10 steering: finish the current weapon-bounds AND physical-melee
work, then verify/fix snap turning across ALL supported titles, report its actual
status, then implement an optional true roomscale tracking toggle. The requested
behavior is physical movement moving the character body and head turning making
the body follow. User confirmed: follow physical position AND head direction, including movement heading. This
inserts snap turning and roomscale ahead of the retained vehicle implementation
queue. Packaging remains on hold. This instruction preceded the completed local
snap-turn pass documented above; runtime acceptance is still pending.

Current weapon/melee progress: H3/ODST/Reach/H4 runtime equipped-model bounds
implemented and locally validated, including per-layout weapon-only melee
regression with stationary hand samples. Read RUNTIME-WEAPON-BOUNDS-2026-09-10.md
for native proof and explicit limits. H2 retains its live-verified reader.
Coverage implementation pass complete; headset results and the broader melee
refinement list remain pending. Snap-turn implementation also completed locally;
wait for user instruction before roomscale, per the newer stop above.

Latest steering also confirms BOTH weapon bounds and physical melee fixes are
in scope. Finish that work, then verify/report whether H2 vehicle controls use
the other titles' control method. A minor third-party H2 reticle-after-tank-exit
report is stored in docs/bug-reports/halo2-tank-exit-reticle.md and its image;
defer investigation. Do not confuse that report with the active vehicle audit.

LATEST user steering: Halo 4 damage black-screen/fade is DEFERRED. Disregard
that investigation until the user explicitly asks to resume it. Retain the
report only; do not spend implementation or research time on it. Physical melee
and automatic equipped-weapon contact remain the current active section.

September 10 latest completed section: all-title anatomical left-hand local
implementation and validation complete. See LEFT-HAND-IMPLEMENTATION-2026-09-10.md
for exact scope, tests and headset caveats. Release build, all three tests and
Reach gate pass. User informed of completion with headset validation pending.
Current section: automatic equipped-model weapon contact and physical melee.
The supplied H4 log is preserved under
out/test-runs/d77c9dd-20260910-weapon-contact-damage/user.log. Its frequent
hand-only fallbacks warrant model-bound/identity investigation. Do not claim
universal weapon collision or the damage blackout is fixed yet.

LATEST September 10 log/priority steering: finish all left-hand work, then
automatic equipped-model weapon contact/true physical melee (including modded
weapons), then H2 vehicle controls before first-person vehicles. Keep optional
dual trajectory and all other retained tasks afterward. New black-screen/fade
on damage (Promethean Knight melee example) is unresolved, not a proven effect
diagnosis. Supplied attachment 4ab61705-1189-4ecf-9a41-9bebeec16a0c/pasted-text.txt
identifies d77c9dd; verify identity rather than assuming the user's description
of the latest GitHub build means current worktree behavior.

September 10 steering: item 10 (slider precision arrows) is implemented and
explicitly removed from pending work; preserve it. User also explicitly requires
the manual VR force-injection/recovery button for failures to enter VR. Existing
F1 and launcher controls currently request H3 recovery only; retain that scope
limitation until additional title recovery is implemented and validated.

The user approved and requested persistent storage of the full 16-item list in
CONTINUATION-REFINEMENT-LIST.md. On every future "continue", use that list and
this checkpoint. Current task: verify existing stability/recovery work, report
its limits, then finalize anatomical left-handed support across all five titles
(H2 Classic/Anniversary), followed by optional independent dual trajectory.
Local implementation/builds/tests authorized; no packaging requested now.

Recovered HEAD is 9569690, which disables the d77c9dd H3 dual-fire experiment
after headset failure. The older "H3 implementation pending acceptance" text
below predates that failure. Accepted pointer remains 4e01f28. Preserve existing
uncommitted recovery/lifecycle, vehicle guard, H2, launcher/menu and test edits.
Current recovery implementation is being audited; do not assert all-title or
headset-confirmed recovery. The older September 9 WIP handoff is historical.

September 10 verification: recovered cumulative Release build, all three CTest
targets (including synthetic hook retirement/recovery-event tests), and Reach
gate pass. See RECOVERY-STATUS-2026-09-10.md for exact covered behavior and limits.
Proceeding with anatomical handedness, starting with both H2 renderer packets.

**User-requested WIP packaging checkpoint.** The user interrupted further
development due to usage limits and asked to wrap up/package. After delivering
the build/source ZIPs, wait for testing and new instructions. Do not continue
feature development automatically. This explicit WIP request overrides the
earlier packaging hold for this handoff only.

Read this alongside CURRENT-STATE.md and
MELEE-HANDEDNESS-DUAL-WIELD-2026-09-08.md when resuming. Preserve unfinished
worktree files; a new chat is continuation, not a request to reset progress.

## Latest user steering

- Focus now on optional left-handed main weapon/aim and independent dual wield.
  The user explicitly released the earlier stop-everything flat-mode priority
  after learning that an H3 recovery change already exists.
- Flat-mode recovery is NOT fully confirmed: H3's 2-second stale-camera
  retirement exists in 11eb89e but has no headset acceptance. The multiplayer
  logs are preserved and hash-verified in
  out/test-runs/4e01f28-multiplayer-feedback/{user,friend}.log. The user's
  stereo stops at 10:35:34.999 without later hook retirement/reinstall; the
  friend records a new install and stereo recovery. Exact loader cause remains
  unproven. Audit also found H4's sceneTargetMissing latch after repeated
  uncaptured eyes; it disables stereo for the level. No new recovery edits
  were made in this session. Keep this on the unresolved ledger.
- Preserve physical melee/world contact, weapon alignment and reticle sliders,
  slider precision arrows, and restoration of snap turning in the full scope.
  Slider arrows are implemented in the existing worktree and previously passed
  build/tests; do not redo or discard them. Snap turning remains to investigate.
- Do not package until the requested refinements are complete, unless the user
  explicitly requests a WIP ZIP due to usage limits. In that case checkpoint
  exact progress, list unfinished items and what may work in the ZIP, and
  deliver build and matching source ZIPs. Never install, launch, write game
  folders or publish/open a PR without a new explicit request. Both editions.

## Recovered state

HEAD 11eb89e descends from accepted physical-melee source 4e01f28. Acceptance
pointer is unchanged. Existing modified/untracked files are backed up with
hashes and a binary patch in out/checkpoints/20260909-handedness-resume-*.
No candidate ZIP was created. The older September 8 paused checkpoint is
historical; its missing-implementation claims have been superseded.

Existing handedness swaps primary/support pose, velocity, trigger/grip and
haptic roles while retaining physical sticks/buttons and D-pad preference.
It has a default-off F1 option, but anatomical hand/arm presentation is still
unfinished. H2 has an optional verified firing scope for independent rays.
H3/ODST/Reach/H4 direction, presentation and ordinary-campaign acquisition
(ODST/Reach/H4) still need work. Detailed retained evidence and prior validation
are in MELEE-HANDEDNESS-DUAL-WIELD-2026-09-08.md.

## Work completed during this session

- Added H2/H3/ODST secondary-presentation exclusion from support-grip coupling,
  independent of world collision/melee. Generation + 150 ms expiration,
  clock-wrap coverage, release-before-regrab on dual drop or handedness change.
- Implemented optional H3 independent firing rays using H3EK-matched retail
  3683A0 -> 3524B0. Native origin preserved; full local weapon/owner handles,
  title/tracking generation, age and install checks. Coherent independent
  controller publication, no render/firing locks, producer/consumer quiescence,
  isolated failure and worker telemetry. This supersedes the earlier statement
  that H3's implementation is entirely missing. Headset acceptance is pending.
- Added tools/verify-halo3-dual-bindings.py. It and the recovered H3 melee
  selector verifier pass against the pinned module. Initial cumulative Release
  build, both CTest targets and Reach consistency gate pass; packaging repeats
  build/tests for the final committed identity. Package manifest identifies it.
- Recovered and retained all prior sliders, melee range, contact smoothing,
  world-object target and H3 response-selection edits. None is newly accepted.
- Full user-facing scope/risks: MELEE-HANDEDNESS-CANDIDATE-2026-09-09.md, copied
  to MELEE-CANDIDATE-NOTES.md in the build ZIP. Accepted pointer stays 4e01f28.

## Exact resume point

No anatomical mesh change was made. H2 review stopped at
Halo2OwnFinalFirstPersonPackets and Halo2OwnDualFirstPersonPackets in
src/common/halo2_render_logic.h. They still bind anatomical right to primary
and anatomical left to support/secondary even after controller roles swap.
Do not fix this by blindly swapping carriers: main-gun ownership, authored grip
relation, contact volume ownership and both renderer packet paths must agree.
H3/ODST shared solver ReconstructVisiblePaletteSource has the same role/anatomy
distinction. More details and evidence: DUAL-WIELD-REFINEMENT-2026-09-09.md.

Next, subject to the user's test results: anatomical left-hand presentation;
ODST/Reach/H4 independent firing and ordinary-campaign acquisition; remaining
melee/contact/alignment cases; restore snap turn. Retain the unconfirmed
multiplayer recovery and H4 capture-latch finding. Do not assume this WIP ZIP
completes any of those requirements. Existing out/ evidence remains preserved.
