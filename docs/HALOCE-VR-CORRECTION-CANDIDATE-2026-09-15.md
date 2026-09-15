# CE Original and Anniversary VR correction

This cumulative candidate supports both CE graphics modes and both MCC editions.
It preserves the existing Halo 2, Halo 3, ODST, Reach and Halo 4 work. The accepted
source remains `4e01f28`; this package requires headset testing for acceptance.

## Corrections since the failed `be2140f` build

- Original now finds the engine's actual output texture and receives its DXGI
  metadata at Present. This corrects the source rejection that prevented all
  Classic eye pairs in the previous test.
- Anniversary preserves the native HUD-enable flag, so its per-eye HUD replay
  can run. Optional HUD failures remain isolated from the camera core.
- Anniversary now requests the engine's static-scene visibility refresh when
  entering or leaving two-eye rendering. Previously the added eye bypassed
  that request, leaving cached geometry visibility from the one-camera view.
  Stable two-eye rendering does not request a rebuild every frame. Authored
  hidden regions and independent geometric culling remain active.
- Desktop presentation uses one completed eye with the correct aspect and gamma,
  replacing the stacked desktop views. Both independent images remain available
  to the headset.
- Working controller-driven hands, weapon placement and shot direction from the
  previous candidate are preserved, along with shared input and graphics switching.

The pinned native-code test reproduces stale first-eye-only geometry admission
without that refresh and correct two-eye admission after it. This establishes
the native omission; it does not establish that every part of the reported
displaced/missing-world headset image is resolved. The four failed CE enables
remain disabled; this candidate has a separate correction enable.

## Package contents and update

The build ZIP contains the DLL, launcher, default configuration, manual and exact
candidate manifest. The matching source ZIP contains this commit's tracked source,
documentation and assets. Both editions use the same build.

Follow `MANUAL-README.txt`. Keep your existing `halomccvr.cfg` when updating. This
package does not install itself or start the game.

## Preserved scope and verification limits

CE roomscale body following, physical melee and world collision remain deferred.
H2 vehicles, all-title zoom and every standing/deferred task remain preserved.
The unusual support-hand angle reported in the previous test is not accepted as
corrected. Broad vehicle, animation, muzzle and custom-content behavior remains
subject to runtime testing.

Offline validation covers pinned native scene-cache/admission code, production
C++ adapters, native shader bytecode and real D3D11 WARP texture/capture/restoration
tests. Original's fixture checks capture after the final native blit and recovery
after an actual 32 x 16 to 48 x 24 output resize. Anniversary's fixture checks
transition-only refresh, stock return during retirement, both native preparation
branches and rejection of foreign scene/camera identities. These checks do not
substitute for the user's headset result or the required Halo 3 regression.
No new screenshot, game launch, installation or game-folder write was performed.

The cumulative Release build and all 20 CTest suites pass, together with the
Reach consistency check, generated contracts, 99 pinned native contracts and
14 production mapped-image binding groups. Packaging repeats Release/build/tests
for the exact committed identity in the manifest.

## Headset check

Check the same scene in both CE graphics modes: a coherent world in both eyes,
head turning and leaning, tracked hands/weapon and controller aim, native HUD,
graphics switching, pause/resume and recenter. The desktop should show one eye.
Also check Halo 3 for regression. Keep the existing configuration and include
HaloMCCVR.log with the result; identify the edition, runtime and headset.
