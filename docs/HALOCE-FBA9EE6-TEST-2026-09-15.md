# CE Original / Anniversary headset result - September 15, 2026

Tested source: `fba9ee60d857de1252960ea0177ce9c583d56ae0`.
The matching delivered DLL SHA-256 was
`C1060A4C1758287B192935491206D5CA6CBE202717121A94A2A2BA4BE5F89272`;
this is the package identity, not an independent hash of the user's installed
file. The supplied log names the source and Release feature configuration.

Steam / SteamVR OpenXR 2.17.9 / Oculus-family / 90 Hz. The log does not identify
an exact headset model. Preserved log:
`out/test-runs/fba9ee6-ce-horizon-hands-hud-20260915/user.log`, SHA-256
`437CF68F74F8F27B5BACB59B251B4CB99535C388792FAD889E9255B8AC59A72C`.

## User observations and current priority

- Original injects and its hands and weapon scale are good. Preserve those
  positive results.
- Recenter in either graphics mode leaves an unwanted vertical world angle:
  the player looks too far up or down. Match Halo 3's level world reference.
- Anniversary renders again, but its first-person hands/weapon are absent in
  the right eye; the left-eye presentation is flat and incorrectly scaled.
  The user explicitly reiterated that fixing Anniversary hands AND weapon
  scale is a primary requirement.
- Restore the HUD in both modes, preserving the working Original path.
  The user explicitly rejects another package with Anniversary HUD unresolved.

Continue through a new build ZIP and matching source ZIP, after these concrete
corrections and their relevant local verification. Package without `-Install`,
deliver both archives, then wait. No installation, game launch, game-folder
changes, PR or publication. Both editions remain supported. No other-title
feature work or CE melee/body-following expansion belongs in this candidate.

## What the log establishes

Anniversary's manual HUD replay is deliberately disabled at 16:51:55.083.
Its `installed=0` and zero draws persist throughout the run. The previous
candidate did not implement an Anniversary HUD replacement.

At 16:53:22.916, Anniversary hand-scale correction reports 52,063 applications
and zero stock fallbacks. Its independent first-person projection correction
reports **zero applications and 11,774 stock fallbacks**, despite installation.
The muzzle-particle correction separately reports 1,740 adjusted emitters.
Thus an installed first-person projection hook is not evidence that the live
weapon or hands used the corrected lens. The zero application count agrees
with the user's scale report and requires native-path investigation.

The run completes 3,439 total stereo pairs, of which 2,384 are Original pairs.
Anniversary world rendering therefore proceeds after the prior manual-replay
rollback. Successful pairs do not establish correct first-person stereo or HUD.
Repeated recenter events are present. A late Present stall occurs after CE
pause/exit transitions; this report does not establish all transition failures
resolved. The accepted cumulative pointer remains `4e01f28`.
