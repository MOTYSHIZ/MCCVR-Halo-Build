# MCCVR Halo Build

An independently maintained continuation of [Halo-MCC-VR by pancreations](https://github.com/pancreations/Halo-MCC-VR), maintained here by **moistman42069**. Future releases and bug reports for this continuation live in this repository. Original contributor credit, history and the MIT license are preserved.

## Latest release: World Contact & Aim Update ? ad7fbf5

[**Download the build and matching source**](https://github.com/moistman42069/MCCVR-Halo-Build/releases/tag/MCCVR-ad7fbf5) ? [**Installation, feature instructions, fixes and known issues**](docs/RELEASE-NOTES-ad7fbf5.md) ? [Report a problem](https://github.com/moistman42069/MCCVR-Halo-Build/issues)

This alpha supports **Halo 2 Classic/Anniversary, Halo 3, ODST, Reach and Halo 4** in MCC, on Steam and Microsoft Store/Xbox app. **Halo: Combat Evolved is not included.**

The maintainer headset-confirmed the contact/current-melee baseline on September 5, 2026 (Quest 3, SteamVR, Steam edition). That acceptance does not establish full hardware, mission or multiplayer coverage.

This cumulative release adds/refines hand and weapon world contact, fixes Halo 2's authored weapon-bound reader, improves Reach collision scheduling/feedback and on-foot aiming, adds melee diagnostics, and clarifies resolution settings. It carries forward tracked hands, controller aiming, two-handed aiming, HUD/crosshair controls and first-person vehicles in Halo 3/ODST/Reach.

**Known limitations:** H2 enemy AI can malfunction; lower-edge visibility needs refinement in H3/H2 Anniversary; sustained collision can still jitter; physical melee sends native melee input rather than direct damage at the contacted NPC. Dual wield, left-handed support, scopes and vehicle behavior are not complete across every title. See the release notes for versioned Reach rendering reports and all current limitations.

## Quick start

1. Download the **build ZIP**, not the source ZIP. Close MCC and back up your existing mod/config.
2. Extract into **`Halo_MCC_VR`** inside the MCC installation root (the folder containing `MCC`; normally `Content` for the Xbox app). Keep your existing `halomccvr.cfg` when upgrading.
3. Start your headset connection and SteamVR with SteamVR as the active OpenXR runtime. Run **`HaloMCCVRLauncher.exe`** with MCC closed, using the anti-cheat-disabled path. Do not use anti-cheat-enabled matchmaking or rename the game's executable.
4. **F1 ? Body & Hands:** enable **World collision**, then optionally **Physical melee**. Both default off. In this release melee requires collision enabled; its default threshold is **5 m/s**, and lower values trigger more easily.
5. **F1 ? Picture:** resolution changes apply after fully restarting MCC through the launcher. Settings save automatically.

The unchanged ZIP contains some historical manual/manifest wording. Follow [the current release instructions](docs/RELEASE-NOTES-ad7fbf5.md) for this build.

## Source and development

The [`MCCVR-ad7fbf5` tag](https://github.com/moistman42069/MCCVR-Halo-Build/tree/MCCVR-ad7fbf5) and the attached source ZIP match the released binary's source commit exactly. The default branch includes later documentation; unfinished gameplay work is not part of this release. See [BUILDING.md](BUILDING.md) for the source build workflow.

Upcoming work includes true hand/gun contact melee, independent feature toggles, H2 AI and visibility fixes, contact smoothing, dual-wield/left-handed consistency, weapon calibration, vehicle and zoom parity, and Reach rendering diagnostics. These are plans, not shipped features.

## Credits and license

Based on Halo-MCC-VR by **pancreations** and its contributors, with subsequent continuation work preserved in Git history. Distributed under the original [MIT license](LICENSE). This unofficial project is not affiliated with Microsoft or Halo Studios. No MCC game binaries or editing-kit assets are included.
