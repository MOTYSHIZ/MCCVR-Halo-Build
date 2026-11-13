# Player IK parity: Halo 3 / ODST vs Halo: Campaign Evolved VR (2026-11-13)

Goal: bring Halo 3 and ODST hands up to what the Halo: Campaign Evolved VR mod
("CampE") does, so Manual Reloading can later show the hands moving along the
gun. Parity means three things: hand poses, a way to configure them, and IK
targets animated relative to the weapon. The last one matters most.

Branch: `h3-odst-ik-parity` (worktree `MCCVR-Halo-Build-player-ik`), cut from
`nitsujack-development` at `555859bc`. Nothing here is merged, installed or
launched.

## The good news: both mods work the same way underneath

CampE does not move Unreal bones. It hooks the game's own first-person builder
and edits the Blam **node palette**, then solves each arm with a two-bone
(shoulder, elbow, wrist) solver. This mod does the same thing for Halo 3 and
ODST (`ReconstructVisiblePaletteSource` in `src/dll/game.cpp`, solver in
`src/dll/ik.cpp`). The two solvers are close cousins:

| | CampE | Halo 3 / ODST (this mod) |
| --- | --- | --- |
| Arm solver | two-bone, law of cosines | two-bone, law of cosines |
| Shoulder | planted on a level torso frame | planted on a level torso frame (`shoulder_level`) |
| Elbow pole | 75 % body direction, 25 % current | 75 % "out and down", 25 % authored |
| Stretch | up to 1.5x | up to 1.8x |
| Hand follows | controller, or a scripted anchor on the gun | controller only |

So the gap is not the solver. It is what the solver is **aimed at**, and what
the fingers do.

## What CampE has

1. **Hand poses.** Seven named poses (rest, index, fist, thumbs-up, point,
   point-down, ok). Each is a set of numbers per finger: a curl from -1 (open)
   through 0 (relaxed) to +1 (fist), plus small per-joint trims and three thumb
   extras. One table serves both hands. The pose is picked from the controller
   (grip, trigger touch, thumb touch) and eased in over about 45 ms. While a
   hand holds the gun, the game's own grip is kept. Reload has its own held
   shapes: magazine, shell, charging handle, shell pinch.
2. **Configuring them.** No in-headset pose editor. The poses live in a JSON
   file the mod writes once from its built-in values and then **reloads by
   itself whenever the file changes** (checked every 2 s). Deleting the file
   resets it. Weapon carry, support-hand grip and reload holds are captured in
   the headset with a "freeze and align" gesture and saved per weapon.
3. **IK targets relative to the weapon.** The off hand can be pulled toward
   an anchor expressed in the **weapon's frame**: the game's own off-hand
   position on the gun (learned live), the charging handle, the shell port, the
   magazine well. Paths are short: one recorded frame, or a straight
   lerp/slerp between two recorded poses over a time `u`. There are no
   keyframe curves. The hand target is `lerp(controller, anchor, w)` with the
   weight `w` eased in and out (tau 60 ms). CampE also lets the free off hand
   **join the game's own actions** (reload, melee, grenade) by blending onto
   the game's animated hand for the duration.
4. **Reload drives it.** Its manual-reload engine publishes, every tick, which
   anchor the hand should go to, how far along a path it is, and what shape the
   fingers hold. A small adapter turns that into a "support hand request" the
   arm solver consumes.

## What Halo 3 / ODST has today

1. **Hand poses.**
   - **ODST:** the free support hand's fingers curl from the controller
     (trigger curls the index finger, grip the others). Evidence-backed
     (`odst_finger_pose_logic.h`, H3ODSTEK bind fixture). It is behind
     *Body & Hands › Body (VRIK) › Tracked full-body IK*, there are no named
     poses, and nothing is per weapon.
   - **Halo 3 first person:** **no finger posing at all.** Halo 3's fingers
     are only posed on the separate full-world avatar. The first-person model
     needs its own H3EK evidence; the ODST layout is not proof for Halo 3.
     That evidence now exists (see "Halo 3 first-person hand evidence" below).
