# CE Anniversary HUD: coherent native attachments

## Scope and headset evidence

Halo 3's reference behavior is the game's native gameplay HUD in both eyes,
with its size/aspect/height controls and independent authored reticle. Preserve
the `22cb813` user's accepted CE Original HUD, both working VR renderers, full
resolution, muzzle effects and smooth rendering. This correction is confined
to Anniversary's optional late gameplay HUD transaction.

The supplied `22cb813` log reports 2,464 Anniversary HUD eye copies, zero
fallbacks and `reason=none`; the user still saw no Anniversary HUD. Those
counters describe completed callbacks/copies, not visible native pixels.
The old fixture painted the texture with `UpdateSubresource`, which bypassed
the actual output merger, viewport, scissor and depth attachment entirely.
Thus its success could not establish that native HUD drawing worked.

## Native allocation and the actual late consumer

All RVAs refer to pinned `halo1.dll`, SHA-256
`0A12DC561780F449D3F4D0DF10BB8D3BC7BE7840A5BEB2B236F672EB6CD42E6C`.

1. Native frame `455A10` requests depth pool role `0x08800001` at `455A93`,
   calls `20B510` at `455A98`, then stores that root into the native parent
   at `+318` at `455AA8`.
2. The independently verified native pool/root/child allocation chain is
   documented in `HALOCE-ANNIVERSARY-FULL-RESOLUTION-2026-09-15.md`. Current
   full-resolution VR preserves this depth root at W x H and grows its two
   eye children to W x H. Packed color role `0x04000001` grows to W x 2H.
3. **The late HUD uses the root, not an eye child.** The second `45E2B0`
   output executes `45E3F0..45E406`, clearing selector bit 15. Actual selector
   `AD5F0` subsequently returns the unsplit root. A trace stopping at the
   per-eye selector would misidentify the consumer.
4. Native frame `456EFA` binds the packed color with parent depth `+318`
   before the natural late HUD callback. See the existing native sequence
   and binder evidence in `HALOCE-LATE-HUD-NATIVE-SEQUENCE-2026-09-15.md`.
   The current `22cb813` dimensions are therefore **2912 x 4200 packed
   color with 2912 x 2100 depth**. The backend cache can publish this intent
   even though it is not a compatible D3D11 drawing attachment set.

`tools/re/test_ce_hud_attachment_native.py` executes the complete existing
native allocation fixture, actual frame parent assignment, actual second-eye
selector clear and actual surface selector. Four cases cover 32 x 16 and the
user's 2912 x 2100 configuration, before and after full-resolution allocation.
The role lookup endpoint returns the exact initialized native pool entry;
allocation/string/driver fixture dependencies are explicitly reported.

The older allocation has root H and packed H and is compatible at this late
stage. This finding does **not** explain every older missing-HUD report.
No statement that older packed H necessarily used child H/2 survives the
actual second-output selector clear. Current full-resolution root H versus
packed 2H is the reproduced native defect addressed here.

Record: `out/ce-hud-native-attachment-proof-20260915.json`. Direct instruction
trace: `out/ce-hud-native-depth-parent-assignment.txt`. No process was opened,
game file changed, new guessed address introduced, or native world allocation
altered by this correction.

## Isolated correction

The rejected unprepared HUD path remains disabled after its separate rollback
commit. `kCeAnniversaryPreparedHudTargetsEnabled` enables the replacement.
Before the already-admitted natural gameplay HUD draw:

- Require the existing exact frame, packed source, context, generation,
  resource revision, single-color target and numeric raster proof.
- Read registered depth-resource dimensions. Keep compatible depth; omit it
  only when width/height/array/sample compatibility fails. Native bitmap
  `B0EC20` and text `CAA9D8` disable depth testing through render state 7.
- Use the already verified native target binder to publish the prepared
  descriptor into **both** native target caches and actual D3D bindings.
  Private reticle capture consequently reads the same coherent native intent.
