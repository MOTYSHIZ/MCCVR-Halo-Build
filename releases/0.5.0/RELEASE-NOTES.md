# Halo MCC VR Alpha 0.5.0 — Experimental Refinement Update

Alpha 0.5.0 collects recent interaction, aiming, comfort, rendering and vehicle
work into one release for Steam and Microsoft Store / Xbox app editions of
Halo: The Master Chief Collection.

> **Experimental release:** every new feature described below is experimental.
> Some options may behave differently by weapon, mission, vehicle, controller,
> headset or runtime, and some may not work as intended yet. They will be
> refined in future updates from player logs and headset testing. Most new
> options are off by default and fall back to stock behavior when their
> title-specific requirements cannot be verified.

This release was built and packaged from `d088171a4a6f63a22270f20c059bef45727f0dfa`.
It passed the local Release build, all 59 automated test suites, 516 production
vehicle-camera checks, 24 pinned signature checks and the Reach consistency
gate. Local verification is not a substitute for headset acceptance.

## New experimental features and refinements

### First-person vehicles across the full campaign lineup

The existing **Sit in the seat (first person)** toggle now supports Halo CE
Original and Anniversary, Halo 2 Classic and Anniversary, Halo 3, ODST, Reach
and Halo 4. The new CE, H2 and H4 adapters use each engine's own seated camera
and verified local character head marker. Turning the option off restores the
native camera, while entry, exit and special cameras remain native.

Universal vehicle-position sliders apply to the new views. H3, ODST and Reach
retain their established per-seat adjustments. CE steering and seated reticle
handling now work in both first-person and following views. The new CE/H2/H4
views still need broad testing for cockpit clipping, animated-head comfort,
body and weapon visibility, and unusual passenger or turret seats.

### Circular gun-side zoom

Halo 2 Classic/Anniversary, Halo 3, ODST, Reach and Halo 4 now have an optional
1024 × 1024 circular gun-side zoom view. Press the right stick to release zoom
and use right-stick up/down to change magnification. The lens follows weapon
handedness and refreshes every rendered frame for consistent cadence. It adds
an extra world render and can cost GPU performance. Halo CE keeps its native
zoom because a safe separate CE lens path is not complete.

### Manual reload and magazine handling

- A separate **Shortened reload animation** option attempts to retain only the
  final chambering/cocking portion of the native animation after a manual
  insertion. The existing full animation-disable option remains separate and
  takes priority.
- Shortening is implemented for all six titles, with an authored insertion
  marker where usable and a final-quarter fallback where it is not. Individual
  weapons can still fall back to the full native animation.
- Held magazines no longer disappear merely because four seconds elapsed or an
  unrelated trigger/button was pressed. Real weapon changes, tracking loss,
  menu transitions and released grip still cancel safely.
- Magazine grabs and insertion/release both provide haptic feedback.
- Insertion targets follow the visible magazine model and live weapon pose.
- All 42 extracted magazine models have authored textures. Known Halo 4
  Promethean models without usable geometry show an orange interaction token.
- Native ammunition eligibility, reserves, ownership and inventory remain
  controlled by Halo.

### Aiming, alignment and visible hands

- Optional independent dual-wield trajectories are available in Halo 2
  Classic/Anniversary and Halo 3, the MCC titles with normal dual wielding.
  Each hand receives its own native acquisition and downstream homing data.
- An optional all-title barrel-origin mode uses each engine's verified muzzle
  position and direction while retaining native target and projectile rules.
- Optional **per-gun alignment** saves and selects weapon offsets for the
  equipped gun.
- Separate left- and right-hand visual-position sliders move only the visible
  hand meshes. They do not move guns, shots or interaction coordinates.
- Zero-offset, left-handed routing and hidden arm-anchor guards were tightened
  to prevent stretched or displaced arm presentation.

### Controls, menus and comfort

- The VR menu pointer now allows **A** to select the pointed item while it
  suppresses conflicting input. Trigger click/drag remains available, and a
  held A is drained when leaving the menu so it cannot become a gameplay input.
- **Disable flashlight input** now blocks the support-grip button by default
  without disabling hand tracking or two-hand aim. Labels show the equivalent
  Quest control, follow handedness and account for Reach's on-foot swap.
  Custom MCC controller layouts still need the matching selector in F1.
- Optional **automatic smooth turning in vehicles** temporarily replaces
  on-foot snap turning while seated and restores the saved setting on exit.
- Roomscale stopping and reference history were corrected in H2, H3, ODST,
  Reach and H4 so the native body is less likely to slide away after physical
  movement. CE body translation remains disabled.
