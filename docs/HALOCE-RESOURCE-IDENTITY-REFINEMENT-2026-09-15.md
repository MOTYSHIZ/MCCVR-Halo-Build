# CE resource identity refinement after 58f71a4

Halo 3's reference behavior is independent, current eye images with bounded
frame rejection and recovery. Original's confirmed camera/palette/lens are
preserved. This change repairs the CE descriptor registry used to validate live
native color/depth resources; it changes no native view/player count or address.

## Failure and reproduction

The second supplied run repeatedly loses Anniversary's right-eye copy after
both cameras reach depth, scene and shading. The old generic copy-shape log
did not record the rejected resource/descriptor, so that log alone cannot prove
which individual shape guard failed. See `HALOCE-58F71A4-TEST-2026-09-15.md`.

The registry did contain a reproducible persistent failure: it assigned exactly
one record to each 13-bit pointer hash. Recording a different texture with that
hash invalidated a still-live source. No new creation/import necessarily occurs
for that source, so frame retries could not recover its descriptor. Allocation
order/address changes across process launches make this mechanism intermittent.
This is a demonstrated implementation defect consistent with the report, not
proof that it accounts for every rejected copy in the supplied session.

The production WARP fixture now records a colliding unrelated pointer between
the two native eye transfers. With the previous registry, the real right-eye
copy fails and later frames remain unavailable. With the correction, both real
eye textures contain their distinct expected pixels and the pair completes.

## Correction and lifetime guards

Each hash bucket has eight fixed records with independent permanent pointer
ownership and monotonically increasing revisions. Recording a colliding
resource cannot evict existing metadata. Reads inspect at most eight entries;
there is no render-time allocation, COM discovery, lock or retry loop. Full
buckets decline a new record rather than borrow an unrelated descriptor.
Owners reset only at the existing core retirement point, after its native
callbacks and synthetic lists have drained. Very long generations with more
than eight distinct identities in one bucket can still exhaust a bucket; this
bounded capacity is not a universal allocation guarantee.

Release now increments the matching revision even if its new payload has not
yet published. The old payload-based release could miss a reserved newer token,
allowing its delayed publication to resurrect metadata for a released texture.
A `Revoke -> Forget -> Publish` regression rejects that stale token.

Cold diagnostics now retain rejected eye resource identities, request/source
dimensions and individual descriptor/box/subresource proof bits. This separates
remaining native shape changes from metadata loss without logging in the hook.

## Validation

`halomccvr_ce_runtime_tests` fails the new collision and pending-publication
regressions against the previous header and passes against this correction.
Records: `out/ce-resource-collision-{before,after}-20260915.txt` and matching
build logs. Existing missing-eye, invalid-box, released-resource, renderer,
generation, recenter and retirement tests still pass. Both-graphics/relaunch
headset verification remains required. Native resolution is a separate issue;
this change neither stretches nor enlarges the eye images.

## Retirement scope retained for follow-up

The core callback drain is not a drain of every descriptor publisher. The
shared `CreateTexture2DHook` still calls `HaloCE_RecordTextureCreated` outside
the CE callback counter, including before native CE hooks are installed. That
early observation is the bootstrap for existing textures and must not simply
be disabled whenever the CE core is uninstalled or retiring. Presentation
texture publication also has a separate shared callsite.

Those publishers are not explicitly excluded from `ResourceRegistry::InvalidateAll`.
Source inspection therefore leaves a possible reset/publication overlap that
can lose descriptor availability; the revision/identity checks still reject
unmatched metadata. This audit did not reproduce that overlap in the supplied
session or establish it as the second-launch cause. The current tests prove
the reproduced collision and reserved-token corrections, not universal
publisher quiescence or all transition/relaunch outcomes.
