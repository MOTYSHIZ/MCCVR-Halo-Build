# Felipe Godoy's working checkpoint (PR #1, October 2026)

Moved here from the top of ACTIVE-WORK-CHECKPOINT.md, unchanged, when PR #1
was staged. These are the contributor's own session notes: "the user" in them
is the contributor's tester, not this repository's owner. They are history and
evidence pointers, not current priorities.

---

## Latest: clarify animation versus grip delay; grip minimum500ms

Reviewed32cd80d4 and574645d5: both change weapon_animation::Reference, not
acquisition. The latter correctly sets animation minimum500ms while retaining
its500ms quiet window and corresponding tests. Authored grip Learner still had
1000ms minimum; user explicitly wants half-second grip delay. Set its minimum
to500ms, retaining500ms quiet and tracking-gap/owner safeguards. Tests cover
stable readiness, motion, replacement and sample-gap restart. No animation
change, no build requested; commit/push on two-handed-grip.

## Latest: additive weapon alignment follow-up, five commits

User explicitly requests increments over prior defaults, one commit per game.
Final values: Reach Needler barrel pitch-9; ODST Needler barrel-18, Brute Shot
barrel-8, Plasma Pistol visual yaw0. H4 shotgun yaw-.3, railgun1.5, carbine-.5,
Boltshot7.6. H2A plasma rifle yaw1, plasma pistol.5. H2 classic SMG yaw.3,
plasma/brute rifles2, BR3, shotgun1.1. Other fields preserved.
ODST muzzle publication now always removes the visual frame, including built-in
visual defaults at zero sliders, before applying the exact-model barrel pitch.
Barrel origin stays fixed. Portable math/table checks pass for every increment,
title separation and ODST downward ray/origin. No DLL build requested.

## Latest: 500ms weapon-animation reference settling trial

Following commit 32cd80d4, reduce the initial Reference::Update minimum from
1000ms to 500ms. Keep the independent quiet window at500ms: observed motion
restarts that window, so elapsed minimum alone does not allow a moving weapon
pose to settle. Regression tests cover stable-pose readiness at500ms and motion
requiring a full500ms quiet period. This is the weapon-animation reference timer
in weapon_animation_motion.h; the separate authored-grip learning delay in
authored_grab_zone.h remains unchanged. Commit requested; no build requested.


## Latest: independent game brightness profiles

User reports brightness changes affecting every game. game_brightness was a
shared Config field, omitted from TitleTunables/descriptor table; the UI and
save comments explicitly described the shared behavior. Add it to the existing
per-title profile table (range.5..2, default1.11), save the shared base rather
than the active title value, and show the edited profile in Picture. Existing
shared config brightness seeds unconfigured profiles, preserving appearance.
No per-weapon brightness overrides. H2A/classic use their existing separate
profiles; CE/CEA share the existing CE profile.
Production config round-trip fixture passes locally with a temporary Linux
Win32 file-API shim, including legacy migration, seven distinct profile values,
save while active, reload and preservation of the shared seed. No DLL build.
Commit requested on two-handed-grip; no compilation workflow requested.

## Latest: separate Halo 2 classic/Anniversary visual defaults

Second requested commit: H2A Magnum pitch1, Plasma Rifle yaw.5, Plasma Pistol
yaw1, Brute Shot roll90. Classic SMG/plasma rifles yaw1, Plasma Pistol/BR yaw1.5,
Brute Shot pitch10 roll90. Add per-model defaults to each hand's own visual
carrier, preserving existing classic-wide pitch/yaw and neutral muzzle frames.
Separate secondary model observation avoids using primary weapon defaults on
the offhand. Portable table checks pass. No Windows compilation requested.

## Latest: Halo 3/ODST visual defaults

User confirms unnamed axes are yaw. Default pitch+1 for H3/ODST, overridden by
Needler+8, sword-25, Automag0. Spiker roll+10; both plasma rifles yaw+1;
Plasma Pistol yaw-1. Shared visual wrist path applies defaults before existing
secondary SMG/plasma rifle correction; neutral ballistic frame unchanged.
Secondary model observation is separate from primary to support different guns
in dual wield. Local table smoke checks pass. Commit only; no build requested.

## Latest: Reach model-specific visual and barrel defaults

User requests13 Reach model adjustments. reach_weapon_defaults.h contains the
exact yaw/pitch/roll baseline table, added to visual sliders at the prepared
controller target. Zero sliders select the new defaults. Identity observation
remains active without manual reload/per-gun options. Existing H4 defaults kept.
Two explicit barrel exceptions: Plasma Pistol yaw+1.6 and Needler pitch-8.
Apply these after removing visual trim from the shared muzzle receipt, using
its exact model checksum and neutral controller basis. Rotate direction/up only,
retain muzzle origin; reticle and barrel-shot consumers share the receipt.
Unknown models unchanged. Portable math check verifies rightward yaw, downward
pitch and fixed origin; Windows/runtime test pending. Commit only, no build.

## Latest: Halo 4 per-model visual yaw defaults

User confirms Reach/H4 grip reacquisition is fixed. Requested22 Halo4 weapon
visual yaw baselines are in halo4_weapon_visual_defaults.h, keyed by the existing
held-model catalog identities. Apply as an additive baseline before visual target
construction; the neutral muzzle frame still zeros visual angles. Slider0 now
means the shipped per-model yaw; saved manual trims remain additive. Observation
stays enabled for H4 even if manual reload/per-gun alignment are disabled.
Unknown models get0. Catalog check resolves exactly22 distinct H4 models; no
Windows build requested. Commit only on two-handed-grip. Headset yaw validation
pending; prior accepted release pointer remains unchanged.

## Latest: Reach/H4 regrip and downward acquisition margin

User reports first grip works, then release/regrab fails until weapon change in
Reach/H4; other changes accepted. Their log source adb695a0 includes both titles.
Replace null publication bases with each title's frozen aimNeutral controller
basis. Attached heartbeat must only refresh an existing matching owner, never
claim an uncalibrated unknown weapon and close legacy acquisition. See evidence
and limitations in AUTHORED-GRIP-ZONE.md. Enlarge shared zone downward5cm only.
Add unknown/foreign/repeated refresh and lower/upper bound tests. Cold counters
now include Reach/H4. No new hook, no relationship state-machine change.
No Windows build requested; source commit/push, headset validation pending.

## Latest: all-title authored grip and requested menu/input follow-ups

User accepts the H3 grip and reports the Plasma Rifle projectile issue resolved.
Port the same authored wrist zone and binary close-hand policy to CE/CEA,
H2/H2A, ODST, Reach and H4 using their existing admitted palette adapters.
Standard handedness remains the supported scope. New title headset tests pending.
Separate commits: all-title adapter, animation reference minimum1000ms (quiet
500ms retained), Crosshair & Trajectory label, H3 occupied support-hand pickup
suppression at proven semantic input aliases. Manual reload alone still permits
pickup. Publication and bridge run when two-handed aiming is enabled even with
manual reload and action mapping off. No new native offsets or hook signatures.

Local309 production aim checks, authored-zone tests, CE palette tests and
animation tests pass; persistent-support guard passes. Windows native input
fixture gains occupied/free-hand and unrelated-caller coverage, to run in CI.
User requests one combined build; trigger exact source workflow after source
push, without monitoring, installing or changing accepted release pointer.

## Latest: binary close-hand protection shared with committed aim

