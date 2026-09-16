# MCCVR ? World Contact & Aim Update (ad7fbf5)

This is the independently maintained MCCVR release from **moistman42069/MCCVR-Halo-Build**. It continues the original Halo-MCC-VR project by pancreations; original history, contributor credit and the MIT license are preserved. Reports and future downloads for this continuation belong in this repository.

**Status:** alpha prerelease. The maintainer headset-confirmed `ad7fbf5` on September 5, 2026, accepting its hand/weapon world collision and current swing-triggered melee across the supported titles. This is not a claim of complete gameplay or hardware compatibility. Validation recorded Steam, Quest 3, SteamVR/OpenXR 2.17.8 and 120 Hz. Microsoft Store/Xbox app remains supported by the same build; that acceptance run was on Steam.

**Important known issue:** Halo 2 AI perception/aim can malfunction with the mod enabled in both Classic and Anniversary. A correction is being developed, but is **not in this release**.

## Downloads and exact identity

- **Play the mod:** `HaloMCCVR-ad7fbf5-Contact-and-Aim-Refinements-Test.zip`.
- **Matching source:** `HaloMCCVR-ad7fbf5-Contact-and-Aim-Source.zip`.
- **Instructions and changes:** `RELEASE-NOTES-ad7fbf5.md` (this document).
- **Verify downloads:** `SHA256SUMS-ad7fbf5.txt`.