2. **Configuring.** Weapon carry is per title, with optional per-gun overrides,
   all in `halomccvr.cfg` and the F1 menu (*Weapon & Aim › Visual gun
   alignment*, *Visible hand positioning*). There are no pose or animation
   files.
3. **Animated IK targets.** **None.** The left hand either follows the left
   controller (arm IK) or, while gripping the gun two-handed, rides along with
   the game's own grip. There is no way to say "put the hand here on the gun"
   or "move it along this path". There is no keyframe sampling code in the mod.
4. **Reload.** Weapon gestures (pouch grab, insert at the receiver) press the
   game's reload button and draw a magazine in the hand. The hand itself is
   never animated: the game's own reload animation of the left hand is thrown
   away because arm IK keeps that hand on the controller.

## The gaps, in the order to close them

1. **A shared, data-driven "support hand track"** (this branch, step 1). A
   track is a short list of keys. Each key says *where the support hand
   should be, relative to the gun*, at a time. Keys can be relative to:
   - `weapon`: the gun (the right wrist), in the gun's forward / left / up
     axes, metres;
   - `authored`: **the game's own animated left hand** on that gun, so a
     zero-offset key simply plays the game's own reload motion on the hand;
   - `controller`: the player's real hand, for relative moves ("push up 5 cm").

   The mod samples the track (straight-line move, smooth rotation between
   keys), then blends the hand from the controller onto it and back with an
   eased weight, like CampE. Pure code, unit-tested, shared by Halo 3 and
   ODST, and usable as-is by Manual Reloading later.
