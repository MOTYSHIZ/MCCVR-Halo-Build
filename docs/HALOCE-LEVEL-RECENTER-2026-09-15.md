# CE level recenter correction - September 15, 2026

## Problem and Halo 3 behavior being matched

The `fba9ee6` headset report says recentering leaves the world looking too far
up or down in both CE Original and Anniversary. See
`HALOCE-FBA9EE6-TEST-2026-09-15.md` for the preserved log and exact test identity.

The actual Halo 3 `ApplyHeadLook` implementation in `src/dll/game.cpp` captures
`g_gameYawRef`, `g_headYawRef` and the room position on manual recenter. Its
subsequent camera composition uses relative yaw and absolute physical head
pitch/roll. Its lean mapping leaves physical room up as game world up. Native
camera pitch and captured head tilt are not a new tilted room reference.
Halo 3's controller-position path likewise rotates the room displacement by
the recentered horizontal heading. No Halo 3 implementation was changed.

CE's old `BuildEye` inverted the complete sampled reference quaternion and
mapped the result through the complete native camera forward/up basis. Both
the captured HMD pitch/roll and native camera pitch/roll could therefore tilt
the room. A horizontal current head could yield a nonhorizontal camera even
after otherwise valid native camera construction and stereo capture.

## Correction and consumer audit

`BuildTrackingFrame` derives a yaw-only inverse reference and a horizontal CE
camera basis with world +Z up. `BuildEye`, `BuildControllerMatrix` and
`HeadRelativeMovement` consume this same frame. The original sampled reference
is retained intact for existing receipt identity and graphics-switch continuity.

- Original's per-eye construction and Anniversary's `StageSaberEye` both call
  `BuildEye`. Native rectangles, near/far planes, flags and non-owned camera
  fields retain their previous preservation rules.
- Controller positions and orientations use the same frame. Physical head tilt
  remains in the live eye orientation, and physical controller tilt remains in
  hand orientation. Positional-off still uses controller-minus-current-head
  displacement; it does not remove eye separation.
- `ControllerShotDirection` calls `BuildControllerMatrix`, so the native shot
  direction follows the corrected hand frame without changing native fire origin.
- Movement projects the physical head heading into the same horizontal frame
  and preserves stick magnitude. It still declines a physically vertical head
  direction, whose horizontal movement heading is undefined.
- At an exactly vertical native camera, its up axis supplies a deterministic
  horizontal fallback. At an exactly vertical HMD reference, its right axis
  supplies a deterministic yaw fallback. Forward yaw is undefined at that pole;
  this is a finite fallback, not a claim that yaw/roll have unique decompositions
  there. Equivalent quaternion signs produce the same result.
- The only remaining CE full-reference transform is `FollowRoomscale`, behind
  `kCeExperimentalRoomscaleEnabled=false`. That dormant body-following code is
  preserved and remains disabled. Any future enable must adopt the shared frame.

No signatures, engine offsets, hook admission, logging or other-title code were
changed by this correction. The helper performs bounded arithmetic only and
keeps output untouched when it declines malformed input.

## Local verification

An isolated x64 Release harness under `out/ce-recenter-20260915` compiles the
production headers and existing test sources without using the shared build
tree. Commands run:

```powershell
cmake --build out/ce-recenter-20260915/build --config Release
ctest --test-dir out/ce-recenter-20260915/build -C Release --output-on-failure
```

The three recenter-related suites pass:

- `tests/haloce_render_tests.cpp`: 27 combinations of native pitch and captured
  reference pitch/roll, both eyes and both graphics conversions; 72 additional
  native yaw/pitch and physical pitch combinations including exact and near
  poles; physical roll/IPD symmetry and separation; native bank isolation;
  equivalent quaternion signs and rejected-output preservation.
- `tests/haloce_controls_tests.cpp`: 36 native-pitch/reference-pitch/physical-yaw
  combinations, native poles, correct movement direction and magnitude, plus
  existing stale-reference and physically vertical-head refusal cases.
- `tests/haloce_first_person_tests.cpp`: 45 native-pitch/reference-pitch/roll
  combinations with independently expected hand position and direction, equality
  of collocated eye/controller pose, positional-off displacement and stale-space
  rejection. The production shot reader uses this exact tested matrix builder.

These checks establish consistent transforms, not headset acceptance. The user
must verify the recentered horizon and controller aim in both graphics modes.
The accepted cumulative pointer remains `4e01f28`.

## Optional packed eye-cache capture verification

For the separate late native HUD integration, `EyeCache::CapturePacked` copies
the top and bottom halves of a proven packed source over an already captured
world pair before `Finish`. The native adapter must prove source identity,
lifetime, top/bottom layout and descriptor. The cache does not infer those facts.

Before either GPU copy it checks the exact generation/space/serial/resource key,
both world eyes captured, frame unfinished, immediate-context identity, distinct
nonalias resources, supported descriptor, exact width/double height and a D3D
copy-compatible format family. It makes no resource getters, COM lifetime calls,
allocations, logging or native calls. Refusal retains the original valid world
pair; the optional HUD cannot invalidate world capture through this API.

`tests/haloce_eye_cache_tests.cpp` passes on a real WARP D3D11 device, compiled
in the same isolated Release harness. It verifies actual top/bottom pixel copies
for UNORM, SRGB and TYPELESS source textures, surviving source overwrite/release,
and preservation of tracking/covers. Four stale-key cases, 12 incompatible
descriptor cases, wrong/null context, null source, eye aliasing and incomplete,
finished or borrowed frames all decline without changing either world eye.
The late native HUD sequencing itself is documented and verified separately.
