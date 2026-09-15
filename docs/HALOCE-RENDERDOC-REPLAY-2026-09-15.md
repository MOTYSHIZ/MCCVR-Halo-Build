# CE RenderDoc replay and capture-boundary evidence — September 15

## Result and limits

Headless GPU replay now works with the existing portable RenderDoc 1.46 DLL.
It reproduces the saved flat fallback frames, not the rejected stereo draw
stream. Neither capture's replay resource inventory contains a 2912x1050 eye
texture. The initial backbuffer separately recovered from each capture retains
the earlier broken stacked stereo image and its capture-failure overlay.
That image confirms the visual symptom but does not contain its draw history.

No game process was launched or attached to during this work, and no game file,
RenderDoc preference, installed mod, enable flag, or accepted pointer changed.
All experiments used existing `.rdc` files or isolated WARP fixtures.

| Capture | GPU replay | PNG exports | Final event |
| --- | --- | --- | --- |
| isolated-warp_capture.rdc | succeeded | 4 | 30 |
| ce-steam-be2140f_frame2768.rdc | succeeded | 45 | 34661 |
| ce-steam-be2140f_frame3212.rdc | succeeded | 8 selected targets | 35971 |

Outputs are in `out/ce-renderdoc-evidence/replayed-fixture`,
`replayed-frame2768`, and `replayed-frame3212`. Each directory has resources,
textures, actions, debug messages, and PNG files. The fixture's red/green eye
targets and their copies replay correctly. Frame 2768's final swapchain image
was visually inspected: it is one full-height stock gameplay image.

## Actual capture log

The task's original RenderDoc log was read from the known RenderDoc temporary
directory and preserved as
`out/ce-renderdoc-evidence/RenderDoc-capture-session.log`.
Its SHA-256 is
`108810EE1410F25ABFE69F8DBB37DAE9339178C75E89A69ED2D46E353BC253B2`.

| Event | First attempt | Second attempt |
| --- | --- | --- |
| Capture begins | 12:43:30 | 12:43:53 |
| Dirty resources prepared | 46,732 | 47,985 |
| Two unmatched Unmap warnings | 12:43:34 | 12:43:56 |
| Rejected frame | 2767 | 3211 |
| Successful automatic retry | 2768 | 3212 |
| Total capture elapsed | 6.03 seconds | 6.54 seconds |

The warning is `Uncapped Map()/Unmap()`. This log does not name the offending
resource, context, map type, or call stack. The saved retries must not be used
as evidence that the stereo world draw used correct or incorrect constants.

## Official RenderDoc behavior

The local references are exact v1.46 source downloaded from the official repo,
preserved under `out/ce-renderdoc-replay-source/official-d3d11`.

- `d3d11_context_wrap.cpp:7798-7813`: outside active capture, dirty resources,
  high-traffic resources, and NO_OVERWRITE maps can go directly to D3D without
  creating an intercepted Map record. This check precedes VerifyBufferAccess.
- `d3d11_context_wrap.cpp:7861`: VerifyBufferAccess prevents the separate
  shortcut that marks an ordinary resource high-traffic after 60 updates.
- `d3d11_context_wrap.cpp:8171-8195`: Unmap during capture rejects an absent Map
  record or a pre-capture Map of a type other than READ/DISCARD. Pre-capture
  DISCARD can succeed if RenderDoc retained its intercepted storage.
- `d3d11_device.cpp:2399-2520`: manual capture failure retries the next frame,
  up to five failures. App-controlled capture gives up immediately. Merely
  extending one app-controlled capture across several frames cannot erase an
  invalid Unmap encountered at the beginning.

