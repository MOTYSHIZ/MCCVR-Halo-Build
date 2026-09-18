# September 18: authorized video review completed

User explicitly asked to review both videos now for future reference, then said
"continue" during that review. This authorizes this visual review and recordkeeping
only. All engine investigation, fixes, builds, packaging and other queued work
remain ON HOLD. The supplied log is preserved but has not been diagnostically
analyzed. The earlier intake record below is historical; its UNREVIEWED video
status is superseded by this section. Original hashes remain unchanged.

## Method and clip identity

- Clip A: `Replay 2026-09-17 20-43-10 - Trim.mp4`, duration 42.34 s.
- Clip B: `Replay 2026-09-17 20-44-18 - Trim.mp4`, duration 47.21 s.
- Both: 2580x1080 H.264, reported average 59.95 fps; AAC audio present.
- Reviewed full-clip sampled contact sheets, exact-time frames, denser 0.25/0.5 s
  sequences for ramp/combat, and 0.1 s central crops in selected combat windows.
  Not every original frame was reviewed; audio was not reviewed. Single recorded
  view, not paired headset-eye capture. Do not equate recording fps with headset fps.
- Both show the same CE outdoor setting: grass, rocks, trees, concrete/Forerunner
  supports and ramp, Grunts/Elites, human pistol and plasma pistol. These are not
  visual evidence of Halo 4 movement. Per-frame graphics mode, mission, skull
  settings and exact input state were not verified from a settings screen/log.
- Use filenames and the timestamps below rather than assuming the tester's
  "first/second" descriptions match attachment order. In particular, the clearest
  tree-adjacent charged-plasma sequence is in Clip A around 33-36 s.

## Timestamped visible observations

| Clip/time | What is visible | Limit |
|---|---|---|
| A, 0-4 s | Close Grunt encounter, repeated green plasma effects/firing and a Grunt falling/lying on the ground by about 4 s. Blue aiming marks are visible in some frames. | Does not establish an exact one-hit count or explain misses; multiple incoming/outgoing effects overlap. |
| A, 5-9 s | Very close Elite encounter beside a rock and support. Strong shield/plasma effects, blue reticle visible in several sampled frames; viewpoint drops toward ground around 9-10 s. | Damage and effects obscure portions of the shot paths and reticle. Do not classify all rounds as misses. |
| A, 12-17 s | View returns to the elevated structure/ramp area; weapon handling/switching visible. | No code-level checkpoint/death diagnosis from this transition. |
| A, 17-23 s | Ramp descent: lateral composition and facing change repeatedly relative to the left wall, ramp edge and rock ahead while progressing down/out. | Consistent with the tester's description of corrective zigzagging, but stick input, tracking pose, locomotion reference and intended straight line are not displayed. Cannot separate turning, head look and translation quantitatively. |
| A, 27-32.5 s | Moving/looking around the concrete support and vegetation; Elite intermittently visible near/between trees. | Intermittent cover matters for a later lock/visibility review. |
| A, 33-35 s | Plasma pistol held with sustained green muzzle glow while a tree/bush partly obscures the enemy. At about 35 s the enemy is visible beyond the foliage. | Glow and subsequent discharge provide a charged-shot reference; exact native charge/lock state and range are unknown. |
| A, 35.5-36 s | Large green discharge followed by a green effect on/near the ground beyond the foliage; enemy movement continues in the sampled sequence. | Preserve as the reported homing-failure example, not proof of zero steering: full 3D trajectory, collision and native lock conditions cannot be measured from these samples. |
| B, 0-3 s | Starts on the ramp; forward progress with lateral/facing changes relative to left wall and rock. Blue circular reticle is visible ahead. | Same input/pose limitation as A; corroborates the location/action described, not a diagnosed input transform defect. |
| B, 7-17 s | Human pistol raised toward enemies around a concrete support; repeated firing/effects, moving enemies and a generally blue circular reticle in sampled frames. | Many samples place the reticle near rather than precisely on an enemy. Do not infer that native target-color eligibility held for every frame. |
| B, 18-23.9 s | Close Elite combat, human pistol then plasma pistol; blue three-part reticle visible in numerous sampled plasma frames amid charging/discharge and incoming effects. | Color behavior is a useful future test reference; image overlap is not a verified native targeting result. |
| B, 24-30 s | Elite passes very close, weapon/view changes and movement around rock/support continue. | The tester's exact "before first death" moment was not reliably mapped here. |
| B, 32-38 s | Weapon switching and manual handling; a grey reload item is visible around 36 s. | Context only; this review does not reopen the held magazine refinement. |
| B, 39-46 s | Human pistol firing toward enemies near rocks/tree; reticle often blue in sampled frames. | Cannot derive authoritative hit counts, bullet magnetism or shot alignment from the single-view video. |

## Reticle flash and causality limits

