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
     (`masterchief/fp`, 37 nodes) needs its own H3EK node export before any
     finger can be touched. H3EK is not installed on this machine, and the
     ODST layout is not proof for Halo 3.
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
5. **Halo 3 first-person fingers** (blocked). Needs an H3EK export of
   `masterchief/fp` (node names, parents, bind pose), the same way ODST's
   fixture was made. Then the ODST pose code can be reused on its own
   evidence.
6. **In-headset capture of anchors** (later). CampE's "freeze and align"
   capture of a hold on the gun, saved to the tracks file, so a player can
   author a mag-well or charging-handle key with their own hand instead of
   typing numbers.
7. **Join the game's other actions** (later). Detect the game's own off-hand
   actions from the animated palette (CampE's ActionWatch) and play the
   `authored` track for melee and grenade too, without a gesture.

## What this branch does not change

The solver, the shoulders, the right hand and the gun, two-handed aiming, and
everything in Reach, Halo 4, Halo 2 and CE. A track only ever moves the
**support hand's IK target**, and only while one is playing.
