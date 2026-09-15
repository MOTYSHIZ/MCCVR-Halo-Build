HALO MCC VR - CE RESOLUTION, RETICLE AND CONTACT - SEPTEMBER 2026
=============================================================

Supports Halo 2 Classic and Anniversary, Halo 3, Halo 3: ODST, Halo: Reach
and Halo 4. Steam and Microsoft Store / Xbox app / Game Pass use the same
files. This CE candidate adds full-resolution Anniversary eye targets, reticle
visibility/aim corrections, HUD mapping, hand-material contention and resource
identity fixes, plus CE world collision and physical melee. Original's working
camera/hands, the single-eye desktop mirror and controller aiming are retained.
This alpha candidate still needs headset confirmation in both CE graphics
modes. The accepted baseline remains 4e01f28.
Read RELEASE-NOTES.md for this update. CANDIDATE-MANIFEST.json records the
exact build identity and file hashes; you do not need to edit it.

For the CE test, compare both graphics modes, switch while looking around,
check gun/hand size and firing effects, then pause and resume in each mode.
Read RELEASE-NOTES.md for the exact status and test steps. CE world collision
and physical melee are available through their separate Body & Hands controls.
CE contact uses hand/weapon nodes; complete weapon surfaces remain unproven.
CE physical roomscale body following remains deferred. Ordinary tracked leaning
is part of CE 6DoF.
CE graphics switching: physical left hand beside the left side of the head,
then click the movement stick. Both graphics modes have a VR path in this test.

BEFORE YOU START
----------------
- A Windows x64 PC with MCC and the campaign(s) you want to play installed.
  Run MCC normally once to finish its first-run setup and game dependencies.
- A working PCVR headset connection, controllers and an active OpenXR runtime.
  Start the headset connection before the mod. SteamVR is the runtime used in
  the latest user feedback; other runtimes/headsets need separate testing.
- Steam running and signed in for the Steam edition; a working Xbox app/game
  entitlement for the Store edition. No special Steam launch options are
  required by this package: use the supplied launcher.
- You do not need Visual Studio, CMake, the source ZIP or a separate OpenXR SDK.
  The mod's C++ runtime and OpenXR loader are included in its compiled DLL.
  Your headset's OpenXR runtime and MCC's normal dependencies are still needed.

FRESH INSTALL
-------------
1. Fully close MCC. Extract the Build ZIP before running anything inside it.
2. Find your MCC installation:
   Steam: Library > Halo: The Master Chief Collection > Manage > Browse local files.
   Xbox app: MCC > Manage > Files > Browse; open Content if necessary.
   Choose the folder containing MCC\Binaries\Win64.
3. Create a folder named Halo_MCC_VR there. The result should look like:

   Halo The Master Chief Collection\
     MCC\Binaries\Win64\...
     Halo_MCC_VR\
       HaloMCCVR.dll
       HaloMCCVRLauncher.exe
       halomccvr.cfg

4. Copy those three files from the extracted Build ZIP into Halo_MCC_VR.
   Keep the included instructions, release notes, manifest and licenses for reference.
   Do not replace original game files or copy the mod DLL into MCC\Binaries\Win64.
5. Start your headset connection and OpenXR runtime, then run
   HaloMCCVRLauncher.exe from Halo_MCC_VR. Make a shortcut to it if convenient.
   The launcher starts MCC in its anti-cheat-disabled mode and loads the mod.
   If MCC presents a launch choice, select the anti-cheat-disabled option.
   Do not use the mod in anti-cheat-enabled matchmaking.
6. For Store, let the launcher activate the game package. Do not prelaunch MCC
   or rename its executable. If package activation fails, follow the launcher's
   displayed instructions. A first Store launch can take longer to load.
7. Load a supported campaign. VR enters automatically with the supplied defaults.
   Recenter with F3 once you are standing or seated comfortably.

UPDATING AN EXISTING INSTALL
---------------------------
With MCC closed, back up your old mod DLL, launcher and halomccvr.cfg.
Replace ONLY HaloMCCVR.dll and HaloMCCVRLauncher.exe with the new pair.
KEEP your existing halomccvr.cfg: controls, alignment and seat preferences
remain saved. Missing settings use current defaults. Roomscale and experimental
hand alignment are off by default, though an existing roomscale setting is kept.

For very old installs, keep obsolete halo3xr.dll / halo3xr_launcher.exe in your
backup outside the game folder. Do not load both versions. If you use both MCC
editions, update EACH edition's Halo_MCC_VR folder. Each has its own config.
Extracting this ZIP does not install anything automatically.