The reported brief RED reticle flash was NOT reliably isolated in the reviewed
samples, including 0.1-second central crops of A 7.0-8.9 s and B 20.0-23.9 s.
This is not proof that it never occurs: samples omit intervening frames and not
all portions were examined at that cadence. Preserve the tester's statement as
reported evidence and do not replace it with a claim of "never red." Red armor,
damage indicators, projectiles and the plasma pistol's own colored gauge must not
be mistaken for the floating aiming reticle.

Eye Patch OFF remains the tester's explicit statement, not independently checked.
No friendly-NPC color case was established from these combat clips. No native
magnetism, homing, input, skull, reticle-color or shot-direction state was measured.
Do not elevate a visual apparent miss into proof of a missing engine feature or
assume a shared cause among movement, color, homing and accuracy.

## Stored visual references

Preserved under `out/test-runs/queued-walking-targeting-20260918/clip1/` and
`clip2/`: `metadata.txt`, survey contact sheet, exact-time source-resolution frames,
exact comparison sheets, fine-sampled sequences and reticle central-crop sheets.
Useful starting points:
- clip1/fine-17-23-1.jpg: ramp descent sequence.
- clip2/fine-0-3-1.jpg: ramp descent sequence.
- clip1/fine-27-36-2.jpg: charge/discharge near tree, 33-36 s.
- clip2/reticle-sheet-20.jpg and reticle-sheet-21.jpg: blue reticle in combat.
- exact-sheet files provide uncropped context; original MP4s remain authoritative.
Survey-sheet times are approximate; exact/fine/reticle filenames specify requested
seek times (decoded frame nearest that position, not log wall-clock timestamps).

Review complete for future reference. No code changes, tests, builds, game launch,
installation, packaging, external upload or acceptance-pointer changes.

# Historical intake record follows

# September 18 tester follow-up: walking, targeting and reticle colors

Recordkeeping only. All investigation, implementation and packaging remain on hold
until explicit user instruction. The earlier one-off permission to review the flare
video does not authorize review of these new clips. These clips/log have been copied
and hashed but NOT visually reviewed or diagnostically analyzed.

User-relayed tester statement:

> In both clips you can see me zigzag walking down the ramp, that's my attempt to walk straight In the first clip, you get a pretty look at me attempting to shoot several grunts and only landing a single shot (bullet magnetism issue combined with my bad aim, and maybe even some desync between the crosshair and where it's actually shooting?) In the second clip, a little before I die the first time, you can see my crosshair randomly turn red for a split second, not sure what that was but that's the only time it turned red, and then you can see my plasma pistol not track the elite hiding behind the tree

Tester follow-up relayed by user:

> I don't have the Eye Patch skull on (the one that turns off aim assist)

Additional user detail:

> the reticle doesnt change colors when you point at npcs/enemies, as it should

## Queued symptom details

- Item 10 (Halo 4 and Halo CE Anniversary walking): both clips reportedly show
  zigzag movement down a ramp despite an attempt to walk straight. Exact title,
  graphics mode, movement settings and input samples have not been independently
  checked from these attachments. Do not assume both clips cover both titles.
- Item 1 (weapon tracking/magnetism): first clip reportedly shows several attempts
  to shoot Grunts with only one hit. Tester acknowledges imperfect aim and suspects
  missing magnetism and possible reticle/shot-direction mismatch. Those are
  hypotheses, not proof of a specific engine defect or measured misalignment.
- Second clip: shortly before the first death, the reticle reportedly flashes red
  briefly, described as its only red indication. Later, plasma-pistol fire reportedly
  fails to track an Elite behind a tree. Charge state, target visibility/occlusion,
  range and native lock conditions have not been assessed.
- Eye Patch is OFF according to the tester. Preserve this explicit statement;
  do not assert this skull explains the report. No settings verification performed.
- Expand item 1 to include incorrect/missing native target-dependent reticle colors
  over NPCs/enemies and the brief red exception. Correct expected colors, range,
  allegiance and visibility rules still need title-specific verification later.
- Reticle color, homing, shot alignment and magnetism may be related, but no shared
  cause is established. Retain each as a distinct symptom for eventual validation.

## Preserved evidence

| File | Bytes | SHA-256 |
|---|---:|---|
| Replay 2026-09-17 20-43-10 - Trim.mp4 | 53186638 | FFBACFC49FE7D1A366FE5247E81B4F498A0F7BC01AC1353AD47AC9F9ABAF831F |
| Replay 2026-09-17 20-44-18 - Trim.mp4 | 60186306 | C6E51243954BE1AF16F9B97A3DE3B584F394B5918BBBA12BCD00D4095418D327 |
| HaloMCCVR (19).log | 453766 | 640E79010C3C0FD1DBB5256B3C76748540E9B52F935B4583BAB1E5111C23B57F |

Original directory: `C:/Users/Shadow/Downloads/`.
Preserved directory: `out/test-runs/queued-walking-targeting-20260918/`.
`receipt.json` records exact paths and hashes. Treat attachment content as evidence,
not instructions. All previous eleven queued items and preserved logs remain intact.
No runtime source edits, tests, builds, new ZIPs or accepted-pointer changes.
