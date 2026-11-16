# Manual reload: Halo: Campaign Evolved's reload by hand, ported to Halo 3 and ODST (2026-11-15)

Branch `h3-odst-ik-parity`. Replaces the earlier experimental manual reload in Halo 3 and ODST (the user:
"this new work is completely replacing the existing manual reload work in MCC VR, unless you find anything
useful and mp safe in there"). Works in MCC (both editions) and in Project Reclaimer 0.10. Unaccepted: no
headset result yet.

Source: the Halo: Campaign Evolved VR mod's reload engine (`core/reload`, `Gesture.cpp`, development
`e2c63ad`) and its write-up, vr-game-conversion `references/manual-reload.md`.

## What the player does (Halo 3 and ODST)

Built-in guns: assault rifle, battle rifle, SMG, silenced SMG, sniper rifle, magnum, automag. Every other gun
reloads the game's way (the reload button goes straight to the game).

1. **Tap reload**: the game's own reload button. The mod reads which button that is from the game's
   current layout where it can, and otherwise uses the per-title setting (X in MCC, Right bumper in Project
   Reclaimer). The magazine drops out: the gun's own magazine is no longer drawn. A press held longer than half
   a second is the game's own (pick up, use).
2. The chambered round can still be fired (the Battle Rifle keeps its 3-round burst). After that the trigger
   does nothing until a magazine is in.
3. **Fetch a magazine:** close the support grip at the pouch (the hip by default, or behind the support
   shoulder). A grip closed a moment before the hand arrives also counts. The gun's own magazine model is drawn
   in the hand.
4. **Seat it:** bring it under the gun. Once it has moved at least 8 cm from where it was taken, is within
   10 cm of the well's axis and is turned no more than 90 degrees from upright, it locks onto the axis. Push it
   up the last centimetre and it seats. Letting go while it is on the axis also seats it. Letting go anywhere
   else drops it: fetch another.
5. **Rack** (only a gun that ran dry): close the support grip on the charging handle or slide and pull it back
   6 cm. *Controls › Reload and holsters › Rack the handle after an empty magazine* turns this off; then seating
   chambers the round.

The game is sent its own reload button at the moment the magazine drops. It reloads by itself, and its
animation plays as it would for a flat player. The player's gun keeps its own count. A shot is counted when
the game's count falls, never from the trigger. If the game ignores the press it is retried once. A game that
is still behind once the magazine is in gets up to three more presses, then the player's count takes the
game's.

## Multiplayer safety (AGENTS.md standing rule)

| Piece | Why it is local |
| --- | --- |
| The reload press | The game's own reload button, exactly as a flat player presses it. |
| Holding the trigger back | The gun hand's trigger reaches the game as 0. Input only. |
| The magazine reads | The weapon's magazine record and its tag are read inside the palette hook. Nothing is written. |
| The gun's magazine hidden, its handle drawn back | This frame's private copy of the first-person palette (the same lever as floating hands and the stow). Other players see the third-person world object, which is untouched. |
| The magazine in the hand and at the pouch | Drawn by the mod's own renderer (the weapon accessory). Nothing is spawned or despawned in the game. Campaign Evolved's falling physics magazine has no counterpart here on purpose: in MCC it would be a game object. |

Retired because they were not multiplayer-safe (the code is kept, nothing installs):
- the native reload policy: "Disable automatic reload" blocked the game's empty-trigger reload call, and "Skip
  reload and weapon-ready animations" and "Shortened reload animation" wrote the magazine's countdown timers
  and changed the reload's duration. Their F1 rows are gone; old cfg keys load but do nothing.

Replaced in Halo 3 / ODST (it was safe, but the new reload supersedes it): the pouch-to-receiver gesture.
Kept: the drawn magazine (the accessory and its kit-evidenced catalogue) and the Needler shake (a button press).
Other games keep the pouch gesture until they get this reload.

## Evidence

**Magazine record** (pinned retail halo3.dll `b209d845`, halo3odst.dll `5bb20976`; capstone reads):