These are the original, unchanged build and source ZIPs. Source commit: [`ad7fbf54e553db7ec4831f369f25088fd1a097c7`](https://github.com/moistman42069/MCCVR-Halo-Build/commit/ad7fbf54e553db7ec4831f369f25088fd1a097c7). The release tag points to that exact commit. The default branch also contains newer release documentation; use the tagged source to inspect this binary.

The original ZIP names and embedded manifest retain their pre-test ?Test?/?UNTESTED? wording. Headset acceptance occurred afterward. The bundled manual contains historical instructions and feature lists; **these release notes are the current instructions for this release**, especially for supported titles, configuration preservation and experimental features.

## Supported games and carried-forward features

| Title | This build |
| --- | --- |
| Halo 2 Classic and Anniversary | Stereo VR, head/controller tracking, hands, primary weapon aiming, hand/weapon world collision and swing-triggered melee; AI and dual-wield limitations below |
| Halo 3 | Existing VR support, hands/weapons, world collision, swing-triggered melee and first-person vehicles |
| Halo 3: ODST | Existing VR support, hands/weapons, corrected world contact, swing-triggered melee and first-person vehicles |
| Halo: Reach | Existing VR support, hands/weapons, refined world contact, swing-triggered melee, on-foot aiming refinement and first-person vehicles |
| Halo 4 | Carried-forward experimental VR support, hands/weapons, HUD/reticle controls, world collision and swing-triggered melee |
| Halo: Combat Evolved | **Not supported by this package** |

The cumulative build includes per-eye stereo, 6DOF head tracking, motion-controller input and aiming, turning/comfort options, two-handed aiming, a shared F1 menu, picture and HUD controls, and title-specific crosshair/vehicle behavior. Feature completeness varies by game; a shared menu option is not proof every title implements it identically.

## Fixes and additions in this contact update

- **Halo 2 weapon collision:** corrected the render-model compression header using a read-only inspection of the loaded battle rifle. The old reader published no authored bounds; this generic format fix makes actual weapon bounds available rather than relying on fallback gun points. Collision reseeds when the held weapon/unit changes, including swaps with equal sample counts.
- **Reach contact responsiveness:** moved collision scheduling to its independently verified native central vector-test path. The previous narrower callback could leave long gaps between contact updates.
- **Reach sliding stability:** queries use the raw desired hand/weapon pose by undoing the exact correction applied to that stereo pair. This removes corrected-pose feedback that could repeatedly release and reacquire contact. Further smoothing is still planned.
- **Reach on-foot aiming:** the native firing helper can use a fresh controller-origin ray for the exact local unit, clipped against the world from the stock safe origin. This addresses head-to-reticle parallax when holding a gun to the side. Some weapon types select a different origin afterward; see known issues.
- **Melee diagnostics:** adds per-title counts for valid/absent velocity, peak swing speed, threshold crossings, native input pulses and cooldown blocks. These help distinguish tracking/input failures from native targeting failures.
- **Melee default:** retained at **5.00 m/s**; corrected the stale F1 help text. Saved custom settings are preserved.
- **Resolution feedback:** F1 now distinguishes the resolution running in the current session from the resolution saved for the next launch.
- **Earlier cumulative fixes retained:** ODST hand/world collision, mapped weapon-bound publication in Halo 3/ODST/Reach, Halo 2 tag-base decoding, and the pose-derived velocity fallback for runtimes that provide no meaningful native controller velocity. Halo 4's accepted collision behavior is carried forward.

## Install or upgrade manually

1. Close MCC. Back up your existing mod folder and `halomccvr.cfg` if upgrading.
2. Find the MCC installation folder containing the `MCC` directory. On Steam use **Manage ? Browse local files**. In the Xbox app use **Manage ? Files ? Browse**; the appropriate root is normally the game's `Content` folder.
3. Create/open **`Halo_MCC_VR`** inside that root. Extract the build ZIP there. The active files are **`HaloMCCVR.dll`**, **`HaloMCCVRLauncher.exe`** and **`halomccvr.cfg`**. Source ZIP contents do not belong in the game folder.
4. On a fresh install use the included config. On an upgrade **keep your existing config** and adjust new options in F1. Do not overwrite your tuning just because the historical bundled manual says to replace all three files.
5. Start your VR connection and SteamVR, with SteamVR selected as the active OpenXR runtime. Start Steam for the Steam edition, or be signed into the Xbox app for the Store edition. Run **`HaloMCCVRLauncher.exe`** from `Halo_MCC_VR` with MCC closed and use the anti-cheat-disabled launch path. Do not use the mod in anti-cheat-enabled matchmaking.
6. When upgrading from old `halo3xr`-named builds, keep backups outside the active mod folder and use only the new launcher/DLL pair. Do not run the old launcher or inject twice. Do not rename MCC's executable, including the Microsoft Store executable.

Publishing this release does not install anything automatically. For a rollback, close MCC and restore your backed-up mod files/config yourself.

## How to use the features

### Hand and weapon world collision

Press **F1 ? Body & Hands ? World collision (experimental)**. It is **off by default**. Hands and the held weapon stop against supported world geometry and give gentle contact haptics. The Halo 2 bounds fix and Reach contact refinements apply automatically with this feature enabled; there is no extra per-weapon registration switch.

### Physical melee in this version

In **F1 ? Body & Hands**, enable **World collision**, then **Physical melee (experimental)** beneath it. Both ship disabled. **This version requires world collision to remain enabled for physical melee.** Independent toggles are future work.

?Required swing speed? ranges from **0.30 to 5.00 m/s**, with **5.00 m/s as the default**. A qualifying tracked swing requests Halo's normal melee action. Lower numbers trigger more easily; five requires a fast swing. This is a speed threshold, not damage strength. Normal controller melee remains available.

The game still decides range, target, animation and damage. This version does **not** apply damage at the exact point where a hand touches an NPC, and alternating punches are not independent native damage transactions. Leave ordinary room-scale clearance around you when testing swings.

### Two-handed aiming and weapon placement

Open **F1 ? Weapon & Aim ? Two-handed aiming**. Choose **Toggle (click grip)** or **Hold grip**. Bring the left hand to the front of the gun and click/hold the **left grip** on its grab line. The support-hand and grab-zone offsets tune engagement. Release/toggle off to return to one-handed aiming.

Weapon size, hand size and per-title placement/calibration controls are also in this category. In this release, some weapon rotation controls affect shared aiming calibration as well as presentation; an independent visual-only alignment UI is still being refined. Complete left-handed primary weapons, per-equipped-weapon profiles and independently tracked dual-wield guns are not promised here.

### Reach aiming refinement

It applies automatically during supported on-foot VR aiming. There is no new checkbox. For checking it, hold a magnum to the side and compare the sight line with close and distant impacts. A later native marker-origin selection can still override the adjusted origin for some weapons; report the exact weapon and situation if it misses.

### Resolution and picture settings

Open **F1 ? Picture**. Resolution presets and the scale slider save the **next-launch** size. Compare **Current session** with **Next launch**; fully exit MCC and run the launcher again to apply a saved resolution change. Reopening F1 or reloading a mission does not recreate the OpenXR render targets. This clarification does not establish that MCC's own graphics preset menu is fixed.

### First-person vehicles, HUD and recentering

First-person vehicle support in this package is for **Halo 3, ODST and Reach**. In **F1 ? Vehicles**, sit in the desired seat and adjust **Seat forward**, **Seat height**, and **Seat left/right**. Seated adjustments are saved for that seat; on-foot adjustments change the shared starting trim. H2/H4 first-person vehicles and complete control parity remain future work.

Use the **HUD** and **Crosshair** categories for their supported title-specific controls. Settings save automatically. **L3+R3** recenters VR space and closes the menu; **F1** opens/closes the menu without that recenter action.

## Known issues and coverage limits

### Present in, or still unresolved for, this release

- **Halo 2 AI:** enemy perception/aim regression reported across missions/difficulties in both renderers, absent without the mod. The source investigation found non-owned units could skip native aim updates. The pending fix is not included.
- **Lower-edge/corner visibility:** world geometry may disappear near the bottom of the view when looking up in Halo 3 and Halo 2 Anniversary. A wider visibility guard is under development.
- **Contact jitter/phasing:** collision is headset-accepted, but sustained sliding along rocks, walls, corners and objects can still need smoothing and steadier haptics, particularly Reach. This does not guarantee every mesh/weapon surface is covered.
- **Melee:** swing-triggered native input, not exact hand/gun-to-NPC damage; native target/range restrictions and cooldown still apply. Melee depends on the world-collision toggle. Other-headset responsiveness needs broader validation.
- **Dual wield and handedness:** secondary guns, shot origins, collisions and per-hand ownership do not yet have complete independent controller behavior across titles. Full left-handed primary support is unfinished.
- **Weapon/reticle alignment:** per-title and per-weapon calibration can still diverge. Reach marker-origin barrels may override the on-foot origin refinement; universal close-range barrel/impact agreement is not established.
- **Reach Show body:** the on-foot first-person presentation uses floating hands; the body toggle is not established as complete in Reach. A later tester also reported collision/melee/body options ineffective, requiring version-specific reproduction.
- **Scopes/zoom:** Halo 3's working weapon-side zoom window is not yet matched across every supported title. General scope behavior remains title-dependent.
- **Vehicles:** H2 control parity and H2/H4 first-person vehicles are unfinished. Existing H3/ODST/Reach seats may need individual adjustment.
- **Hardware and modes:** no comprehensive coverage of all headsets, connection methods, multiplayer/co-op, every mission, refresh rate or long session. MCC updates can invalidate native signatures.

### Older or unconfirmed reports retained for follow-up

- Reach doubled grass/visual effects were reported on older `f5b3081`, mission unknown; the maintainer did not reproduce this on the later baseline. **No fix is claimed.**
- An older Reach report described Ultra/lower graphics presets appearing ineffective. Only the mod's resolution UI was clarified; MCC's own preset behavior remains unconfirmed.
- Other headset testers reported missing physical-melee activation on older builds. Their versions and tracking/runtime behavior differ; the new diagnostics and earlier velocity fallback are not proof every headset is fixed.
- Reach graininess/clarity complaints need comparison of actual resolution, runtime/connection and AA/sharpen settings.
- Earlier fork releases listed Halo 4 damage/projectile-effect artifacts, incomplete cinematics and suppressed/misplaced weapon effects. The contact update does not establish all those historical rendering issues resolved.

If doubled effects or unstable rendering cause discomfort, stop that test and include the title, mission and settings when reporting it.

## Future work ? not included in ad7fbf5

Priority is actual hand/held-gun contact damage against the contacted NPC, both hands independently, without controller-button dependence, with title-specific native validation and duplicate-hit/cooldown handling. Also planned: independent melee/collision toggles; smoother sustained world contact; H2 AI and lower-edge visibility fixes; full dual-wield tracking/aim/contact and optional left-handed play; visual-only per-title and optional equipped-weapon alignment; H2 vehicle controls; H2/H4 first-person vehicles; weapon-side zoom parity; and the unresolved Reach rendering/clarity reports. Local work in progress is not packaged here and is not announced as complete.

## Reporting a problem

Use [this repository's issues](https://github.com/moistman42069/MCCVR-Halo-Build/issues). Include **ad7fbf5**, title, mission, weapon(s), Classic/Anniversary when relevant, exact reproduction steps, Steam or Xbox app edition, headset, connection method, OpenXR runtime/version, refresh rate, GPU, resolution scale and the relevant F1 toggles. Attach `HaloMCCVR.log` and `HaloMCCVRLauncher.log` from your mod folder, checking them for personal information first. Say whether the behavior also occurs without the mod. The log's build identity is more useful than just ?latest.?

## Verification and license

Release/core tests, the Reach consistency check and pinned native collision-binding checks passed during preparation. Published ZIP hashes and embedded DLL identity were independently verified; every source ZIP entry matches `git archive` of the tagged commit. No rebuild or unfinished source changes are included.

- Build ZIP SHA-256: `4FE90053205C23F301EDBBA2E470E03C820B85D2F9B04C97EF0FBFF9DAF341FD`
- Source ZIP SHA-256: `CF899F02C032E659660041A1605D81FFD11D0944A875C943B665778BB27C5E2D`
- DLL SHA-256: `18363917C2A503A233C6A1D999F1D53F3C3B74A4000EA5BC0E3912AC0731594C`

The original MIT license and attribution remain included. This unofficial project is not affiliated with Microsoft or Halo Studios and does not distribute MCC game binaries or editing-kit assets.
