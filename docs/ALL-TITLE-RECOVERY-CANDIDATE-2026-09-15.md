# Title switching and Force Inject recovery

This candidate builds on user-tested `558fb2c`. CE Original and Anniversary
are now explicitly headset accepted; their camera, rendering, HUD, graphics
switching, tracking and contact implementation is preserved.

Reach failed to enter VR after CE because its display verification required
every other game module to unload. Display admission now follows the selected
active title, then verifies Reach's exact current native display resources.
CE can retain its safe native cleanup lifetime without blocking Reach.

Automatic title selection includes CE's verified native clock when other game
modules remain loaded. Force injection / recover VR in the launcher and F1
Status retries the selected game's VR setup across CE, Halo 2, Halo 3, ODST,
Reach and Halo 4. CE and Halo 2 include both graphics modes. Recovery retains
normal level, camera, signature and resource checks and reports its progress.

Keep your existing `halomccvr.cfg`. One build supports Steam and Microsoft
Store. This package is prepared for testing; it has not been installed or
launched by the agent. Source and build ZIPs describe the same commit.

## Headset checks

1. In one MCC session, play CE and switch to Reach. Confirm VR starts, then
   test Reach's HUD height control.
2. Switch among all supported games and return to earlier titles. Exercise
   Force Inject after entering each level; allow the normal camera-ready wait.
3. Confirm Halo 3 still plays normally and CE Original/Anniversary retain their
   accepted behavior, including switching graphics both ways.

The new lifecycle/recovery changes still require these headset results. The
accepted cumulative pointer remains `4e01f28`; this candidate does not claim
new acceptance of Reach HUD height or the previously deferred features.
Evidence and local verification: `docs/ALL-TITLE-REENTRY-2026-09-15.md`.
