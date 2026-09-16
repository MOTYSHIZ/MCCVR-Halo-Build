# CE complete-frame retention

Halo 3's reference behavior is to reject one bad stereo frame while keeping
VR alive and recovering on the next usable frame. CE previously erased its
only completed pair at every native frame entry and at submission release.
Any isolated capture failure then left CE with no world projection: the CE
submission branch requires a completed pair, while native stereo ownership
correctly prevents substituting an unrelated flat desktop image.

Report 14 samples an incomplete Anniversary pair at 22:48:37.722 (only the
left eye copied) and an eye-cache copy rejection at 22:54:26.035. Other sampled
frames and the depth/consumer receipts remain valid. These observations prove
that isolated pair failures occur; the log does not establish their exact
correspondence to each reported visible black flash or the source of every
rejection. They do not explain the separate light-streak report.

The correction maintains two preallocated CE eye-pair banks. Native rendering
uses one bank; only a fully completed pair swaps into the submission bank.
A partial, invalid or contended successor leaves both previous eyes and their
original tracking/FOV metadata intact. Submission release ends the borrow
without erasing those completed pixels. No allocation or COM ownership work
is added to render/capture callbacks. The extra bank costs two color textures.

The existing adapter still rejects a retained frame after eight tracking
serials or 250 ms, as well as on recenter, tracking-space change, module/title
generation change, graphics-mode change and resource replacement. The OpenXR
submission already uses the captured pair's own pose and FOV; it does not
describe old pixels using a newer pose. This keeps a brief failed frame
available for compositor reprojection without allowing indefinite freezing.
Both CE graphics modes use the cache; other titles' caches are unchanged.

`halomccvr_ce_eye_cache_tests` runs real D3D11 WARP copies and verifies the
retained eyes survive source recycling, partial writes and rejected frames,
keep the original pose, are atomically replaced by a new complete pair, and
are revoked by resource reset. `halomccvr_ce_runtime_tests` exercises the
production CE callbacks and frame-acquisition gates, including incomplete
output, depth/source corruption, recenter, space changes and elapsed age.
Tests distinguish a rejected current pair from a permitted prior good pair.
Both suites passed locally after the implementation change; final cumulative
verification and headset confirmation remain required. No accepted pointer
is advanced by these tests.
