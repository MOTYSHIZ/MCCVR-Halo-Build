# Optional controller mouse for native MCC menus

Historical 59f2a82 implementation. The user reports native pointing failed;
read `GAME-MENU-POINTER-FIX-2026-09-16.md` for the log-proven mode rejection,
correction, visible cursor and magazine visibility audit.

User request: continue from the latest packaged b4ffa80, preserve its complete
reload/accessory/holster work, and extend F1-style mouse pointing to the actual
game menus with a toggle. No other standing/deferred work is advanced here.

Reference behavior: Halo 3's existing shared F1 menu intersects the primary
controller ray with its displayed quad, smooths UV by 0.35 and uses trigger
hysteresis 0.65/0.35. Handedness already maps the logical primary pose and
trigger together. The existing F1 implementation remains unchanged.

## Source evidence and implementation

- `vr.cpp::MakeQuad` supplies the actual VIEW/LOCAL pose and aspect ratio of
  the native stock-screen quad. Only that quad in a successfully submitted
  frame can publish native menu input. Immersive projection, cinematic theater,
  failed frames, F1, loading/gameplay modes and unfocused XR do not admit it.
- The ray is transformed through the submitted quad's actual pose (and current
  head pose for VIEW space), then uses existing `IntersectMenuQuad`. Native
  menu input requires front-face intersection and fresh controller tracking.
- `d3d11_hook.cpp` already establishes the physical desktop client domain for
  DLL callers, with MCC-executable-only fitted-window cursor remapping. The
  optional pointer moves the real cursor in physical client coordinates once;
  no new engine signature, structure, address or hook is introduced.
- `native_menu_pointer.cpp` coalesces atomic samples into a registered window
  message. MCC's window thread performs cursor placement and Windows mouse
  button delivery. No Windows input, locks, allocation or logging is added to
  render/palette hot hooks. Failed message delivery is logged by the existing
  diagnostics worker; window/input failures log on the window thread.
- Window focus, window coverage, client size, finite UV, input freshness and
  current menu mode are rechecked at delivery. A 50 ms window-thread watchdog
  releases a held mouse button after stale input (>200 ms), F1, disabling,
  focus loss, or leaving menus. A release failure retains ownership and retries.
  A physical mouse drag remains native. Reentering with a held trigger requires
  release before another click. No native ammo/aim/controller layout is changed.
- Only the outgoing VR primary trigger is consumed while pointing, and an
  already consumed hold drains through release to prevent firing on resume.
  Original tracking/trigger samples and physical gamepad controls are retained.

`game_menu_pointer = 0` is the default for existing and fresh configurations.
F1 > Controls > Point at game menus saves this independently. All earlier
defaults, bindings, CE vehicle/crosshair/beam fixes, H3 Cortana facing, reload
art and interactions, and both MCC editions remain preserved.

## Verification and limits

The dedicated fixture executes the shipping Windows transport with fake API
endpoints: no real desktop input, game launch, installation or process access.
It covers coalescing, physical coordinate mapping, held-trigger admission,
drag/release, focus/F1/gameplay/staleness cancellation, physical mouse ownership,
invalid UV/client size, wraparound timestamps, delivery failures and retries.
The production OpenXR/pad fixture checks menu-trigger consumption and release
before resumed gameplay. Core tests cover default-off and config persistence.

The cumulative Release build, all 39 CTest suites and Reach consistency gate
pass locally. Exact final checks and ZIP hashes are recorded in
`out/game-menu-pointer-current-handoff.json`. This is a candidate, not headset
acceptance. Test shell/settings and in-game pause menus on the target title,
plus Halo 3 regression, both handedness modes, normal/fitted desktop sizes,
focus changes and returning to gameplay. Native MCC hover/click behavior still
needs real headset confirmation. This adds pointing/click-drag, not a new laser
renderer or remapped scroll stick; ordinary gamepad navigation remains available.
The desktop MCC window must be foreground. The existing game's cursor supplies
the visible pointer. Accepted cumulative source remains d47a98c.
