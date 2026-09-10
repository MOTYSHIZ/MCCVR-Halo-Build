# Anatomical left-hand implementation checkpoint

Local implementation is complete for H2 Classic/Anniversary, H3, ODST, Reach
and H4. This is not headset acceptance; CURRENT-STATE.md remains unchanged.
The Halo 3 behavior being preserved is controller-owned primary/support aim,
grip, contact and haptics, with the visible gun seated at the primary wrist.
Optional left-handed mode now also routes the actual native left hand to that
primary grip and the actual right hand to support. Physical stick/button
preferences remain independent.

The solve transfers each anatomical wrist's controller-relative orientation
to its destination grip using proper rotations and positive scale. It keeps
the native finger subtree and animation. Gun transforms are retained separately;
swapping the anatomy must not swap the gun onto the support hand. Invalid
remaps/transforms refuse this presentation feature locally.

- H2: both renderer packet consumers carry a frozen handedness flag. Single,
  support-gripped and dual packets stage the anatomy before committing. Native
  Chief/Elite hand remaps identify the actual subtrees, including partial dual
  remaps. Collision samples follow controller roles after the anatomical swap.
- H4: the stereo pair freezes handedness. Its separately evidenced Storm hand
  masks follow routed wrist targets; the held model retains the saved primary
  weapon delta. Hand volumes and the weapon root use the primary anatomical
  left wrist when enabled.
- Reach: its existing prepared-target and final palette path routes the native
  hand masks while excluding appended held-object nodes. It retains native
  anatomical arm-collapse semantics and swaps only the contact role context.
- H3/ODST: each native render-model remap separates body nodes from appended
  weapon nodes. Both anatomical wrist subtrees move while the gun remains in
  place. Visible arms use their actual shoulder/arm chain and the existing IK
  solver. Each title's existing tag resolver and proven node-block layout are
  used independently. Native faults are isolated by a guarded reader.

Pure transform arithmetic is shared in anatomical_palette_logic.h; it imports
no title-specific engine layout. Pair tracking is frozen before both eyes.
Refusal counters are reported by the worker, not logged from palette hooks.

Validation: cumulative Release build, all three CTest targets, Reach parity
check, and git diff --check passed. Build/test logs are
out/continuation-20260910-left-hand-final-build.txt and
out/continuation-20260910-left-hand-tests.txt. Tests cover Chief/Elite single
and dual remaps, unchanged guns, finger preservation, role scales, two eye
roots, appended weapon records, and atomic refusal for invalid/overlapping
maps or nonfinite final fingers.

Headset work still required: primary/support seating and finger pose, visible
arm calibration, native dual-hand region visibility, both H2 renderers, target
titles and Halo 3 regression. Custom hand skeletons lacking the required native
wrist/remap evidence fall back locally. No universal custom-hand-mesh claim is
made. Independent dual bullet trajectory remains a separate pending feature.
No package, installation or game launch was performed.
