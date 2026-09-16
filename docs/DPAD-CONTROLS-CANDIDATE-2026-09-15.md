# D-pad controls and left-hand alignment candidate

This candidate adds two settings under **Controls**, beside the existing
D-pad hand selection:

- **D-pad head radius**: 10-50 cm. A smaller value requires the controller to
  be closer to the head. The default remains the existing 30 cm.
- **Quest 3 left thumb-rest D-pad**: off by default. Enable it, rest the left
  thumb on the left controller's thumb-rest area, and move the right stick to
  send D-pad directions. Lift the thumb to restore ordinary right-stick use.
  This always uses those physical hands, including in left-handed weapon mode.

Both settings save automatically with the other controls. Existing configs
receive the preserved defaults when the new settings are absent. Keep your
existing configuration.

The head gesture retains its movement-stick directions and Back/View click,
including CE/Halo 2 graphics switching. CE still requires the physical left
hand on the left side of the head. The optional thumb-rest gesture only
redirects right-stick directions: it does not create a new graphics-switch
click. It consumes that stick before turning and scope zoom can use it.
The F1 menu retains ordinary pointer/scroll controls.

The binding uses the standard Oculus Touch thumb-rest sensor. If the runtime
cannot bind the optional sensor, the complete original controller bindings
remain available. Inactive touch, tracking/focus loss and stale samples cannot
retain D-pad directions. Actual sensor delivery depends on the active runtime
and controller connection and needs the user's headset check.

Accepted source is `7ff9697685e78389dad16126e4d1ec7188a3acbe`; its CE haptics
and stereo/HUD/recovery paths are preserved. One build supports Steam and
Microsoft Store.

The existing **Left-handed main weapon > Fix Hand Alignment (Experimental)**
option also receives a correction. It remains opt-in. Hand placement follows
the title's authored grip relationship instead of assuming opposite wrists
have the same grip anchor. CE uses its own verified bilateral hand geometry
and each stock weapon's model axes. Gun transforms, right-handed mode, and
left-handed mode with the correction off remain unchanged. Existing finger
animations remain native; this does not add newly authored grip animations.
Unrecognized CE graph layouts retain the prior placement and report it in
the log rather than applying an unverified offset.
H2, H3, ODST, Reach and H4 use their own semantic palm markers. H2 Elite,
H3 Dervish and Reach Elite lack native right-palm markers, so their own
bilateral hand landmarks supply a documented mirrored counterpart. Unknown
H3/ODST/Reach rig identities retain the existing tracked presentation and
report that the optional correction was unavailable.

Local verification: Release build, all 33 automated test suites, the Reach
consistency check, eight title-specific marker comparisons, and all twelve
stock CE weapon graphs at three scales pass. These do not replace headset tests.

Verify the radius, thumb-rest directions and release, existing graphics-switch
click, CE vibration and Halo 3 controls. With left-handed mode and its alignment
option enabled, also check single/dual guns, grip/release and both CE/H2 graphics
modes. New alignment and thumb-rest sensor behavior still need headset testing.

Package only; nothing is installed, launched or published. New shared input
headset acceptance is pending. The accepted pointer remains 7ff9697.