Remove gradual distance influence; close controllers restore the entire primary
pose with no support steering. Apply in the shared live/committed constructor
(the prior wrapper left committed/native-camera consumers unprotected).
Initial authored-grip learning delay1000ms; quiet check500ms unchanged.
309 production aim checks, zone tests and persistent-support guard pass.
No Windows build/headset validation. Plasma Rifle dual projectile report pending
which firing hand was affected; independent aim is enabled in supplied log.
No dual-shot behavior changed. See AUTHORED-GRIP-ZONE.md.

## Latest: larger grip zone and zero lateral-controller authority

User confirms source6735313e solves Magnum placement and reacquisition bug.
On two-handed-grip, expand long authored zone to54cm capsule/12cm radius and
short grip area to44cm wide/28cm tall/34cm long. Distance influence remains
unchanged; additional primary-local lateral guard zeros offhand directional
authority for side-by-side controllers, smoothly restoring it forward.
Tests pass (zone boundaries, distance unchanged,307 production aim checks).
Preserve stable owner publication and grip state machine. No build requested.
See AUTHORED-GRIP-ZONE.md for exact bounds and validation limits.

## Latest: H3 authored grip test follow-up

User reports Magnum requires forward placement/pulls aim down, far barrel still
acquires, and outside attempts appear to prevent reacquisition. Log confirms
frequent legacy fallback plus final continuous misses, not a proven stuck latch.
Preserve last settled owner-specific point with live attached/settling heartbeat;
supported pending/stale data no longer reopens barrel fallback. Use controller
grip positions and generic short-separation lateral volume. Add H3 distance-based
orientation authority (0<=16cm, full>=30cm) before calibration for stock on/off.
State machine and other titles unchanged. See AUTHORED-GRIP-ZONE.md.307 extracted
production aim checks, new geometry/publication tests and wiring guard pass.
Windows DLL/headset validation pending. Commit on two-handed-grip; no build yet.

## Latest: first authored two-handed grip acquisition adapter

Work on user branch two-handed-grip (created from6e13ded0). New H3 primary-only
acquisition zone derived from authored left/right wrist relation; exact owner,
fresh publication and continuous settling required. Both grip acquisition paths
use it, falling back to old barrel geometry. No permanent pose cache and no aim
solver changes. Other games and left-handed mode retain prior behavior. See
AUTHORED-GRIP-ZONE.md for semantics, known limits and headset test matrix.
Portable new tests and69 legacy checks pass; Windows/headset validation pending.
Commit only; no compilation workflow triggered. Preserve prior accepted fixes.


## Latest: accepted settling rollback; additional 5mm H3 offhand adjustment

User confirms the last build works. Move Halo 3 offhand SMG/Plasma Rifle another
+0.005m forward: shared model/effect forward trim changes +0.01m to +0.015m.
Preserve yaw, lateral trim, animation settling and all other accepted changes.
Update existing numeric production fixture; user requests a new build without
monitoring. No installation, launch or release publication.

## Latest: disable reused animation references after equip-time reload

User source2d0cad23 log and headset report: first offhand Spiker pickup followed
immediately by reload leaves the gun pointing upward, persisting on re-equip.
The initial 1500ms/500ms quiet heuristic was still enabled; settled-reference
cache bypassed that guard on reacquisition and could preserve a bad reference.
Disable cache consumption/storage in shared Reference::Update, keeping dormant
implementation. CE/CEA, H2/H2A and H3 secondary always settle on reacquisition.
Preserve smoothing/rotation-only motion, native H3 primary and disabled H4 path.
This rollback does not prove a reload plateau cannot fool the first-acquisition
heuristic; native animation-state gating would be separate evidence-backed work.
Regression fixture seeds a poisoned cache and verifies both acquisitions wait
and recover the idle reference. User confirms other implementations work;
preserve live HUD, bloom and alignment changes. Commit only, no build trigger.

## Latest: Ultimate Firefight live selected-record HUD candidate

User supplies vanilla/Firefight logs source0497eea9. Stock scan succeeds with3
records; mod fails on zero scales. Keep accepted stock scanner and rejected
alternate locator dormant. New optional exact selector/caller hook adapts only
selected nonstock/zero-scale HUD records through a DLL-owned copy, avoiding map
writes and double application. See HALO3-ULTIMATE-HUD-2026-10-25.md for verified
retail selection/consumption, limitations and portable tests. New counters log
adjusted/native uses. Signatures verified against supplied halo3.dll; portable
tests pass. Windows/headset validation pending. Commit only; no CI trigger.
Preserve bloom defaults/descriptions, offhand alignment and all prior features.

## Latest: accepted shader toggles and new bloom defaults (commit only)

User reports everything working. Reach/ODST Disable bloom now defaults on for
new installs and once for existing configurations via bloom_defaults_version=1.
Migration saves immediately; later explicit choices persist. Exposure default
unchanged. Scene descriptions explain restarting for bloom and recommend lower
brightness when exposure is disabled. Config regression fixtures updated; no
compilation requested or performed. Prior offhand adjustment preserved.

## Latest: H3 offhand SMG/Plasma Rifle forward adjustment (commit only)

User requests +0.02m relative to the current position, without compilation.
Change shared secondary forward trim from -0.01m to +0.01m. Visible models and
muzzle effects share this transform; preserve lateral trim, yaw and animations.
Update existing numeric fixture expectation. No build or workflow trigger.

## Latest: Reach/ODST bloom and ODST exposure shader toggles

Adapt supplied ShaderToggler pixel CRCs without ReShade. Picture -> Scene has
Disable bloom in Reach/ODST and Disable autoexposure in ODST. Exact matching
draws are omitted on the observed real immediate context; old Reach/ODST SRV
substitution disabled, Halo 3 preserved. Native defaults and existing bloom
preferences preserved. See BLOOM-CONSUMER-INTEGRATION-2026-09-23.md for hashes,
upstream semantics, limitations and tests. Portable policy test passes; Windows
fixtures and headset validation pending. Preserve prior H3 pickup/reload and
shared model/muzzle setback, all accepted animation behavior. Build only; no
monitoring, installation or accepted-pointer changes.

## Latest: accepted animations, H3 pickup regression and 1cm secondary setback

User confirms animations now perfect: preserve animation/filter/reference code.
Manual Reload was preventing left-hand weapon pickup because input aliases were
shared. Roll back alias suppression in037db157, then use a separately verified
H3 native player-action edge hook: clear only secondary reload pressed bit6,
leaving pickup-held bit5 and native records intact. Exact local-player datum,
caller, generation and fresh blocked-action publication required. See updated
NATIVE-VR-ACTION-BRIDGE doc and read-only verifier; kit/retail checks pass.
Windows runtime/assembly fixtures updated, require CI in this Linux workspace.

User requests SMG/Plasma Rifle left-hand visual model -0.01m forward (backward)
and explicitly the same muzzle flash movement. Add kH3SecondaryForwardMetres
=-.01 to H3CorrectSecondaryMatrix, the shared masked visual/effect transform.
Preserve +30 yaw and +.06m lateral. Existing checksum mask still limits it to
those two offhand models. H3 secondary production test passes, including an
independent numeric -1cm world-scale check and model/effects registration.

Prepare source/build without installing, launching MCC, advancing accepted
release pointer or monitoring Actions. Preserve all prior world-scale and
other-title fixes, and disabled H4 animation smoothing.

## Latest: H4 rollback, faster animation response, cached references, H3 dual reload

