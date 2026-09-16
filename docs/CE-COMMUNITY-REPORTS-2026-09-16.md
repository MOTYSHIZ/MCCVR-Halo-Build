# CE community reports and custom-map loading investigation

September 16 update: the user confirms that installing the missing **CE
Multiplayer** content resolves mod loading and requests a new verified ZIP pair.
The earlier hold below is lifted. Read `HALOCE-CURSED-LOADING-2026-09-16.md` for
the same missing-file loop with and without VR and two post-install d7dbfcb logs.
Preserve d7dbfcb runtime, including Original tracking/movement and confirmed
Anniversary behavior. This handoff changes documentation/package metadata only;
remaining headset coverage limits and all prior scope are preserved.

Active user scope (September 16): correct CE Classic VR startup/custom-map
compatibility, Anniversary periodic black flicker and vertical light streaks,
head/controller-relative movement and firing/native VR reticle, player/grenade
and audio orientation, AR firing camera jitter, and difficult physical melee
reach. Preserve all accepted title behavior and both MCC editions. These are
open reports, not findings that every symptom has the same cause.

Halo 3 is the player-experience reference: both eyes render coherently, tracked
aim and head-relative movement work, native reticles follow the weapon, camera
recoil does not shake the headset, and physical melee remains usable. Establish
CE-specific evidence for each native integration; never borrow H3 offsets.

## Preserved evidence

Accepted runtime remains `d47a98c947dc60dd98d7259a29a7582d5f46df7f`.
The two newly supplied logs were found in Downloads and copied unchanged to
`out/test-runs/d47a98c-ce-community-reports-20260916/`:

| File | SHA-256 | Environment |
| --- | --- | --- |
| report13.log | 657659B1E910F8F224DB06E3AFA41622938B7DF733D29F6BA66A42008C6B319C | Steam; SteamVR/OpenXR 2.17.9; Oculus-family; Virtual Desktop; 90 Hz; 2912x2100 launch raster |
| report14.log | EFB2C0288013B3AAFCD777D93617C47A099F3115F33C676C860FBAD160E37DAD | Steam; SteamVR/OpenXR 2.17.10; Oculus-family; Virtual Desktop; 90 Hz; 3786x2730 launch raster |

Both logs identify source d47a98c. Their installed DLL hashes and exact headset
models are not supplied. Do not assign either quoted reporter to a specific
log without corroboration.

Earlier local custom-map attempts remain under
`out/test-runs/d47a98c-ce-custom-campaign-failure/attempt1.log` and
`attempt2.log`. The user confirms Cursed Halo Again and Minecraft 2 load without
VR, but stall at the completed loading bar with music continuing under VR.
Attempt 1 never reaches a CE camera or any recorded render/resolution work;
attempt 2 renders Anniversary gameplay before leaving it. A later expired
camera heartbeat is not evidence of the original load-stall cause.

## Initial verified observations

Report 14 records zero completed Classic pairs, 1,290 dropped Classic attempts,
1,348 Classic outputs, zero source misses and `failure=4`. Current production
code names 4 `PairPreparation`, which includes both view staging and eye-cache
admission. This narrows the failure; it does not yet identify its exact branch.
Anniversary completes many stereo pairs in the same process.

## Delivery and remaining proof

The user explicitly requires ALL reported issues to be addressed before any
new ZIP is packaged. Modded campaigns/Classic entry are the first priority,
not permission for a partial delivery. This current cumulative refinement
scope supersedes the older one-behavior-per-candidate packaging cadence;
keep individual fixes and evidence independently reviewable. Deliver a new
build ZIP and matching source ZIP only after the full fix/verification work.
Do not call local verification headset acceptance.

The user subsequently DEFERS the Game Pass-specific investigation until a
specific log is available: focus on the original CE reports for this package.
Both community logs identify Steam. Existing support for both editions must
remain intact; no new Game Pass defect or runtime coverage is inferred here.

After testing the user intends to replace the current GitHub release asset,
but explicitly says NOT to do that until a later instruction. No GitHub asset
deletion, replacement or publication is authorized now.

Work locally through evidence-backed corrections and appropriate regressions.
Keep distinct behavioral fixes reviewable; do not label a speculative change
as a diagnosed fix. Package matching build/source ZIPs with
`tools/package-candidate.ps1` WITHOUT `-Install`, then wait for headset results.
No installation, game-folder writes, MCC launch, PR or publication is authorized.
Do not advance `docs/CURRENT-STATE.md` until explicit headset acceptance.

The live stalled-thread capture request is pending. No MCC process was running
at the September 16 resume inspection. Diagnostic helpers may be prepared
offline; do not launch the game autonomously. The new logs allow renderer and
controls work to continue independently of that capture.

## Headset acceptance and delivery checklist

- [ ] Classic startup and modded campaigns, including Cursed Halo Again and Minecraft 2.
- [ ] Anniversary intermittent black frames on Halo.
- [ ] Anniversary vertical streaks around lights in Halo's first tunnel.
- [ ] Head-relative movement, controller-directed firing, and native VR reticle, including recovery.
- [ ] Head/body orientation, grenade direction and spatial-audio orientation.
- [ ] AR firing recoil/jitter comfort, with other affected weapons checked.
- [ ] Usable physical melee reach against Grunts and Jackals; preserve native damage and collision guards.
- [x] Preserve existing Steam and Game Pass support; new Game Pass-specific investigation is deferred by the user pending a log.
- [x] Complete local implementations, independent review, cumulative Release, all 35 CTest suites, Reach gate and native evidence checks. Details are in the candidate notes.
- [ ] Final committed build/source archive verification and delivery; exact result is recorded in `out/ce-community-current-handoff.json`, followed by a testing hold.
- [ ] User headset confirmation and required Halo 3 regression after delivery; accepted pointer stays d47a98c meanwhile.

Controls recovery has a concrete compiled retirement defect and regression
correction; read `HALOCE-CONTROLS-RETIREMENT-2026-09-16.md`. Classic output
scaling, cold installation, complete-frame retention, melee reach and the
separate native orientation/comfort features are tracked individually in
`CE-COMMUNITY-CANDIDATE-2026-09-16.md`. Local implementation/verification does
not close the headset acceptance checklist above or prove every report's cause.