2. **A tracks file the player can edit** (step 1, same candidate).
   `halomccvr_hand_tracks.txt` beside the DLL, written once from built-in
   defaults, reloaded automatically when it changes (CampE's approach),
   deleted to reset. Tracks are chosen per weapon (by the weapon's name, as in
   the mod's weapon catalogue) with a default for every other weapon. Switches
   stay in the one universal `halomccvr.cfg`.
3. **Play a track when a reload happens** (step 2, first behaviour change).
   When a weapon-gesture reload fires, the support hand plays the weapon's
   `reload` track. The built-in default joins the game's own left-hand reload
   animation (an `authored` key), which is the motion of the hand along the gun
   that is currently lost. F1 gets a preview button so a track can be watched
   without reloading.
4. **Named finger poses for ODST** (step 3). Replace "trigger curls index,
   grip curls the rest" with CampE's named poses (per-finger curl values from
   the same file), eased the same way. A track key can name a held shape
   (e.g. magazine) to use while the key is active.
5. **Halo 3 first-person fingers** (unblocked, not built). The H3EK export
   of `masterchief/fp` is done (below). What is left is runtime wiring: a
   Halo 3 bind fixture generated from that export, accepting Halo 3's own
   model checksum, and calling the finger solve from Halo 3's palette hook.
6. **In-headset capture of anchors** (later). CampE's "freeze and align"
   capture of a hold on the gun, saved to the tracks file, so a player can
   author a mag-well or charging-handle key with their own hand instead of
   typing numbers.
7. **Join the game's other actions** (later). Detect the game's own off-hand
   actions from the animated palette (CampE's ActionWatch) and play the
   `authored` track for melee and grenade too, without a gesture.

8. **Empty-hand finger gestures** (not built). CampE's free hands make
   gestures from the controller (`pose_for_inputs` and `pa_gesture` in CampE):

   | Grip | Trigger | Thumb | Pose |
   | --- | --- | --- | --- |
   | yes | yes | yes | fist |
   | yes | yes | no | thumbs-up |
   | yes | no | yes | point down |
   | yes | no | no | point |
   | no | yes | yes | ok |
   | no | yes | no | index |
   | no | no | any | rest |

   "Trigger" is the trigger *touch* where bound, otherwise the pull. "Thumb"
   is any thumb sensor: A/B touch, thumbrest or stick touch. Poses ease in
   over about 45 ms. The support hand gestures whenever it is off the gun;
   the gun hand only while unarmed or with the weapon holstered ("emote
   mode"), easing in and out over about 120 ms. One pose table serves both
   hands, mirrored. A setting turns gestures off.

   Here, ODST's free support hand only curls (trigger closes the index,
   grip closes the rest), with no thumb input, no named gestures and no gun
   hand. Halo 3 has nothing yet (gap 5). What it needs:
   - thumb and trigger *touch* inputs in the mod's controller actions. This
     changes the shared bindings for every title, so it is its own change,
     tested in every title;
   - the gesture table above, picking from the `pose` lines this branch
     already reads (add `thumbsup`, `pointdown` and `ok` to the defaults);
   - the gun hand's fingers, when it holds no weapon, for ODST and Halo 3
     (the ODST finger solve covers only the free support hand today).

## What this branch does not change

The solver, the shoulders, the right hand and the gun, two-handed aiming, and
everything in Reach, Halo 4, Halo 2 and CE. A track only ever moves the
**support hand's IK target**, and only while one is playing.

## What was built (2026-11-13, unaccepted)

| Commit | Change |
| --- | --- |
| `8e2a78c4` | This document; `src/common/hand_track_logic.h` (tracks, sampling, blending, text format) and its tests. No runtime effect. |
| `52d9582b` | Gaps 1-3: `halomccvr_hand_tracks.txt` (written on first run, reloaded when edited, rewritten when deleted); the support hand plays the weapon's `reload` track after a gesture reload; *Body & Hands › Hand animations › Animate the support hand on the gun* (`support_hand_tracks`, default on) and a *Preview hand animation* button. |
| `3dfa40c4` | Gap 4, the data half: `pose` lines give each hand shape its finger curls; a key's shape sets ODST's free support-hand fingers while the track plays. |

How it runs: the worker thread reads the file; the VR input thread starts
and advances the track; the palette hook only samples it (no locks, no
allocation, no logging). The log says `Hand tracks: playing 'reload' ...`
when a track starts and `Hand tracks: finished; ... in N arm solves` when it
ends. `N = 0` means the arm solver never took the track (arm IK off, the
support hand gripping the gun, or no first-person arms at that moment).

Differences from CampE, on purpose:

- Finger poses are one curl per finger. CampE also has per-segment curl and
  rotation trims and three thumb extras. Those can be added to the same
  `pose` line later without breaking old files.
- Poses are not picked from finger *touch* (trigger touch, thumb rest). The
  mod's controller actions have no touch inputs today, and adding them changes
  the shared input bindings for every title. ODST keeps "trigger curls the
  index, grip the rest", and a track's shape overrides it while it plays.
- The track file is plain text in the style of the cfg, not JSON. The mod has
  no JSON reader, and the file is meant to be edited by hand.

Advice from the CampE manual-reloading session, for whoever builds Manual
Reloading on this:

- An absolute hand position on the gun, in a frame guessed from one weapon,
  was 50 cm off on another weapon. Prefer moves relative to a hand that is
  already right: `authored` (the game's own hand) or `controller` keys.
  `weapon` keys are for points measured on that specific weapon.
- Keep CampE's names when the reload state machine arrives: states `Idle`,
  `MagOut`, `MagHeld`; hand places 0 controller, 1 pump/foregrip,
  2 charging handle, 3 shell port, 4 magazine into the well. A grab counts
  when the grip *closes* inside a zone, never when it is already closed.
  Count a shot by the game's ammo count falling, never by the trigger.
- Units there: holds in cm plus an xyzw quaternion; zones and radii in metres.
  This file uses metres and degrees throughout.

## Halo 3 first-person hand evidence (2026-11-13)

H3EK is installed at `D:\SteamLibrary\steamapps\common\H3EK` (not the
`N:\SteamLibrary` path in older notes), with its tags still packed in
`H3EK.7z`. Selective extraction of one tag, then the kit's own exporter:

    7z x D:\...\H3EK\H3EK.7z -oout\h3ek-fp tags\objects\characters\masterchief\fp\fp.render_model
    (cwd out\h3ek-fp, with an empty data\ folder)
    D:\...\H3EK\tool.exe export-tag-to-xml <that tag> h3_masterchief_fp_fp.render_model.xml

Export SHA-256 `b85a4960c113e0a1b69287dbef38ed559342ae98f7277e0a46d50e6e38a54f99`
(kept in the ignored `out/h3ek-fp`, regenerate with the steps above).
Runtime import info checksum `504041493`.

Findings:

- 37 nodes: `base`, left/right upper arm, forearm, hand, then five fingers
  per hand (index, middle, pinky, ring, thumb) at low, mid and tip. Names,
  order and parents are **identical to ODST's recon/ONI first-person
  models**, so the ODST `Parents` and `Chains` tables describe Halo 3 too,
  now on Halo 3's own evidence.
- Default bind pose: 36 of 37 nodes match ODST's fixture to within 1e-5.
  The one difference is node 16, `r_thumb_low` (largest field difference
  0.059). A Halo 3 fixture must use Halo 3's own values, not ODST's.
- Still to prove at runtime: that the palette Halo 3's hook edits is this
  model with these indices, and what checksum the game reports for it (the
  tag's import checksum is not proof of the runtime value).

## Headset test (Halo 3 regression and ODST check)

Settings needed: *Body & Hands › Body (VRIK) › Arm IK* on; *Controls ›
Reload and holsters › Manual Reload* on; and *Controls › Reload and holsters ›
Skip reload and weapon-ready animations* **off** (the default reload track
follows the game's own reload motion, so it needs that motion to play).
*Body & Hands › Hand animations › Animate the support hand on the gun* is on
by default.

**Halo 3, regression first.** Play a campaign mission with the assault rifle.

1. Aim, shoot, two-hand grip, holster and swap as usual. Nothing should look
   or feel different from the last build: the gun hand, the aim, the
   two-handed grip, the shoulders.
2. Let go of the gun with your support hand. It should follow your
   controller exactly as before.

**Halo 3, the new part.**

3. Reload by gesture (grab a magazine at the pouch, bring it to the gun).
   When the reload goes in, your support hand should leave your controller,
   move along the gun the way the game's own reload moves it, and come back
   to your controller after about one and a half seconds.
4. Open F1 › *Body & Hands* › *Hand animations* and press *Preview hand
   animation* with your support hand off the gun. The hand should go to the
   gun, dip down about 8 cm, come back up, then return to your controller.
5. Turn *Animate the support hand on the gun* off and reload by gesture
   again: the hand should stay on your controller (the old behaviour).

**ODST check.**

6. Repeat steps 3 and 4 in an ODST mission (the Rookie's SMG or the silenced
   pistol). The same hand motion should play.
7. Optional: turn on *Body & Hands › Body (VRIK) › Tracked full-body IK*,
   press *Preview* again, and watch the support hand's fingers. Half way
   through they should close into a magazine-holding shape, then go back
   to following your trigger and grip.

What to send back: which steps looked right, anything that looked wrong
(where the hand went, if it popped or jittered), the MCC edition, and
`HaloMCCVR.log`. The log's `Hand tracks:` lines show each track start and
how many arm solves it moved.

**Known limits.** Halo 3's fingers do not move yet (the evidence exists,
the runtime wiring is not built). A track does nothing while your support hand
is gripping the gun or while dual wielding. If the game's own reload is
shortened by the mod's reload settings, the `reload` track follows that
shorter motion and then holds the hand at the game's grip until the track
ends.
