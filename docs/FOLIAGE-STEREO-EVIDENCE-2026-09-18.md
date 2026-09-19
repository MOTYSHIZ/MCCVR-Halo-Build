# Foliage stereo investigation, September 18

The target is the stable binocular scene expected from Halo 3: both eyes see
the same animation time while retaining their own projection and visibility.
This is an unaccepted local candidate, not a headset-confirmed fix.

## Reach: verified draw-driven wind accumulation

Primary discovery used the official HREK `reach_tag_test.exe` retained under
`out/deps/re-tools/inputs`. Its decorator renderer `0x140831790` selects the
native wind tag for the camera's cluster (scenario fallback), then calls
`0x1402AAA50` from `0x1408318A6` when native player-window index is zero.
That function advances a 48-byte wind state: four integer accumulators followed
by two four-float shader vectors. It reads render delta, clamps it at 1024,
accumulates phase at 65536 and spatial phase at 1048576, and calculates the
wind vectors from the authored wind tag. HREK upload `0x1413F8D60` sends those
vectors to the decorator shader. This is render-driven animation, not merely
a simulation snapshot consumed by both eyes.

Pinned retail matching confirms:

| Role | haloreach.dll RVA |
| --- | --- |
| Decorator renderer | `0x2A0B40` |
| Window-zero guarded wind-update call | `0x2A0C58` |
| Wind update, specialized to one argument | `0x77EC2C` |
| State, exactly 48 bytes | `0xD1F150` |
| Shader upload | `0x77EB40` |
| Render delta reader, TLS slot +0x420, field +0xC | `0x1D46AC` |

The upload reads state +0x10 and +0x20 for shader constants `0x2C0000` and
`0x2C0001`. The delta reader does not consume/reset delta. Native stereo invokes
the decorator renderer separately for each eye; prior VR code did not replay
this state. Therefore it could advance wind twice per stereo pair.

The optional correction captures state immediately before the first native eye
render, captures its result and restores its input before the second, and keeps
the first completed native result afterward. Cleanup also runs when the second
eye faults or fails its copy. Left-first/right-first order has the same policy.
Camera-dependent wind-tag selection, decorator geometry, LOD, frustum tests,
textures and native shader uploads are unchanged. A cluster-boundary selection
difference can still be native and is not claimed fixed by this correction.

Four distinct signatures (update entry, accumulator instructions, guarded caller,
and both upload references) each matched exactly once at their pinned RVAs.
Runtime repeats those checks and requires writable committed module storage.
Unknown bindings keep only this feature stock. A hot copy fault increments an
atomic counter; subsequent pairs keep native state. The cold weather report
logs replay-pair counts, faults and fallback. No new native hook is installed.

Retained decompilation: `out/reload-policy/reach-grass-kit.{c,txt}`,
`reach-grass-wind.{c,txt}`, `reach-wind-retail.{c,txt}`,
`reach-decorators-retail.{c,txt}`, `reach-wind-upload.{c,txt}`.

Release build and all 40 CTest suites pass, including both eye orders, second-eye
abort, failed capture and partial rewind cleanup. Reach consistency gate passes.
Actual grass appearance and Halo 3 regression still require headset testing.

## Broader audit results

HREK decorator LOD derives from camera distance/FOV, with native sphere/frustum
tests. The wind shader constants are shared by the decorator draw path, covering
its nearby foliage consumers. No evidence currently justifies removing foliage,
forcing one eye's projection, or disabling native LOD/culling.

The official H3, ODST and H4 kits also expose their own `render_decorators`
controls. Their renderer locations were found from those native descriptors,
independently of Reach. Those implementations are being checked for analogous
draw-time mutation; Reach state offsets must not be reused in another engine.

The official-kit comparison found analogous accumulation in all three engines:
H3 renderer `0x1408E0170` calls `0x14053F7B0`, ODST renderer `0x14093F690`
calls `0x14059D260`, and H4 renderer `0x1408FA170` calls `0x1403C2830`.
Each independently has four integer accumulators and two wind shader vectors;
H3/ODST add spatial phase while H4/Reach subtract it. This confirms a similar
potential stereo hazard, not a reproduced retail defect in those titles.
Their retail bindings and actual stereo call admission still need independent
verification before extending the correction. No foreign-engine offset or
wind sign was copied. Evidence: `out/reload-policy/{h3,odst,h4}-foliage*.{c,txt}`.
CE/H2 foliage is not established to share this path; no speculative patch was
made there. Per-eye decimation, alpha coverage and cluster selection remain
headset verification limits, rather than asserted causes of the report.
