# Snap-turn audit and local implementation

Local implementation/verification complete. No headset acceptance, installation,
game launch or package was performed. Both MCC editions remain supported.

## Audit result

- H3: ApplyVrTurn from CamCopyHook changes g_gameYawRef by turn_snap_deg on
  a deliberate right-stick edge. Smooth mode and seat/wheel ownership retain
  their established rules. Halo3SeatAuthorsSteeringNow rechecks the active
  H3 seat gate rather than allowing a stale latched H3 driver flag to suppress
  ODST/Reach turning after a title transition.
- ODST: its camera-copy path calls the same ApplyVrTurn before head look.
- Reach: the prepared-pair camera path calls ApplyVrTurn(tracking.pad) before
  ReachApplyHeadLook. The published yaw pair carries this to hands and movement.
- H4: shared input calls Halo4ApplyVrTurn before aim-stick mapping; its separate
  gameYawReference uses the same snap-angle setting and edge helper.
- H2 Classic/Anniversary: audit found NO snap handler. The on-foot input path
  passed raw horizontal stick to native turning regardless of turn_smooth.
  That missing path has now been implemented as described below.

## H2 implementation

The H3 player behavior being matched is one configured angular step on stick
deflection, no repeated turns while held, rearm after centering, and a coherent
view/hand/aim/movement frame. H2 has a native body-yaw observer, so its method
differs deliberately from H3's camera-reference implementation.

The same prepared snapshot now includes fresh turn-stick input. The common
observer accepts one edge per serial and sets a world-yaw target from native
body yaw (or the outstanding target for rapid subsequent deliberate snaps).
The tracked camera/reference immediately supplies the residual angle between
that target and current native body yaw. Both renderer publications, stable
hand/gun carriers, direct firing direction, movement mapping and contact
tracking consume that same observer publication. No new hook or guessed field
was added. The established observer/native look feedback is documented in
H2 C-H2-8/C-H2-23; this is not the rejected on-foot hand-driven aim loop.

Game_ComputeHalo2SnapStick uses ordinary native RX and existing measured
look-axis/deadzone feedback to bring body yaw to the stick-selected target.
As native yaw changes, the reference's residual cancels that motion in the
already-snapped view. At the native axis's measured rest threshold, a
generation/target-matched atomic acknowledgement parks the small residual in
the reference, retaining the exact view instead of stepping it backward.
Stale observer input (>250 ms), invalid data, pause/F1 and theater emit no
pending native turn command. Returning tracking while deflected requires
centering first. Recenter/new generation resets the turn state. Vehicle
steering retains its native input ownership; an unfinished on-foot snap is
cancelled on vehicle entry. Smooth mode retains native continuous turning
after any already-started snap finishes.

The helper and readers use bounded arithmetic, no allocation, no scanning,
no logging in the observer hook and no new engine memory writes. Snap counts
are reported in the existing cold H2 report. Existing native damage and
collision remain untouched; changing the H2 tracking reference safely reseeds
physical melee rather than turning an artificial snap into a strike.

## Validation and remaining acceptance

Release build and all three CTest targets pass. Tests cover exact configured
angle/sign, duplicate observer/eye calls, held stick, centering, opposite
successive snaps, vehicle/menu takeover, smooth mode, lost tracking, yaw seam,
and identical camera/gun heading while the body converges. Synthetic native
feedback converges at 60/90/120/144 Hz for either look-axis sign with the
existing stick deadzone floor. Existing shared H3/ODST/Reach/H4 edge/ownership
tests remain passing. Reach consistency check and diff checks pass.

Logs: out/continuation-20260910-snap-turn-{build,tests}.txt.
Local uncommitted-worktree Release DLL SHA-256 (not a packaged candidate):
4A7DD459DCB640622D3340E8A167E623BC70B9829B10EFA4E9E409C7ECB5E640.
Headset checks still required: H2 both renderers, H3 regression, all-title
snap feel, actual game sensitivity, menu/vehicle/title transitions and handedness.
The synthetic model does not prove the native look loop's runtime convergence.

Latest user stop: resend the full goals list with accurate completion marks,
then wait for explicit instruction before implementing roomscale movement.
