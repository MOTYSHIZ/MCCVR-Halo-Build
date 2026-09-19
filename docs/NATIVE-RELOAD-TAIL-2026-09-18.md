# Shortened native reload animation (unaccepted candidate)

The requested reference behavior is the weapon's own reload beginning at its
authored insertion point and continuing through its chambering tail. It must not
substitute a separate equip/ready animation or finish the reload instantly.

The user's subsequent correction explicitly preserves the existing
`manual_reload_skip_animations` setting and its full-disable behavior. The new
`manual_reload_shortened_animation` setting defaults off, requires Manual Reload,
and has lower precedence than full disable. Both values persist independently.
The menu says "Shortened reload animation" and explains precedence. Shortened
mode alone does not suppress ready animations or change their duration queries.

## Native evidence

All addresses below are independently read from each official editing kit and
matched to the pinned retail module. Retail addresses are RVAs. Reach discovery
started in HREK, not retail. The generated binding ledger records input hashes
and unique signatures; no address copied between titles is used as evidence.

| Title | Official kit seek / position evidence | Retail animation getter / event getter / seek |
|---|---|---|
| CE | `005cb400` writes FP animation index +18 and frame +1A; own FP state/graph selection | FP play `b285a8`; tag resolver `a9b648`; own animation record primary +34, length +22 |
| H2 | `0068c0e0` animation_channel.cpp frame seek; `0068b360` descriptor | `7a0730` / `79df70` / `7a0d60` |
| H3 | `14080fa80` channel seek; `14046bae0` descriptor | `15d174` / `231968` / `26b1a0` |
| ODST | `140876910` channel seek; `1404c07d0` descriptor | `18b158` / `26062c` / `296ff8` |
| Reach | `1407022a0` channel seek; `1406fe070` descriptor | `211310` / `217d38` / `20ef14` |
| H4 | `140566e70` channel seek; `1405d28a0` descriptor | `2f9028` / `2ef5a4` / `2f9a00` |

H2/H3/ODST primary event is 0. Reach/H4 primary event is 1, as confirmed by
their own duration routines. H4 seek has a third manager argument: the official
function asserts it exists, and manager apply `140549510` initializes its primary
channel using the same manager as context. Retail `2f93b8` uses that context to
resolve the channel's cached animation state. Passing an invented two-argument
H3 ABI to H4 would be wrong.

Each FP layout is verified at its own native play routine:

| Title | FP storage | User stride | Slot stride | Weapon handle | Primary channel | Frame |
|---|---|---|---|---|---|---|
| CE | global pointer `2d9cd90` | 1e94 | one | 8 | FP record | short +1a |
| H2 | global pointer `187c300` | 20fc | 1028 | 10 | 14 | float +20 |
| H3 | native module TLS +568 | 2430 | 11bc | 3c | 5c | float +18 |
| ODST | native module TLS +598 | 4f38 | 2740 | 3c | 5c | float +18 |
| Reach | native module TLS +6a0 | 53a8 | 2978 | 3c | 60 | float +24 |
| H4 | native module TLS +6a0 | 5f48 | 2ec8 | 6c | b8 | float +28 |

The PE TLS directory supplies the module's loader-owned TLS index. CE/H2 global
references have separate unique instruction proofs. CE's animation array has
stride B4 and is resolved through its own tag-bank globals `2d9ce10` / `2ea3410`,
each verified in the native duration routine. The actual selected FP animation
index is used; a copied generic reload animation index would select the wrong
empty/nonempty or shotgun variant.

## Transaction and timing

The existing native start/state/action/play functions still run. A stack-scoped
receipt records the exact played user, weapon slot and animation. The seek occurs
only after the native state/start returns, including H4's ordinary fallback play
after its specialized animation checker returns false. Multiple play receipts,
foreign handles, stale generation, nonzero initial frame, different animation
identity, absent channel or missing insertion event keep the full native reload.

For CE, the frame counter advances within the same selected animation. Modern
engines call their own channel seek, updating event bookkeeping and normalized
position. A clamped event restores frame zero and keeps native timing. Guards and
SEH isolate access faults to this optional feature; no camera teardown occurs.

Native elapsed insertion ticks come from the initialized magazine primary timer
(H2 +12, H3/ODST/Reach/H4 +16). CE uses its selected animation's authored frame,
with a check that the remaining gameplay timer covers the visible tail. Both
native total and transfer countdown retain the tail. Ammo transfer is deliberately
left until the end of that retained countdown, using the engine's own update and
reserve/eligibility rules. No ammo, reserve, reload state, original-duration copy,
network ownership or firing callback is fabricated or replayed.

H3/ODST's post-transfer delay is magazine +20. Reach/H4 moved that delay to +22;
their +20 is a separate native reload-event countdown. The post-transfer delay
is zero because transfer now occurs at the end of the tail. Positive Reach/H4
event countdowns are advanced by elapsed time, with at least one tick retained
so native transition-to-zero delivery still runs. Other timers remain untouched.
Native loop-intro/exit states and animations without an interior insertion event
remain stock. Shotgun continuation still belongs to native code.

## Verification and limits

The original full-disable regression suite still passes. Added production-detour
fixtures exercise all six titles, native play, insertion seek, remaining gameplay
time, H4 manager ABI, missing/clamped event fallback, foreign handle, generation
change, duplicate receipt, independent failures, all config combinations and full
disable precedence. 387,769 native reload checks passed locally. Generated
signature verification and cumulative Release/CTest results are recorded in the
work ledger when complete.

This is source/native-code and fixture validation, not headset acceptance. Actual
chambering appearance, custom animation tags, network hosts/clients and Halo 3
regression still need user testing. CURRENT-STATE is unchanged. Packaging remains
held for the entire standing refinement list.

Retained RE output: `out/reload-policy/` files `h3-channel-seek`,
`h3-tail-helpers`, `h3-tail-transfer`, `h2-kit-seek`, `h2-tail-seek`,
`h2-tail-channel`, `odst-tail-kit-seek`, `odst-tail-native-seek`,
`reach-tail-seek`, `reach-tail-native-seek`, `h4-kit-seek`, `h4-kit-apply`,
`h4-tail-native-seek`, `h4-tail-context`, `ce-fp-play`, `ce-native-play`,
`ce-start-match`, plus the previously retained native magazine state/update reads.
