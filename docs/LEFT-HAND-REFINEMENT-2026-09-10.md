# Released left-hand positioning restored; correction is opt-in

Latest user release checked live via GitHub API: MCCVR-d77c9dd at
https://github.com/moistman42069/MCCVR-Halo-Build/releases/tag/MCCVR-d77c9dd .
Downloaded source asset SHA256:
57A62EFE9078586F5F102F5DB0ED77735CA29D1BE05C75AE216186594313CA67.
The release game.cpp, vr.cpp, halo2_observer_6dof.cpp, halo2_render_logic.h and
halo4_render_logic.h are byte-identical to local d77c9dd. This is the hand
presentation reference only: its rejected H3 dual-fire behavior stays disabled.

Compared the exact source delta. Controller role swapping/aim/trigger/grip is
already in that release. The later anatomical palette/wrist routing caused the
reported misalignment. It is now disabled by default, retained behind
experimental_hand_alignment = 0. F1 offers Fix Hand Alignment (Experimental)
under Left-handed main weapon only while left-handed mode is enabled.
The correction flag is distinct from actual controller handedness and frozen
in prepared H2/Reach/H4 frames and H3/ODST's tracking snapshot. When off, all
titles use the released role-based hand placement without the later anatomical
swap. Collision-volume role selection uses that same presentation choice.
Runtime equipped-model bounds, contact/melee algorithms and role-routed
tracking/haptics are preserved. No speculative mesh offsets were added.

The experimental correction remains unproven and can still misalign hands;
turning it off restores the released presentation path. A new correction is
not claimed without title-specific evidence/headset results.

Release build, three CTest suites, Reach consistency and diff check pass.
Config tests cover legacy/default-off behavior, left-handed saved settings and
explicit opt-in surviving save/load alongside collision and melee settings.
Existing H2 single/dual/Chief/Elite and all-title transform regressions pass.
Headset validation remains pending; CURRENT-STATE.md is unchanged.
