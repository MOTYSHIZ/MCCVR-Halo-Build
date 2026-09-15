# CE Anniversary retained GPU evidence — September 15

The user's screenshot and headset report already establish the failed behavior:
one world view shows normal nearby ground and the tracked weapon; the other
shows sky and isolated geometry with most nearby surfaces absent. The desktop
shows those views stacked with the stock HUD spanning the split. This document
records additional GPU evidence, not a request to reproduce or prove the symptom.
Halo 3's reference behavior is a coherent tracked world in both eyes and a single
desktop view. No accepted pointer or CE enable flag changes follow from this audit.

## Preserved captures and the distinction that matters

Both `.rdc` files are from the explicitly authorized existing Steam `be2140f`
diagnostic run. Files and full run log are in `out/ce-renderdoc-evidence/`.

| File | SHA-256 |
| --- | --- |
| `ce-steam-be2140f_frame2768.rdc` | `B67A3A0F7AEB31D48424A3197890609D7A45EB0795902F882BE325482178B561` |
| `ce-steam-be2140f_frame3212.rdc` | `DFD8CF1260AD765805F561D98239CA3905509A469F59D9B33B63AEA520FDD1B5` |

The **initial backbuffer contents** in both files preserve the actual broken
stacked image. They match the supplied screenshot's pattern. RenderDoc's overlay
on those retained images says respectively:

- `Failed to capture frame 2767: Uncapped Map()/Unmap()`
- `Failed to capture frame 3211: Uncapped Map()/Unmap()`

The saved command streams for frames **2768 and 3212**, however, render flat
fallback frames. Do not confuse their later mono draws with the preceding broken
stereo image retained at capture start. Earlier analysis that described the whole
recording as flat omitted these useful initial texture and buffer contents.

`tools/re/extract_ce_renderdoc_initial.py` decodes the official XML/ZIP raw initial
texture contents directly, without GPU replay or third-party Python packages.
It verifies resource descriptor, row pitch, payload length, format, and single
subresource/sample assumptions and records raw/PNG hashes. The backbuffer is
resource **382**, `2912 x 2100`, `R8G8B8A8_UNORM`; initial payloads are 10007 and
10400 respectively. Verified images/manifests:

- `out/ce-renderdoc-evidence/frame2768-initial/initial-resource-382.png`
- `out/ce-renderdoc-evidence/frame3212-initial/initial-resource-382.png`
- corresponding `initial-textures.json` files in those directories.

The retained full-height native targets are not the missing stereo eye targets.
For example initial 2768 resource 13707 contains a visibly older full-height world
image with weapon ammo 60; its initial backbuffer shows the broken stereo image
with ammo 59. Their prior producer times are not captured. Neither export contains
a referenced `2912 x 1050` texture descriptor. This limits target lineage.

## What the saved command streams prove

Existing `analyze_frame3212.py` and its frame2768 copy produce the draw-state and
lineage JSON records in the same output directory. Common CB0 initial data,
subresource updates and mapped writes are read from the official raw payloads.
The world groups below bind their viewport/CB0/targets after explicit state
changes; their result does not depend on assuming a starting pipeline state.
Both streams have an inline deferred command list after the main world groups;
its two RenderDoc ExecuteCommandList records must not be counted as two world
passes. This limited parser is not a substitute for GPU replay.

| Saved frame | World group target/depth | Draws | Viewport | CB0 `+0x240` origin |
| --- | --- | ---: | --- | --- |
| 2768 | 13662 / 13859 | 1665 | 2912 x 2100 | (83.968239, 180.429443, 288.787811, 1) |
| 2768 | 13707 / 13859 | 546 | 2912 x 2100 | same |
| 3212 | 13662 / 13859 | 1718 | 2912 x 2100 | (81.927658, 180.317551, 289.029327, 1) |
| 3212 | 13707 / 13859 | 570 | 2912 x 2100 | same |

End-of-frame copies are full backbuffer 382 to OpenXR image 603 (2768) or 597
(3212), followed by a full `2912 x 2100` copy to resource 617. Each stream also
copies `16 x 16` from resource 618, debug-named `BlankEyeBuffer`, to both eye
swapchain resources. There are no captured native half-height eye-cache copies.
These are concrete flat fallback outputs, not evidence that the headset failure
had no stereo rendering.

## Why the saved frames differ from the reported symptom

`HaloMCCVR-after-captures.log` reports these sequences:

| Window | CE heartbeat disarm | Presentation detach | Subsequent record |
| --- | --- | --- | --- |
| frame2768 capture | 12:43:30.924 | 12:43:34.094 | invalid receipt; stock frames; rearmed 12:43:37.592 |
| frame3212 capture | 12:43:53.585 | 12:43:56.801 | invalid receipt; stock frames after capture |

The installed `be2140f` source has a camera freshness requirement of less than
500 ms in `HaloCE_Poll`; losing it clears `armed`. `FrameBody` requires `armed`
for a current receipt. `TrackingNow` separately requires a sample younger than
250 ms. The exact logged reason is **camera heartbeat expired**; the log does
not independently identify which other freshness predicate also failed.
The user reported the capture freeze and subsequent flat image. The retained
RenderDoc overlays prove the attempted preceding frame failed capture because
of the Map/Unmap problem. Together these explain why the saved retries contain
fallback rendering. They do not explain the original bad eye or justify removing
runtime freshness guards.