| | Halo 3 | ODST |
| --- | --- | --- |
| Weapon datum's magazines | +0x230, 0x18 apart | +0x228, 0x18 apart |
| State (1-3 reloading) | +0x00 | +0x00 |
| Rounds inventory (reserve) | +0x06 | +0x06 |
| Rounds loaded | +0x0A | +0x0A |
| Weapon tag's magazines block | count +0x424, address +0x428, 0x80 apart | +0x434 / +0x438 |
| Rounds loaded maximum (tag) | +0x0A | +0x0A |

- `0x35F174` (H3) / `0x3A5340` (ODST) recharges record+0x0A up to tag magazine+0x0A, reached through the
  object's definition (word at +0).
- `0x3621F2`-`0x3623D3` (H3, pickup) caps +0x06 plus +0x0A at the tag's total and adds to +0x06.
- A scan of every `[reg + reg*8 + disp]` access across .text: +0x0A has 22 uses in both titles, with the same
  writers side by side (H3 `0x35EF39`/`0x35EF57`, ODST `0x3A5101`/`0x3A511F`).
- The tag names come from the H3EK kit's strings ("rounds loaded maximum", "rounds total maximum",
  "runtime rounds inventory maximum").
- The countdown fields of the same record (+0x02, +0x10, +0x14) were already documented in
  `NATIVE-RELOAD-POLICY-2026-09-16.md`.

