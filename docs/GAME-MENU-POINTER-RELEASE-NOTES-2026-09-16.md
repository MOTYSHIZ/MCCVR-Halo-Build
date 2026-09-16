# Game menu pointer correction

Continues the 59f2a82 ZIP and keeps its reload accessories, modded-weapon
fallback, holsters and earlier fixes. Supports Steam and Microsoft Store.

Corrects the menu check that blocked pointing when MCC preloaded all six games.
Adds a visible white VR cursor ring and diagnostic logs for mouse delivery.

Close MCC and the launcher, then replace the DLL and launcher in your existing
Halo_MCC_VR folder. **Keep your existing halomccvr.cfg.** For a fresh setup use
the supplied config and MANUAL-README.txt. Nothing was installed automatically.

1. Open F1, go to **Controls**, and enable **Point at game menus** (default off).
2. Close F1. Aim the same controller used for the F1 pointer at an MCC main,
   settings or pause menu. Pull its trigger to click; hold it to drag a slider.
3. Keep MCC's desktop window focused. Release the trigger before the first
   click after entering the panel. Left-handed mode uses the primary left hand.

A white ring follows the cursor and contracts when clicked. Existing buttons/sticks still navigate;
scrolling remains on the existing controls. Disabling this option restores the
previous menu behavior. F1, tracking/focus loss and leaving the menu release the
mouse click; a consumed trigger must be released before firing in gameplay.

Please test main/settings/pause menus, sliders, opening/closing F1, returning to
gameplay, and both handedness modes. Include Halo 3 as a regression check and
your current title. Send HaloMCCVR.log with the edition, runtime and headset if
anything is wrong. Local tests do not substitute for headset confirmation.

Earlier reload/holster features and their limits are documented in the included
RELOAD-ACCESSORIES-NOTES.md. No other deferred work is claimed completed.

Magazine visibility was audited: the supplied log records Manual Reload on
before the first accessory draw, and all 4,945 accessory checks pass, including
reload disabled with holsters on/off and while holding a part. No off-state
magazine defect was reproduced. To check in the headset, turn Manual Reload off
and test holsters alone, then enable reload, grab a part, and disable it again.
