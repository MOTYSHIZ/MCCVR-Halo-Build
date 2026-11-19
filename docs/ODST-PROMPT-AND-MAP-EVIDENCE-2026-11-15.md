# ODST tutorial prompts and the VISR map: game state and input (2026-11-15)

VR Mappings' ODST prompt and map rows (`vr_mapping::OdstAction`, F1 Controls › VR Mappings, "Settings for: ODST") act
only while one of ODST's tutorial prompts waits for an answer or the VISR map is open. This records what tells the mod
that, and what the game reads for the answer. Read only: the mod writes no game memory for this.

Binaries (SHA-256):

- `halo3odst.dll` `5BB20976EFDFD9E1CE59C589339804725FEC239021027C8D65B2733EAB94829A` (Steam; the Store module is
  byte-identical apart from its signature, `MCC-EDITIONS-EVIDENCE.md`)
- `halo3.dll` `B209D8454B12DC77E54CCD2C9924EC8D44B8619D21CF98E36FFAF601E67EFB63`
- H3ODSTEK mission scripts (`H3ODSTEK.7z`: `data/globals/global_scripts.hsc`,
  `data/levels/atlas/h100/scripts/h100_mission_l100.hsc`)

## The prompts (H3ODSTEK scripts)

The tutorial prompts are the global scripts `f_display_message_accept`, `f_display_message_cancel` and
`f_display_message_confirm`. Each shows its text (`chud_show_cinematic_title`), resets the unit's action tests, calls
`unit_confirm_message` and/or `unit_confirm_cancel_message`, and sleeps until `unit_action_test_accept` or
`unit_action_test_cancel` (`f_display_message_confirm`: either, then the A or the B sound). Mombasa Streets' look
training (`f_l100_look_training`, `f_training_look_rejected`) asks with `f_display_message_confirm` ("accept or
invert") and the PDA training asks with `f_display_message_accept` throughout. `f_display_repeat_training` uses a retrain
prompt and cancel.

## The game state (halo3odst.dll)

Script function table entries (+0 return type, +8 name, +0x20 evaluate):

| Script function | Entry | Evaluate | What it does |
|---|---|---|---|
| `unit_confirm_message` | +0x81F230 | +0x22D1AC | controller's action-test record +0x1C = 1, clears test bit 1 |
| `unit_confirm_cancel_message` | +0x81F410 | +0x22D234 | record +0x1D = 1, clears test bit 2 |
| `unit_action_test_accept` | +0x82DA10 | +0x225180 | test bit 1 (+0x114A64) |
| `player_action_test_accept` / `_cancel` | +0x82E3E0 / +0x82E3A0 | +0x224B84 / +0x224BC4 | test bits 1 / 2 |
| `pda_is_active_deterministic` | +0x81FE30 | +0x224FB0 | test bit 3 |
| `player_action_test_pda_active` | +0x82E5C0 | +0x224A44 | test bit 3 |

- **The action-test record.** Engine TLS (`gs:[0x58][index]`, index at +0xA8FB1C, the same index both confirm functions
  load) + 0xD8 points at four 0x30-byte records, one per controller (`lea rcx,[rax+rax*2]; shl rcx,4`). +0x00 is the
  tests word; +0x1C..+0x1F are four prompt bytes.
- **Who sets the tests.** The tests updater +0x114C38 (called with the controller, its abstract input state and the
  control builder's output) sets test bit 1 from output +0x28 bit 5 and bit 2 from bit 6 (+0x114CCA), and sets or clears
  test bit 3 from output +0x30, the PDA up (+0x114FB9).
- **Who sets the output.** The control builder +0x10FBFC reads action **0x12** through the abstract reader +0x1B8420 into
  +0x28 bit 5, and action **0x13** into bit 6 (+0x10FD11..+0x10FD6E). While a prompt byte is set and its answer is
  pressed, the builder consumes the answer's record (+0x10F4DC) and the other actions on that button (+0x10FB4C): in flat
  play, A answering a prompt does not also jump.
- **Who clears the prompt.** +0x11240A (inside +0x112184): once a set prompt byte's test fires, all four bytes
  (`and dword [r15+0x1C],0`) are cleared.
- **The buttons.** No MCC control converts to 0x12 or 0x13: running ODST's MCC-control converter +0xF08A4 for all 66
  controls gives neither. The engine's default preferences (+0x1B9328) put 0x12 on button 0xA and 0x13 on 0xB of the
  binding table, A and B (`GestureTransport`). The host layout's face-button swaps (+0x305B5F on) can move them like any
  other action; the mod reads the player's own binding for both (`Game_ReadOdstPromptButtons`).

## The VISR map

The map is the PDA. `pda_input_enable_dpad` and the other `pda_input_enable_*` (+0x22D064 etc.) set bits of a PDA
input mask (engine TLS + 0x1D0, 0x20 bytes per player, +0x1C). The PDA screen's event filter +0x30A3B0 tests that mask by
**UI controller event**: 0 (A) bit 0, 1 (B) and 0xD bit 1, 2 bit 2, 3 bit 3, 8-0xB (the D-pad) bit 4, 0x12-0x15 and
0x18-0x19 (the sticks) bit 5. So the map takes the gamepad's own buttons as UI events -- A, B and the D-pad themselves,
not game actions -- and the map rows press those.

## What the mod reads (MCC)

`src/common/odst_prompt_resolve.h` resolves all of it with no recorded address, once the ODST action bridge's hooks are
installed. Six signatures must each match exactly once in the module's code, wherever they are: the builder's accept read
(+0x10FD11 on the pinned build), both confirm functions' record stores (+0x22D1C6, +0x22D24E), the tests (+0x114CCA), the
PDA test (+0x114FB9) and the prompts' clear (+0x11240A). The numbers used are read from those instructions: the reader's
return address, the engine TLS index, the record block (0xD8), the prompt bytes (0x1C, 0x1D). Cross-checks: the
builder's read calls the reader the bridge hooked; both script functions load the same TLS index, which is the module's
own (PE TLS directory), and name the same block; the cancel byte follows the confirm byte; the builder and the tests
name the same output field (+0x28); the PDA test lies in the tests' function (.pdata); the clear names the same bytes.

Then at the builder's read of accept for controller 0 (on the game thread, once a tick) the mod reads the record's
prompt bytes and the PDA bit and publishes them with a time stamp. The input thread takes a prompt (confirm or cancel
byte set) or the map only from a publication less than 250 ms old: a paused game runs no control tick, so the rows go
quiet.

If anything is not found or does not agree, one log line names each ("ODST prompt and map rows: off, normal controls
kept; not found or not agreeing: ..."), nothing is read, and every control keeps its usual job. The ODST action bridge
itself still proves its own hooks at recorded offsets (its existing design); if it does not install, the rows are off
and the log says so.

The offline check `halomccvr_odst_prompt_resolve_check` (tests/odst_prompt_resolve_check.cpp) runs the same resolution
on the DLLs on disk (`--mcc <MCC folder>`, `--dll <halo3odst.dll>`, or `HALOMCCVR_MCC_DIR`; skipped without one):
`halo3odst.dll` must resolve whole and `halo3.dll` must resolve nothing. On the pinned builds (2026-11-15) it reports
halo3odst.dll RESOLVED with the values above and halo3.dll NOT RESOLVED (all six signatures not found).

## Project Reclaimer

Project Reclaimer runs ODST's levels on Halo 3's engine (`halo3.dll`, recompiled in 0.10 and later). `halo3.dll` has
`player_action_test_accept` (+0x7E1F80) and `_cancel` but **no** `unit_confirm_message` and no PDA: none of the six
patterns above occurs in it, and its control builder has no PDA output. The Project Reclaimer UpKeep session confirms
(0.10.4 client): Reclaimer runs `unit_confirm_message`, `unit_confirm_cancel_message`, `unit_action_test_accept`,
`unit_action_test_cancel` and `pda_is_active_deterministic` as native functions of its own exe ("[hs] the native <name>
runs"), and its PDA is the client's own ("[odst] PDA: {} opened for controller {}"). Reading the client's own code goes
past AGENTS.md's Reclaimer exception (locating halo3.dll's functions only); the user asked for these controls on
Reclaimer (2026-11-16), and the client finds follow the shape-proven, offline-checked pattern of
`reclaimer_client_finds.h`. ### Reclaimer's prompts (0.10.4 client, read since 2026-11-16 at the user's request)

Client `project-reclaimer-client-v0.9.23.exe` (0.10.4), SHA-256
`2197A2F2933177ABBE1D7A77A007164A703835CE0E2D098B568875D015FD190C`.

- **The script table.** Entries of 0x38 bytes: the name's address and length, the return type's, the parameters', then
  the native function. `unit_confirm_message` (exe+0x6176290) runs exe+0x3734120 (`xor edx,edx; call setter`);
  `unit_confirm_cancel_message` runs exe+0x3734140 (`mov dl,1; call setter`).
- **The setter** exe+0x373A3D0 ORs `1 << kind` (0 confirm, 1 cancel, 2 retrain) into a byte per controller at
  exe+0x694A3C4 (client data): `lea rdx,[rip+..]; mov r8b,1; mov ecx,ebx; shl r8b,cl; ...; lock or byte [r14+rdx],r8b`.
- **The answer.** The client's control step (exe+0x3FC3400) tests the controller's byte, asks whether Halo 3's own
  abstract input record for action **0x12** (confirm) or **0x13** (cancel) is pressed (exe+0x3733DD0: `mov edx,0x12` /
  `0x13`, then the record at guest image + 0x2229B54 + controller * 0x518, Halo 3's abstract state), and clears the byte
  once answered (exe+0x3FC3AD4). A level reset clears all four (exe+0x3733090).

So on Reclaimer the confirm and reject rows press the buttons Halo 3's layout gives actions 0x12 and 0x13 (read like
every other action from Halo 3's binding table), and the mod reads controller 0's byte (bits 0 and 1).

`reclaimer_client::FindOdstPrompt` (src/common/reclaimer_client_finds.h) finds it by shape: the setter's store and the
control step's clear, each unique, must name the same bytes; the bytes must lie in the client's data; both natives,
found through the script table by name and by their own bodies, must call that setter. Offline:
`tests/reclaimer_client_file_check.cpp` reports `ODST prompt setter exe+0x373A3D0`, `ODST prompt bytes exe+0x694A3C4`.

### Reclaimer's VISR map: not found

Reclaimer opens the PDA as a screen in Halo 3's UI (exe+0x36E8D70 logs "[odst] PDA: {} opened for controller {}").
Its `pda_is_active_deterministic` (exe+0x3731B40) reads a 20-byte record per player (exe+0x372F8E0) whose flags word
gives "active" by bit 5 or bit 11, but the record is found through run-time pointers into the game's state (with a
static fallback in the client's data), not a fixed place. Unproven without a live probe, so the map rows stay off on
Reclaimer and the right stick keeps turning there.

**Log-only probe (2026-11-18, 0.10.5 client SHA-256 `F2E6F0B7…8173`).** `reclaimer_client::FindOdstPdaProbe` finds the
native through the script table, its one call of the record reader (exe+0x3756CF0), and in the reader its "ready" and
snapshot checks, the game-state record's address arithmetic (guest image global +0x204FD18; + lo32 + hi32 of the
layout struct + 0x20000 + its dword +8 - 0x1C8DC, aligned down to 16; + 0x1854 + player * 20) and the static record
(client data + 0x1858 + player * 20). On 0.10.5 the prompt find also moved and still resolves (setter exe+0x37617E0,
bytes exe+0x69CF404). The DLL reads player 0's two records and logs a line each time either flags word, the snapshot's
selecting byte (+0x44) or "ready" changes ("ODST map probe (Project Reclaimer, log only): ..."). Nothing acts on them:
the user's run with the map opened and closed decides which record, if either, tracks it.

## Not established

- In-headset behaviour. The user's report ("A and Y both confirm, the right stick click rejects") is not explained by
  this evidence; the engine default is A and B. The log line `ODST prompt and map rows: the game's accept is on 0x..,
  its cancel on 0x..` names the buttons the player's layout gives them, for the next report.
- Split screen: only controller 0's record is read.