User log source5897cbe0 confirms H3 secondary animation too slow; primary reload
blocked but left reload still works. H4 smoothing causes flashing: disable both
H4 smoothing call sites in a separate rollback commit ca1d70cd, leaving dormant
helper/code per repository rules. No H4 filter is active in this candidate.

Root cause of slow response: H3's 0.02 EMA is per native interpolation call,
not per 90Hz display frame. Supplied log reports about2600 secondary samples
per5s, ~520Hz. Change new shared time-normalized filter from90 to520 samples/sec
(~95ms time constant rather than550ms), for CE/CEA, H2/H2A and H3 secondary.
This approximates the measured primary cadence; it is not proof of identical
primary/secondary native animation curves. H3 primary is unchanged.

No-wait path explanation: H3/ODST dynamically measure the authored wrist/barrel
and apply a continuously updated alignment; Reach uses its own mapped native
wrist/gun frames. Restored paths use a relative neutral delta on top of calibrated
controller carriers. Native axes are not interchangeable (known CE custom-model
axis exceptions), so copying the absolute wrist would risk alignment regressions.
Do not claim universal removal of first-acquisition delay. Add bounded16-entry
per-adapter caches of ONLY settled full-identity references. Exact owner/weapon/
model/renderer/stance key reuse on re-equip applies motion immediately, including
native draw pose, without relearning it as neutral. Unknown identities still use
1500ms plus500ms quiet guard. Ownership changes and cache eviction remain guarded.

H3 secondary reload uses dedicated aliases: see NATIVE-VR-ACTION-BRIDGE doc and
new read-only verifier. Local kit/retail proof, animation smoothing/cache tests
and H3 secondary production tests pass. Windows native runtime fixtures need CI.
Preserve prior world-scale/input fixes. Package/build only, no game install,
release-pointer change or build monitoring.

## Latest: user-requested smoothing for restored animations and Halo 4

Apply authored-rotation-only smoothing to CE/CEA, both H2 renderers/hands,
H3 secondary, and H4's exact Storm wrist reroot. H3 primary remains the accepted
0.02 barrel-alignment EMA. New filter uses shortest-arc rotation interpolation
with alpha=1-pow(0.98,elapsedMs*0.09), matching a 2% response at the user's 90Hz
baseline while avoiding per-eye/per-palette rate dependence. It does not claim
identical timing to H3 primary at every native sampling rate.

Filter camera/controller-local animation before composing with current tracking;
never low-pass the tracked controller or add animation XYZ. Keep the existing
1500ms/500ms reference acquisition guard for CE/H2/H3 to prevent equip offsets.
Reset filter on reference identity changes, invalid poses or >500ms gaps.
H4 uses independently proven owner weapon and generation, plus support/handedness
mode; preserves native attachment orientation and locked support-hand delta.
Unknown H4 owner leaves the established target unchanged. No new engine hook.

Local checks pass: animation tests (rate independence, duplicate samples,
convergence, invalid/stale/owner resets, H4 relative-parent rotation/XYZ), H3
secondary production regression, CE first-person, H2 left-hand 1005 checks.
Windows/MSVC/SEH/MASM suites still require CI. Request candidate with prior
native caller and Reach/ODST world-scale fixes; do not monitor the build.

## Latest: animation findings, native caller forwarding, live Reach/ODST scale

User confirms source7124f9d6 animations work across titles and suppression works
in CE/CEA/H2/H2A. H3 right hand feels smoother; CE/H2/H3 secondary takes 1-2s
on equip. Verified source causes: primary H3 barrel-in-wrist alignment uses a
0.02 EMA, whereas secondary uses direct frozen-reference animation. Reference
requires 1500ms plus a 500ms angular quiet window; this deliberately prevents
equip pose becoming the reference. Explain both; leave accepted animation
behavior unchanged rather than removing the guard and restoring equip offsets.
Portable animation and H3 secondary regression suites pass locally.

Fix later-title input bridge caller forwarding; see NATIVE-VR-ACTION-BRIDGE-
2026-09-30.md. User log: attachments/0a17e915-ee24-4a08-a8dd-d639f55378b2/
HaloMCCVR.log, Steam/OpenXR session, source7124f9d6. Added actual thunk caller
assertion to Windows assembly tests, preserving register probes. Cannot run
Windows/MSVC/SEH/MASM tests locally in this Linux environment.

World-scale report confirmed by source: Reach/ODST eye separation and head
translation used native fixed units while ODST hands already used live scale.
Use g_worldScale for stereo eyes, head translation, roomscale and physical
crouch; Reach controller placement, reticle projection and tracked muzzle lift
follow the same conversion. Telemetry reports the live value. Native tag/engine
distance constants and dormant visual-hand positioning remain unchanged.
Preserve per-title 50-150% slider/reset and other titles' accepted behavior.
World-scale log: attachments/c98e715c-b9e0-482c-84a4-3a250175e71a/
HaloMCCVRWORLDSCALE.log. Headset verification still needed for these fixes.
Do not advance accepted-release pointer, install, launch MCC or monitor CI.

## Latest: repair controller fixture link dependency

CI log109297293880 reports LNK2019 for NativeVrActions_CanRoute in the
D-pad action fixture. The fixture extracts shipping VR functions but did not
provide this new native-routing dependency. Add an independently controlled
mock and verify gesture-only routing with action mapping disabled, including
rejection of cached transport bindings and loss of native routing readiness.
Production code is unchanged. Windows link/runtime validation remains in CI;
request a new build containing all latest fixes without monitoring it.

## Latest: repair CE runtime fixture compilation and rebuild

User-supplied CI log reports C2660 in the contact transaction fixture: its
ApplyPalette call omitted the newly required native animation-root argument.
Pass nullptr explicitly because this fixture has no native animation owner/root;
keep its success, rejected ownership and rejected copy assertions unchanged.
All production behavior and latest gesture-only input changes are preserved.
Reviewed all ApplyPalette call sites and checked the diff locally. Windows
runtime tests require CI; no local Windows build or headset acceptance claimed.
User authorizes a new build and asks not to monitor it.

## Latest: manual reload / holster native button policy (commit only)

User requests disabling controller reload while Manual Reload is enabled and
controller weapon switch while Weapon Holsters is enabled, independent of control
layout. Explicitly no compilation requested. Do not touch the workflow trigger.

