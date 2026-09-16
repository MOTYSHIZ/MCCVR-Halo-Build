# CE renderer resource lifetime and graphics-switch crash

Evidence ID: **E-CE-RES-LIFETIME-1**. This is an offline crash diagnosis and
native-control-flow verification, not headset acceptance.

## Observed failure

The user's `2cf002b6b41dd69b2515469b...` log names MCC PID 26776. The matching
Windows crash dump is preserved under
`out/test-runs/2cf002b-ce-reach-feedback-20260915/` as
`MCC-Win64-Shipping.exe.26776.dmp` (SHA-256
`98E8590634329CE53B2872807D483CC913B36EB4E06FF38CCDB8B594C4305863`).
It records `0xC0000005`, a read of address `0xC`, at `halo1+0xB0EBC1`.

The native instruction reads the current technique index from an effect
record. Its preceding load at `+B0EBB8` obtains the record from `R14`.
In the dump, `R14=halo1+1B7D1F0`, the `hud_meters` slot, and its record pointer
is zero. All 138 effect-record pointers are zero. Native initialized state
`+2EA2D5C` is also zero, while the late HUD callback and its draw gates are
active. The local dump for PID 24844 records the same fault and same loaded
mod PE identity; it is another failure of this tested build.

The actual AMD64 unwind and native call instructions place the failing draw
inside the ordinary late HUD callback `+740B0`, through the mod's original
HUD fallback. It did not enter the prepared HUD target transaction. Disabling
that transaction alone therefore cannot repair this crash.

The current directory in the dump is the MCC installation root. The existing
native `halo1/shaders/fx.bin` file is present. These observations do not support
the proposed wrong-current-directory explanation.

## Native owner and reset sequence

Pinned retail `halo1.dll` SHA-256:
`0A12DC561780F449D3F4D0DF10BB8D3BC7BE7840A5BEB2B236F672EB6CD42E6C`.

The constructor at `+82440` and destructor at `+82520` install the vtable at
`+17F0C98`. Relevant entries are:

| Slot | Entry | Observed behavior |
| --- | --- | --- |
| `+60` | `+80DD0` | Recreates the backend; reloads Classic resources only when `+2EA2D5C` is nonzero. |
| `+68` | `+82410` | Marks backend state dirty and calls a context callback; it does not recreate the texture pool. |
| `+70` | `+82170` | Clears `+2EA2D5C`, destroys shader resources and all 138 effect records, then releases backend resources. |

The live backend object page is absent from the minidump; the table identity
is established from the pinned constructor and checked against both live
virtual entries before the mod performs a managed reset.

The full-resolution management hook called native `+4F1AD0` followed by
`+4F11E0`. The first function drains the native outstanding job and eventually
calls backend virtual `+70`, thus `+82170`. Its native `+1F0250` tail clears
backend `+128` bit 26, so reconfiguration enters its backend recreation branch
and calls virtual `+60`, thus `+80DD0`.

At `+821C1`, disposal writes zero to `+2EA2D5C`. At `+80E3C`, reload reads that
same state and branches directly past the Classic resource reload when it is
zero. Therefore this isolated release/reconfigure sequence disposes the
effects and then skips their recreation, despite successfully rebuilding the
Saber texture pool. The native late HUD later dereferences one of those empty
slots. This reproduces the dump's state without depending on HUD target
binding behavior.

The alternative outer function `+4F10E0` includes the desktop settings-window
event pump. It is not used as a replacement reset operation.

## Effect ownership

The record table spans `+1B7C290` through `+1B7D3D0`, exclusively, with stride
`0x20`. The native cached loader `+B11AF8` and record construction functions
`+B10484` / `+B10330` create these records from the native SH02 shader cache.
The loader cleans up the entire table on shader-creation failure.

The HUD's `+B0EC20` consumer directly references:

| Native instruction | Effect slot | Name |
| --- | --- | --- |
| `+B0F2F6` | `+1B7D1D0` | `screen_clr_const` |
| `+B0F3CB` | `+1B7D1F0` | `hud_meters` |
| `+B0F4C2` | `+1B7D210` | `hud_meters_bg` |

The late HUD setup intentionally sets native draw state `+29E0580` to one
through `+B29438`. It is not evidence of selecting the wrong graphics backend.
The quad dispatcher `+B58ADC` directly calls `+B0EC20`; there is no alternate
renderer branch at that call.

## Correction and failure isolation

The management-owned reset now captures whether this exact backend's Classic
resources were initialized before release. After the native release has
cleared the flag, it restores that already-existing lifetime before invoking
native reconfiguration. This allows `+80DD0` to execute its authored resource
reload. It does not initialize a renderer whose state was previously zero.

The retained intent identifies the module base, module generation and backend.
Both virtual functions, the active title and the title adapter's generation
must still match. Compare/exchange requires the post-disposal state to be
exactly zero before restoring one. Successful recovery requires the same
owner, successful native reconfiguration, and all 138 effect records present.

A failed reload retains only the original ownership intent for a later cold
management retry. Its zero state cannot be mistaken for a successful
dimensions-only pool rebuild. Failure or an exception revokes a lent flag
when native resource restoration did not finish; native exceptions retain
their original propagation. A different module, generation or backend cannot
borrow the saved intent.

The public HUD-readiness check validates the initialized flag and all three
HUD record pointers. It remains effective for ordinary native fallback draws
after camera heartbeat admission expires. The calling HUD hook supplies its
own retained module base and generation; those must match the core and title
adapter before either readiness or optional-feature pass-through is admitted.
If the optional resolution feature was never installed for that same owner,
this check leaves native behavior unchanged. No shader
loading, allocation, COM operation, logging, lock or signature scan was added
to a render/HUD hot callback.

## Offline verification and limits

`tools/re/test_ce_resolution_lifecycle_native.py` executes pinned native
disposal, the native reload gate, and the native SH02 loader with emulated
driver services. It demonstrates:

- The old release/reconfigure gate returns success with all 138 records empty.
- Preserving a previously active lifetime enters the native reload branch.
- The real cached loader constructs all 138 records and 141 shader objects.
- A previously inactive renderer remains inactive.
- An injected shader-creation failure cleans all 138 records.

The native gate and cached loader are exercised separately. The complete
terrain, texture and driver initialization inside `+80DD0` is not emulated;
this is not a claim of a complete native reset or headset test. Runtime
transaction tests separately cover retained ownership, failed reload and
retry, and the ordinary HUD fallback readiness guard.

Entry signatures, immutable instruction witnesses, relative operands and
vtable pointers are recorded in
`HALOCE-ANNIVERSARY-RESOLUTION-CONTRACTS.json`, the central evidence manifest,
and the generated runtime contract header. Named data constants come from
verified RIP-relative operands, rather than unverified copied offsets.
