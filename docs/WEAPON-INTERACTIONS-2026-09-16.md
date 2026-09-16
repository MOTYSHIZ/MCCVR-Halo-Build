# Combined candidate: manual reload, weapon holsters, CE glare, H3 Cortana

The user requested all four items in the SAME candidate before ZIP delivery.
The staged 918e2f2 beam/Cortana-only packages are superseded for this handoff.
Both Steam and Microsoft Store remain supported. Cumulative accepted source
is still d47a98c; the new features and the two preserved corrections require
headset testing. No installation, game launch or publication is authorized.

## Controls

Both **Manual Reload** and **Weapon Holsters** are OFF by default in **Weapon
& Aim**. Keep your existing configuration; missing keys default off.

- Manual Reload: put the support hand at the support-side hip, press and hold
  its grip, bring that hand just below the weapon hand, and release the grip.
  The initial grab gives a short vibration; completing the gesture gives
  feedback and requests Halo's normal reload animation. Releasing elsewhere
  cancels. Take a new magazine from the pouch for each subsequent request.
- Weapon Holsters: put the weapon hand at its same-side shoulder, hold grip,
  and draw the hand away at least 25 cm while holding. This exchanges the
  held gun with the other carried weapon through Halo's normal Switch Weapon
  action. Release before another draw. The location can be changed to the
  weapon-side hip. Grab and completed draw have distinct haptic strengths.
- Match **MCC Reload button** and **MCC Switch Weapon button** to the chosen
  game's controller layout. The initial choices are X and Y. These are user
  mappings, not automatic detection or a claim about every MCC preset.
  Choices are saved independently for CE, H2, H3, ODST, Reach and H4; the two
  graphics modes within CE or H2 share that game's choices. These refer to
  the MCC/Xbox button names, before the mod's Reach left-trigger/X swap.
- Both gestures mirror with left-handed mode. Pouch/hip depth and body-zone
  radius are adjustable for seated and standing play. The zones follow the
  tracked head's yaw and position; there is no body tracker requirement.
- Ordinary reload, switching and interaction buttons remain available.
  Gestures operate in focused, tracked, on-foot, single-weapon gameplay.
  They cancel during pause, menus, cinematics, vehicles, death, tracking loss,
  title changes, handedness changes, recentering or other button/trigger use.
  Dual-wielding retains native controls so a holster gesture cannot drop the
  secondary gun. Taking a pouch magazine or grabbing the holster releases
  the two-hand aiming latch. Deliberately grip the barrel again afterwards.

## What is implemented

This is a shared VR gesture implementation for all six titles. The Halo 3
reference behavior is its existing controller transport: the native game owns
weapon availability, inventory, ammo consumption, reload timing, weapon-change
animation and damage. Every other title uses that same accepted XInput input
boundary with its own user-selected button. No title's native address, action
number, structure, reload animation or magazine marker is copied to another.

The reload interaction is a pouch-to-weapon gesture, followed by the native
reload. It does not pause or scrub the engine's animation, manipulate separate
magazine meshes, change ammo counts, or disable native automatic reloads.
Non-reloadable weapons obey the native rules. Haptic acknowledgment means the
gesture was recognized/requested, not proof that the engine reloaded.

The holster is a body gesture zone for exchanging the native carried weapons.
There is no extra body-mounted gun model, persistent empty-hand mode, extra
inventory slot or dropped weapon object. The native game determines the result
when only one weapon is carried. The reload receiver is a small controller-local
zone just beneath the weapon grip, deliberately independent of native gun scale,
visual calibration and unproven per-weapon magazine-marker layouts.

Implementation: `src/common/weapon_interaction_logic.h` owns bounded geometry
and the per-frame gesture state; `vr.cpp` feeds role-correct, physically tracked
controller/head poses at one predicted display time. OpenXR grip actions must
be active. Finite pose/quaternion checks and a 200 ms continuity limit reject
invalid or stale trajectories. A 4-second unfinished gesture times out.
Fresh released grips arm the gesture; carrying an already-held grip into a
zone does nothing. A draw/insert needs at least 120 ms, and completed requests
have a 500 ms cooldown. Already-claimed grips stay consumed through release
even when an interaction is cancelled, so a cancelled grab does not turn into
the grip's ordinary bumper action. The original raw support-grip edge still
feeds the two-hand latch, with explicit gesture exclusion.

The published input pulse has a hard 120 ms expiry. The XInput-facing reader
checks focus, mode, tracking ownership, title, generation, space epoch,
handedness, option state, selected buttons and sample age (150 ms). Optional
output is discarded independently; it cannot detach VR or stop the camera.
Native physical-gamepad input is preserved. Gesture melee does not add a second
button attack while a weapon interaction owns a grip or input pulse.
The existing haptic bridge routes feedback to the correct physical hand and
honors Controller vibration intensity. Counters and readiness/configuration
logs are emitted by the cold title worker, never by the gesture hot path.

## Preserved corrections and tests

The 918e2f2 CE Anniversary flare-envelope guard and Halo 3 shot-qualified facing
correction remain unchanged. Their evidence and limits are in
`CE-BEAM-H3-CORTANA-2026-09-16.md`. Earlier CE vehicle steering and seated native
crosshair capture remain in the cumulative source.

1. Enable each new toggle separately, then together. Match MCC button layouts.
   Try partial magazines, full magazines, no spare ammo, non-reloadable weapons,
   one/two carried weapons, repeated gestures, ordinary buttons, two-hand aim,
   both weapon hands, seated/standing play, and shoulder/hip holsters.
2. Repeat in CE Original/Anniversary, H2 Classic/Anniversary, H3, ODST, Reach,
   and H4. Check pause/resume, death/respawn, recenter, title changes and briefly
   lost controller tracking. Existing controls should recover without a delayed
   reload or weapon switch. Check dual-wield native controls in H2/H3.
3. CE Anniversary: watch a Forerunner beam tower fire, turn/walk past it, and
   compare lingering glare and both eyes. Check ordinary lighting and the
   preserved CE vehicle steering/crosshairs.
4. H3 Cortana: approach and melee the rescue object. Check steady facing through
   the effects, the real rescue cutscene and return to gameplay. Also check
   ordinary aiming, movement, recoil comfort, snap turn and recenter.
5. Send the new log and observations, including title/mode and times of any
   symptom. This shared-input addition needs target-title and Halo 3 regression
   results. Offline tests do not establish headset comfort or acceptance.

## Local verification / delivery record

`weapon_interaction_tests.cpp` executes shipping gesture logic across all six
titles, both hands, both holsters, configured button/trigger transports,
coordinate transforms, release/cancel/recovery and stale-state cases.
`dpad_action_tests.cpp` additionally executes the actual shipping pad-publication
reader against title/mode/focus/generation/space/configuration failures. Core
tests cover default-off behavior, per-title config persistence and invalid data.
Final local Release, all 36 CTest suites, the Reach gate, 1,281 gesture checks
and 570 production OpenXR/pad checks pass. The CE glare/Cortana and existing
CE vehicle/crosshair source was audited unchanged from 918e2f2. Final package
process exit and archive identities must be recorded in `out/weapon-interactions-current-handoff.json` after completion.

The previous chat already verified package process exit 0, all 35 pre-addition
tests, and the Reach gate. The reported discrepancy was PowerShell handling
of the MinHook CMake deprecation warning. The final package captures stdout,
stderr and the real process return code separately. Run package-candidate.ps1
without -Install, verify matching build/source archives, deliver both, then
wait. Launcher source is unchanged; generated defaults now include the two
new disabled options and their settings, so the old config-byte-equality claim
does not apply to this expanded candidate.