Use existing verified post-device action adapters for CE/H2/H3/ODST/Reach/H4.
Input publication carries a separate blocked semantic mask (Reload/SwitchWeapon)
and excludes ordinary mapped input for those actions, while preserving gesture
requests. Verified native consumers replace those actions with released private
records, or gesture counters when requested. CE restores scoped digital fields;
H2 edits only the private control-builder copy. Keep native context Use/pickup/
vehicle actions separate (especially H2's shared context button). No guessed
new binding, global config remap or raw physical-button mask. Menus/pause and
vehicle modes do not request suppression; stale/cancelled publications retain
existing guarded fallback. Automatic reload policy is unchanged.

Adapters now operate for either feature even if VR Action Mapping is off.
Once native cadence is verified, weapon gestures use semantic IDs regardless of
mapper setting; older in-flight transport pulses are decoded only from the
validated gesture packet. This policy also suppresses keyboard bindings for the
two named actions because native abstraction merges input devices. Menu explains
that consequence. Optional bridge proof/fault rejection remains feature-local;
no claim of unconditional enforcement when a title's native binding fails.

Added runtime fixture checks for released action overrides, gesture bypass,
context-action preservation, disabled mapper, CE restoration and H2 private-copy
isolation, plus transport decoding/semantic-mask checks. Static diff review only;
Windows tests/build not executed per user request. No workflow/build/release.
All weapon animation candidates and accepted muzzle corrections preserved.

## Latest: rotation-only animation, H2 inventory-pair identity, CE native root

User tests source643ce498: H3 secondary animation and equip-reference fix work,
H2 animations work in both renderers, CE animation works only in Anniversary.
User explicitly requests only pitch/yaw/roll animation, matching H3 primary;
remove authored XYZ transport in H3 secondary, CE and both H2 hands. Preserve
accepted muzzle effects and native internal-part animation.

Reference acquisition now ignores translation when deciding orientation
stability, and returned animation deltas have zero translation. CE/H2 adapters
also keep carrier positions unchanged if supplied a translated delta. H3 uses
the same rotation-only reference output, preserving the consumed-context fix.
The 1500ms acquisition delay/500ms angular quiet window remains a heuristic.

H2 primary retained its reference when the secondary changed, even though its
native stance changes with dual wield. Scope references to the full inventory
pair by including the partner handle as well as owner/model/count; reset on
single/dual and partner swaps. Combined with removal of XYZ transport, this
addresses the reported lateral shift. Requires headset validation.

CE log shows0 applied/2406 settling before successful interval, then374 applied
while settling resumes after return. It does not prove the exact per-pass root
mismatch by itself. E-CE-FP-1 proves the palette builder's position/forward/up
arguments are its native camera frame. Capture those arguments before native
composition at the already verified callsite, and remove that exact frame for
animation observation rather than the earlier outer-prepare camera receipt.
Keep the outer coherent XR context for controller placement, collision and
publishing. No new address/hook or scanner. This plus angular-only settling is
the CE Classic candidate; do not claim runtime acceptance from local tests.

Validation: reference/rotation tests, H3 scope/effects regression, H2 left-hand
suite1005 checks and CE first-person suite pass locally. Added checks for XYZ
invariance with nonzero angular delta, inventory-pair transitions, and a CE
reference settling across three different native roots per frame. Windows CI
and headset result pending. User says do not monitor CI. HUD work deferred.

## Latest: stabilize equip references; restore CE/H2 authored motion

User tests source6bf3cadd on Steam. H3 secondary motion now works, but removing
and equipping weapons seeds a bad permanent offset; a save loaded with SMG
already equipped is correct. H3 log shows reference resets rising1..5 during
these switches and motionApplied>0. Preserve accepted consumed-context fix and
muzzle effects. The first-frame neutral-reference acquisition is superseded.

New reference acquisition keeps the established controller mount while waiting
at least1500ms and a500ms quiet window (within2cm/3degrees). Freeze that stable
pose for subsequent recoil/idle/reload; do not continuously average animations
away. Owner/model/generation/renderer changes and gaps over500ms reacquire.
This is an explicit settling heuristic, NOT a native idle-state binding; a long
custom equip/reload paused at a non-idle pose remains a runtime validation risk.
Tell the tester to leave the weapon idle briefly after equipping, then exercise
fire/reload and repeated drop/regrab. No new native addresses/hooks introduced.

CE log: Magnum graphE8C2074E identity24585B0C2FBD0622,44nodes,gun7,wrists5/6;
post-solve gun forward/up exactly match controller aim even while authored pose
changes. H2 log confirms Chief packet ownership in both graphics paths, including
dual Magnums; no ownership rejection explaining missing motion. Native placement
currently cancels whole-weapon rotation and wrist translation in both titles.

Restore these components upstream of the existing solvers. CE uses its coherent
native center camera to remove world motion. H2 uses each renderer's own packet
camera and remapped carrying wrist (right primary/left secondary); references
are separate by renderer/slot. Observe gun rotation BUT wrist translation: the
existing solve already preserves gun-to-wrist translation, so using gun position
would count moving gun parts twice. Hands, gun and native effects continue through
the existing common transformed palettes/carriers. Keep free CE support hand
independent. CE owner receipt is captured even when barrel aim is disabled;
revalidate owner before motion observation. Unsupported/invalid samples keep the
prior tracked presentation. World scale and user offsets remain unchanged.

Offline tests passed: settling/equip/regrab/identity changes, repeated eye reads,
nonfinite rejection, camera-motion invariance, independent gun/wrist translation,
H3 consumed-context/effect idempotence, CE first-person regressions plus animation
transport, weapon alignment, and H2 left-hand suite(1005 checks). Linux runs of
existing IK code use a temporary std::isfinite compatibility include; no IK source
change. Windows CI and headset validation pending. Do not advance release pointer.
User says not to monitor CI. Ultimate Firefight HUD remains deferred.

## Latest: accepted H3 flash, rejected motion; consumed-context fix

User source57eaa410 log (Steam, Halo3) confirms correct muzzle flashes but no
secondary whole-hand animation. Preserve the accepted effect correction. Log
has17970 motion captures,1 reference reset,0 capture refusals,17786 effect
applications and0 effect refusals; captures do NOT prove visual application.

Concrete code defect: FpVisiblePaletteHook copies then invalidates the live
interpolation receipt before solving. ApplyH3SecondaryMotion incorrectly checks
that invalidated receipt, so rejects the visible solve. Disable rejected motion
in8b7094f1, then supply an invocation-scoped pointer to the validated local
receipt. Never rearm the live receipt. Nested scopes restore previous context;
primary invocations cannot borrow secondary motion. Add applied/skipped counts.
Tests reproduce receipt consumption and scope cleanup, and preserve existing
owner/generation/time and effect-idempotence checks. Headset test pending.

User also reports missing recoil/idle/reload whole-hand motion in CE/CEA and H2
Classic/Anniversary, including H2 single-wield right hand. This log contains only
Halo3. Source audit: CE BuildGripCarriers/BuildTrackedFirstPersonPalette derive
mounts from the current animated gun/wrist and overwrite wrist positions. H2
final-packet ownership similarly anchors current authored roots to controller
carriers (both graphics paths). These can cancel rigid animation; this is
separate from the H3 consumed-receipt defect. Do not transplant the H3 local-node
reference into world-space CE/H2 matrices: it could retain camera movement.
Need title-specific runtime captures/weapon and renderer comparison before
claiming a CE/H2 correction. No CE/H2 behavior changed in this candidate.
User says not to monitor CI; push the requested candidate and provide its link.

## Latest: H3 secondary motion and effect correction candidate (2026-10-26)

User requests a build containing actual animation and muzzle-flash corrections,
with Ultimate Firefight HUD explicitly deferred. This candidate changes behavior,
not only diagnostics. Accepted release pointer remains unchanged.

The secondary wrist inverse cancels authored whole-weapon motion. Preserve the
relative authored wrist motion against a per-equipped-weapon reference in the
shared target used by rendering and effects. Owner/generation/count/wrist changes
and a capture gap reset the reference. This is a first-valid-pose reference, not
an identified native idle pose; pickup/reload/idle transitions require headset
validation, particularly alignment after the initial equip animation finishes.
No new engine address or hook is introduced; other titles are gated out.

SMG/Plasma Rifle effects now share the existing 30-degree/.06-metre visual
correction. Select effect nodes from the native render-model remap and rebuild
from the untouched authored palette, so a second effect path cannot accumulate
the same correction. Preserve the existing visual constants and other models.
Counters are reported off the render path. No HUD layout changes.

Validation: portable production-inl tests and existing weapon alignment tests
pass with g++; the motion/effect harness also passes when substituted with the
actual game.cpp matrix composition/inversion implementations. Coverage includes
motion, stale/owner/generation rejection, both corrected models, repeated effect
calls, untouched nodes and nonfinite input rollback. Windows CI and headset
validation remain pending. Test SMG, Plasma Rifle and Spiker in each hand:
idle, fire, reload, drop/regrab, plus ordinary single-wield behavior.

## Latest: diagnostic compilation declaration fix

User-provided logs109063516715 identify MSVC C3861 at
halo3_dual_animation_probe.inl:25: SafeReadBytes not declared at include site.
Add matching static int forward declaration before the diagnostic include;
implementation and memory-read guards unchanged. New CI source also includes
ece85a85 stock HUD locator restoration. Full Windows build pending.

## Latest: rejected Ultimate Firefight HUD experiment disabled

User reports no HUD size/aspect/curvature response in Ultimate Firefight or
vanilla. Log868b628e proves alternate pair discovery and successful writes but
not visible consumption. No separate stock-map scan appears in this log, so
vanilla regression cause remains unproven. Disable alternate-anchor candidate
and restore accepted original Halo3 adapter (stock anchor, exactly3 records),
retaining candidate dormant per repo rules. This is a rollback for regression
isolation, not a claimed Ultimate Firefight fix. No new CI trigger requested.
Need active native HUD consumer evidence before broader mod-layout changes.

## Latest: percent world scale and native-remap animation probe

User log868b628e has286 valid slot0 SMG0771 samples, zero slot1 samples. Do not
claim this proves no secondary animation: the held-source filter excluded the
secondary. Probe now samples native boneMap[0], bounded by context count and
verified SMG/Spiker/Plasma Rifle model checksum/count, independent of classifier.
World scale UI becomes50..150%,100%=.33 native units/metre, reset to100%.
Underlying per-title config remains native units, bounded .165..495. Existing
in-range saved values preserved; defaults remain100%. No animation/effect fix
claimed. Further headset diagnostic comparison needed.

## Latest: diagnostic video and weapon sample starvation

User supplied 35.78 s video (Drive1yNpxPujFh-9J0KOKh5v8iWbqhIojQq4n),
SMG/Spiker comparison. Previous link was the map. Log800325cc contains mostly
body tag3C82 with held node-1; rate limit is consumed by body submission before
weapon tags0771/1265. Skip no-held-source submissions before the rate limiter
so both slots report actual held source and final weapon palette. This changes
only diagnostics. Flash mismatch remains tied to SMG/Plasma Rifle visual-only
postcomposition correction; no effect fix claimed yet. Video downloaded to
/tmp/dual-video-download; contact sheet /tmp/dual-contact.jpg.

## Latest: H3 dual animation diagnostic candidate

User authorizes diagnostic build for missing secondary recoil/reload whole-gun
motion; adds secondary muzzle flashes displaced right. Add bounded atomic
snapshots at 10 Hz per slot: authored/solved wrists and first classified held
source, plus eye root/final render node0 after visual corrections. Log from
existing report path only, no logging/allocation/locks in palette hook. Role3
is root/final node0, not a muzzle marker. Log timestamps and tag/generation
allow matching both hands. No animation/effect behavior changed in this probe.
Capture SMG and Spiker firing/reloading each hand separately; video optional.

## Latest: Ultimate Firefight HUD alternate resolution candidate

Log proves layout scan rejection (stock anchor safe frames0/0). Supplied map
metadata proves live-scale1600x580 and960x580 records, omitted by old anchor.
Add exact alternate signatures and accept verified2..3 blocks; preserve safety
checks and stock signature. See HALO3-ULTIMATE-HUD-2026-10-25.md. Runtime/headset
validation pending; this is bounded layout support, not universal discovery.

## Latest: persistent World scale per title

User requests World scale per game. Added world_scale to title tunables,
shared config parsing/writing and slider save handling. Active title/renderer
profile is applied to the runtime atomic scale during the existing frame profile
selection. H2 Classic/Anniversary retain separate profiles. Default .33 and
range .05..2 retained. No compilation requested; CI untouched.

## Latest: secondary visual alignment and World scale slider

User requests no compilation. H3/ODST DesiredWristWorld skipped visual trim on
left via else-if; now apply it after the existing left aim mount and capture
neutral basis after that mount. Both muzzle publishers undo visual trim for
slot1 as well, preserving authored ray behavior. H2 packet builder already
trims both carriers in both renderers. CE/Reach/H4 have no native dual wield;
their free support hands retain existing presentation behavior.
Added World scale in Comfort using existing runtime units-per-metre value,
range .05..2, finite setter, session-only (explicit in UI). No CI trigger.
Windows/headset verification pending.

## Latest: retire hidden visual-hand offsets from stored config

User suspects removed Visible hand positioning sliders still affect Halo4.
Log source2661f34b, Steam; no six-value offset dump, so exact session offset is
not proven. Code confirms Halo4ApplyVisualHandOffsets still applies saved
nonzero values after the UI removal. Fast-forwarded remote through e7076e45,
preserving the user's latest H3 lateral change and MSVC parser nesting fix.

Zero all six retired live fields in Clamp and constrain their title-profile
descriptors to [0,0]. Detect stale shared/title values before normalization and
save the cleaned config immediately with a migration log message. Keys remain
recognized, but later config edits cannot reactivate them. Preserve independent
gun offsets and dormant transform code. Windows round-trip test covers upgrade,
reload, title switch, all six fields/all profiles and surviving Halo4 gun trim.
Static checks pass; Windows execution and headset validation pending.

## Latest: independent hand offsets and flat, initially collapsed menu

User requests separate forward/right/up sliders per hand retaining supplied
per-title baselines, removal of Visible hand positioning controls and category
parent headers, collapsed sections at each process start, and H3 secondary SMG/
Plasma Rifle lateral correction +0.02 m. Compilation authorized.

Added persisted left_gun_forward/right/up_m fields to title and per-weapon
profiles, config read/write and CE frozen rig. All title controller paths now
select independent hand values; left lateral values are already signed and are
not mirrored. Existing right config keys retain their values; missing left
profile keys initialize from requested left baselines. Contact reference hashes
include new fields. Old left-only forward trim remains additive.
Removed only visible-hand menu controls, leaving stored runtime calibration
compatible. CategorySection now only filters/scopes controls, no parent header.
Child headers use no DefaultOpen flag; ImGui IniFilename remains null, so open
state lives only within the process. H3 correction replaces +0.04 with +0.02.
Portable CE FP, CE contact and alignment tests pass ASan/UBSan; Windows
round-trip coverage extended for independent defaults and persistence. Full
Windows build and headset/UI validation pending. No accepted pointer advance.

## Latest: collapsible menu sections; compilation requested

User asks each settings section to open/close like Quality of Life and authorizes
compilation of accumulated weapons-alignment changes. Added independent
CollapsingHeader groups for existing section headings, Virtual Stock, tracking
calibration, telemetry and the experimental lab. Category scopes remain stable;
individual pages start open, QoL category groups start closed. Collapsing does
not change configuration or disable features. Existing widget callbacks and
save handling retained. Static brace/widget-call checks pass; Windows build
and UI interaction validation pending. Pin CI to this source after committing.

## Latest: per-title zero-labelled gun offsets and H2 Classic yaw

User requests source commit only, no build. Added calibrated right/left base
positions for all seven title profiles in common/weapon_alignment.h, with the
left values already signed. CE/Reach left baselines mirror right lateral because
no separate left values were supplied. Existing absolute config/weapon-profile
values are preserved; missing title fields start at the new right baselines.
Menu sliders display deltas from those bases; Reset gun offsets restores zero.
Left carriers use left base plus shared deltas (only lateral delta mirrored),
without changing right carriers. Existing left-forward additive trim remains.
H2 Classic visual yaw adds -1 degree beside -9.5 pitch, using the same neutral
muzzle-frame separation. Tests extended for every base, signed left deltas and
new-config defaults; not compiled/run at user request. CI workflow untouched.

## Latest: H3 secondary lateral calibration corrected by user

User reports -0.4 m was excessive and moved in the wrong direction. Disable
that displacement in its own rollback commit, then replace it with +0.04 m
relative to the original position for secondary SMG and Plasma Rifle. Keep
+30 degree yaw, model/slot guards, H2 pitch and one-time offset reset unchanged.
No cumulative displacement. Headset verification pending.

## October 24 latest: user accepted alignment build; follow-up calibration

User confirms build/test success and correct secondary H3 SMG yaw, but requests
-0.4 m lateral displacement, plus +30 yaw/-0.4 m for secondary Plasma Rifle.
Extend the existing scoped palette correction using H3 catalog checksum
16120C11/count7 (SMG remains171C0B10/count4), world-unit conversion and hand-frame
lateral axis. Primary weapons, hands and other models unchanged.
H2 Classic fixed visual pitch becomes -9.5 degrees. Left-only forward default
becomes zero; missing left_hand_forward_reset_version triggers a one-time reset
of shared and title profiles, saved immediately. Subsequent player edits persist.
Config round-trip regression covers legacy upgrade and subsequent edits.
Windows build/headset validation of these follow-ups pending.

## October 24 latest: Windows test failures from run40129474979

User supplied the complete Actions log. Windows compilation succeeded; CTest
failed 2/201 tests (one other test skipped). CE muzzle runtime fixture inherited
an incomplete controller frame, newly required by visual/neutral publication.
Initialize its camera, matching reference, scale and primary pose explicitly.
Committed aim concurrency test could attempt its last read during a write,
then observe completion and exit without a stable read. Observe completion
before each attempt, ensuring a final read after the writer completes.
No production behavior changed and no test disabled. Portable CE frame setup
check plus existing alignment, muzzle (615 checks) and CE FP suites passed.
Full Windows test rerun pending; accepted release pointer unchanged.

## October 24 latest: weapons-alignment packaging gate repair

User reports failed Actions run40128327607 (source a59b39c8). Public job page
shows package step failed in one second; detailed logs/API inaccessible here.
Deterministic local finding: C-H2-88 package gate still requires the retired
H2 Classic pitch/yaw labels and therefore rejects this source before CMake.
Remove those two obsolete checks while preserving the rest of C-H2-88, and
replace the old -9.5/+1 defaults gate with the active -11.5 correction, neutral
barrel publication, absent legacy sliders and registered alignment test checks.
Runtime behavior unchanged. Re-pin branch CI to repaired source. Windows
compilation still pending; do not claim that inaccessible run's exact log text.

## October 24 latest: weapons-alignment branch, three requested changes

User ended mod-weapon fixes and switched work to weapons-alignment. Preserve
all accepted fixes and disabled Cursed inference. User requests: H2 Classic
fixed visual -11.5 pitch without barrel feedback, H3 dual secondary SMG +30 yaw,
shared gun forward/right/up offsets on both hands with mirrored left lateral.
Clarification: visual alignment/barrel separation must apply to ALL games.

Implemented title-local neutral/visual carrier separation in CE/H2/H3/ODST/
Reach/H4 muzzle publication. Authored marker transform and ownership receipts
remain; visual mesh/effect rendering unchanged except requested H2 correction
and SMG yaw. Removed H2-specific menu sliders; legacy config fields retained
but no longer drive the Classic model. Shared visual slider zero adds no trim
to the fixed -11.5 baseline. Left-only forward slider remains additive.
H3 SMG tweak is slot1, verified model checksum171C0B10/count4, right-handed
presentation only; hands/primary/other models unchanged.

Portable alignment, 615 authored-muzzle/lifetime checks and CE FP suite pass
ASan/UBSan. Updated Windows publisher fixtures and CMake test registration;
full Windows build/headset validation pending. GitHub API read returned Forbidden,
so do not claim a successful Actions run. Deliver source + branch-specific pinned
workflow. No install, release, PR or accepted release pointer advance.
See WEAPONS-ALIGNMENT-2026-10-24.md for scope and testing.

## October 24 latest: Cursed candidate disabled at user request

User reports 74406fd4 removes 2D but places the Cursed objects far from hands;
asks to remove latest 55dec9d8. That commit only pins CI, so disable the actual
74406fd4 inferred sibling-root behavior as well. Keep failed implementation
dormant per AGENTS.md; preserve accepted Mercury/XAM/ODST/dual shake changes.
Restore accepted CE tests and assert all five Cursed fixtures reject again.
No further Cursed correction requested. Pin CI to this rollback source.
No history rewrite, installation, PR or accepted release pointer change.

## October 24 latest: Mercury accepted; Cursed Halo generic sibling-root candidate

User confirms Mercury Rising fix worked. Cursed Halo sword/crossbow/bow/potions
remain 2D. New 4b650bfe log shows stage11/count38 for five graph datums; supplied
b30.map proves all share full CE body37 plus a root-child frame bow (including
sword/potions). Generic fallback identifies the unique non-body root child only
with complete named arm/finger chains and exact disjoint body/weapon coverage.
Known bindings retain prior behavior. No per-weapon names/hashes added.
CE tests including actual five graph fixtures and malformed/ambiguous variants
pass ASan/UBSan. Windows/headset pending. Preserve all accepted previous fixes;
MP5/MP7 axes still deferred. See CE-CURSED-2026-10-24.md and graph JSON. Source
and pinned CI only; no install, release pointer change or PR.

## October 24 latest: Mercury Rising sibling-gun candidate

User accepts XAM launchers; MP5/MP7 orientation explicitly deferred. Preserve
accepted ODST dual wield. Mercury Rising SMG/excavator/sniper reported 2D.
New eceed9ed log shows stage11 binding refusal for hose/sniper. Supplied map
shows all three have frame gun parent0 beside arms, not below right wrist.
Add unique root-child frame gun topology without per-asset hashes; retain
ambiguity, overlap and existing graph validation. Production gunMask carry
already independent; update standalone relocation to include gunMask too.
Actual-map fixtures plus full CE tests pass ASan/UBSan. Windows/headset pending.
See CE-MERCURY-2026-10-24.md and CE-MERCURY-GRAPHS.json. No title-other edits,
installation, release pointer update or PR. Source and pinned CI delivery.

## October 24 latest: ODST secondary accepted; CE hidden-left launcher candidate

User confirms ODST dual-wield fix works. Preserve 1ab75005 and all other title
fixes. CE source 41b14137 failed: standard AR normal, Stinger/RPG head-locked,
MP5 backward and MP7 sideways-left. Disabled failed marker-axis override in
its own commit; implementation retained dormant. No release-pointer advance.
Log proves both launchers reject stage 3 with exactly the left arm/finger mask
F81F01F2A invalid and zero left segment lengths. New narrow candidate permits
right-only staging ONLY for exact launcher graph identity/count/roles and
finite zero-scale left nodes, with all remaining matrices valid. Hidden left
nodes preserved; right IK, gun parts and floating hands retained. Zero scale
was not recorded by the old probe: this candidate adds that mask if it still
refuses. No claim launcher fix is headset verified. MP5/MP7 orientation remains
unresolved; do not re-enable marker override or guess inverse rotations.
CE portable tests pass ASan/UBSan; Windows build and headset check pending.
See CE-XAM-HIDDEN-LEFT-2026-10-24.md. Source/pinned CI only; no install or PR.

## October 24 latest: XAM muzzle-axis candidate; launchers still unresolved

2935040d user result: MP5 tracks but sideways, Uzi/MP7 points up, RPG/Stinger
still 2D, last M4-like gun normal. Actual runtime identities show the UI Uzi is
HL2 SMG, not the asset named uzi. Raw root axes already match aim. Map trigger
markers prove different root-local firing axes for HL1 MP5/HL2 SMG; M4A1 identity.
Candidate uses these two exact graph/count/root marker rotations for wrist
carriers; unchanged other graphs. CE-XAM-MUZZLE-2026-10-24.md records scope/limits.
Launchers NOT fixed: added active-graph refusal stage, invalid-node bits and arm
lengths to cold log. Need several seconds on each launcher in next test. CE tests
pass ASan/UBSan including rotated muzzle-axis cases and real HL2 graph. Windows
build/headset verification pending. All prior title fixes preserved; ODST left
secondary 1ab75005 still untested. No installation, release, PR or pointer update.

## October 24 latest: CE XAM graph correction; Uzi diagnostic pending

User defers ODST secondary test and prioritizes CE XAM. Supplied a30.map now
available at out/ce-xam/download via Drive. RPG/Stinger reject a detached named
left-upperarm root; HL1 MP5 rejects mp5_master gun-root spelling. Corrected these
specific graph cases, retained existing topology guards and added actual-map
fixtures. CE suite passes ASan/UBSan with documented Linux compatibility flags.
Uzi orientation is NOT solved: graph/model evidence does not justify a fixed
rotation. New cold log snapshot compares native/staged gun axes to controller
axes. Need new headset log after several seconds each on Uzi and normal gun.
See CE-XAM-WEAPONS-2026-10-24.md. Preserve all previous title fixes; ODST left-hand
candidate 1ab75005 remains pending user test. No install/release/PR/pointer change.

## October 23 latest: New Covenant primary accepted; secondary partial-map fix

User confirms e3b5924f works except dual left plasma pistol remains head-locked.
Native log proves the Elite secondary palette emits only left-arm/finger outputs;
all other body entries are -1. Full-map admission rejects it. Added a strictly
slot-1/exact-checksum/exact-mask exception using the same Elite source topology
and accepted dual left-hand carrier. Primary and human/Mythic paths preserved.
See ODST-ELITE-SECONDARY-2026-10-23.md. 25,596 portable checks pass ASan/UBSan;
Windows/headset validation pending. Next test: left plasma pistol and another
dual gun, acquire/drop, reload, and primary regression. No install/release/PR
or accepted-release pointer update; source delivery and pinned CI only.

## October 23 latest: Mythic accepted; New Covenant Elite candidate

User confirms bdad3ab1 works perfectly for all tested Mythic weapons. Preserve
that human mapping/alignment and dual shake. New Covenant (not Prophet's Hand)
uses a confirmed 31-node Elite body (10000E0B): short graphs fail the old 39-node
gate; longer graphs put gun sources into human finger masks. Added an isolated
Elite identity/remap resolver and pending short-graph observation path, reusing
the already accepted identity/epoch receipt. Mythic resolver unchanged.
See ODST-NEW-COVENANT-ELITE-2026-10-23.md. 25,452 asset/helper checks and 1,475
dual-shake checks pass; Windows/headset validation pending. User compiles/tests
from modded-weapons-fix. Ask for New Covenant and Mythic/original-ODST regression
results. No install, release, PR or accepted-release pointer advance.

## October 23 latest: preserve corrected hands; Mythic alignment refinement

User confirms e6edf4ad removes hand/finger/weapon deformation. Gun seating is
still sideways/backward and hands tilt down; Automag upper-part loss remains
unconfirmed in dark photos. Preserve corrected body roles and dual reload.
New candidate reuses the observed Mythic layout before interpolation alignment,
resets wrong-wrist filter history on first recognition, avoids double body-palette
calibration/marker transforms and preserves final gun-component carry. Identity,
generation and epoch changes discard the per-slot reuse hint. The failed union
cache stays disabled; normal/unrecognized layouts retain baseline control.
See ODST-MYTHIC-ALIGNMENT-2026-10-23.md. 25,006 portable checks pass ASan/UBSan;
Windows/headset validation pending. Source and pinned CI delivery only; no game
installation, release or accepted-pointer advance. Ask for gun seating/top-part
and normal-weapon regression results after this build.

## October 23 latest: native Mythic maps confirmed, direct correction candidate

552124dd log confirms early-camera body [0,2,...,37], gun [38,...,count-1]
for affected Mythic weapons; reload menu now confirmed present. Added scoped
body-palette correction using the exact checksum and complete observed map.
Retains baseline for other layouts; learned-palette candidate remains disabled.
Corrected context survives for the gun palette; markers rebuild from the saved
untouched graph. See ODST-MYTHIC-DIRECT-MAP-2026-10-23.md. 24,845 portable checks
pass; Windows/headset testing pending. Preserve accepted dual shake behavior.
Other mods remain deferred, no installation/release/accepted-pointer update.

## October 23 latest log: older DLL loaded, diagnostic build not tested

The user reports continued deformation and missing dual reload menu. Supplied
HaloMCCVR.log identifies source 5fdf982e979cc4dab693bbaccfb941d2bc199307,
compiled Oct 23 01:45:47, before dual reload and the passive ODST probe. There
are no baseline map probe records. This is not a headset failure of fa29efe0.
The current menu contains the exact requested label and description next to
Manual Reload. No gameplay/menu behavior was removed or changed this turn.
Workflow now checks those source features and the restored ODST probe, and
uploads BUILD-IDENTITY.txt with the exact expected log source. Its reviewed
source pin is 552124dd (includes fa29efe0 and accepted dual reload).
User must test that build and return a fresh log before native-map diagnosis
can continue. Existing five-weapon deformation remains unresolved.

## October 23 latest: dual shake accepted; ODST restored for Mythic diagnosis

User confirms dual shake now works in all requested games. Preserve 444f7a15.
Both supplied ODST logs reject 62ef856f: all guns remain head-locked, accepted
mapped palettes zero. User requests the pre-ODST 5fdf982e behavior while keeping
new dual reload. Separate rollback disables the mapped candidate, retaining it
inert. The active solver is back to the original count-based implementation.

Reinspection of sc100 confirms all five reported Mythic weapons have camera at
1 and wrists 6/7; original solver assumes last camera and wrists 5/6. The failed
cache never completes despite observing both body and gun; actual native maps
are missing from old logs. Added a passive bounded map probe, not a new pose
fix. Normal guns should recover baseline tracking; affected guns are still
expected to deform. See ODST-MYTHIC-BASELINE-PROBE-2026-10-23.md.

Portable production-probe tests pass with ASan/UBSan; retained dual tests pass
1,350 + 125 checks. Windows compilation/headset test pending. User compiles
source on modded-weapons-fix. Request a short Mythic session with one normal gun
then Automag and one other affected gun, plus new log. Other mods deferred.
No release, installation, launch, PR or accepted-pointer advance.

## October 23 dual shake reload: Classic/H3 failure correction candidate

User confirms H2 Anniversary works, but H2 Classic and H3 do not (source
7c05a6a6, supplied HaloMCCVRDUALRELOAD.log). Classic native ticks continue with
no requests; H3's weapon callback stalls for seconds, expiring owner receipts.
Retain the Anniversary acceptance as scoped only. Separate rollback c162db87
disables the failed H3 weapon-callback invocation without removing its code.

Replacement uses OpenXR pose timestamps for shake derivatives instead of coarse
GetTickCount64 (reproduced repeated-timestamp reset bug), and H3's existing native
unit-action update for idle owner receipts/consumption. Native reload requests
and H2 simulation route remain unchanged. New per-title input counters separate
tracking, owner, action and gesture/queue failures in the next user log.
See DUAL-SHAKE-CLOCK-AND-IDLE-2026-10-23.md. 1,350 gesture/channel and 125 production
H3 idle-routing checks pass with ASan/UBSan; pinned native verifier also passes.
Full Windows build and headset acceptance pending; user compiles. Preserve ODST
62ef856f and all prior Reach/H3 modded-weapon changes. Source commit/push only;
no installation, launch, release, PR or accepted-pointer update.

## October 23 ODST Mythic/previous-mod regression: replacement source candidate

The user reports all weapons head-locked after the prior ODST mapped fix and
reconfirms this in New Covenant. Their log identifies f7a0778d, before dual-shake.
Rollback commit 05d53019 separately disables the failed candidate, retaining code.
The replacement restores mapped admission with a newly evidenced Mythic human
body, camera inference from complete body+gun maps, and reusable interpolation
receipts across both palette calls. Automag's actual graph has camera at 1,
wrists 6/7, fingers 8–37 and gun 38–42; count-based assumptions explain its prior
corruption. New Covenant remains in regression fixtures, not excluded.

See ODST-MYTHIC-REGRESSION-2026-10-23.md for evidence, limitations and test steps.
24,186 ODST checks across 202 map-derived combinations and H3/Reach regressions
(828/2,793) pass with ASan/UBSan. Full Windows build and headset tests pending;
user compiles. No DLL/package produced. Cold-worker diagnostics now distinguish
missing receipt, stale pair, unreadable map, incomplete evidence and acceptance.
Preserve dual-shake and prior Reach/H3 behavior; do not advance accepted pointer.
Delivery is commit/push to modded-weapons-fix, without install/launch/release/PR.

## October 23 dual wield shake reload: source candidate ready for Windows test

User supplied all four requested kit/retail binaries. The H2/H3 request paths
are now verified and wired: each controller shake requests the native reload of
its own currently owned dual weapon on the simulation thread. Menu/config option
`dual wield shake reload` defaults off and works independently of Manual Reload.
Reuse Needler detector per logical hand, including handedness and settle/rearm.

See `DUAL-WIELD-SHAKE-RELOAD-2026-10-23.md` for own-title kit/retail evidence,
thread handoff, hook dependencies, tests, binary identities and headset steps.
1,335 portable gesture/channel checks and the four-binary verifier pass; retained
ODST/H3/Reach fixtures pass (4,742/828/2,793). Windows native-detour tests updated
but not run here. Full Windows DLL build and headset acceptance remain pending.
The user compiles; source delivery is commit/push to modded-weapons-fix. No game
writes, install, launch, release, PR or accepted-pointer changes. Prior ODST
source 89ab89c4 remains included and awaits user testing.

## October 23 ODST New Covenant: mapped body/held palette candidate

User confirms Halo 3's fix makes all previously invisible guns visible and
reports everything working; no further H3 log supplied. Preserve H3 and Reach.
New ODST report/log/map show Elite 31-node rigs rejected below 39 combined
nodes or corrupted by the old 37-node human digit masks above that threshold.
The candidate learns the native body remap, freezes it per stereo pair, and
selects each gun's explicit native outputs for carry/visibility. Four body
identities are evidenced in the supplied ODST map; no weapon allowlist or new
native binding. See `ODST-MODDED-WEAPONS-2026-10-23.md` for evidence and bounds.

4,742 ODST production-fixture checks, 828 H3 and 2,793 Reach checks pass with
ASan/UBSan. Windows DLL build and headset acceptance pending; user compiles.
Source delivery remains commit/push to modded-weapons-fix. Existing CI is pinned
to 5fdf982e and does not auto-build source changes. No game/map writes, launch,
installation, publication or accepted-pointer update. The helper remains
bounded to evidenced rigs, 64 sources and camera-last graphs; do not describe
this as verified for arbitrary custom rigs before user testing.

## October 22 Halo 3 modded weapons: detached gun candidate

User confirms Reach works across all mods they tested. Preserve that correction;
this is scoped user evidence, not all-mod/all-title release acceptance. The new
Halo 3 Ultimate Firefight report has tracked hands but invisible Bulldog/Ravager;
Skewer is suspected. The supplied firefight_shrine map confirms all three gun
roots are parented to base, outside both wrist subtrees. H3 floating-hands
visibility collapses those sources, and default arm IK does not carry them.

Candidate adds H3-only native held-model output selection before reconstruction,
using the existing validated remap classifier; those outputs participate in IK,
scaling and visibility. No ODST or Reach runtime changes. See
`HALO3-MODDED-WEAPONS-2026-10-22.md` and its hash-pinned asset JSON/inspector.
828 focused H3 checks and 2,793 Reach checks pass with ASan/UBSan. Full Windows
build/headset confirmation pending; user compiles. The existing CI workflow is
still pinned to f70f3b93 and will not build the new source without updating its
pin/trigger; do not mistake that old DLL for this candidate. Source delivery on
modded-weapons-fix follows the user's established commit/push workflow. No game
writes, installation, launch, accepted-pointer update or publication requested.

## October 22 Reach modded weapons: generic mapping candidate

Latest user request: commit and push the generic correction to
`modded-weapons-fix`; the user will compile it on Windows for testing. Branch `modded-weapons-fix`, baseline HEAD `6919a53a`.
Local source changes implement verified-body remapping and explicit per-model
held-output selection; no weapon allowlist and no new native bindings. The
initial body-complement WIP was replaced because it misclassified extra fingers
and IK nodes. See `REACH-MODDED-WEAPONS-IMPLEMENTATION-2026-10-22.md` for precise
scope, commands and limitations, and the analysis/evidence JSON alongside it.

Offline validation: 2,793 focused production/helper checks with ASan/UBSan and
40 prior layout/pair/palette checks pass. CMake Release configuration cannot run
here: the Linux environment lacks Visual Studio 2022. Full DLL build, cumulative
CTest, PowerShell Reach gate, package and headset/Halo 3 regression results are
outstanding. Source delivery by commit/push is now explicitly authorized. No
installable DLL/build ZIP, installation, launch or accepted-pointer update.
The user will perform Windows compilation and headset testing. Precision SMG's
identity as the user's plain "SMG" remains qualified. Do not claim universal
custom-arm-rig compatibility or in-game acceptance from offline tests.

