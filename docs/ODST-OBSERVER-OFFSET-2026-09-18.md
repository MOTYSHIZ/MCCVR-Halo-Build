# ODST controller aiming report: camera admission failure

`HaloMCCVR (21).log` is the ODST failure; `(20)` is the successful Halo 3
comparison. Originals are preserved under
`out/test-runs/queued-odst-stick-aim-20260918/`. Both identify source `1a9766c`,
Steam, SteamVR/OpenXR Meta compatibility 2.17.10, Oculus-family at 120 Hz.

ODST passes the level-load liveness gate at 10:30:38.459, then waits for camera
admission. At 10:30:38.615 its logged camera has ordinary single-user tail,
mode zero, blend 1, FOV 1.641310, reference 1.345714, near 0.007812,
far 10240, correct nested source, inactive other slots, and **offset 0.17**.
It never installs/arms its camera core, and the log continues reporting
`stereo off` through 10:32:45.843. Thus this report is not evidence that an
installed ODST motion-aim hook chose stick aiming: motion aim never became
eligible. Halo 3 installs and renders stereo in its comparison log.

The concrete source defect was `verticalOffset == 0.0f` in
`OdstCompactCameraIsStereoRedirectable`. Earlier captures happened to contain
zero, which became a false requirement for all users. ODST's own compact
layout identifies +0x34 as an authored observer scalar, separately from the
custom-projection enable and parameters at +0x7C/+0x80. The native viewport
`0x2CAC5C`, projection `0x2CAED0`, matrix builder `0x2CB1C4`, and FP rebuild
`0x2A6F5C` were re-read in the pinned retail module. None interprets compact
+0x34 as a special projection mode. FP rebuild's view+0x34 is compact+0x2C
(reference FOV), not the observer-offset field; the enclosing view starts its
compact block at +8. Retained output: `out/reload-policy/odst-offset-projection`.

The correction accepts finite observer offsets while preserving the native
value. All existing mode, FOV, clip, bounds, oblique-plane, custom-projection,
single-user and ownership guards remain. Cold diagnostic flags now report
finite offset rather than exact zero. Tests include the log's exact camera
scalars, positive/negative offsets, NaN/infinity, bad modes, FOV and clips.
Release and all 40 CTest suites pass. Headset reproduction remains pending;
the logs do not prove whether every other unlogged field would pass afterward.

## Cross-title check requested by the user

The other title camera admission paths were inspected for the same assumption:

| Title | Checked path | Result |
| --- | --- | --- |
| Halo 3 | camera copy, RenderViewHook and auto-arm | No exact-zero observer-offset gate; native compact fields are preserved through per-eye rebuild. |
| Reach | ValidateReachCompactCamera and outer/stereo admission | Checks its own basis, FOV and display bounds; no zero observer-offset requirement. |
| Halo 4 | Halo4ValidateCameraBasis and camera admission | Checks basis, FOV and positive reference ratio; no zero observer-offset requirement. |
| Halo 2, both renderers | observer camera basis and stereo receipt admission | Checks its own pose/ownership/freshness; no zero observer-offset requirement. |
| CE, both renderers | halo_ce::Valid/ValidPrimary and Saber stereo receipt | CE-specific pose/FOV/viewport/depth checks; no copied ODST observer-offset gate. |

No matching defect was found in those paths. Their native layouts were not
changed or made to use ODST offsets. This bounds the static audit; it is not a
claim that every possible camera setting/title transition has been headset tested.
