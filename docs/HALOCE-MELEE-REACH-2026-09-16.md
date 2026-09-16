# CE physical melee reach refinement

Halo 3's reference is a speed-qualified physical hand or gun swing against the
contacted target, with independent hands, native collision/damage, and one hit
until the hand retracts. CE retains those controls and guards. This refinement
adds a bounded CE reach allowance for the reported difficulty hitting Grunts
and Jackals. It is a deliberate VR usability policy, not a diagnosed native
damage failure or a claim of headset acceptance.

## Evidence and difference from stock CE

The preserved source-d47a98c community report 13 ends with 519 qualifying melee
queries, one native contact and one right-hand application. Report 14 ends with
3,263 queries, 57 contacts and twenty right-hand applications. Neither reports
contact exceptions, dropped publications or a melee feature fault. These logs
establish successful native submission, but contain no target species, hand
trajectories or rejected near-miss distances. They cannot identify which
reporter attempted each melee action or prove the right allowance length.

Official HCEEK `halo_tag_test.exe` function `0x8D4A90` builds a 5 by 5 ray fan
from the native melee marker: forward displacement 0.8 native units, with
perpendicular displacements -0.2, -0.1, 0, 0.1 and 0.2 units on each axis. It
uses the same native collision flags `0x1000E9` as the physical adapter. The
primary capture is `out/ce-contact-kit-melee.txt`; assertions identify
`units/units.c`. Retail homologue `halo1+0xB0BFAC` retains that fan, verified
in `out/ce-contact-retail-rayfan.txt`. The existing melee contract already
checks the native player's call to this helper. No new runtime binding is
introduced.

`tools/re/test_ce_contact_native.py` now executes the unchanged retail search
instructions in addition to its resolver and damage-selection checks. A clear
world and a centre-ray biped contact both produce exactly those 25 vectors;
the selected salted target and material are preserved. The isolated fixture
declares perpendicular-vector, marker and collision services. It does not
execute a real collision world. Both primary binaries retain the hashes pinned
in `HALOCE-EVIDENCE-MANIFEST.json`.

Physical melee previously used only the actual short interval between two
tracked hand/weapon samples. A near miss could not benefit from CE's broad
button-melee search. The native fan is evidence of that behavioral difference;
its constants are not copied into the new physical reach policy.

## Implemented policy and retained guards

Each already-qualified physical sweep may extend 20 real centimetres along
its own motion direction. This small allowance applies to either physical hand
and the held gun, in both graphics modes. The scale comes from the same CE
tracking-to-world transform as the visible geometry. Downward and diagonal
swings retain their actual direction. There is no head-directed ray or target
search, new native lunge, speed-threshold reduction, weapon animation or change
to another title. World contact and the visible hand/gun pose are unchanged.

Both native queries use the identical original-start-to-extended-end segment.
A wall or non-biped first obstruction prevents damage; it is never skipped to
find an enemy behind it. The second query still requires the exact salted
target, native material and finite position/normal/fraction. The private damage
scope retains owner attribution and the exact native caller; native authored
damage and responses remain native-owned. Impact position comes from the
actual collision result. Swing speed, the 60 ms/25 mm retraction latch,
duplicate-eye suppression, tracking/reference/shape resets, feature toggles
and native exception isolation remain unchanged.

The cold log names the 20 cm policy on install and reports per-hand successful
applications that used the extra reach. These are subsets of the existing
total applications, not extra damage events. No logging, allocation, file I/O,
signature scan or additional native query enters a palette or contact hot path.

## Local validation and remaining headset proof

The two Release C++ contact suites pass. Math checks cover three swing
directions at five world scales and refusal of invalid or zero-length inputs.
The production native-call fixture reproduces the prior miss 15 cm beyond a
tracked fist endpoint, then exercises both hands at three world scales. It
checks the bounded hit, unchanged subthreshold rejection, wall/non-biped
obstruction, a target beyond the allowance, newly occluded damage dispatch,
salt replacement, contact coordinates, per-hand diagnostics and existing
latches, queue lifetime, toggles and native exception isolation.

The expanded isolated native check passes three resolver, ten authored-damage
selection and two native fan cases (5,549 instructions). Its exact report is
`out/ce-melee-reach-native-20260916.json`. These checks do not prove real Grunt
or Jackal geometry, damage authority, balance, comfort or game performance.
The 20 cm tuning value requires the user's headset result in both CE modes,
with both hands and short/long guns, including blocked contact near a wall.
The accepted pointer remains d47a98c. Other standing CE contact limits in
`HALOCE-NATIVE-CONTACT-EVIDENCE-2026-09-15.md` remain open.
