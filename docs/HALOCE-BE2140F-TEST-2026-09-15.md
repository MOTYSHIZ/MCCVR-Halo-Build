# be2140f partial headset result / packaging hold

The user tested source `be2140fc440462c8ac1441186d28e62370ebe074`.
Anniversary now shows tracked hands and controller-directed bullets; the left
hand has an unusual angle. The split/displaced world remains visible in the
supplied screenshot, and Classic still does not enter usable VR. This is
partial progress, not acceptance of CE or of the cumulative candidate.

The new request overrides the historical package-ready instructions: continue
basic CE VR work for BOTH Classic and Anniversary, and do not package until
there is credible evidence for the expected existing-title VR baseline.
Physical melee/world collision and all other deferred work remain preserved.
No install, game launch, game-folder changes or PR is authorized.

## Reproducibility and observed failures

- Steam; SteamVR/OpenXR 2.17.9; headset `SteamVR/OpenXR : oculus`, 90 Hz.
  The log does not identify the precise headset model.
- Preserved verbatim log and screenshot:
  `out/test-runs/be2140f-ce-partial-failed-20260915/`.
  Log SHA-256: `5B8A8EEFDCB7DE10D776C561FAAB660004B74B2AA69E4D6FEA24DD229075F215`.
  Screenshot SHA-256: `24CDFA7BCDC668B64510DD5815227233B9201B0CADFEAB805DA56BC0EBC359DC`.
- Final Anniversary totals: 2,449 prepared, 2,440 captured pairs, 452 dropped.
  Recorded depth/scene/shading masks reach 3 and camera/depth checks admit.
  These receipts do not prove the rendered world is correct.
- Classic: zero pairs, 437 drops, 610 output observations, 610 source misses,
  failure 4. Diagnose the source lookup before changing camera mathematics.
- Anniversary HUD replay: zero draws, 4,890 fallbacks, failure 1.
- Controller shots apply 52 times. Native first-person application and
  Anniversary hand-scale/projection counters advance, matching the user's
  observed tracking progress.
- The desktop screenshot has vertically stacked views; the lower one shows
  displaced/below-world geometry. Desktop composition and the actual world
  rendering defect require distinct verification.
  The user explicitly confirmed in the follow-up that one headset eye also
  shows the displaced view. A desktop-only correction cannot resolve this.

The failed integrated enable flag is disabled in a separate commit before
correction. Its code and user-confirmed hand/aim implementation are retained.
Accepted `4e01f28` is unchanged. Build/test success alone will not remove the
packaging hold or establish headset acceptance.