## Retained common constants: useful negative result

`tools/re/inspect_ce_renderdoc_constants.py` reads initial `CreateBuffer` payloads
of the actual 1440-byte common CB0 instances. Native/shader field meanings are
established in `HALOCE-STEREO-AUDIT-2026-09-15.md` and
`HALOCE-WORLD-MATERIAL-EVIDENCE-2026-09-15.md`: origin at `+0x240`, common
world matrix at `+0x270`, alternate/FP matrix at `+0x2B0`, shader origin at
`+0x320`, and the first 48 bytes of the model matrix at `+0x380`.

The tool matches exact retained origins to the final pre-timeout logged eyes,
checks finite values, groups matrices/models and evaluates the same world points
through each dominant world matrix. It does not label buffer counts as draws.

| Retained state | Eye 0 matching buffers | Eye 1 matching buffers | Origin separation |
| --- | ---: | ---: | ---: |
| 2768 initial | 1794 | 186 | 0.071541913 Saber units |
| 3212 initial | 2174 | 182 | 0.071532593 Saber units |

2768 origins are `(83.9866638,180.3908691,288.8556213)` and
`(83.9902573,180.3961182,288.7843628)`. 3212 origins are
`(81.8147736,180.3308868,289.0297852)` and
`(81.8473587,180.3292847,288.9661255)`.

For every matched buffer, origin `+0x240` equals origin `+0x320` exactly. The
dominant `+0x270` world matrices are bit-identical between the two origin groups
in each capture. Five eye-0 buffers in each capture have a different depth row;
all eye-1 matches use the dominant world matrix. All inspected fields are finite.
The supplied same-world-point samples at 0.25, 1, 10, 100 and 1000 units have
positive clip W in both eyes. There is no large translation or eye-dependent
world-matrix difference in these retained constants.

The retained model sets differ substantially: eye 0 has 78/81 distinct first
48-byte model transforms; eye 1 has only six in both captures. Most models are
identity. This is a lead for native geometry/visibility admission, **not proof**
that culling is the cause. Buffers are cached per material/object and initial
contents may predate the immediately preceding frame. The saved mono draw stream
does not establish their producer chronology or the failed frame's target binding.
No guessed transform or visibility-mask change is warranted by these records.

Reports contain exact resource IDs, raw hashes, matrix groups and point samples:

- `out/ce-renderdoc-evidence/frame2768-retained-eye-constants.json`
- `out/ce-renderdoc-evidence/frame3212-retained-eye-constants.json`

Reproduction examples (read-only inputs; output confined to `out/`):

```powershell
python tools/re/extract_ce_renderdoc_initial.py out/ce-renderdoc-evidence/frame2768.zip.xml --output out/ce-renderdoc-evidence/frame2768-initial --resources 382 603 617 13662 13677 13692 13707 13722 13737
python tools/re/inspect_ce_renderdoc_constants.py out/ce-renderdoc-evidence/frame2768.zip.xml --output out/ce-renderdoc-evidence/frame2768-retained-eye-constants.json --left-origin 83.98666381835938 180.390869140625 288.8556213378906 --right-origin 83.9902572631836 180.3961181640625 288.78436279296875
python tools/re/inspect_ce_renderdoc_constants.py out/ce-renderdoc-evidence/frame3212.zip.xml --output out/ce-renderdoc-evidence/frame3212-retained-eye-constants.json --left-origin 81.81477355957031 180.3308868408203 289.02978515625 --right-origin 81.84735870361328 180.32928466796875 288.96612548828125
```

Both tools completed on both captures. Raw decoded backbuffer images were
visually inspected. This is local evidence analysis, not headset acceptance.

## Source-archive review, later September 15 continuation

The latest user instruction requests a candidate without repeating screenshots
or capture/replay attempts. This review used source, existing JSON reports, and
new synthetic data only. No capture was decoded or replayed again.

The extraction and constant-inspection tools now reject invalid input with
explicit exceptions, including under Python `-O`. Initial texture decoding
admits only RGBA/BGRA UNORM and UNORM_SRGB; integer, signed and typeless formats
need separately established interpretation. The constants report now records
the original 64 world-matrix bytes and distinguishes numeric equality from
byte identity, including signed zero. Both preserved reports' dominant matrices
also retain identical float32 bytes when round-tripped from their existing JSON.

All four Python files pass syntax compilation. The existing isolated C++ helper
build passes without downloads or helper execution. Five synthetic checks cover
valid extraction, differing signed-zero matrix encodings, and optimized-mode
rejection of missing eye origins, truncated texture data and integer formats.
Record: `out/ce-evidence-tool-review-20260915.txt`.

Archive/output paths are caller-supplied for analysis tools; the documented
commands write under `out/`. ZIP entries are read by numeric payload ID and are
never extracted as filesystem paths. `prepare.py` confines its build directory
to repository `out/`, accepts only simple header filenames, and only invokes
compiler/build utilities. None of the reusable helpers launches MCC. The capture
boundary fixture operates on its own WARP device when explicitly run; the replay
helper opens an existing capture when explicitly run. Historical launch examples
in the replay evidence document remain unauthorized by this continuation.