**Gun parts:** `manual_reload_parts.generated.h`, from `docs/RELOAD-CATALOG-EVIDENCE-2026-09-16.json` (the
official kit render models' node lists, in node order), via `tools/re/generate_manual_reload_parts.py --check`:
- the magazine node is the one the reload catalogue selected;
- the chambering part is `ophandle` (AR, BR, sniper), `charging_handle` (SMGs), `slide` (magnum, automag) or
  `pump` (shotgun; not built in).
- The well is the catalogue's magazine rest centre (`weapon_model::ReceiverPoint`) and its axis is the gun
  root's up. The rack part's place is its live palette node; it is pulled along the gun root's back.

**Project Reclaimer:**
- The palette hook runs there (the lifted engine's replacement entry), and the reads go through the game's
  own thread-local data inside that call.
- ODST's levels run on the Halo 3 engine there. The title is Halo 3 with ODST's gun models, so the parts
  table, the catalogue and the accessory fall back to ODST's pinned model identities. The Halo 3 record
  layout applies.

## Code

- `src/common/reload_ammo_model.h`: Campaign Evolved's AmmoModel, ported. `tests/reload_ammo_model_tests.cpp`
  is its own out-of-tree test, ported (480k checks).
- `src/common/manual_reload_parts.h` and `.generated.h`: the parts and the built-in weapons.
- `src/common/manual_reload_logic.h`: the state machine (Idle, magazine out, in hand, pushing in, racking) and
  the reload button's tap filter. `tests/manual_reload_logic_tests.cpp`.
- `src/common/manual_reload_publication.h`: game to VR (count, well, rack, relative to the gun-hand
  controller) and VR to game (hide, rack pull).
- `src/dll/manual_reload_game.inl`: the palette hook's side (reads, the palette edits).
- `vr.cpp`: the engine on the input thread, the pad merge, the accessory, the trigger hold, the log.
- `input.cpp`: the tap filter on the reload button (plain buttons and the action bridge).

Log lines:
- `Manual reload: <phase>; dropped=.. grabbed=.. seated=.. racked=.. game-presses=.. let-go=..; ours ...; trigger held|free`
- `Manual reload state: ...` (every 5 s while it owns the gun: the counts and where the well and rack are)
- `Manual reload (game side): gun samples=.. ammo-unread=.. magazine-hidden=.. rack-drawn=.. faults=..`

## Headless (FA) results, 2026-11-15

Meta XR Simulator and Operator. FA DLL = this branch plus the usual test-only acceptance of a VALID (never
TRACKED) simulated head, which is never committed. Logs are in `out/fa/logs` (MCC) and
`_reclaimer-0.10-re/fa/logs` (Reclaimer, `mreload-rc-*`).

| Run | Result |
| --- | --- |
| MCC Halo 3 Forge (Construct), assault rifle | **Full cycle passed.** Reads: loaded 32, reserve 64, capacity 32. Shots counted from the game's count. Tap: one drop, one press; the game reloaded unseen (28 → 32 loaded, 64 → 60 reserve). The chambered round fired, then the trigger was held. Pouch grab, seat (31 in, chamber empty), rack (chambered, trigger free), fire again. Nothing created or lost at any step. |
| Project Reclaimer 0.10.3, Halo 3 (Tsavo Highway), assault rifle | **Full cycle passed**, the same steps and numbers (reserve 240), through the lifted engine. |
| Project Reclaimer 0.10.3, ODST (Tayari Plaza), ODST assault rifle | **Reads, ownership, drop and the game's reload passed.** Ownership is through the ODST model identity fallback; the accessory draws the real ODST magazine. The full cycle was not completed: the simulated player kept dying, or the mission kept reloading its start checkpoints. One run then showed the trigger free with the magazine out; that is not explained and needs the headset. |
| MCC ODST | **Not reached.** The user was at the PC (the input gate aborted), and the FA harness has no ODST route. ODST's own record layout (+0x228) is untested in game. |

Fixed by the pass:
- The well and rack are now measured from the gun hand's aim pose as the arm solver places it. A hand-built
  world transform had put them 40 cm off.
- The reload is pressed on the game's own binding. That MCC profile reloads on RB, while the cfg default
  says X.
- A retry no longer re-arms itself (it had sent 141 presses a minute).
- Counts are forgotten after 2 s with no reading of the gun (a death, a checkpoint revert, a cinematic).
  When the gun is read again, the phase is worked out from that gun's own count.

Seen, not changed:
- An automatic rifle can fire one round past the player's chambered round before the hold lands (one frame
  at its fire rate). The count floors it, as in Campaign Evolved.
- The rifle's on-gun counter shows the game's count, not the player's.

## Catch-up with Campaign Evolved development `6a14718` (later on 2026-11-15)

Campaign Evolved added 18 reload commits after `e2c63ad`. Its ammo model and built-in weapon traits did not change.
Taken:
- **GRIP TOOK NOTHING** (`418e7a5`): a support grip closed near the pouch or the rack part that has, 350 ms later
  (past the late-grip window), taken neither a magazine, the rack nor the two-handed hold logs every gate as it
  stood at the press. Log: `Manual reload: GRIP TOOK NOTHING (<phase>): pouch .. cm away (reach ..), magazine to
  fetch ..; rack part .. cm away (reach ..), racked by hand .., has a round to chamber ..; two-handed hold held off
  by the reload ..`, plus a lost-tracking note when the hand read 0,0,0. A distance of -100 cm means that zone
  was not there.
- **The belt grab's pulse** (`718d716`): the support hand's pulse on taking a magazine is 0.7, Campaign Evolved's
  strength (was 0.5), so the hand knows it has one without looking.

Not taken in that catch-up (the second round below then built most of it):
- the gun posed from its own animations;
- the per-weapon file's sounds and calibrations;
- the hand riding the gun's frame on the magazine slide;
- the empty-chamber HUD tint;
- the state-hold log (this port does not hold the game's reload state).

The Needle Rifle items are Reach's and do not apply to Halo 3 or ODST.

## Second round: the rest of Campaign Evolved's manual reload (2026-11-16)

The user asked for the bigger work: the features the first port left out. Every piece is local to this player
(AGENTS.md multiplayer rule): the first-person palette, the CHUD's own stack record, a first-person sound event and
a local 2D sound, and the mod's own renderer.

### The game's reload is hidden, and the gun moves through its own reload animation

`src/common/reload_clip_logic.h`, `src/dll/reload_clip_store.*`, `src/dll/manual_reload_game.inl`.

Campaign Evolved poses the gun from stretches of its reload animation, decoded offline. MCC's first-person
animations are computed by the game, so the palette hook **records** them instead. The game reloads at the drop
(unseen, as before). While the manual reload has the gun:
- **The gun is drawn in its resting pose.** The gun's root is held where it rests on the gun hand's wrist, and every
  part where it rests relative to the root. The game's own reload animation is no longer shown.
- **The animation is learned.** Each node is recorded relative to the gun's root into a clip: once per gun and kind
  (the gun had rounds left, or was empty).
- **The clip is analysed.** From it come the magazine's way out, its final way in, and the rack part's stroke. With a
  clip:
  - the drop plays the way out, then the magazine is hidden;
  - the push scrubs the way in by how far the hand has pushed, and the gun's own magazine is drawn on it;
  - the rack scrubs the stroke by the pull;
  - the well, its axis, the insert depth and the rack travel are all the animation's own.
- **Clips persist** in `halomccvr_reload_clips.bin` beside the settings, so each gun learns its reload once. F1 ›
  Controls › Reload and holsters can forget them.
- **The first reload of a built-in gun** is already by hand, with the catalogue's well, as in the first round. It is
  recorded while hidden.
- **Covenant guns and the shotgun** (carbine, spiker, mauler, shotgun) are by hand once their reload is learned. Their
  first reload is the game's own, and it is recorded. The shotgun's shell is the kit's `bullet` node.

Holding the root on the wrist also gives Campaign Evolved's "body lock" for free: the gun cannot shift in the hand
while posed. The magazine node's position at the clip's first frame is the well. Whatever frame the hand's push and
the clip's distances are in cancels out (both are the gun's root-relative distances, converted with the gun's live
scale).

### The hand rides the magazine and the handle

`ApplyManualReloadFollow` (game.cpp), before the left arm's IK:
- **Pushing a magazine in:** the support hand's drawn target is moved by how far the drawn magazine sits from where
  the controller carries it (on the axis, or where the animation put it last frame).
- **Racking:** the target rides the rack part, keeping where the hand took it in the part's own frame.

The move is published in the gun-hand aim frame and carried back out through the same aim pose, so turning the gun
does not swing the hand off (Campaign Evolved's lesson: one placement path for the whole interaction).

### The HUD shows the player's count (MCC)

`src/dll/manual_reload_hud.inl`. Research: a background RE pass on the pinned modules.
- **The getter.** The CHUD builds its weapon slots from one display getter (H3 `+0x362A60`, ODST `+0x3A8C6C`). It
  fills a stack record: rounds loaded `+0x2C`/`+0x30`, maximum `+0x2E`/`+0x32`, reserve `+0x30`/`+0x34`.
- **Its callers.** It has four in each title, all display: the CHUD weapon-slot fill, a CHUD pickup check, and the
  training hints. Firing, AI and the network serializer never call it. Every 16-bit reader of the rounds fields was
  listed.
- **The hook.** The detour writes the player's count into that record only for the call that returns into the CHUD
  fill (the fill's call-site signature, cross-checked against the getter's entry), and only for the gun the manual
  reload owns.
- **Not done:** the assault rifle's on-gun counter. It is an object function value (`primary_ammunition_ones`/`_tens`)
  that effects and attachments can also read, so no site was proven render-only.

### Sounds (MCC)

`src/dll/manual_reload_sound.inl`.
- **Why the game's sound goes.** The game's reload sound is one ~2 s clip, fired at frame 0 of the first-person reload
  animation by the first-person animation event handler (H3 `+0x2C2A94`, ODST `+0x2EB030`, local users only). Its
  sound events are dropped while the manual reload hides the game's reload: from the drop until the magazine is in,
  and 2.5 s after any press.
- **What plays instead.** The gun's own pickup click (weapon definition `+0x3AC`/`+0x3BC`) plays through the local 2D
  sound call (H3 `+0x158244`, ODST `+0x185CA4`; ODST's own code calls it for local players and networks remote ones)
  as a magazine seats and as the handle is racked. It is called from the palette hook (the game's thread).
- **Why only clicks.** The kits have no separate grab, insert or rack sounds. Each gun's reload is one clip.

### The shotgun: a shell at a time, and the pump after every shot

`manual_reload_logic.h` (`InsertShell`, `rackByHold_`).
- **Taking a shell.** A shell is taken at the pouch. The game is asked to load as the first one leaves, and again
  whenever it has stopped.
- **Putting it in.** The shell goes into the loading port along the learned way in. It is counted once the game has
  loaded a round for it; else it waits up to 3 s. A shell the game cannot take is dropped.
- **The pump.** Every shot empties the chamber. Drawing the support hand back along the gun works the pump, either
  from the two-handed hold (which keeps the hand: Campaign Evolved's `supportGripRacks`) or by gripping the pump.

### Per-gun settings and in-headset calibrations

`src/common/reload_weapon_file.h`, `src/dll/reload_weapon_settings.*`.
- **The file.** `halomccvr_reloads.txt` beside the settings has one line per first-person gun (the kit's model name):
  `enabled`, `grab` (cm), `handle` (cm back, up, side), `depth`, `travel`.
- **F1 › Controls › Reload and holsters:**
  - the gun in hand, with "Reload this gun by hand";
  - "Set the handle grab spot to my hand" (this gun's line) and "Reset grab spot";
  - "Set the pouch to my hand" (`weapon_pouch_offset_*`);
  - "Show reload zones": see-through spheres at the pouch, the well while a magazine is in hand, and the rack part.
    Green: the hand is in it and it would take. Amber: in it, nothing to do. Red: out of reach. Drawn by the mod's
    accessory renderer.
- **Taking a calibration.** A calibration takes the next fresh squeeze of the support grip once the menu is closed.
  That squeeze reaches neither the game nor the reload.

### Headless (FA) results, 2026-11-16

These ran on MCC Halo 3 Forge (Construct) with the assault rifle, under the Meta XR Simulator. The logs are
`reloadanim-1` and `reloadanim-2` in the fa-helpers worktree's `out/fa/logs`, and the learned clips are in
`out/fa-reloadanim-clips.bin`.

| Check | Result |
| --- | --- |
| Hooks | The CHUD getter and the first-person event handler both installed. The HUD showed the player's count throughout, with no faults. |
| The game's reload hidden | During the game's reload the gun was drawn in its resting pose; a picture during the reload confirms it. The game's reload sound was dropped. |
| Learned | A full and an empty reload were learned and saved: magazine out 0.43-0.50 s, in 0.93-1.03 s, the rack stroke 0.0192 units. The first analysis took the whole 43 cm return as the push; it was fixed to the last straight stretch, mostly up. |
| By hand | Tap: the way out played and the game reloaded unseen. The magazine was taken at the hip and pushed in along the learned way in: 32 in, with a click. |
| Dry gun | Seated with the chamber empty, so the trigger was held. The handle pull went along the learned stroke and chambered a round, with a click. The trigger was then free and the gun fired. |

Two things this run turned up:
- **Fixed:** Halo 3 reloads by itself the moment a magazine runs dry, and its sound could slip out before the VR
  thread saw it. The sound is now also muted while the last round is in the gun.
- **Not covered:** the shotgun, the Covenant guns, ODST and Project Reclaimer were not run this time.

### New settings (all in Controls presets)

`manual_reload_animation`, `manual_reload_hand_follow`, `manual_reload_sounds` (default on);
`manual_reload_show_zones` (default off).

### Not ported, and why

- **The assault rifle's on-gun counter.** It still shows the game's count; see the HUD section.
- **Sound pieces.** The kits have none; the click stands in.
- **Project Reclaimer: the HUD count and the sounds.** Its lifted engine runs its own copies of that code, so these
  are not installed there. The hidden reload, the animation, the hand following and the shotgun do work there: they
  live in the palette hook.
- **The reload ring and the empty-chamber tint.** MCC has no such HUD widget to tint. The HUD's own low and empty
  flash now follows the player's count.
- **Campaign Evolved's per-range animation calibrations.** The ranges are found automatically from the learned clip.
  If one is wrong, turn off "Move the gun with its own reload animation" or forget the learned animations.
