# Roomscale feedback and input correction

User report: 644148a roomscale did not move the body in any tested game,
including H3. Preserve improved weapon collision/melee; correct roomscale.
Log preserved at out/test-runs/644148a-roomscale-left-hand-feedback/user.log;
SHA256 8E1FF4F9567BDCC62F669E04EE7C511AA51364D036270841C5BB682C28FFE75A.
Steam, SteamVR/OpenXR 2.17.9, Oculus-family headset, panel 120 Hz.
The exact headset model is not in this log.

Evidence, not a claim of headset success:
- H2 recorded 3,170 unavailable camera samples and zero admitted samples or
  movement polls. H4 recorded 1,808 unavailable, zero admitted/polls. Reach
  recorded 1,977 unavailable, zero admitted/polls.
- H3 recorded 1,284 admitted, 3,084 unavailable and only 66 movement polls.
  That does not establish native body travel. The old log had no consumed
  travel metric. The user's no-body-follow report remains authoritative.
- Source Roomscale_Input was gated by Game_AllowsSharedGameplayFeatures. That
  function explicitly rejects ODST/Reach/H2/H4; it is an older H3-only policy.
  Roomscale now uses its own all-supported-title Gameplay admission while
  retaining each native camera's existing positive on-foot proof. No other
  shared gameplay feature's admission was widened.
- Both IatGetStateShim and the hooked exports called ProcessGetState after
  their previous handler. If the import handler calls a hooked export (or one
  export forwards to another), the same poll merges twice. The second merge
  reads the first merge's roomscale LX/LY as a physical stick and invalidates
  the input epoch. This is a reproducible source defect, not a claim that the
  old log proves which forwarding chain was active in every H3 poll.

The failed body-follow path was disabled in f868203 before refining it. The
corrected version uses one per-thread nesting scope shared by import and export
wrappers: native calls forward unchanged; only the outermost return fabricates
or merges VR input. Physical pad state therefore remains distinguishable from
our own output. Controller-admission loss/invalid pad also cancels roomscale
immediately instead of relying only on the 100 ms expiry.

H3 reference behavior retained: normal head-relative walking and controller
aiming. Body translation comes from native XInput walking in each title; only
observed horizontal travel adjusts its existing tracking reference. No new
engine hooks, guessed fields, native yaw writes or velocity/teleport writes.
The same equipped-model bounds and melee code remain enabled.

Worker telemetry now separates native, tracking and input rejections, manual
movement, demand samples, applied input polls and consumed travel in millimetres
(rounded per sample, diagnostic only). A movement poll is not labelled as
confirmed body travel. Hot paths do only bounded arithmetic and atomics.

Validation: Release build, all three CTest suites and Reach consistency pass.
The new test fixture compiles the actual roomscale.cpp against fake clock/title/
tracking services and exercises its camera/publication/input functions. It
covers all supported titles, nested import/export chains, actual physical-stick
priority, stale input, invalid tracking, on-foot rejection, changed generation,
toggle-off and non-gameplay modes. Synthetic closed-loop walking at 60/90/120 Hz
and three world scales reaches the requested body position inside the 2 cm
deadband, stops, preserves height, and consumes travel exactly once. Existing
math tests also cover recenter, blocked motion, teleports and nonfinite input.
These simulations do not prove Halo's acceleration, collision or rendering.
Pinned native weapon-bound verifiers are rerun for preservation checks.

Headset acceptance is pending across titles, both H2 renderers and H3 regression.
Required: physical steps/body following, stopping, walls/steps, stick priority,
turns, camera/hand/contact coherence, pause/recenter, tracking loss, respawn and
vehicle/title transitions. Independent head-following body yaw stays deferred.
CURRENT-STATE.md remains at 4e01f28. Package only; no install or launch.
