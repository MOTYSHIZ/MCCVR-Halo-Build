# September 18 cumulative refinement candidate

One build supports Steam and Microsoft Store and all six Halo titles. Based on
accepted Alpha 0.4.2 source `1a9766ca971a9e5f09b942d508abecd9753bdb35`.
This is a test candidate, not headset-accepted or completion of the entire
standing list. Nothing was installed, MCC was not launched, and the accepted
pointer is unchanged.

## Implemented in this candidate

- Circular 1024x1024 gun-side zoom lens: H3, ODST, Reach, H4 and H2 Classic/
  Anniversary. R3 release toggles it; right-stick up/down changes magnification.
  Main views remain wide; placement follows handedness. The lens refreshes every
  rendered frame rather than alternating expensive/cheap frames. The old refresh
  divisor is readable but ignored. Actual headset clarity/frame rate remain
  untested, and the extra world render costs GPU time. CE retains native zoom.
- **F1 > Picture > Disable CE Anniversary lens flares**, default **off**.
  `ce_anniversary_disable_lens_flares` suppresses the verified native flare draws
  only in owned Anniversary VR eyes. Both eyes use the same frozen setting.
  Turning it off restores existing flare behavior. World lighting, CE Classic
  and other titles remain unchanged. This is a workaround to test against the
  white/cyan streak, not a confirmed diagnosis or general lighting fix.
- Separate default-off **Shortened reload animation**, all six titles. Manual
  insertion retains the native chambering tail and ammunition rules. Existing
  full animation disable is preserved and takes priority. Missing native
  animation evidence keeps the full reload.
- Authored textures for all 42 extracted visible magazine models; orange reload
  token for known H4 Promethean zero-geometry models.
- Magazine insertion/release haptic alongside grab feedback; per-weapon insertion
  targets derived from the actual visible model and current pose.
- Optional per-gun alignment saving/automatic selection; separate left/right
  visible-hand position sliders. Zero offsets preserve native bytes; weapons,
  shots and interaction coordinates remain independent of visual-hand offsets.
- Default-off independent dual aim for **H2 and H3**, including per-hand native
  target acquisition and later homing consumers. No ordinary dual-wield expansion
  is enabled in ODST, Reach or H4.
- Default-off authored barrel origin/direction option in all six titles using
  each title's verified muzzle data and native shot/target handling.
- Roomscale stopping-tail/reference-history corrections for H2/H3/ODST/Reach/H4.
  Movement uses native collision-constrained walking. CE body following stays off.
- Vehicle input audit and targeted H2/CE/H4 corrections: coherent H2 steering/view
  references, unrotated seated throttle, CE seated turn/camera follow and H4
  seat-aware input. Existing good title behavior retained.
- Optional automatic smooth turn in vehicles preserves saved on-foot snap.
  H2 retains continuous native seated controls.
- Optional flashlight-input disable preserving two-hand grip.
- Reach wind-state replay prevents the second eye advancing the same foliage
  wind state again; other foliage mismatch causes remain unconfirmed.
- ODST camera-admission correction for the log-21 right-stick-aim report;
  analogous title paths audited. Headset confirmation remains due.
- H2 full-salt ownership, no replay of faulted native aim calculations and safe
  observer retirement; ODST checkpoint seat-storage lifetime correction.
- CE continuous controller-directed native targeting preserving native range,
  team, visibility, homing and reticle rules.
- Radial walking deadzone preserves direction. Thumbrest D-pad and successfully
  delivered menu-pointer modes consume conflicting XR/gamepad/melee inputs and
  drain held controls before normal input resumes.
- HUD-hide option covers all seven D3D11 draw variants; native callbacks and
  menus remain available.

Earlier all-title manual VR recovery, anatomical handedness, separate contact/
physical-melee/gesture controls, melee speeds and slider arrows are preserved.
Recovery was already expanded beyond H3 in the September 15 work.

## Investigated or partial, still unfinished

- **CE separate circular zoom lens, both graphics modes.** Anniversary's third
  primary view reuses/clears left-eye depth before the left scene draws. Pinned
  native selector/binder/clear emulation confirms this. Replaying the whole
  frame also consumes worker-completion state twice. Neither route is enabled.
  Independent scope rendering/resources remain to implement; Classic's separate
  adapter is also not implemented.
- **CE Anniversary streak/lighting cause.** Video shows the vertical streak, but
  the log has no synchronized effect attribution. New toggle effectiveness needs
  the user's comparison; broader lighting is not declared fixed.
- **H2 Cairo firing / Outskirts post-cutscene crash and vehicle checkpoint crash.**
  Ownership/lifetime defects were corrected, but supplied logs do not establish
  these reported exceptions' causes. Co-op compatibility remains unconfirmed.
- **Reach grass/broader foliage.** Wind advancement corrected; other causes and
  version/headset-specific reports remain open.
- **CE reticle/targeting appearance.** Brief red flash and in-headset homing/color
  behavior remain unconfirmed despite continuous-acquisition implementation.
- **Contact/melee:** sustained/sliding jitter, sticking, unarmed/secondary damage,
  ODST Mythic SMG, damageable world objects, custom weapons and other-headset
  coverage need further work/testing. Runtime model bounds are already present.
- **Alignment/handedness:** stock calibration, dual-weapon profile behavior and
  complete interaction/haptic acceptance remain to verify. New offsets and
  hidden-anchor checks pass locally.
- **Transitions/recovery:** all-title recovery exists; exhaustive mission,
  multiplayer, title-switch, custom-map and long-session regression tests remain.
  Zoom timing/clarity, handedness and Halo 3 headset regression are unverified.

## Not implemented in this continuation / retained queue

- H2 Classic/Anniversary and H4 first-person vehicle presentation/seat expansion,
  beyond the input and reference corrections above.
- H3/H2 Anniversary lower-edge/corner visibility when looking up, preserving H2
  Classic visibility.
- Enabling/validating CE roomscale body following.
- Remaining version-specific Reach graphics-setting/image-quality/body/contact
  reports not closed by the specific changes above.

Explicitly deferred: H4 damage blackout, minor H2 tank-exit reticle report and
independent head-following body yaw. Launcher signing/SmartScreen warnings are
excluded. Non-native ordinary dual-wield expansion was removed from scope by the
user. No deferred request is marked complete.

## Validation and test priorities

Packaging runs Release, all 58 CTest suites and the Reach consistency gate.
Production scope fixtures: ODST257 checks, Reach899, H43478, H2both226, including
fault cleanup and state restoration. CE native projection emulation:384 cases;
depth-routing emulation includes the rejected third-view case. These local checks
are not live gameplay or headset acceptance.

First compare the CE streak with Picture's flare toggle off/on. Then test R3 zoom
in implemented titles, both handedness modes, transitions and ordinary H3 play.
Test reported crash scenarios separately. Include edition, runtime, headset and
package source identity with results.

Full preserved scope: `docs/CONTINUATION-REFINEMENT-LIST.md`. Current evidence:
`docs/ZOOM-AND-CE-FLARES-2026-09-18.md` and
`docs/REFINEMENT-WORK-2026-09-18.md` in the matching source ZIP.
