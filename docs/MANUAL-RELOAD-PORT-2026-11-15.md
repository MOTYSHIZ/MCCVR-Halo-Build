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
- **The guns' own counters** (`src/dll/manual_reload_gun_display.inl`, 2026-11-16):
  - **Where the digits come from.** The assault rifle's and battle rifle's digit parts (first and third person, both
    titles) take their frame from `primary_ammunition_ones` / `_tens`. The weapon type's compute-function-value
    routine works these out (H3 `+0x36177C`, ODST `+0x3A78E8`).
  - **Why changing them is local.** Both official kits were scanned whole: only those display shaders name the two
    inputs. No engine code uses their ids outside the routine (H3 0x214/0x215, ODST 0x219/0x21A, read from the
    routine's own comparisons).
  - **The hook.** The detour rewrites only those two values, only for the gun the manual reload owns, with the
    routine's own math.
  - **Never touched:** `primary_ammunition` (loaded / maximum). Other guns' tags switch firing effects with it.
  - **FA (MCC Halo 3 Forge):** the assault rifle's counter read 01, the player's count, while the game had 32. The
    picture is `out/fa-gun-counter-01.png`.
  - **Not installed in Project Reclaimer.**

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

- **Sound pieces.** The kits have none; the click stands in.
- **Project Reclaimer: the HUD count, the guns' counters and the sounds.** Its lifted engine runs its own copies of that code, so these
  are not installed there. The hidden reload, the animation, the hand following and the shotgun do work there: they
  live in the palette hook.
- **The reload ring and the empty-chamber tint.** MCC has no such HUD widget to tint. The HUD's own low and empty
  flash now follows the player's count.
- **Campaign Evolved's per-range animation calibrations.** The ranges are found automatically from the learned clip.
  If one is wrong, turn off "Move the gun with its own reload animation" or forget the learned animations.

## Review against Campaign Evolved `5adbc7c` (2026-11-17, workbench only)

The user asked for a review of the manual reload and the Player IK work: the same as Campaign Evolved or better,
multiplayer-safe, and working in MCC and Project Reclaimer. Six read-only reviews (Campaign Evolved's commits since
`70c5c1b`, a full feature inventory, the hands, multiplayer safety, MCC against Reclaimer, crashes and frame cost)
were checked against the code before anything was changed. Nothing was run in the game.

### Fixed in the reload

| What was wrong, or missing | Now |
| --- | --- |
| Pulling a magazine out and working the handle were measured in the room: moving the gun (both hands) read as a pull. | Measured along the gun's well and the handle's line. |
| The handle chambered the moment it reached 92% back, with one pulse. | Campaign Evolved's stroke: a handle with a round waiting is drawn held back; a quarter of its travel and letting go chambers the round (ridden forward again, nothing does). The pump goes back all the way, then forward all the way. Each end has its own pulse. The handle runs home over 90 ms. |
| The shotgun jammed for good with *Rack the handle after an empty magazine* off. | The next shell chambers by itself (the same for any gun whose handle is not found for 1.5 s). |
| A magazine seated before the game had loaded counted the game's old count. | Only on proof (the game full, out of reserve, or its count risen since the drop); otherwise it counts as a full magazine and the game is pressed until it catches up. |
| The empty reload's animation was chosen from the game's count, which has usually refilled already. | From the player's gun. |
| Every reload pulse lasted one frame. | Campaign Evolved's lengths (30-150 ms) through a new timed pulse (`VR_PulseHandHaptics`), swapped for left-handed players. |
| The handle's reach was 10 cm whatever was due. | 1.5 times that when a round waits and the hand is not where the two-handed hold takes it (Campaign Evolved 30c7c1b). The zone sphere shows that reach, at the calibrated grab spot. |
| A grip had to be fresh at the pouch. | A grip held there takes a magazine, however long ago it closed (Campaign Evolved), except a hand on the two-handed hold. |
| A shell ahead of the game held the hand until counted, and was dropped after 3 s. | The hand is free at once; waiting shells are counted as the game loads them (Campaign Evolved's waiting shells). |
| A push could be dragged any distance down the line and still seat. | Abandoned beyond the push's start plus 30 cm. |
| A magnum's or automag's magazine could be "pulled" by the hand on its grip. | Not those (Campaign Evolved's pullNeedsRelease); the button drops them. |
| Holding a gun the reload does not cover (a plasma rifle) or riding a vehicle for 2 s forgot every count. | Only no gun at all for 2 s (a death, a level load) forgets. |
| Retries and catch-up presses ran in menus and cutscenes; after three the count was clamped to the game's. | No presses and no clock while not playing. A press inside a running press never lengthens it (a held reload is the game's pick up). |
| Nothing said why a loaded gun would not fire while the game finished its own reload. | A **WAIT** prompt at the handle when the trigger is pulled then (Campaign Evolved's ring). PULL now also shows at once on a pull that fires nothing. An empty shotgun gets **SHELL** at the pouch. |
| No pulse as the magazine reached the well, none on reaching the pouch. | Both (the pouch's only for a free hand). |
| The receiver's reach was fixed at 10 cm. | Per gun (`seat=` in halomccvr_reloads.txt), sized in the headset: *Size this gun's magazine well*. |
| The game's reload sound could be heard on a long reload (the shotgun filling). | Muted while the game's reload runs and while shells wait. |
| A learned clip seen in part, or through a pause, stayed for good; the store filled at 16. | 48 slots; only covered guns record; a pause or gap ends the recording; a clip must show the magazine's way in. *Forget this gun's learned reload* in the menu. |
| A file read that failed (being written) emptied the per-gun table, and the next change wrote every other gun's line away; the clip file was treated as absent. | Both keep what they had and read again. A clip file of another format is kept aside as `.bad`. Non-finite numbers in a clip are refused. The per-gun file is looked at every 2 s, not every pass. |
| *Animate the support hand on the gun* could no longer start (it followed the retired pouch gesture). | It plays when a gun the reload does not cover reloads the game's way. |

### Fixed in the hands

- The support hand closes on the magazine and the handle (the `magazine` and `handle` shapes), with or without
  touch sensors. Before, it showed the gesture: a pointing finger around the magazine.
- A stowed gun's grip no longer reaches the game (a gesture made with that hand pressed the grip's button). A stow
  ends in a vehicle seat and when no gun is drawn for 1.5 s (death, level change), as Campaign Evolved's
  holster_reset does.
- A tracks file without some `pose` lines keeps the built-in shapes for them.
- A repeated frame time no longer snaps the fingers.
- The touch sensors are read only in Halo 3 and ODST with Finger gestures on.

### Multiplayer safety

- Nothing writes the simulation and nothing is spawned (re-checked line by line).
- The **gun's digits** change only while the frame is being drawn (the stereo eye loop's scope on the render
  thread), which comes after the simulation's step: nothing the simulation reads can see them, in co-op as in solo.
  In a game no other machine shares, a query at other times is answered too, as before. The log counts both kinds
  of query, so a run shows where the counter is asked. (A first version of this round turned the digits off in
  co-op; the user asked why they could not stay local, and they can.)
- The **seat/rack click** plays in co-op too: the game itself calls that 2D sound for its local player only (the
  respawn beeps; a remote player is sent a network event instead), so it touches nothing shared.
- The **reload sound mute** stays everywhere: the first-person animation's event handler runs only on the machine
  whose player it is (a co-op partner never runs this player's first-person animation), so skipping one of its
  sound events cannot part the simulations.
- The **HUD count** writes only the HUD's own record on the stack, for one caller.
- The three hooks now forward to the game with spare stack slots (`CallNativeWithSpareSlots`) and count themselves
  out in a `__finally`; the digit hook's install check reads the four name ids at their proven places (the fourth
  was read two bytes late) and in order.
- A fault in the palette hook stops the reload's posing for that title load instead of faulting every frame.

### MCC and Project Reclaimer

| | MCC Halo 3 | MCC ODST | Reclaimer Halo 3 | Reclaimer ODST |
| --- | --- | --- | --- | --- |
| The reload by hand, the trigger, the count, hiding and posing, learned animations, prompts, zones, calibrations, hand shapes | yes | yes (never run) | yes | yes (Halo 3's path, ODST's guns) |
| HUD count, sound mute, click, gun digits | yes | yes (never run) | not installed | not installed |
| Finger gestures, held shapes | yes | yes | yes | no: the ODST arms are refused by the Halo 3 finger path |

Still MCC only, and why: the four hooks need a Reclaimer run with them switched on to show that the client's own
code still calls the engine's copies (the layer logs it). The recipe is in the review notes
(`reclaimer-mcc.md`): mostly removing the guards, plus `HALOMCCVR_GAME_RETURN_ADDRESS()` in the HUD filter.

### Not done (follow-ups)

- Project Reclaimer: the four hooks, and fingers in ODST levels (both need a Reclaimer run).
- The rocket launcher's cover and the fuel rod's pull (Campaign Evolved 6466c55): a cover state and its node.
- The support arm held at rest during the hidden reload (Campaign Evolved 30c7c1b): shows only with Floating hands off.
- The pouch in a body frame that holds still while you look down (Campaign Evolved efc7918), and a per-gun pouch
  spot and turn (ae2f101).
- The shotgun's firing animation trimmed so the gun does not ride up with a pump it has not done.
- One prompt image per label (each change uploads 4 MB on the present thread), and one stereo decision per eye pair
  in the palette hook.
- The game's rumble during the hidden reload (Campaign Evolved 084e891): measure first whether Halo 3 rumbles then.
- Finger poses from recorded hands, and a shot kick per gun, if the headset says they are wanted.

### Gates that were not needed (2026-11-17, later)

- Finger gestures no longer need touch sensors: a controller without them uses Campaign Evolved's table with the
  thumb off and the trigger pulled.
- The hand closing on a magazine or the handle no longer needs *Finger gestures* on.
- The reload sound mute while the game's gun is on its last round now holds only around a shot (the trigger pulled
  or let go under half a second ago), so melee and ready sounds are not dropped with it.
- Still gated, for a reason: the four Project Reclaimer hooks and ODST-level fingers there (each needs one
  Reclaimer run to show it works); the Covenant guns and the shotgun until their reload is learned (the way in comes
  from it); dual wield (two guns); other titles (no finger rig or palette posing there yet).

## Catch-up with Campaign Evolved `4a6f997` (2026-11-21, workbench only)

Campaign Evolved's reload commits since `5adbc7c`, checked against this port. Nothing was run in the game.

| Campaign Evolved | Here |
| --- | --- |
| `5c52fd7`, `312bc78`: the reload button reaches the game as pressed; only the tap (250 ms) is the reload's, and a press that swapped the gun is no tap. | Ported. Swap, board, pick up and use were held back 500 ms before the game's own hold began. The press now opens the hidden-reload window at once, so the game's reload that press starts is unseen and unheard from its first frame. `manual_reload_hold_button = 1` restores the old withholding. |
| `7039707`: a press or shot inside an open sound-mute window re-arms it. | Already so: the window here is only ever extended (now with a compare-exchange, since the pad's thread opens it too). |
| `cee86b8`, `14b7fb5`: the seat's travel gate counts closing in on the receiver, then is off by default (the user's call). | Ported: off by default; when on, closing in on the well counts. A magazine just pulled out still needs 8 cm, or it would go straight back in. |
| `177883d`: the held item lagged the gun by one frame of the pawn's move. | Not needed: the well is published in the gun hand's aim frame and rebuilt from the same pose, so the player's movement cancels out. |
| `a389b7a`: a grip still closed as a shell goes in is not the two-handed hold's until it opens (`loadGripHolds`, off on the shotgun); pump sounds on the player's emitter. | Ported the grip rule (built in, no file key). The emitter fix is Unreal-only; the click here is the game's 2D sound. |
| `1daf662`: manual reload out of Experimental and on by default. | Not changed: the branch is on hold, and `manual_reload` stays off by default until the user decides. |
| Menu restyle, Player IK | No Player IK commits since `5adbc7c`; the menu restyle is Campaign Evolved's own UI. |

## Setup layout and headset notices (2026-11-21, workbench only)

The user found the setup steps well described in the menu but with nothing in the headset once it was closed, and the
game's own blue HUD messages too short (about 95 characters) for instructions. So the mod's notice panel is now the
primary message for this, restyled in Halo 3's HUD look. Nothing was run in the game.

- **The notice panel** (`src/dll/native_subtitles.cpp`, `RasterizeNotice`): Bahnschrift (the HUD-like face the
  waypoint labels use) in the HUD's light blue with a soft glow, on a dark see-through bar sized to the text with a
  thin top and bottom line and bracket corners, under an optional small letter-spaced heading. Warnings are amber.
  The vehicle camera note's fallback uses the same panel, so it changes look too. Showing the same notice again only
  extends it (no new picture). `NativeNotice_Hide` takes an instruction away when its step is cancelled.
- **The calibration steps** (`vr.cpp`, `ShowReloadCaptureInstruction` / `ShowReloadCaptureResult`): when the menu
  closes on an armed step, the headset says what to do (which hand, which stick, the size in cm, updated as the stick
  changes it), then what was kept ("POUCH SET", "MAGAZINE WELL SIZED", "HANDLE GRAB SPOT SET") or why nothing was
  ("NOT SET": no squeeze in 30 s, no gun reloaded by hand, the gun has no part to set, the headset's position lost).
  The handle grab spot step now shows a small blue sphere at the current spot, as the other two steps do.
- **A gun learned its reload** (`manual_reload_game.inl`, `reload_clip_store::LearnedThisSession`): "RELOAD LEARNED",
  once a gun per session (it learns a full and an empty reload), only with "Move the gun with its own reload
  animation" on. Clips loaded from the file do not count.
- **F1 › Controls › Reload and holsters**, under Manual Reload, now reads in order: what it is; *Set up the gun in
  your hand* (the gun's name as players know it, "Reload this gun by hand", then three numbered steps: 1 pouch,
  2 magazine well, 3 handle grab spot, each with its current value and Reset); *How it plays* (rack, reload
  animation, hand on the magazine); *Helpers* (zones, prompts, sounds); *Fine tuning (every gun)* (grab radius,
  pouch location and offsets, the other games' insertion radius, the generic item, needle shake). Same settings and
  config keys; descriptions are wrapped to the panel instead of fixed lines.
- Multiplayer: all of it is the mod's own panel and spheres in this headset; nothing reaches the game.
