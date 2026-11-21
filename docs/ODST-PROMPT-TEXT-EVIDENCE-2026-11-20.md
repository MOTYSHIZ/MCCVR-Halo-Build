# ODST tutorial text in VR words (2026-11-20)

ODST's tutorial lines name controls with button pictures. In VR those pictures are wrong: MCC shows gamepad buttons, or
keyboard keys while it thinks the player uses a keyboard, and neither is the control the player holds. The mod now
rewrites each English tutorial line that names a control into the player's own VR controls, read from VR Mappings
(F1 › Controls › VR Mappings, Settings for: ODST). `src/common/odst_prompt_text.h` holds the finds and the wording;
`tests/odst_prompt_text_tests.cpp` shows every rewritten line.

Presentation only. The only thing that changes is the chud's copy of the line it shows. No game state is written and
nothing is spawned (memory `mp-safety-standing-rule`).

## Where the text comes from (H3ODSTEK)

- The tutorial scripts show each line with `chud_show_cinematic_title (player_get player_short) <title>`, directly or
  through `f_display_message_*` (`global_scripts.hsc`, `h100_mission_l100.hsc`).
- Each title names a string in `tags\ui\chapter_titles\atlas\h100.multilingual_unicode_string_list` (UTF-8, one string
  per language).
- Button pictures are private-use characters. The kit's glyph table (`atlas_tag_test.exe`, 0x18-byte entries
  `{name, glyph}` at +0x132D380) names them. The ones the tutorials use:

| Glyph | Kit name | What the game's default gives it |
|---|---|---|
| U+E451 / U+E452 | `button_accept` / `button_cancel` | A / B (actions 0x12 / 0x13) |
| U+E454, U+E453 | `stick_look`, `stick_move` | right stick, left stick |
| U+E448 | `button_flashlight` | ODST's VISR (action 0x11, the flashlight row) |
| U+E44C | `button_back` | Back (opens the VISR database) |
| U+E501 / U+E502 | `button_activate_waypoint` / `_alt_` | actions 0x25 / 0x26: D-pad up / down |
| U+E503 | `button_retrain` | action 0x27: Y |
| U+E505..U+E512 | `button_pda_*`, `button_comm_*` | the map's own buttons (below) |

The glyph callback (halo3odst.dll +0x323DCC) turns E501/E502/E503 into actions 0x25/0x26/0x27. The engine's default
preferences (+0x1B9328) put those on buttons 2, 3 and 0xD (D-pad up, D-pad down, Y) of the binding table (button
0xA is A). MCC's own resolver (+0x323BB8) gives the map's glyphs a gamepad picture, or a keyboard key name when the
input device reads 1 (keyboard): E505 place waypoint A, E506 next waypoint Y, E507 remove waypoint X, E508 close Back,
E509 play A, E50A play all Y, E50B pause A, E50C stop X, E50D move left stick, E50E zoom right stick, E50F/E510
D-pad up/down, E511/E512 LB/RB.

## MCC (halo3odst.dll)

`chud_show_cinematic_title`'s script entry (+0x821A60) evaluates at +0x22B164, which calls the implementation
+0x1BA610 (player, title). The implementation looks the title's string up (+0x127684) into a 256-character buffer on
its stack, then calls the chud's text setter **+0x2FE0D4** (user, text, seconds in xmm2). The setter copies at most
0xFF characters into the user's chud block (+0x1110, 0x3B88 bytes per user) and sets the expiry. Its other caller
(+0x1BA568, every player) calls it in a loop with xmm2 set once: LTCG keeps xmm2 live across the call. So the hook is
entered through the register-preserving thunk (`OdstTitleTextThunk`, `native_vr_reader_thunks.asm`), which
`native_vr_reader_thunk_tests` checks.

Found with no recorded address. Two signatures must each match once in the module's code and agree:
chud_show_cinematic_title's call site (+0x1BA684, its rel32 read) and the setter's own body (+0x2FE0D4, a function
start in .pdata). `halomccvr_odst_prompt_resolve_check` runs this on the DLLs on disk: halo3odst.dll RESOLVED
(setter +0x2FE0D4, call +0x1BA684), halo3.dll NOT RESOLVED. The hook is installed with the ODST action bridge. If it is
not found, one log line names what is missing ("ODST tutorial text: the game's own words kept; ...") and the bridge
is otherwise unchanged.

### HUD prompts (MCC)

"Press [Back] to open VISR NAV data" (and COMM, INTEL, database, "activate Visual Intelligence System") is not a title:
it is a HUD message (`tags\ui\hud\hud_messages`, in every ODST map). Seen in the 2026-11-21 video at 03:46-03:50 and
absent from the title log. halo3odst.dll **+0x30052C** (user < 4, string id, buffer) fetches a HUD message for a
player: the user's chud definition (0x1FC-byte records, its message list at +0x184), the string lookup (+0x127684)
into the buffer, then an in-place post-pass with the buffer's size 0x100 (+0x32301C). It is hooked through the
preserving thunk (`OdstHudMessageThunk`; the thunk now hands the native r8, the buffer, to the hook in r9) and the line
is rewritten in place after it returns. The chud fetches its lines as it draws them, so a line is counted once per
showing. Found by one signature (`kHudMessage`, a function start); `halomccvr_odst_prompt_resolve_check`: RESOLVED
+0x30052C. Only lines the rewrite knows change ("Hold [use] to open door" keeps its picture).

