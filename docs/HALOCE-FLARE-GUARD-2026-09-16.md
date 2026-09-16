# CE Anniversary lens-flare projection domain

Evidence ID: **E-CE-FLARE-1**. Halo 3's reference behavior is a stable tracked
world in both eyes. A screen-space light sprite must not expand from an
unprojectable light behind the tracked eye. This corrects a demonstrated
projection-domain defect; without a capture of the reported first-tunnel
streak, it does not prove the defect caused every reported light streak.

Pinned `halo1.dll` SHA-256:
`0A12DC561780F449D3F4D0DF10BB8D3BC7BE7840A5BEB2B236F672EB6CD42E6C`.
The following are CE Anniversary's native functions, not H3 or H2 layouts:

- Postprocessing setup `0x44ED40` resolves the authored `lens_flares` resource
  and passes it to the effect's setup routine `0x447340`.
- Primary postprocessing `0x450B20` calls flare projection `0x446F70` at
  `0x451096`, passing the current eye camera and the effect at owner `+0x38`.
- `0x446F70` reads source player at camera `+0x220`. Its two light records are
  effect `+0x70 + player*0x50` and `+0x98 + player*0x50`; position occupies
  the first twelve bytes, and the nonzero intensity at record `+0x1C`
  authorizes projection. Native camera-matrix builder `0x2EBBE0` supplies
  the world-to-clip transform for the supplied eye.
- Divides at `0x4470FA` and `0x447169` project the first light without a
  positive-W or near-plane test. The second record follows the same sequence.
  Screen coordinates are passed to `0x447EA0` at `0x447185/0x4472E6`.
- Sprite draw `0x447EA0` computes distance from screen center, then expands
  flare sprites using `max(1, (distance - 0.2)/0.4)`. Thus a cached light
  center at the eye plane can feed nonfinite coordinates to sprite geometry;
  centers very near that plane can produce extremely large projected sprites.
  Existing intensity/visibility inputs do not clip those coordinates.

`tools/re/test_ce_flare_projection_native.py` executes the complete pinned
flare projector, native camera rebuilds and matrix producer in read-only
Unicorn image memory. Dimension queries and final sprite draw are explicitly
modeled leaves; the draw records the actual coordinates passed by native
code. It tests both community raster sizes and the aligned Anniversary raster,
translated cameras, two camera orientations, and depths in front of, inside,
at and behind the eye plane. Each input is repeated with neither, either or
both native light records enabled. It checks the real effect pointer, record
pointer and return address of each native sprite call against the adapter's
ownership guard; zero intensity remains a native no-draw.
Zero depth produces nonfinite screen coordinates; +/-0.01 depth sends the
light center more than a screen height away. This is a native arithmetic
result under synthetic inputs, not a live scene or GPU rendering result.
Output: `out/ce-flare-projection-native-20260916.json`.

The independent optional adapter establishes an exact primary-eye scope
around the native projector. At either verified sprite call it matches the
original effect and corresponding player-zero record, rechecks the current
eye receipt, and refuses a sprite only when the light center is behind that
eye's near plane or its projected screen coordinates are nonfinite. All
front/near-safe flares retain original arguments and native drawing. It does
not change lights, bloom, materials, source-player identities, effect records,
or native visibility. Foreign, stale, nested and unproven calls stay native.
Binding failure logs a stock fallback for this feature alone. An unreadable
record or invalid camera/depth proof also stays native, counted as unproven;
it cannot become a blanket flare disable. Cold counters
report clipped, visible and unproven owned records and native exceptions; no logging, allocation
or COM operation enters either hot hook. Exception cleanup restores the
thread-local scope and both callback pins before propagating. The actual hook
entries own SEH and callback pins themselves: a tail-jump wrapper could omit
the unwind metadata required by native retirement. Both explicitly non-inlined
injected-caller dispatch functions and actual hooks participate in native
quiescence checks, including the interval before callback pin acquisition.
Cleanup failure keeps only this feature's module/trampolines retained, logs
the failed phase, and retries cold; it never changes camera-core ownership.

`halomccvr_ce_comfort_tests` exercises both actual adapter dispatches and
both native records, front/near/behind and nonfinite inputs, malformed and
unreadable proof, source-player identities, ownership retirement between
projection and drawing, nested calls, native exceptions and recovery. The
fixture calls the real counter-owning dispatches, proves both counters are
held during the native sprite, and verifies deferred optional cleanup can
finish after callbacks drain. It also queries `RtlLookupFunctionEntry` for
every comfort/flare hook and flare-dispatch address; a missing compiled unwind
record fails the fixture. Exceptions are also driven through both actual hook
entries to prove their own callback pins retire. Native evidence captures are
`out/ce-lighting-{audit,flares,streak-consumers,flare-draw}-20260916.txt`.
The HDR `streak_*` settings are a separate native path; their names alone
were not used to justify a blanket effect disable.

September 16 local verification: the isolated Release comfort target built
and its CTest fixture passed. The expanded pinned-native projection test
passed **384 cases / 432 native calls / 139,602 instructions**. These results
establish the projection defect and dispatch/cleanup invariants offline; the
user's first-tunnel headset result and cumulative regression remain required.
The separate pinned contract audit passed both unique entries, all eight body
witnesses and four relative call operands; its report is
`out/ce-flare-contract-verification-20260916.json`.
