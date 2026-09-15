# CE exit-to-shell presentation refinement - September 15, 2026

## Player behavior being matched

Halo 3's reference behavior is a readable native pause screen during the level,
then the ordinary stable MCC screen after leaving it. CE must release its
head-locked pause override when its level presentation ends, even if the CE DLL
remains loaded in the shell. This change is confined to CE's presentation
branch; it adds no native bindings and changes no Halo 3 behavior.

## Evidence

The user reports that source `e524d219c91015aa4f97193fedb96e43ae009eac`
leaves the MCC menu attached to their head after exiting CE. Both supplied logs
are preserved in `out/test-runs/e524d21-ce-refinement-feedback-20260915/`:

- `run-152646.log`, SHA-256
  `1D13C9E9065B6216ADD8B9D13F691D3B3A4EB6410137D44357FB166F01D68418`.
  At 15:28:52.285 native pause requests head-locked presentation. At
  15:28:54.799 the camera heartbeat expires; at 15:28:56.083 CE detaches
  presentation and stereo turns off. There is no pause-clear request.
  CE remains the detected title with generation 1 in the shell.
- `run-152917.log`, SHA-256
  `A000DBC5DE58A377917146090CE09F1CA7201E7807B9106AE10792EB1D442C1C`.
  The same sequence occurs at 15:30:34.382 (pause), 15:30:39.168 (camera
  expiry), and 15:30:40.359 (detach). The pause override finally clears at
  15:31:00.847, after Halo 3 becomes the active title.

Both logs identify Steam, SteamVR/OpenXR 2.17.9, an Oculus-family headset, and
a 90 Hz panel. They do not identify an exact headset model.

Source confirms the missing lifecycle edge: CE reconciled pause solely from
its native clock, and only cleared the requested pause target when the active
title changed. The controls transaction retires on camera disarm, so the clock
becomes unavailable; unknown clock samples intentionally cannot mean native
resume. `VR_DetachGamePresentation` releases title rendering resources but does
not clear the shared pause target. Loaded-module identity therefore left the
old level's head-lock active in the shell.

The earlier preserved `a2b526a-ce-refinement-feedback-20260915/ce-user.log`
also has camera expiry (14:12:04.860) followed by paused-to-loading
(14:12:06.271), but that build had no connected CE pause presentation. This
distinguishes the new presentation leak from the existing native exit sequence.
No corresponding CE deployment-backup log exists: these candidates were
delivered as ZIPs, not installed by the agent.

## Change and regression

CE's pause reconciliation now receives the same ownership decision that drives
its existing presentation attach/detach branch. Losing that ownership clears
the pause target immediately and resets pending native mismatch timing. The
existing comfort fade completes normally. A retained native paused bit cannot
re-enter head-lock without renewed CE ownership. Missing clock data while CE
still owns presentation remains unknown, preserving actual pause behavior.
The clear request is issued once against the requested target, so subsequent
shell frames cannot restart the fade. Same-generation level re-entry begins a
new 50 ms native mismatch debounce.

The CE controls regression replays pause, unknown native state, ownership
expiry with the same module generation, repeated shell frames, stale paused
data, and same-generation re-entry. It also covers exit during a pending pause
debounce and complete generation retirement. With the old reconciliation
retained, the test fails on the first ownership-expiry sample; after the fix,
the Release `halomccvr_ce_controls_tests` passes.

## Transition stalls and limits

The reports also mention possible crashes. These logs establish presentation
stalls, not an exception address or a proven crash cause. The second run
successfully reaches Halo 3 gameplay after CE exit, then ends after another CE
load begins. Its CE worker continues reporting diagnostics after presentation
stops. The first run likewise ends with worker diagnostics still running.
This change must not be described as a proven crash fix.

The CE teardown audit retains the existing safety barriers: manufactured view
lists remain protected until native reset or stock preparation retires them;
hooks disable before callback/quiescence draining; the 14 core entries drain
in supported batches of eight and six; the eye cache refuses resource release
while borrowed. No unsupported lifecycle or native cleanup experiment was
added to explain the unlocated stall.

Headset verification remains required for CE pause, Save & Quit, stable MCC
screen placement, same-title re-entry, and a transition into Halo 3. A clean
build and these regressions do not advance the accepted-build pointer. No game
launch, installation, or game-folder write was performed.