Sources: [Map/Unmap implementation](https://github.com/baldurk/renderdoc/blob/v1.46/renderdoc/driver/d3d11/d3d11_context_wrap.cpp),
[capture and retry implementation](https://github.com/baldurk/renderdoc/blob/v1.46/renderdoc/driver/d3d11/d3d11_device.cpp),
[public replay API](https://github.com/baldurk/renderdoc/blob/v1.46/renderdoc/api/replay/renderdoc_replay.h).

## Isolated boundary experiment

`tools/re/renderdoc_offline/map_boundary_fixture.cpp` owns a 1024-byte dynamic
vertex buffer and WARP device. It performs 64 background DISCARD updates, then
places the selected Map across capture start or wholly inside capture.
Each case runs in a new isolated process, launched through portable RenderDoc.

| VerifyBufferAccess | Map type | Map begins inside capture | Capture saved |
| --- | --- | --- | --- |
| false | DISCARD | false | no |
| true | DISCARD | false | yes |
| false | NO_OVERWRITE | false | no |
| true | NO_OVERWRITE | false | no |
| false | NO_OVERWRITE | true | yes |

These results match the official source. Verification is a narrow remedy for
the tested high-traffic DISCARD case. It does not cure a NO_OVERWRITE map that
crosses capture start. Original outputs are
`out/ce-renderdoc-evidence/map-boundary-1-0-0_capture.rdc` and
`map-boundary-0-1-1_capture.rdc`; the three rejected cases saved no capture.

## Saved Map census and narrower diagnostic lead

`tools/re/renderdoc_offline/map_census.py` separates initial resource setup
from the saved frame and reports types, resources, contexts, and unmatched
boundaries. Results: `out/ce-renderdoc-evidence/map-type-census.json`.

Both saved frames contain 30 DISCARD Maps and 30 DISCARD Unmaps, plus 174
NO_OVERWRITE Maps and 174 corresponding Unmaps. Every NO_OVERWRITE pair is
resource 13377 on context 133. One extra DISCARD Unmap belongs to initial
resource setup and is excluded from these frame counts.

Both frames have the same two DISCARD resources crossing the frame boundary:

| Capture | Unmap of resource 1447 / 1450 | Map of resource 1445 / 1448 |
| --- | --- | --- |
| 2768 | chunks 44702 / 44703 | chunks 44704 / 44705 |
| 3212 | chunks 46362 / 46363 | chunks 46364 / 46365 |

The Unmaps have no matching Map within the saved frame. The following Maps
remain open at frame end. All four operations use context 133 and DISCARD.
Created resources 1447 and 1450 are each 699,008-byte dynamic buffers with
vertex/index bind flags and CPU write access.

**Inference, not an identified failing resource:** these two rotating DISCARD
buffers are a plausible explanation for the two Unmap failures at the first
capture boundary. Their shape makes VerifyBufferAccess=true a more specific
unchanged-mod diagnostic than another identical F12 capture. The failed frame
stream is absent, so its precise resource identity remains unproven.

## Concrete next diagnostic

No further MCC launch is authorized by this document. Preparation is complete:

1. If a new diagnostic session is authorized, the narrow unchanged-mod option
   is VerifyBufferAccess enabled from process startup, with capture-all-command-
   lists retained. The first captured stereo frame must succeed without an
   automatic mono retry. This option can increase overhead and cannot be called
   a CE rendering fix.
2. Capture startup currently spends several seconds preparing resources. Even
   if Map failure is removed, the existing heartbeat stall remains a separate
   reason the saved frame may lose stereo ownership. If this happens, stop
   repeating F12 and prepare a separately reviewed diagnostic adapter that can
   identify the actual Map boundary and retain the live stereo transaction.
3. Analyze actual per-eye world targets, draw constants, depth, and final copy
   lineage only after the saved frame is proven to contain the reported stereo
   symptom. Retain the original unmodified symptom images as reference.

## Reusable build and replay

### Prepared optional second session, not launched

The reviewable settings file is
`out/ce-renderdoc-evidence/ce-steam-installed-be2140f-verify-map.cap`.
JSON comparison with the original settings confirms that the only semantic
change is `settings.options.verifyBufferAccess: false -> true`.
`autoStart=false`, `captureAllCmdLists=true`, child-process hooking, and existing
callstack settings remain as before. No game configuration was edited.
The GUI's loading of this settings file remains unverified; the CLI flags below
were checked against this exact portable executable's `capture --help` output.

Read-only installed-file verification still matches the original diagnostic:

| File | SHA-256 |
| --- | --- |
| HaloMCCVRLauncher.exe | `54A0E91B642BE153767584E916BE4D8AAF7928E46F78FBF46CB19D9E57834771` |
| HaloMCCVR.dll | `FB5EDB7271C94153F4F7A27882375D6055FFDD301A40857C7D64F710E1005B35` |
| halomccvr.cfg at preparation | `4BB44699F98407C6701ACCEACB651A11AC6EC006429E34F289CB9CAF2300EBBE` |

The first preserved game-log line identifies source
`be2140fc440462c8ac1441186d28e62370ebe074`. The manifest is
`out/ce-renderdoc-evidence/verify-map-preparation.json`.

**This command starts MCC immediately. It has not been executed and requires
new explicit launch authorization under the user delivery override.** It
launches the existing installed candidate; it installs no source corrections.

```powershell
& out/deps/renderdoc/1.46/RenderDoc_1.46_64/renderdoccmd.exe capture --working-dir 'C:/Program Files (x86)/Steam/steamapps/common/Halo The Master Chief Collection/Halo_MCC_VR' --capture-file 'C:/Users/Shadow/Documents/Codex/Halo-MCC-VR/out/ce-renderdoc-evidence/ce-steam-be2140f-verify-map' --opt-hook-children --opt-capture-all-cmd-lists --opt-capture-callstacks --opt-capture-callstacks-only-actions --opt-verify-buffer-access 'C:/Program Files (x86)/Steam/steamapps/common/Halo The Master Chief Collection/Halo_MCC_VR/HaloMCCVRLauncher.exe'
```

The limited rationale is the two saved DISCARD crossings and the isolated
verification result above. A successful launch is not successful capture;
successful capture is not a correction or acceptance of CE stereo behavior.

### Build and export commands

The tracked helper source is `tools/re/renderdoc_offline`. No vendor headers,
DLLs, import libraries, or executables are committed. `prepare.py` checks the
exact portable DLL SHA-256, downloads official v1.46 public headers into ignored
`out/`, generates an import library from its exports, and builds both helpers.
It never runs them or changes capture settings. Build hashes and header hashes
are written to `out/ce-renderdoc-offline/build-manifest.json`.

```powershell
python tools/re/renderdoc_offline/prepare.py
$env:PATH = (Resolve-Path out/deps/renderdoc/1.46/RenderDoc_1.46_64).Path + ';' + $env:PATH
& out/ce-renderdoc-offline/build/Release/ce_renderdoc_replay.exe out/ce-renderdoc-evidence/ce-steam-be2140f_frame2768.rdc out/ce-renderdoc-evidence/replayed-frame2768
python tools/re/renderdoc_offline/map_census.py out/ce-renderdoc-evidence/frame2768.zip.xml out/ce-renderdoc-evidence/frame3212.zip.xml --output out/ce-renderdoc-evidence/map-type-census.json
```

The replay command accepts optional numeric texture resource IDs after the
output directory. Without them it exports all color targets. It advances to
the final recorded action; it does not synthesize missing stereo resources.
An unsuccessful image save, any requested texture not saved, zero exported
textures, or failed report stream produces a nonzero exit code. Report streams
are opened before GPU replay. The tracked helper was checked against the WARP
fixture and a deliberately missing texture ID; the latter shuts down cleanly
and returns 6 instead of reporting success.

To repeat an isolated map case, use the existing `renderdoccmd.exe capture
--wait-for-exit --capture-file <prefix>` command with the built
`ce_renderdoc_map_fixture.exe` as target, followed by three integer arguments:
VerifyBufferAccess, NO_OVERWRITE (zero means DISCARD), and Map-inside-capture.
This target is an isolated fixture, never the MCC executable or launcher.
