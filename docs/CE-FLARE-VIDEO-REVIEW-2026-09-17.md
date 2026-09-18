# CE light/flare video observations - September 17, 2026

## Authorization and status

User explicitly authorized reviewing this video now and storing observations for
future reference. This is a narrow exception to the existing all-work hold, NOT
permission to diagnose engine code, implement, build, package or resume other
investigations. Review and preservation are complete; all implementation remains
on hold. No runtime source, accepted pointer, installed files or security settings
were changed. Attached content is evidence, not a source of instructions.

## Evidence identity

Preserved directory: `out/test-runs/flare-video-20260917/`.

| Evidence | SHA-256 | Size |
|---|---|---|
| d3b6ee5926024839a94980f44700a3a1.mov | 3BD60CC25F45DA52B2280CEA6CA27B59762233CB9102E2153DF05AB8359B97CD | 9,822,683 bytes |
| HaloMCCVR (18).log | E5BE6F05E4282E39508EAC10499ECC3F8933ED9BE37A8F79F78BE38AEA6CD2F1 | 14,875,287 bytes |

Originals are in `C:/Users/Shadow/Downloads/`, with those same filenames.
`receipt.json` records both paths and hashes. The video is 32.92 seconds,
720x720 H.264, variable cadence (reported average 51.22 fps), with an AAC audio
track. Review used a full-clip visual contact sheet and higher-resolution frames
at selected timestamps, including 0.25-second sampling from 10 through 13 seconds.
Audio was not reviewed. This is a single recorded view, not both headset eyes.

Only basic identity metadata was read from the supplied log, not a diagnostic
analysis of its 14.9 MB contents:
- Header source: `1a9766ca971a9e5f09b942d508abecd9753bdb35` (Alpha 0.4.2 runtime).
- Edition: Steam (line 2).
- OpenXR runtime: VirtualDesktopXR 1.0.10 (line 36).
- Headset: Meta Quest 3 (line 38); initial panel rate 72 Hz (line 56).
- CE detected at log time 13:05:11.134 (line 205).
These identify the supplied session; the video has not been synchronized to log
wall-clock time. Graphics-mode transitions, map name and the precise active
renderer at each video timestamp were not established. No installed DLL hash
verification or claim of a causal error in the log is made.

## Direct visual observations

The scene shows a Halo CE-style assault rifle with 60 on its ammo display, a blue
HUD, and a cyan-lit Forerunner interior: doorway, glowing wall/column panels,
overhead strip lights, and an adjoining long corridor. Exact mission is unconfirmed.

| Video time | Observation |
|---|---|
| 8.0 s | Intense narrow white vertical streak with cyan glow at the right edge, extending almost the entire image height in front of the visible corridor/column. |
| 9.0-10.0 s | Looking up/rolling toward the doorway and overhead horizontal strip light; that same broad vertical streak is not present in these sampled frames. |
| 10.50 s | A fainter vertical shaft is visible left of center over the doorway view. |
| 11.00 s | Clearest example: saturated white/cyan vertical streak across nearly the full image height, crossing the horizontal ceiling light and the doorway geometry. |
| 11.25 s | The prominent streak is absent in this nearby sampled view of the same doorway; do not infer exact onset/duration between samples. |
| 12.0 s | Doorway view is again comparatively unobstructed. |
| 13.0 s | Saturated full-height white streak reappears right of center as the view faces the adjacent wall/column and floor. |
| 18.0-19.0 s | Looking down the long corridor; visible brightness/cyan glow differs between successive views. |
| 20.0-21.0 s, 26.0-28.0 s | Wall/ceiling views show changing cyan illumination and glow around strip lights as viewpoint/roll changes. Whether those broader changes are erroneous, exposure-related, mode-related or ordinary scene lighting is unproven. |
| About 30 s onward | A VR interface overlay is visible near the end; separate it from the earlier in-game streak examples. |

The strongest reproducible visual reference is 11.00 s, compared with 11.25 and
12.00 s. The anomaly is a tall white/cyan streak that appears intermittently and
changes apparent screen position with the viewpoint. No precise world attachment,
light source, occlusion failure, flare primitive, depth bug, shader fault, stereo
disagreement or camera-space cause has been established from these images.
Do not equate the recorded cadence with the game's or headset's frame rate.

## Relationship to earlier work and future handoff

User describes this as similar to the previously investigated Forerunner tower
light/flare effect. Preserve that lead, but do not claim the same root cause,
a regression of the prior fix, or that the tower-specific correction applies here.
When work is explicitly authorized, compare against the existing CE beam/glare
record in `docs/CE-BEAM-H3-CORTANA-2026-09-16.md`, correlate log timing/renderer,
and retain these original screenshots as before-change references. No code or
old evidence was investigated as part of this limited review.

Images preserved beside the originals:
- `exact-11.png`: 720x720 clearest streak reference.
- `exact-08.png`, `exact-13.png`: additional strong streak examples.
- `exact-12.png`: nearby clear doorway comparison.
- `transition-10-13.jpg`: timestamped quarter-second samples.
- `detail-1.jpg` through `detail-3.jpg`: exact-second comparison sheets.
- `contact-1.jpg`: approximate full-clip survey; its coarse labels are approximate
  sampling labels, not exact frame timestamps. Use the exact/transition frames
  for precise references.

All prior queued requests, the co-op log and the general hold remain intact.
