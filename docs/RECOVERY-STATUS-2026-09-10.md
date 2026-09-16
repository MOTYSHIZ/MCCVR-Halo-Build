> September 15 update: the current candidate extends recovery to all titles.
> Read [all-title recovery evidence](ALL-TITLE-REENTRY-2026-09-15.md). The
> H3-only statements below describe the earlier implementation; the new shared
> candidate still needs headset verification.

# Recovery verification — September 10, 2026

Recovered worktree at 9569690 (failed H3 dual-firing behavior disabled).
The cumulative Release build succeeds, all three CTest targets pass, and the
Reach consistency gate passes. Build log: out/continuation-20260910-recovery-build.txt.
No MCC launch, installation, package or headset acceptance occurred.

Existing local changes provide H3 stale-camera retirement, staged core-hook
activation, callback quiescence before trampoline removal, guarded handling of
unmapped/restored/foreign hook targets, and full-handle vehicle lookup guards.
The synthetic hook-lifecycle test covers live disable, unmapped target,
same-address native restoration, foreign bytes, previously disabled hooks and
cancelling queued activation. It also checks the launcher/DLL recovery event.

F1 > Status and the launcher both expose "Force injection / recover VR".
The launcher sends a process-specific event to an already loaded mod after
checking the installation; it does not inject a second DLL. The worker retries
H3 camera setup through normal binding/load/tracking checks, coalesces repeated
requests, and reports runtime failure or unsupported title. The OpenXR session
cannot be recreated by this button. An unloaded/older mod cannot receive it.

This is local verification, not proof that the reported headset failure is
fixed. Current manual recovery covers H3 only. All-title transition recovery,
the H4 sceneTargetMissing latch, and target/H3 headset regression remain open.
Continue anatomical handedness as requested; retain those recovery limits on
the standing list instead of holding handedness work until headset testing.