On Project Reclaimer the HUD path is the client's own and not found yet. Its title-lookup hook now logs any other
caller that fetches one of these VISR prompts ("ODST HUD prompts (Project Reclaimer): ... is looked up by exe+0x...").
That log line names the caller to hook next.

## Project Reclaimer 0.10.7 (client SHA-256 `D361EC8D8A22BB2A…`)

The client runs `chud_show_cinematic_title` as its own native (script table entry, native exe+0x432A180). That native:

1. clears a 0x800-byte stack buffer (1024 characters);
2. has the string looked up into it through the client's version of Halo 3's lookup (exe+0x4250170, the guest context
   in rcx, the buffer at +0x40);
3. measures it (at most 255 characters, `cmp rdx,0xFE`);
4. copies it inline into each player's chud block (+0x1110).

There is no setter to hook. Instead the lookup is hooked (through the same preserving thunk). Only when the lookup
returns to one of the native's two calls, and only while an ODST level runs, the buffer is rewritten before the native
measures it. The rewrite is never more than 255 characters, well inside the 1024. `reclaimer_client::FindOdstTitleText`
finds all of it by shape: the native through the script table, the buffer's clear and hand-over naming the same frame
slot, the 255 measure, one lookup function for every call, and the lookup's read of the buffer from its context.
`halomccvr_reclaimer_client_file_check` reports `ODST title native exe+0x432A180` and `ODST title lookup exe+0x4250170`.

**The lines arrive with Reclaimer's pictures already in place (0.10.8, log of 2026-11-21).** The lookup's buffer holds
the client's own button pictures (U+E100 A, U+E101 B, U+E102 X, U+E103 Y, U+E104/E105 bumpers, U+E109 D-pad, U+E10D
Back, U+E110/E111 sticks) or, while the client thinks a keyboard is in use, keyboard pictures (U+E13A, U+E143) and key
names in brackets ("[Z]", "[1]"). None of the round-2 run's 24 lines was rewritten. A line with only key names carried
no picture, so it was not counted either (the "activate VISR database" prompt). So the rewrite first matches each line
by its words to the kit's English line (`detail::kKitLines`, every 11th string of the h100 list; a picture or a
bracketed key name is one mark, whitespace runs one space) and says the kit line. The find still resolved on 0.10.8
(native exe+0x44BB730, lookup exe+0x43E0B60).

The input-device byte (exe+0x84C3913, which picks controller or keyboard pictures) belongs to the Project Reclaimer
UpKeep session's fix and is not touched here. The VR words replace the pictures in the tutorial lines either way.

## The wording

Only English lines of the forms the tutorials use are rewritten:

- `Press|Use <picture> [or <picture>] to|for ...` becomes `<control> [or <control>] to ...`, with any picture later in
  the line said too ("... and press X to stop").
- a choice line `<picture> Continue` / `<picture> Repeat <picture> Decline` / `Accept Settings` / `Invert Camera`
  becomes `<control> to continue`, apart by commas.

Each control is said as an instruction: "press A", "pull the right trigger", "squeeze the left grip", "push the right
stick up", "hold the left grip and press X". A D-pad direction is the D-pad gesture: "hold the left controller by your
head and push its stick up" (the right controller with `dpad_hand`). Back is that gesture's click. Two controls that
share words are said once: "push the right stick up or down", "squeeze the left or right grip". Left-handed swaps the
trigger and grip sides. Lines longer than 60 characters break at a space. VR Mappings must be on.

What names what:

- accept, the map's place waypoint, play and pause: the **Confirm** row; cancel: the **Reject** row;
- the map's zoom, data block up and down, pages: the **Map zoom in/out**, **Map D-pad up/down** and
  **Map previous/next page** rows;
- VISR: the flashlight row's own control (else its first further input);
- the map's X and Y lines and the repeat-training answer: the row whose action the game's layout puts on X or Y;
- look: "aim with your gun hand". The look training waits on `player_action_test_look_relative_*`, test bits 7-10,
  which the tests updater (+0x114FD0) sets from the control builder's look deltas (+0x18 yaw, +0x1C pitch): the
  look stick. In VR the mod steers that stick from the gun hand's aim ("VR aim override active"), so the headset
  alone does not answer it;
- move map: "push the left stick"; NAV beacons: the D-pad gesture up and down.

Kept as the game's own words: a line in another language or of another form, a picture this file does not know, a
control the player left unbound, or a line too long for the chud. The log counts both ("ODST tutorial text: N lines said
in VR words, M kept as the game's own") and names the last line kept.

## Not established

- In-headset: that the chud shows the rewritten lines in both MCC editions and Project Reclaimer, and that 60
  characters fit the title's text box.
- The beacon lines assume the engine's default layout (D-pad up and down); a player who moved those actions in MCC's
  own layout would see the defaults named.