LEFT-HANDED MODE
----------------
Open F1 > Weapon & Aim and enable Left-handed main weapon.
The primary weapon, aim and trigger follow the physical left controller; the
right controller supports it or carries a secondary weapon. Stick and face-button
bindings retain their existing physical layout.

Default positioning now follows the released MCCVR-d77c9dd behavior requested
by the user. It can retain reversed hands or imperfect orientation, but the
newer anatomical correction no longer replaces that positioning automatically.
The current weapon-collision and physical-melee improvements are preserved.

Fix Hand Alignment (Experimental) appears directly underneath only while
left-handed mode is enabled. Leave it OFF for the released positioning.
Turn it on to try the newer anatomical hand/arm correction. It remains
experimental and can misalign hands. Turn it off to return to the default.
Config keys: left_handed = 1; experimental_hand_alignment = 0 (recommended).

ROOMSCALE BODY MOVEMENT
----------------------
This section applies to H2 Classic/Anniversary, H3, ODST, Reach and H4.
CE body following remains deferred; CE still has positional head tracking.
Open F1 > Controls and enable Roomscale body movement. Head tracking and
positional tracking must be enabled. Recenter with F3, close F1, and begin in
an open, level area with the movement stick idle. Take small forward, backward
and sideways steps. The CHARACTER BODY should follow those physical steps.
Config key: roomscale_movement = 1; use 0 to disable.

This update fixes title admission and input cancellation that prevented follow
commands from working. Native walking handles ground, walls and steps. As the
body travels, the matching tracked offset is consumed so the step is not added
twice. Catch-up speed depends on the game's movement settings; headset validation
of this fix remains pending. The movement stick takes priority and cancels
pending follow motion. Vertical head movement remains leaning/crouching, not
jumping. Roomscale does not stop the tracked camera leaning through a wall.

Body following suspends for menus, pause, vehicles/turrets, unavailable on-foot
evidence and stale tracking/input. F3 resets the reference. Switching roomscale
off returns to the previous tracked-lean behavior. Controller aiming and
head-relative walking are retained. Independently turning the native body to
follow head direction remains deferred; this option adds physical translation.

EVERYDAY CONTROLS
----------------
F1: VR settings. F3: recenter. L3+R3 together: recenter and toggle F1.
F1 > Controls: snap/smooth turning and angle/speed.
World contact, physical melee and gesture melee have separate settings.
For this CE test, enable World collision and True physical melee individually in
F1 > Body & Hands. New configs leave these off; existing choices are preserved.
Physical melee uses a 5 m/s default threshold. Test both hands in both CE modes.
Slider arrows move by the last displayed digit. F1 saves changes.
H3, ODST and Reach have first-person vehicles; F1 > Vehicles adjusts the seat.

KNOWN LIMITS AND TROUBLESHOOTING
------------------------------
- Independent dual-weapon trajectories are unfinished across titles. H3's
  rejected experimental dual-fire hooks remain disabled. Left-handed mode alone
  does not provide independent secondary-gun shots.
- H2/H4 first-person vehicle parity, complete scope parity, some visibility
  edges and further weapon calibration remain unfinished. Specific modded
  weapons and unarmed/secondary melee damage need individual testing.
- H4's reported black screen/fade after damage and H2's reported reticle issue
  after leaving a tank remain unresolved.
- F1 > Status and the launcher offer Force injection / recover VR. With the
  current DLL already loaded, this requests H3 camera recovery only. It does
  not recreate a lost OpenXR session. If VR remains flat, restart MCC; do not
  inject duplicate DLLs. All-title transition recovery is not confirmed.
- If performance falls to half refresh rate, try a lower VR resolution.
  Settings marked next-launch require restarting MCC.

For feedback, keep HaloMCCVR.log and HaloMCCVRLauncher.log from Halo_MCC_VR.
State the title/mission, Steam or Store edition, headset model, runtime and
refresh rate; for CE and H2, state Classic or Anniversary. Include the manifest.
For roomscale, test physical steps with the stick idle, then normal stick
movement, stopping, turning, recentering, pause and vehicle entry/exit.
Check Halo 3 as well as other titles. Roomscale log entries now distinguish
command requests, unavailable conditions and consumed native travel.

TO REMOVE
---------
With MCC closed, move or remove only Halo_MCC_VR and its shortcut. Original
game files do not need replacing. Keep a copy of your config if desired.