- Vehicle input was audited across the titles, with targeted H2, CE and H4
  corrections for steering/view references, seated throttle and seat-aware
  input. Existing good title behavior was retained.
- Radial walking deadzone handling preserves direction, and delivered menu or
  thumb-rest modes consume and drain their conflicting inputs cleanly.

### Rendering and title-specific corrections

- A default-off **Disable CE Anniversary lens flares** option suppresses the
  verified native flare draws in VR eyes while preserving world lighting, CE
  Classic and other titles. It is a testable workaround for reported white or
  cyan streaks; effectiveness still needs headset comparison.
- Reach prevents the second eye from advancing the same foliage wind state a
  second time. This targets grass movement that differed between eyes; broader
  foliage causes remain under investigation.
- ODST camera admission was corrected for a report where aiming fell back to
  the right stick. Analogous paths in the other titles were audited.
- HUD hiding covers all seven D3D11 draw variants while keeping native callbacks
  and menus available.
- H2 ownership and fault cleanup, ODST checkpoint seat lifetime, and CE
  controller-directed native targeting received additional guards.

## Retained features

All previous all-campaign stereo/6DOF support, tracked hands and weapons,
left-handed primary routing, manual reload, Needler shake reload, weapon
holsters, world contact, physical and gesture melee, native/title-appropriate
HUD and reticles, spatial audio, cutscene presentation, Force Inject / Recover
VR, snap/smooth turning and CE Original/Anniversary graphics support remain.

## Known limits

- All additions in this release remain experimental and can require further
  title-, weapon-, seat- or controller-specific refinement.
- CE does not yet have the new separate circular scope lens. Its native zoom is
  retained.
- The CE Anniversary flare toggle is a workaround awaiting headset comparison,
  not a confirmed general lighting fix.
- The Reach second-eye wind correction does not prove every foliage mismatch
  fixed.
- H2 Cairo/Outskirts and vehicle-checkpoint crash reports are not conclusively
  reproduced or closed; co-op coverage remains incomplete.
- Campaign switching can occasionally crash MCC or leave the next title flat.
  Fully close MCC, restart through the launcher and load the destination again.
- CE graphics switching during cinematics remains unsupported.
- Contact, custom weapons, unusual seats, every mission, multiplayer, co-op,
  long sessions, hardware/runtime combinations and Store-specific paths have
  not been exhaustively tested.
- H4's reported damage blackout, the minor H2 tank-exit reticle report,
  independent native body-yaw following and CE roomscale body translation are
  still deferred.

## Install or update

For a fresh install, extract `Halo-MCC-VR.zip` and place `HaloMCCVR.dll`,
`HaloMCCVRLauncher.exe`, and `halomccvr.cfg` in a folder named `Halo_MCC_VR` at
the MCC installation root. Start the headset connection/OpenXR runtime, then
launch through `HaloMCCVRLauncher.exe` with anti-cheat disabled.

When updating, close MCC, replace the DLL and launcher, and **keep your existing
`halomccvr.cfg`**. Steam and Microsoft Store installations have separate mod
folders if both are installed. Wait at least seven seconds after entering a
title or level before using Force Inject / Recover VR.

Set MCC's per-title campaign FOV to 120°, V-Sync off, maximum frame rate 120,
and MCC FSR off. Recenter with F3 after entering gameplay.

## Downloads and build identity

**Players:** [Halo-MCC-VR.zip](https://github.com/moistman42069/MCCVR-Halo-Build/releases/download/MCC_VR_ALPHA_0.5.0/Halo-MCC-VR.zip)

**Developers:** [Halo-MCC-VR-Source.zip](https://github.com/moistman42069/MCCVR-Halo-Build/releases/download/MCC_VR_ALPHA_0.5.0/Halo-MCC-VR-Source.zip)

[SHA-256 checksums](https://github.com/moistman42069/MCCVR-Halo-Build/releases/download/MCC_VR_ALPHA_0.5.0/SHA256.txt)

```text
Build source    d088171a4a6f63a22270f20c059bef45727f0dfa
Build ZIP       ACA22D0541068ADD83CFA94CF95E19A44B5BBDB7F0D3A923931E7CA79D9F2EB7
Source ZIP      0528EBD7A9B7224F4554A0EEC9AEFDB13A530D46DE59E4A8BDD432429EA12169
HaloMCCVR.dll   7C73727A11DB07A81082FDFEA0AE70FC0BBEEF548F1769808E2E61DE4180D5C0
Launcher        35D5286506515000D39312F0E24D8A509C9637CD591E8ABA4CD07A6AFCEDD79C
Config          7862C022E649E8C4302002767D5C21304A965FB3F9DF73CF74F2387A583CBE6A
```
