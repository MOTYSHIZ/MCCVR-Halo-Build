# CE Anniversary first-person source visibility - September 15, 2026

Halo 3 behavior being matched: the local tracked weapon and hands appear in
both VR eyes while the game's own hidden-weapon decisions remain effective.
This correction addresses Anniversary's missing right-eye weapon separately
from the material-worker lens correction that restores its apparent scale.

## Native cause and correction

The pinned `halo1.dll` is SHA-256
`0A12DC561780F449D3F4D0DF10BB8D3BC7BE7840A5BEB2B236F672EB6CD42E6C`.
The Saber-specific preparation facts below come from that executable's actual
instructions. They do not claim an Anniversary renderer equivalent in HCEEK.
Official-kit-backed gameplay visibility and player ownership facts are recorded
separately in `HALOCE-FIRST-PERSON-EVIDENCE.md`.

| Native code | Verified behavior |
| --- | --- |
| `0x7B2E0` | Produces first-person container exclusion flags from native registration records and currently active models. |
| `0x7AFB1..0x7AFCD`, `0x7B080..0x7B092` | Native backend stereo selects both players; ordinary player-zero preparation clears only its own exclusion request. |
| `0x2CFE50` | Prepares one native model using the supplied complete view list. ABI: RCX list, EDX update serial, XMM2 elapsed interval, R9 container. |
| `0x2CFF94..0x2CFFD9` | A view's selector at `list+0x1C+ordinal*0x3C8` chooses inclusion bit `0x02`/`0x04` and exclusion bit `0x80`/`0x100`. |
| `0x2CFFF5` | The output bit uses the loop's eye ordinal independently of that source-player selector. |

For an active player-zero weapon with native backend stereo off, the producer
leaves source-player-one exclusion `0x100` set. VR's synthetic view selectors
`0,1` therefore admit the weapon only to eye zero. Changing the second selector
to source player zero admits both ordinal eye bits. Source-zero exclusion and
inclusion still apply equally to both eyes; an inactive/hidden model is not
forced visible. The native exclusion flags themselves are never patched.

The optional production hook requires the exact current player-zero model
registration and container, native first-person flags `0x4400`, and an owned
current synthetic list. It snapshots all `0xBDE8` bytes, changes only the second
view's source-player selector in that private snapshot, and calls the native
preparer synchronously. Distinct eye cameras, view flags, auxiliary views,
opaque tail bytes and the original list remain unchanged. The 256-record
lookup limit is a mod safety bound, not a claim about native capacity.

### Private-list lifetime audit

The complete preparer `0x2CFE50..0x2D0713` was inspected, including code after
the emulated admission loop. The list stays in R15 and local derivatives; no
list/camera address is stored into persistent state or compared against a
global list identity. Its highest fixed list access is the dword at `+0xBDE4`.
Container updates and TLS publication retain the native container, not the
temporary list. Output visibility uses scalar masks and distances.

The only helpers receiving list-derived data are `0x11CA70`/`0x11CB10`
(read-only frustum tests), `0x130680` (camera-position reads followed by local
scalar math), and `0x1298C0` (position reads with scalar/point output). Their
complete bodies were inspected for pointer escape. None retains its input
list/camera pointer. First-person flag `0x4000` bypasses the first frustum branch;
the optional submodel branch also uses these synchronous helpers. Thus the
full native list can be borrowed for this call without retaining the stack
snapshot. This is a static native lifetime audit, not an emulated execution of
every submodel/geometry branch.

## Worker handoff

At `0x45526C..0x455284`, native code copies exactly `0xBDE8` bytes from
`job+0x70` to `renderer+0xB0`. Phase-one submission at `0x455506` enters
`0x4AA740`; its exact return address is `0x45550B`. This is distinct from the
phase-zero call returning at `0x455378`. An auxiliary-view branch can first
replace the live list count with `list+0xBDE4`.

The complete submission body `0x4AA740..0x4AAA28` replaces incoming RCX with
its own global engine context, retains RDX as the list, and consumes only R8B
as the phase. It has no incoming R9 or stack arguments and no produced return
value. The primary phase-one caller writes R8B without clearing the rest of
R8, so a full-width phase parameter would be incorrect. Native type-one job
dispatch receives the exact list, the corresponding camera and the saved
phase byte. The function has fragmented unwind ranges: inspecting only its
first unwind range ending at `0x4AA80B` misses most of its body.

`VisibilitySubmitHook` publishes before entering that native submission. The
publisher requires the exact post-copy caller, phase one, owned render job,
copy/render flags, exact source `job+0x70`, destination `renderer+0xB0`, and a
fresh source preparation ticket. Matching camera bytes alone never establish
that a copy happened. A new copied job revokes an old active-list ticket before
overwriting the renderer. Reset, invalid new publication, reference/renderer
changes and teardown revoke copied ownership with an atomic revision.

Worker readers require the exact prepared source or explicitly published copied
destination; both native camera source-player tails must be zero. They recheck
ticket, generation, tracking space, reference, renderer epoch, tracking age and
presentation state. Optional hook or proof failure keeps native visibility and
does not disarm the camera. Both visibility hooks join the existing detour
drain/retirement protocol.

## Verification and limits

Run from the repository root:

```powershell
python tools/re/test_ce_first_person_visibility_native.py --output out/ce-first-person-visibility-native-resume-20260915.json
```

The verifier checks all four binding signatures for unique executable matches,
function-entry unwind records, instruction witnesses and relative targets.
It executes 32 native producer/consumer cases (4,430 instructions), covering
both source players, native stereo on/off, active/inactive models and all four
inclusion combinations. It reproduces the native right-eye omission and proves
the source-zero correction preserves native visibility policy.

Six further cases execute the complete native submission and the relevant
native copy/caller instruction sequences (1,268 instructions). They verify the
full list copy including opaque bytes, the auxiliary count reduction, exact
per-view list/camera dispatch and byte-sized phases including contaminated
upper R8 bits. The original source list stays unchanged.

These are offline instruction tests with named fixture services. Model/palette
lookup is modeled around the executed exclusion branch. The native copy's
arguments execute, while its `memcpy` implementation is a byte-copy fixture.
Submission's memset, security-cookie check, per-view job population and final
asynchronous worker endpoint are fixture services. This test does not execute
an actual asynchronous worker or the complete model preparer.

Separate compiled production fixtures exercise actual hook wrappers and list
ownership, including the full private list, auxiliary/tail preservation,
unmodified hidden flags, foreign models/players, stale/revoked tickets,
stationary-list reuse, blocked presentation, exceptions and recovery, copied
submission publication before native entry, and fourteen-hook retirement.
Native service endpoints and tracking inputs in those fixtures are synthetic.
Final cumulative build and CTest results are recorded by the candidate package.

The first compiled native-exception regression exposed an in-flight callback
counter leak: ordinary C++ scope destruction did not handle Windows structured
exceptions in these new wrappers. Both new hooks now use explicit
`__try`/`__finally` counter cleanup, consistent with the existing particle hook.
The targeted production fixture passes after that correction. Its native
exception still propagates to the fixture's handler; the original list is
unchanged, callbacks drain to zero and the next valid call recovers. This does
not claim to make arbitrary native faults recoverable in a running game.

Preserved disassemblies under `out/ce-visibility-*-resume-20260915.txt` include
the complete native preparer, complete submission and copy/phase-one caller.
The user's headset still determines visible right-eye hands, all animation and
weapon variants, graphics transitions and final scale. This evidence does not
advance `CURRENT-STATE.md` or establish cumulative headset acceptance.
