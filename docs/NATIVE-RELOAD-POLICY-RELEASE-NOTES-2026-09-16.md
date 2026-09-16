# Optional manual reload refinements

Candidate above accepted Alpha 0.4.1. Supports CE (both graphics modes), Halo 2
(both graphics modes), Halo 3, ODST, Reach and Halo 4, on Steam and Microsoft Store.

In F1 > Weapon & Aim, enable **Manual Reload** to see two new options:

- **Disable automatic reload**: an empty gun no longer starts the normal automatic
  reload. Magazine insertion, Needler shake and ordinary reload input still request
  a native reload.
- **Skip reload and weapon-ready animations**: suppresses the identified first-person
  reload/ready animations and shortens their native waits. The engine still performs
  the ammo transfer on its simulation update; it is not an immediate ammo write.

Both options default off and can be enabled separately. Existing settings persist.
Ammo reserves and native reload eligibility still apply. Shell-by-shell weapons
retain their native sequence. Authored non-animation delays, charging, overheating,
and unproven special animation variants remain stock.

The new behavior requires headset testing. Check each option separately and together:
empty a gun, insert a magazine, switch weapons, reload a partly full gun, and try a
shell-loaded gun. Include a Halo 3 regression test and title transitions. Report the
title, edition, headset and OpenXR runtime with HaloMCCVR.log.

Release build, automated tests and signature checks validate preparation; they do
not establish in-game success. This package was not installed or launched by Codex.
The matching source includes NATIVE-RELOAD-POLICY-2026-09-16.md with evidence and limits.