- Preserve the observed native raster around binding. Frame the existing
  gameplay HUD into both packed eye regions and restore its original native
  descriptor/cache before the callback's ordinary cleanup.

No outer HUD callback, preamble, lock or target-stack operation is replayed.
There is no render-hook COM getter, allocation, signature scan or logging.
Original and other titles do not enter this transaction.

The possible mutation receipt is set before native binding. A failed readback
or native exception therefore still reaches guarded cleanup. Restoration
accepts only the exact original/prepared descriptor with the same live color
source. It revalidates detached depth resource revision, wrapper, surface and
DSV before the binder can dereference them. A foreign native target retains
its descriptor and raster; stale raster observation is invalidated. A bounded
repair handles a native restore that copied its original descriptor before
failing to publish the view cache. Native gameplay SEH also restores the
transaction, retains the camera and propagates the original exception.

Failure 54 names preparation refusal and 55 names restoration refusal.
Cold telemetry adds `targetsPrepared` and `incompatibleDepth`; neither is a
claim of headset visibility. Optional failure keeps the previously captured
world pair rather than disarming CE VR.

## Validation and limits

The production runtime fixture now uses actual WARP vertex/pixel shader
`Draw` calls, native-style depth-disabled rendering, and enabled scissors.
It reads back both eye textures and checks that surrounding distinct world
pixels survive. The incompatible native attachment case failed with the old
unprepared adapter and passes with the correction.

The focused Release runtime suite passes:

- Both eye pixels, size controls, authored canvas/native raster resets,
  source changes, frame ownership, no-gameplay and recovery cases.
- Before/after-mutation native binding exceptions and failed cache readback.
- Foreign descriptor plus distinct raster preservation, without a cleanup bind.
- Partial original-descriptor restoration retry after incomplete cache state.
- Detached depth release and changed depth view, rejected before rebinding.
- Native gameplay exception cleanup, followed by a successful next frame.

The native allocation/assignment/selector verifier separately passes all four
cases. Final cumulative build/test/package records are owned by the candidate
manifest. These offline checks prove this attachment defect and correction;
they do not execute all real MCC HUD drawing descendants or establish headset
acceptance. The user must still confirm Anniversary HUD visibility/controls and
the retained Original HUD and smooth rendering. The accepted pointer is not
advanced by local verification.

## September 15 evening: native partial-bind correction

The `2cf002b` graphics-switch crash is now attributed to disposed native
shader resources, **before** target preparation. See
`HALOCE-RENDERER-RESOURCE-LIFETIME-EVIDENCE-2026-09-15.md`. The prepared-path
enable described above was disabled separately in `8b6fd06`. The corrected
candidate uses `kCeAnniversaryNativeReadyHudTargetsEnabled`; the earlier
manual, unprepared and prepared enables remain false.

An independent execution of actual `+205E40 -> +1DC120` binder instructions
found another bounded cleanup case. After copying the descriptor, native code
zeros its width/height at descriptor `+38/+3C` before validating attachments.
A refusal or exception can leave this descriptor alongside the previous view
cache. The old cleanup checked only the complete original/prepared descriptor
and therefore refused its own partially published state.

Cleanup now also recognizes either exact owned descriptor with only those
eight bytes zero. All existing source, backend, context, wrapper, revision and
detached-depth checks remain required. No arbitrary foreign target is repaired.
The production fixture reproduces failures during both preparation and
restoration: two assertions fail before this correction and pass afterward,
including the next successful draw and the existing foreign/stale cases.

`tools/re/test_ce_hud_target_native.py` covers ten actual native cases. It
also verifies native dimension constants, clearing of 160 pending counts,
alias removal from six native shader caches, preservation of unrelated state
and unchanged effect pointers. The driver setters and gameplay draw are
stubs; this is not a headset result or the cause of the observed shader crash.
Reports: `out/ce-hud-target-prepared-final-20260915.json`,
`out/ce-hud-native-partial-before-20260915.txt` and
`out/ce-hud-native-partial-after-20260915.txt`.
