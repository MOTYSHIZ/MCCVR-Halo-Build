# CE seated view coordination

Halo 3 behavior being matched: independent headset look, configured VR right-stick
turning, and optional view-follow driven by actual vehicle hull yaw. Accepted CE
controller steering remains the existing native private-packet adapter.

The previous CE turn adapter admitted only on-foot players. Seated right-stick
look therefore used MCC's native stick sensitivity instead of the configured VR
turn rate. The following camera separately reads native player-control angles;
controller-directed desired facing does not update those angles. See
HALOCE-VEHICLE-CAMERA-2026-09-16.md and HALOCE-VEHICLE-CONTROL-2026-09-16.md.

The same verified native input-angle-delta phase now admits a locally owned seated
biped with a salted vehicle parent, nonnegative seat and native following-camera
perspective. Vehicle Motion must be enabled. It applies the shared configured
snap/smooth turn, and, when Vehicle View Follow is enabled, the delta of actual
hull yaw. HMD pitch remains independent. Follow-off retains independent camera
yaw. Entry and enabling follow preserve the current relative view direction.
Native camera track position, collision, pitch constraints and interpolation run
normally; no camera buffer or vehicle object is patched.

## E-CE-VEHICLE-VIEW-2

Official HCEEK vehicle physics `008e1b60` resolves object mask 2 and its `vehi`
and `phys` tags. It constructs current orientation from object `+0x30`/`+0x3c`
through matrix builder `007b2a20`, independently of desired facing `+0x204`.
The matrix builder places forward, up-cross-forward and up in its three basis
columns. These are the actual object orientation vectors, not a guessed desired
steering direction.

Pinned retail `halo1.dll` homologue `b45178` retains the same physics-tag lookup,
speed/force computation, current and desired orientation matrices and torque
response. `b45498..b454a8` passes object `+0x3c` and `+0x30` to matrix builder
`ba24ec`. The function-entry signature and exact call witness are unique in the
pinned image. HALOCE-VEHICLE-VIEW-CONTRACTS.json and the manifest record them.
Inspection: `out/reload-policy/ce-vehicle-heading{,-retail}.c` and
`ce-vehicle-basis-kit.c`. Identities remain those pinned in the CE manifest.

This new optional proof is checked separately from accepted steering. Failure
logs a vehicle-view stock fallback and preserves on-foot turning and steering.
The reader uses the existing verified CE native object accessor, full salted
handles, finite orthonormal basis checks, current ownership and SEH boundaries.
Nested vehicle parents and near-vertical forward projections retain native
camera input rather than inventing world-space orientation.

Only wrapped yaw differences are accumulated. Seat/unit/parent, generation,
tracking space, reference and renderer changes, backward clocks and gaps of
250ms or more reset the baseline. Disabled follow observes but does not apply
hull rotation. No view change is derived from hand direction. Native packet
steering, throttle, buttons and seat role forwarding are untouched.

## Local checks and limits

Production native-turn fixtures cover seated takeover, configured snap, actual
hull changes, repeated calls, follow toggles, stock fallbacks, inaccessible and
reused object identities, and preservation of unrotated vehicle throttle.
Yaw sweeps cover both directions, repeated wrap seams and 30/60/72/90/120/144 Hz;
all reference and identity resets are tested. Release, all 41 CTests, Reach
consistency gate and pinned manifest verification pass. Logs are under
`out/refinement-20260918/*ce-vehicle-view*`.
Both renderer modes use the same native input path. These checks do not emulate
all vehicle physics, network replication or native camera interpolation; headset
Original/Anniversary driving and the Halo 3 regression remain unaccepted.
