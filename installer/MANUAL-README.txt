HALO MCC VR - ROOMSCALE CANDIDATE - 10 SEPTEMBER 2026
====================================================

This test candidate includes Halo 2 Classic and Anniversary, Halo 3, Halo 3:
ODST, Halo: Reach and Halo 4. Both Steam and Microsoft Store / Xbox app / Game
Pass editions use the same DLL and launcher. Halo CE VR is not implemented.

Read CANDIDATE-NOTES.md for what changed and what still needs headset testing.
CANDIDATE-MANIFEST.json identifies the exact source and SHA-256 file hashes.
This is an unaccepted test build, not a new accepted public release.

INSTALL OR UPDATE
-----------------
1. Fully close MCC. Keep a backup of your previous mod DLL, launcher and config.
2. Locate the MCC installation:
   Steam: Manage > Browse local files.
   Xbox app: Manage > Files > Browse, then open Content if necessary.
   Use the folder containing MCC\Binaries\Win64. Do not rename game executables.
3. Create or open Halo_MCC_VR inside that folder.
4. Copy HaloMCCVR.dll and HaloMCCVRLauncher.exe into Halo_MCC_VR.
   NEW INSTALL: also copy the supplied halomccvr.cfg.
   UPDATE: KEEP your existing halomccvr.cfg to preserve controls, alignment,
   seat settings and other preferences. Missing keys use current defaults.
   Roomscale is OFF until enabled. The included config is generated from this
   exact source; it is not an older, separately tuned configuration.
5. Keep one active mod DLL/launcher pair. If updating a very old install, move
   obsolete halo3xr.dll / halo3xr_launcher.exe into your backup outside the game
   folder. Do not inject both versions.
6. Start your headset connection and its OpenXR runtime. SteamVR is the runtime
   used in the preserved user tests; make sure your chosen runtime is active.
   Steam edition: have Steam running. Store edition: sign into the Xbox app.
7. Run the included HaloMCCVRLauncher.exe from Halo_MCC_VR. It detects the edition
   and launches the anti-cheat-disabled game. For Store, let the launcher start
   MCC through package activation; do not prelaunch it or rename its executable.

If you alternate editions, repeat the copy into EACH edition's Halo_MCC_VR
folder. Each installation keeps its own config; copy your preferences between
those folders manually if you want them identical. Extracting this ZIP does
not install anything automatically. Never use the mod in anti-cheat-enabled
matchmaking.

ROOMSCALE
---------
F1 > Controls > Roomscale body movement (or roomscale_movement = 1 in the config).
Head tracking and positional tracking must be enabled. Recenter with F3 after
standing comfortably. Start in an open, level area with the movement stick idle.

Horizontal physical steps request normal character walking. Native movement
handles walls, ground and steps; as the body moves, the tracked camera offset is
consumed so the same step is not added twice. This is a follow controller, so
body catch-up depends on the game's native movement and current settings.
Vertical head motion remains tracked crouching/leaning; it does not issue a jump.
The movement stick takes priority. Pause, VR menu, D-pad gesture, unavailable
on-foot evidence and stale tracking/input suspend body following. Turning the
option off restores the previous tracked-lean behavior.

Controller gun aiming is preserved by request. Walking stays head-relative.
Independent native-body rotation that always follows head direction is DEFERRED;
this candidate does not claim to separate body yaw from gun aim in every game.
Roomscale has not yet been headset-accepted in any title. It does not guarantee
that your camera can never lean through a wall. See the notes for test steps.

CONTROLS AND SETTINGS
---------------------
F1: VR settings. F3: recenter. Both stick clicks (L3+R3): recenter and toggle F1.
F1 > Controls selects smooth or snap turning and its speed/angle.
Left-handed main weapon, world contact, physical melee and gesture melee have
separate options. New installs leave optional experimental features off unless
the generated config says otherwise. Existing saved values are preserved.
Slider arrows step the last displayed digit. F1 saves edited preferences.

First-person vehicle placement is implemented in H3, ODST and Reach. While in
a seat, F1 > Vehicles adjusts that seat's forward/height/lateral offset. H2/H4
first-person vehicle parity is still pending. Reach swaps LT and X on foot and
restores native actions in vehicles.

DIAGNOSIS AND RECOVERY
---------------------
Keep HaloMCCVR.log and HaloMCCVRLauncher.log from Halo_MCC_VR. The game log names
source identity, edition, OpenXR runtime and headset information. Report title,
mission, H2 renderer when applicable, headset model, refresh rate and symptoms.
Include the manifest so the build is unambiguous; the log alone is not a DLL hash.

F1 > Status and the launcher offer Force injection / recover VR. With an already
loaded current DLL, this requests H3 camera recovery. It is NOT all-title recovery
and does not recreate a lost OpenXR session. Restart MCC if recovery does not
help; do not inject duplicate DLLs. Transition/flat-mode recovery is not fully
headset-confirmed. H4 damage black-screen/fade investigation remains deferred.

A temporary Store loading stall has been observed; duration varies. A frozen
frame alone does not diagnose its cause. Half-refresh app cadence can indicate
missed runtime frame deadlines; compare the log's panel/app cadence and try a
lower VR resolution. Resolution changes marked next-launch require a restart.

TO REMOVE
---------
With MCC closed, move or remove only the dedicated Halo_MCC_VR folder and its
shortcut. The mod does not require replacing original game files.
