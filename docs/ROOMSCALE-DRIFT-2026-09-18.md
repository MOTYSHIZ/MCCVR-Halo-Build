# Roomscale body drift and response refinement

User reports that physical walking with roomscale enabled lets the body slide
away, eventually requiring recentering. Preserve native movement/collision and
make the body follow physical travel responsively, without automatic recentering.
Halo 3 reference behavior sought: body travel plus remaining tracked lean equals
one physical movement; looking/turning does not create extra translation.

Two source-level defects were reproduced by production-code fixtures:

- RoomscaleFollow consumed native travel only while its immediately previous
  requested stick was nonzero, and only while travel pointed toward the old
  remaining error. Native acceleration/braking can continue after a zero request
  or overshoot the target. That travel was then added to the rendered view rather
  than subtracted once from remaining lean. A 20 cm physical step followed by
  native positions 19/21/23 cm reproduces the drift without any native offsets.
- Roomscale_Camera kept feedback history thread-local while changing a shared
  title tracking reference. Alternating sequential engine callback threads could
  seed a new history mid-step and lose the displacement. Its existing nonblocking
  publishing guard already provides exclusive access; the history now belongs
  to that guarded title stream instead of one thread.

The follow loop accounts for observed stopping travel, including overshoot and
resolved sideways motion, while the same input/tracking/native admission remains
valid. It then corrects remaining position error. A velocity filter (60 ms) and
bounded position/velocity feedback (3.5 position gain, 0.18 velocity coefficient,
maximum 0.8 stick magnitude) improve response and braking. The final-position
quiet band is 1.5 cm. These parameters are simulation-validated, not measurements
of every Halo title's native acceleration. Native walking still owns walls,
steps, networking and speed. No unit-position write, teleport or auto-recenter.

Stopping authority ends after a quiet interval (150 ms), and always within
500 ms without another movement request. The loop retains finite-value,
generation, native-teleport, manual-input, recenter, tracking and stale-input
rejections. Height is untouched. Later unrelated native movement is not consumed
once settling has expired. The algorithm cannot independently classify every
external impulse occurring during an active native walk; no such runtime claim
is made.

Validation: new regressions fail before the fix and pass after it. Fixtures use
actual roomscale.cpp and shared math, with fake clock/title/tracking services.
They cover callback migration, stopping overshoot, delayed/accelerating movement
at 60/90/120 Hz, 30 Hz and per-frame native body samples, 0/33/66 ms input delay,
2/4 m/s synthetic native speeds, a head turn during the step, and 24 repeated
out-and-back walks. Simulated final body error is below 2.5 cm; repeated view
drift stays below 1 mm. Those are fixture limits, not promised headset accuracy.
Existing wall, manual stick, recenter, stale tracking, generation, scale and
height tests remain passing. Core production transport covers H2, H3, ODST,
Reach and H4. CE's pre-existing experimental roomscale path remains disabled;
this does not claim newly enabled CE roomscale.

Release/all49 CTests/Reach consistency gate pass. Logs:
- out/refinement-20260918/roomscale-before.log (reproduced failures)
- out/refinement-20260918/roomscale-after.log (pass)
- out/refinement-20260918/{build,tests,gate}-roomscale-drift.log

Shared-path target-title headset and Halo 3 regression results remain required.
Accepted pointer stays 1a9766c. No ZIP yet: every other standing item remains in
scope. No install, MCC launch or publication.
