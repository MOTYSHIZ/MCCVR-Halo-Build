# CE controller haptics candidate

This candidate restores the existing controller vibration path for Halo CE in
both Original and Anniversary graphics. The accepted runtime is source
`5ac02f53a7896ffd6b8dff37ddc5bc4700890559`. The only runtime change is granting
CE the haptics capability in its title descriptor and armed runtime publication.

Native game vibration uses the same XInput-to-OpenXR bridge as Halo 3. Both
controllers receive the game's blended low/high motor feedback. Existing
short-pulse retention, controller-vibration intensity, left-handed mapping,
contact feedback and stop conditions are preserved. CE haptics remain unavailable
until its camera is armed; vibration is not a requirement for VR admission.

All other accepted CE and other-title behavior is preserved, including stereo,
graphics switching, controller aim, HUD, contact/melee, title re-entry and Force
Inject. No native hook, offset, configuration default or shared haptic function
is changed. One build supports Steam and Microsoft Store.

Verification includes the real CE runtime publishing/withdrawing haptics in
both renderer modes and a fixture that executes the shipping XInput, capability,
amplitude and OpenXR output functions with simulated endpoints. It checks both
controllers, low/high motors, brief gunfire, held/zero feedback, intensity,
contact merging and pause/menu/tracking/focus/title-ownership stops against the
unchanged Halo 3 reference. Offline verification cannot establish the felt
strength or timing of native gameplay events; headset acceptance is pending.
Release, all 29 CTest suites and the Reach consistency gate pass; the production
haptic fixture completes 339 checks with no failures.

Test CE guns and gameplay vibration in each graphics mode on both controllers,
including stopping on pause and resuming gameplay, then check Halo 3 vibration.
Keep the existing configuration; its controller-vibration intensity still
applies. Zero intensity intentionally disables vibration.

This is a package-only candidate. No MCC installation, game-folder replacement,
launch or GitHub publication is performed. The accepted pointer remains 5ac02f5
until the user tests this candidate. Existing known limitations remain in the
accepted baseline's release records.
