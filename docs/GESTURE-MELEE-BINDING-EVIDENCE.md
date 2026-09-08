# Gesture melee: active controller binding evidence (work in progress)

This is offline evidence and source progress, not runtime acceptance.
The requested behavior preserves Halo 3's accepted velocity-triggered gesture,
but sends the active game's melee binding instead of assuming right shoulder.
True contact remains independent. Both modes must not attack twice per swing.

## Verified action identity and remap readers

Each row comes from that title's official editing kit. A named UTF-16 UI
`button_melee_attack` table entry selects the glyph callback and switch case;
the callback selects the action in the current controller preferences. These
are action numbers, not XInput buttons or controller preset numbers.

| Title | Kit named table entry RVA | Glyph case / kit callback RVA | Melee action | Kit abstract-state reader RVA | Pinned retail homolog RVA |
|---|---:|---|---:|---:|---:|
| H2 | AA8E3C | E44A / 312EC7 | 5 | Still tracing the H2 binding record | Unresolved |
| H3 | 1281110 | E447 / 98D390 | 5 | 4D9BC0 | 187028 |
| ODST | 132D3E0 | E447 / 9F4780 | 5 | 52DE30 | 1B8420 |
| Reach | 20A24B0 | E448 / 909730 | 4 | 1D6B70 | D1070 |
| H4 | 24FE440 | E44A / 97B530 | 3 | 1FFED0 | 13F5CC |

Verified state layouts and bounds (kit assertions explicitly name
`button_remap`, `k_button_action_count`, `k_abstract_gamepad_button_count`):

| Title | Remap offset | Action count | State stride | Controller-index offset | Gamepad state size | Keyboard state offset |
|---|---:|---:|---:|---:|---:|---:|
| H3 | C0 | 43 | 518 | 514 | C | 1CC |
| ODST | C0 | 47 | 558 | 554 | C | 1DC |
| Reach | 100 | 49 | 700 | 6FC | 10 | 224 |
| H4 | 100 | 55 | 7E8 | 7E4 | 10 | 254 |

All values above are hexadecimal except melee action. The retail readers
repeat the relevant kit's layout, controller-index/input-device branch and
action-to-gamepad remap. Device mode 1 selects keyboard input; it must not be
interpreted as a gamepad binding. Other values still need admission review.
The engine readers write a last-action identifier into the returned button
state; the VR lookup should read the validated remap directly, without calling
these mutating functions or installing another hook.

H4's reader BSim top hit `4EDD68` was unrelated and rejected. The second result
`13F5CC` matches the complete kit remap logic. This is why similarity rank alone
is not a binding.

## H4 controller transport and state

H4EK raw XInput conversion `342E60` reads XINPUT_STATE byte6 into abstract
button0 (left trigger), byte7 into button1 (right trigger), then word4 through
the 14-dword mask table at `19B56E0` into buttons2..15. The table is:
`0001 0002 0004 0008 0010 0020 0040 0080 1000 2000 4000 8000 0100 0200`.
The kit's `201B40` copies these 16 raw button states into abstract state slots
and copies preferences+18 into state+100 for its 55 action remaps.

H4EK preferences getter `200560` reads controller preferences at `44728100`,
stride18C; state getter `200EF0` returns `44729004 + controller*7E8`.
Retail init `13F628` initializes preferences at `2E94AD0`, sets controller
indices at `2E961B8 + controller*7E8`, and initializes 16 states. Retail caller
`A2818` multiplies the controller by7E8; RIP load at `A281F` resolves the state
array to `2E959D4`, then `A2832` calls the verified reader13F5CC. Unique loaded
signatures, native update consumer checks and refresh/lifecycle guards remain
required before publishing a runtime binding.

## H2 lead, not a retail binding

H2EK `9BCC0` is explicitly the input-abstraction controller preferences getter:
it copies17C4 bytes from `FF98B8 + controller*17C4`. The named melee glyph case
passes action5 through `314B6B -> 314C01 -> 9BA80`; this path emits a typed
binding record, rather than proving that another title's remap layout applies.
Continue tracing this record and its native input consumer.

## Preserved files and remaining work

### Connected source update

`gesture_melee_bindings.inl` now performs the cold, loader-pinned read and
publishes generation/title-tagged atomic snapshots for the input hook. It
checks two unique signatures per title, resolves and bounds each native state
array, and expires input snapshots after150 ms. No engine function is called
and no engine state is written by this lookup. Named identities, ten unique
native patterns, five state pointers and all five kit transport tables pass
`tools/verify-gesture-melee-bindings.py`.

H2's complete matched path is kit9BA80 / native6D9A00. Native6D9B10 matches the
kit controller-preferences getter and resolves preferences to15EB7A0, stride
17C4. Action blocks are100 decimal bytes, with count at+1C and up to eight
12-byte {type,index,hold} records at+20. The named gamepad type is2; native
6D9A00 maps it to the preferred-device family1. Kit9C150's parser explicitly
bounds the count to8 and identifies the third field as held input. Axis or
held bindings are currently declined and logged instead of fabricated.

H2's own raw XInput mask table is kit7B2F78 / retailB15928 (native reference
6D20C5), with the first two buttons as analog triggers. Its buttons10/11 are
shoulders and12..15 are face buttons, unlike H3/ODST/Reach/H4. The named kit
table A5A1E0 begins `gamepad_analog_button_left_trigger`; right shoulder is
entry11 atA5A20C. Kit conversion49D80 uses the14-mask table. Other kit converters
and tables are H3:4FC7B0/109F038, ODST:554A90/115F8A0,
Reach:2511F0/1649098. Each was inspected independently.

The resolved native state arrays are H3:2229B54, ODST:227AA54,
Reach:287FF94 and H4:2E959D4. Their own native stride/RIP-load patterns resolve
these addresses at runtime; no other title's address is reused.

`Game_GestureMeleeInput` now drives the configured XInput button or trigger.
Both mode toggles enabled gives True physical melee priority, explicitly
shown in F1; gesture input cannot add a second attack. Gesture-only mode uses
the accepted velocity latch and cooldown. A new title, binding or availability
waits for each hand to settle before admitting a new swing. The speed history
is enabled by either mode. Release/core tests/Reach consistency and native
binding checks pass after connection; the admission-seeding guard is covered
by the subsequent contact-hand-fallback build. Headset validation is pending.

Decompiles: `out/contact-gesture-{h2,h3,odst,reach,h4}-*-kit-console.txt`.
Retail reader disassembly/call leads: `out/contact-gesture-retail-readers-proof.txt`.
H4 additional proof: `out/contact-gesture-h4-{map,state}-retail-console.txt`
and `out/contact-gesture-h4-xinput-kit-console.txt`.
These PowerShell logs are UTF-16; decode explicitly when using Python.

Remaining: complete H2, verify each title's native state publication and
transport, add unique signature/loaded bounds checks, wire read-only lookup,
restore independent gesture detection with contact/gesture deduplication,
exercise the mode combinations and changed bindings, and package only once
the rest of the contact candidate is ready. No installation or accepted-pointer
change. The shared speed history now runs when either melee toggle is enabled;
the prior physical-only gate made Gesture-only mode unable to read velocity.
