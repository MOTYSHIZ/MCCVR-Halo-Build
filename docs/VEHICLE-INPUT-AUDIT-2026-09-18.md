# Vehicle input audit across titles

Halo 3 is the behavior reference: vehicle throttle passes through unchanged by
head-relative walking, controller steering retains native turn/seat constraints,
and known turret oscillation is damped by the accepted measured-step servo.
Local source/numeric tests do not establish headset driving quality.

| Title | Result |
| --- | --- |
| Halo 3 | Existing seat-specific steering, wheel/manual steering while hull-following, native-limit reticle and turret hysteresis retained. Existing seated throttle bypass retained. |
| ODST | Existing own seat follow and shared turret hysteresis retained. Fixed missing ODST throttle bypass: the shared walking branch checked only Halo 3's seat predicate. Now checks ODST's own installed, debounced, fresh native seat state first. |
| Reach | Existing native occupancy, root-hull hand-steering deadband/gain, wheel/manual follow rules, flight exclusions and turret damping retained. Existing seated throttle bypass retained. |
| Halo 2 | Coherent seated reference and throttle bypass corrected; see HALO2-VEHICLE-REFERENCE-2026-09-18.md. |
| CE | Configured seated VR turn and optional actual hull view-follow added without changing native control packets; see CE-VEHICLE-VIEW-2026-09-18.md. |
| Halo 4 | Added independently verified local seat admission and seated throttle bypass. Existing aim-loop gain and native engine steering limits retained; no unsupported title-specific vehicle classes or tuning constants added. |

No evidence here establishes that an unspecified vehicle handles correctly in a
headset. H3/ODST/Reach accepted steering was not retuned from assumptions. This
audit does not add vehicle camera anchors, wheel support or new HUD coverage.

## H4 local seat admission evidence

Official H4EK `1FED00` reads input-user unit from player mapping TLS `4E8`,
array `14`. `1FE570` finds an active input user through player array `4`.
`1FF970` identifies `player_mapping.cpp`, joins those input records to a player,
and calls `1FEF60` to update the input-unit record. `1FEC30` reads output-user
player array `B8`; `1FED90` reads output-user unit array `C8`, including a
debug/output override whose object must resolve as a native unit.

Retail setter `959A8` matches `1FEF60`, including its recursive detach, array
`14` write and control-reset call. Retail simulated-input function `144B50`
matches H4EK `2539F0`, including its input-unit lookup, damage-direction
projection branches and trigonometry. This independently verifies the retail
mapping TLS offset **138**, which differs from the kit's 4E8.

Retail output getter `95BB4` matches `1FED90`, including its user bound, output
override branch, native object-mask 2003 query and ordinary array C8 path.
The mapping cluster's input-player reads (`958D6`, `95927`) and output-player
getter (`95BAC`) retain arrays 4 and B8. The cold witnesses cover these readers.

H4EK `UnitInstance::Update` at `E6A1B0` resolves parent `24` with signed seat
`2C`, object mask 2003. Retail `5ECCB4`, specifically `5ED434..5ED44C`, retains
the same branch and native object getter. The already established H4 datum
getter `43AD8`/object getter `5DA400` prove full salt, kind and object storage;
see CONTACT-RESUME-CHECKPOINT.md. No H3 or Reach layouts were copied.

The new reader requires output zero's native unit to match the mapping and a
unique active input player/unit pair. It validates complete salted object
handles and type masks, then rereads ownership and seat before publishing.
Remote units, debug/spectator overrides with no matching input, reused slots,
missing tables, inconsistent parent/seat and generation changes remain unknown.
Unknown retains existing input for this feature and is counted in worker logs.
Memory/native-query exceptions leave the output unchanged and drain the reader.
Removal disables admission and proves callback/function quiescence before the
module dependency is cleared. No new native hook or game-memory write is added.

Cold signatures are in HALO4-VEHICLE-INPUT-BINDINGS-2026-09-18.json; offline
`tools/re/verify_h4_vehicle_input_bindings.py` checks the pinned hash, uniqueness
and generated header. Inspection is under `out/reload-policy/h4-*unit*.c`,
`h4-player-routing-{kit,retail}.c`, `h4-player-mapping-kit.c`, plus the preserved
`out/contact-h4-unit-parent-kit-console.txt`.

## Validation

ODST bypass: cumulative Release/all 41 CTests passed (`*odst-throttle.log`).
H4 production reader tests pass 27 checks, including remapped input/output,
both seat states, stale/foreign ownership, ambiguous input mapping, native and
memory faults, and actual compiled unwind-based retirement. Full cumulative
Release/all 42 CTests/Reach gate pass (`*h4-vehicle-input.log`); all seven pinned
H4 bindings verify uniquely and match the tracked header. No headset acceptance,
installation, launch or package has occurred.
