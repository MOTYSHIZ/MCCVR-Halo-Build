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

Not taken, because this port has no counterpart yet:
- the gun posed from its own animations (push-in and insert ranges for an empty and a full gun, the charge handle by
  animation, the body lock, the parts that show the rounds);
- the per-weapon file's sounds (grab, insert) and calibrations (grab zones, the support grip zone tube);
- the hand riding the gun's frame on the magazine slide;
- the empty-chamber HUD tint;
- the state-hold log (this port does not hold the game's reload state).

The Needle Rifle items are Reach's and do not apply to Halo 3 or ODST.

## Not ported yet (Campaign Evolved has these)

- The shotgun's shells one at a time and its pump; the Covenant magazines by the weapon's own animation
  (carbine, spike rifle, mauler).
- The per-weapon data file and its in-headset calibrations (grab zones, holds, ranges).
- The hand following the magazine on the gun (place 4) and the hand on the charging handle (place 2). The
  support hand stays on the controller.
- Hiding the game's own reload animation. Campaign Evolved holds a local first-person state; MCC's only way
  so far wrote the simulation and is retired.
- The HUD showing the player's count (the game's counter is shown).
- The reload ring, the empty-chamber tint, and the sounds.
