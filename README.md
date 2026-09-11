# MCCVR Halo Build

An independently maintained continuation of [Halo-MCC-VR by pancreations](https://github.com/pancreations/Halo-MCC-VR), maintained here by **moistman42069**. Future releases and bug reports for this continuation live in this repository. Original contributor credit, history and the MIT license are preserved.

## Latest release: Left-Hand Refinements and Experimental Roomscale (cfb22ed)

Experimental prerelease / work in progress. Roomscale body movement is
new, disabled by default, and has passed local build and simulation checks,
but this exact correction still needs headset testing. Keep your previous
working build and configuration backed up.

This cumulative build supports Halo 2 Classic and Anniversary, Halo 3,
Halo 3: ODST, Halo: Reach, and Halo 4 on both Steam and the Microsoft Store /
Xbox app. Halo: Combat Evolved is not supported by this package.

Downloads and exact identity
Play the mod:
HaloMCCVR-cfb22ed-roomscale-left-hand-update-20260911-034759932Z-Build.zip
Matching source:
HaloMCCVR-cfb22ed-roomscale-left-hand-update-20260911-034759932Z-Source.zip
Source commit: cfb22eda5c968fc10b1083b56817a710855bd546
Build ZIP SHA-256:
1BCBC6BBC4EF6D420FCDCB0CBEFEB73908AB30C9842846BB3F1189CC77D9B48F
Source ZIP SHA-256:
1F4980151BB99333E6129E663CE00C440EF284E7FCC3048252DFA6693DD51922
DLL SHA-256:
C48458548CE5EC7C584EB3AB1B38779526A0994667511876AE2C9D04B0D29F7E
Launcher SHA-256:
54A0E91B642BE153767584E916BE4D8AAF7928E46F78FBF46CB19D9E57834771
Included config SHA-256:
E67114F4BD30A06A4750F57EC925D0598775C617454D76DCD57FF3902292AB3B
The tag and matching source archive identify the exact source used for these
binaries. The build ZIP also contains CANDIDATE-MANIFEST.json, the complete
manual, release notes, licenses, and file hashes.

Required MCC settings for every game
Set these before judging image scale, aiming, or performance:

Setting	Required value
Video > Max Frame Rate	120
Video > V-Sync	Off
Halo 2 campaign Field of View	120°
Halo 3 campaign Field of View	120°
Halo 3: ODST campaign Field of View	120°
Halo: Reach campaign Field of View	120°
Halo 4 campaign Field of View	120°
MCC FSR	Off
The FOV setting is stored separately for each title in MCC. Check all five even
if one already reads 120. For ODST, also set Look Sensitivity to maximum and
Look Acceleration off so native aim can catch up with the controller reticle.

What changed since MCCVR-d77c9dd
Experimental roomscale body movement
Adds Roomscale body movement under F1 > Controls. It is off by default.
Converts horizontal physical headset movement into the active title's normal
walking input so Halo remains responsible for ground movement, walls, steps,
and collision.
Covers H2 Classic/Anniversary, H3, ODST, Reach, and H4 through title-specific
on-foot admission checks.
Fixes the first roomscale candidate's H3-only admission mistake and prevents
nested XInput wrappers from reading the mod's own merged stick as a physical
stick and cancelling roomscale movement.
Gives a real movement stick priority, expires stale commands after 100 ms,
resets across recenter/title/tracking changes, and consumes native travel so
the same physical step is not applied twice.
Keeps vertical physical movement as lean/crouch rather than issuing jump.
Preserves controller aiming and head-relative stick walking.
Roomscale remains experimental and WIP. This build does not independently
turn the native character body with physical head rotation, and tracked camera
lean can still pass visually through nearby geometry. Body following suspends
in menus, pause, vehicles/turrets, and whenever current on-foot proof is absent.

Left-handed mode and hand alignment
Restores the released MCCVR-d77c9dd left-handed weapon positioning as the
default after the newer anatomical correction produced poor alignment in a
headset test.
Adds Fix Hand Alignment (Experimental) directly below Left-handed main
weapon in F1 > Weapon & Aim. It appears only in left-handed mode and defaults
off. Turn it off at any time to return to the released placement.
Carries forward the primary/support pose, trigger, grip, velocity, haptic,
melee, and contact routing across supported titles.
The experimental anatomical option is not headset-accepted and can still
misalign hands or arms. Start with it off.

Contact, physical melee, controls, and stability
Preserves the accepted responsive physical-melee baseline and its separate
World collision, Physical melee, and Gesture melee controls.
Keeps the 5.00 m/s physical-melee default and selectable 0.30–10.00 m/s range.
Expands current weapon-contact bounds from the actually equipped render model
in H3, ODST, Reach, and H4, while retaining H2's live-verified authored path.
Invalid or unreadable custom data falls back locally instead of disabling VR.
Carries forward bounded contact-release smoothing, physical hand/weapon
contact, haptics, and broader native damage-target admission.
Restores snap-turn handling across all supported titles, including H2's shared
renderer path. Headset validation of the cumulative behavior is still needed.
Keeps precision arrow buttons beside every F1 slider. An arrow changes the
last displayed digit while normal dragging still works.
Keeps improved hook cleanup and manual Force injection / recover VR
controls. Automatic/manual recovery is still H3-specific and not fully
headset-confirmed across title re-entry.
The failed Halo 3 independent dual-fire experiment is disabled. Dual-wield
presentation work remains, but independent per-gun projectile direction is
unfinished and is not claimed by this release.
Supported-game status
Title	Current coverage and important limits
Halo 2 Classic / Anniversary	Stereo VR, 6DOF, hands/weapons, controller aim, HUD/reticle work, contact, physical melee, snap turn, left-handed routing, and experimental roomscale. Seated controller aim exists, but full H3-style vehicle-control policy and first-person vehicles remain unfinished.
Halo 3	Established VR path, hands/weapons, HUD, first-person vehicles, contact/melee, snap turn, left-handed routing, and experimental roomscale. Independent dual-fire remains disabled after headset failure.
Halo 3: ODST	Stereo VR, hands/weapons, HUD, first-person vehicles, improved weapon contact/melee, snap turn, left-handed routing, and experimental roomscale.
Halo: Reach	Stereo VR, hands/weapons, HUD, first-person vehicles, runtime weapon bounds/contact, snap turn, left-handed routing, and experimental roomscale. Some version/mission-specific effects and clarity reports remain open.
Halo 4	Experimental stereo VR, hands/weapons, HUD/reticle controls, runtime weapon contact/melee, snap turn, left-handed routing, and experimental roomscale. First-person vehicle parity remains unfinished.
Fresh installation
Fully close MCC and extract the Build ZIP somewhere temporary.
Open MCC's installation root, the folder containing MCC\Binaries\Win64:
Steam: Library > Halo: The Master Chief Collection > Manage > Browse local files.
Xbox app: MCC > Manage > Files > Browse, then open Content if necessary.
Create a folder named exactly Halo_MCC_VR in that root.
Copy HaloMCCVR.dll, HaloMCCVRLauncher.exe, and halomccvr.cfg from the
extracted Build ZIP into Halo_MCC_VR. Do not extract the Source ZIP there
and do not replace files under MCC\Binaries\Win64.
Start your headset connection and OpenXR runtime. SteamVR is the currently
recommended/tested route; make it the active OpenXR runtime.
With MCC closed, run HaloMCCVRLauncher.exe from Halo_MCC_VR and use the
anti-cheat-disabled launch path. For the Store edition, let the launcher
activate MCC through the Xbox app; do not rename the game executable.
Set the required 120° FOV in every supported game's own settings, then
load a campaign and press F3 to recenter.
Do not use this mod in anti-cheat-enabled matchmaking.

Updating an existing installation
Close MCC completely and back up the existing Halo_MCC_VR folder.
Replace HaloMCCVR.dll and HaloMCCVRLauncher.exe with the new pair.
Keep your existing halomccvr.cfg so your controls, alignment, seat, and
picture settings remain saved. Missing new settings use current defaults;
roomscale and experimental hand alignment default off.
If you use both Steam and Store editions, update each edition's separate
Halo_MCC_VR folder.
Remove obsolete halo3xr.dll / halo3xr_launcher.exe from the active mod
folder if upgrading from a very old build. Do not load both generations.
The included MANUAL-README.txt has expanded paths, controls, rollback steps,
roomscale testing instructions, and troubleshooting.

Testing the experimental additions
Leave Fix Hand Alignment off for the first run. To test roomscale, open
F1 > Controls, enable Roomscale body movement, press F3, close F1, and take small
physical steps on level ground with the movement stick idle. Then test normal
stick movement, stopping, controller aim, recenter, pause, death/respawn,
vehicle entry/exit, and title changes. Check Halo 3 as a regression and identify
H2 Classic versus Anniversary.

When reporting a problem, attach HaloMCCVR.log and HaloMCCVRLauncher.log and
state the game/mission, Steam or Store edition, headset, OpenXR runtime, and
refresh rate.

Known WIP items
Roomscale translation in this exact corrected build awaits headset testing;
independent native body-yaw following is deferred.
Independent dual-wield projectile direction is unfinished. Halo 3's failed
experimental firing hooks remain disabled.
Complete H2 vehicle-control parity and H2/H4 first-person vehicles remain open.
H3-style weapon-side zoom screens are not yet available across every title.
Some sustained/sliding contact, modded weapon, unarmed, and secondary-weapon
damage cases need broader testing.
Per-title/per-weapon alignment defaults, automatic weapon presets, gun-stock
calibration, and some lower-edge visibility work remain unfinished.
All-title flat-mode/title-reentry recovery is not confirmed. Restart MCC if
the H3 recovery control cannot restore VR.
H4's reported damage blackout and the minor H2 tank-exit reticle report remain
unresolved and are not claimed as fixes here.
Hardware, OpenXR runtime, co-op/multiplayer, mission, vehicle, and long-session
coverage remains incomplete. MCC updates can invalidate native signatures.
Validation
Exact Release x64 candidate packaging passed.
Three CTest suites passed, including roomscale input transport and hook
lifecycle coverage.
Roomscale simulations passed at 60, 90, and 120 Hz and multiple world scales.
Manual-stick priority, freshness/generation cancellation, nested input
handling, hand transforms, contact/melee, sliders, and existing lifecycle
checks passed.
Reach consistency and pinned weapon-bound verification passed.
Build and source ZIP hashes were rechecked immediately before release.
Offline checks do not reproduce MCC's native movement dynamics or headset view.
The accepted physical-melee baseline remains 4e01f28; this cumulative roomscale
candidate does not advance that acceptance pointer until headset testing.

This is an unofficial derivative of
pancreations/Halo-MCC-VR, under
the MIT license. It is not affiliated with Microsoft or Halo Studios and
contains no MCC game binaries or editing-kit assets.

