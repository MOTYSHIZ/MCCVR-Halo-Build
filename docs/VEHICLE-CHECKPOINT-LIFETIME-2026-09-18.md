# Vehicle/checkpoint lifetime audit (unaccepted)

The user's checkpoint crash is not identified to a title or captured exception.
The separate supplied co-op log concerns Halo 2 Cairo/Outskirts, but has no
exception address or stack. These reports are not assumed to share a cause.

## Concrete ODST defect and correction

`OdstRestoreNativeSeatPatch` previously dereferenced its saved flags pointer
directly. SEH caught inaccessible memory but could not prevent restoring into
readable storage that had been replaced. It also lacked the generation check
already present in Halo 3's restoration path.

The correction retains ODST's existing first-person seat behavior and verified
ODST tag layouts. Cleanup now requires the original generation, tag base,
instance table and resolved definition, bounded live seat block, and exact
resolved flags address. Only then does a compare-exchange restore the exact
word written by this feature. An external writer wins. Failed validation or a
memory fault retires this optional lease without affecting the camera core.
No engine function/address/layout or native seat policy was added.

The production implementation is included from `odst_native_seat_patch.inl`
by both game.cpp and the lifetime fixture. Fifty checks cover normal restoration,
an external writer, generation changes, replaced/unloaded storage, changed
definitions/seat blocks, an inaccessible flags page, and config-off restoration.
Release and all 41 CTests pass; Reach consistency passes. Logs are under
`out/refinement-20260918/{build,tests,gate}-seat-lifetime.log`.

## Related paths and limits

Halo 3 already checks generation and re-resolves its seat flags before comparing
with its saved address/value. Reach uses an exact full-handle lease key and
re-resolves unit, parent, definition and seat before atomic restoration. Neither
uses ODST's former unconditional saved-pointer restore. H2/CE/H4 have no instance
of that persistent seat-flag patch path. Their independent camera/lifecycle paths
still require separate review; this is not a blanket checkpoint-crash diagnosis.

These checks cannot distinguish storage replaced at exactly the same addresses
with indistinguishable metadata. They do not synchronize with arbitrary engine
tag writers or prove headset/checkpoint/co-op acceptance. The original crash
attribution remains unproven; the concrete unsafe restoration has been removed.
