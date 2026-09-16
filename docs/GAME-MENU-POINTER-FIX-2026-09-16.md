# Native menu pointer correction and magazine visibility audit

The user reports 59f2a82 is otherwise tolerable and asks to preserve/package it
first, then correct missing native-menu pointing. They explicitly confirm
F1 > Controls > Point at game menus was enabled. The follow-up asks whether
magazines appeared before Manual Reload was enabled, and to fix that if proved.
No other standing work is advanced. Both editions remain supported.

## Evidence

The supplied log is preserved at
`out/test-runs/59f2a82-native-pointer-report-20260916/user.log`, SHA-256
`50EA419D2AFD0672085B99274F50C4E21178BA944B0F68FB3660A17AB985A326`.
It names source 59f2a825f5c8656079bc14e1fe2daa4a84a516e3, Steam,
SteamVR/OpenXR 2.17.10, Oculus-family headset, 90 Hz.

At 16:01:35.617 all six title modules are resident in the shell. The next line
changes runtime mode from shell to unsupported. `FallbackRuntimeMode` in
title_adapter.cpp deliberately reports Unsupported for this ambiguous module
set. That classification concerns camera ownership, not presence of MCC's menu.
The 59f2a82 `game_menu_pointer::MenuMode` rejected Unsupported in both the
compositor and window-thread delivery. Thus an enabled pointer could never
reach mouse delivery in the reported shell interval. F1 pointer hits and its
primary trigger are present later in the same log. No game ownership change is
needed or made to correct this optional feature.

## Correction

Halo 3 reference behavior is the shared F1 primary-hand pointer, 0.35 UV
smoothing and 0.65/0.35 trigger hysteresis. Preserve that F1 implementation.
Native menus additionally admit Unsupported, only with an actual stock-screen
quad. Loading, gameplay, vehicles, death and cinematic modes still reject
unless the existing explicit pause presentation permits the screen.

The native pointer now supplies its own white ring in OpenXR, centered on the
last successfully delivered physical cursor coordinates. A click contracts the
ring. Its small texture uploads once during session initialization; no pointer
texture allocation, COM work or Windows input is added to per-eye/palette hooks.
Optional texture initialization failure leaves native input and core VR intact.
Input publication still requires successful frame submission. Window focus,
coverage, freshness, trigger release/rearming and physical mouse drag checks
remain in place. The setting remains default-off and existing config is kept.

Worker diagnostics report enabled state, primary hand, runtime mode, ray gate,
delivery state and move/click counts. A ring means successful OS delivery; it
does not prove MCC accepted the hovered widget or that the headset displayed it.
No engine hooks, offsets, weapon behavior, camera ownership or F1 layout change.
The failed optional behavior was disabled in its own commit before correction;
the legacy ray implementation remains dormant.

## Magazine audit

The first gesture record, 16:03:27.110, says reload=1, holsters=0. The first
accessory draw success is at 16:03:37.936, after that enabled record. There is
no logged disabled draw. This cannot confirm the user's suspected symptom.
`weapon_accessory::Build` rejects settings.reload=false before returning any
presentation. The compositor independently passes g_config.manual_reload to
`weapon_accessory::Current` before preparing either eye's draw. Command lists
are local to that frame. Default Manual Reload is false. Holsters and generic
visual preferences cannot bypass these guards. Native weapon animation/parts
are separate from the optional pouch/hand accessory.

Regression coverage now checks disabled reload for all catalogue models, both
hands, holsters on/off, retained held-magazine state and unfamiliar generic
models. Existing coverage rejects cached presentations after disable. No
unproven magazine behavior change is made.

## Verification and handoff

The user-requested unchanged package was created before source edits; its exact
build/source identities are in `out/pointer-preserved-baseline-handoff.json`.
The correction must pass Release, all CTest suites and the Reach consistency
gate, then be committed and packaged without -Install. Verify both archives
against the manifest and exact Git blobs. Final identity and verification
results belong in `out/game-menu-pointer-current-handoff.json`.

Release, all 39 CTest suites and the Reach consistency gate pass locally.
Focused tests pass 158 pointer checks and 4,945 accessory checks.
Local tests exercise shipping ray/cursor math and Windows transport with fake
OS endpoints, including the reported Unsupported mode. They do not launch MCC,
inject real desktop input or prove native widget/headset behavior. Test enabled
pointing in MCC shell/settings and target-title pause menus, click/drag, closing
F1, focus changes and resuming play; repeat Halo 3 and both editions. Check
Manual Reload off at startup, holsters-only, and disable after holding a part.
Headset and Halo 3 regression results remain pending; CURRENT-STATE.md stays
unchanged. Deliver build/source ZIPs, then wait. No installation, game launch,
game-folder writes, GitHub publication or PR was requested for this handoff.
