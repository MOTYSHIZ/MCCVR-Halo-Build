# CE / Halo 2 / Halo 4 first-person vehicle candidate

The Halo 3 behavior being matched is a local occupied-seat viewpoint with live
head tracking, the existing `vehicle_first_person` switch, and stock chase view
when disabled. The existing Halo 3, ODST and Reach implementations are unchanged.
This is an unaccepted candidate; local checks do not establish headset parity.

## Native evidence

The official editing kits establish the camera decision and evaluator semantics.
Each is matched independently to the pinned retail DLL, not another title's
offsets. `NATIVE-VEHICLE-FIRST-PERSON-CONTRACTS.json` records module hashes and
unique runtime signatures. RVAs below exclude the respective image base.

| Title | Official kit selector / first-person evaluator | Retail selector / evaluator | Head query |
| --- | --- | --- | --- |
| CE | `101040` / `103e50` | `b14f70` / `c523c4` | `b37178(unit,"head",record,1)` |
| Halo 2 | `8a270` / `1ca940` | `6d4c50` / `77dc50` | `8d6570(unit,0x4000095,record,1)` |
| Halo 4 | `219510` / `3444e0` | `10d268` / `125190` | `5d5b74(unit,0x122,record,1,0,0,1)` |

Selectors write mode 2 for a normal occupied seat, modes 1/3 for authored
entry/exit transitions, and 4 for forced special cameras. Their return selects
chase versus first person. CE/H2 mode storage is 16-bit; H4 is 32-bit. Only
normal mode 2 for the verified local occupant is overridden. H4 additionally
rejects its unit+664 special-camera state before overriding a decision.

CE seat flags are in unit-definition block +2e4, stride 11c. H2's kit block
is +264/stride c0, but **retail is +1c8, pointer +1cc, stride b0**. H4's
block is +510, pointer +514, stride 16c. This implementation writes none of
these tags: it changes the local camera selector result instead. No shared
seat-flag lease survives a checkpoint or affects another occupant.

Forcing the selector alone is insufficient. CE native camera-position function
`b04d00` and H2 `8f9190` still use vehicle camera markers while parented. H4
`5f8a1c` also retains its third-person seat predicate. The first-person evaluators
can subsequently substitute a turret's primary-trigger marker. The new detour
runs the full native evaluator and then replaces only its result position +4
with the occupant's own head point. Facing, flags, effects and other fields
remain native. Existing tracked-camera consumers then compose the HMD pose.

Marker translation +96, forward +60 and up +84 are verified through each
title's native camera consumers. The universal forward/up/right sliders apply
in that marker frame using the existing world scale; right is negative native
left. This is deliberately a native animated head anchor, not Halo 3's authored
and calibrated seat-point implementation. Per-seat preset banks remain H3,
ODST and Reach only. No seat IDs or authored points are invented for new titles.

Head query returns must report exactly one marker. Each native helper fills a
fallback transform even on failure; using that without checking its zero count
would put the camera at the body origin. Nonfinite/nonorthogonal marker frames
are rejected. CE's literal `head` is also present at HCEEK VA `b1beb8`.
H2 wrapper `8d6570` supplies two zero flags to `8d11a0`. H4 supplies the same
three flags as its own camera-position head query, including interpolation.

H2/H4 already detour those marker helpers for muzzle placement. The vehicle
feature neither claims nor replaces those hooks. Its cold marker proofs use
the unchanged interior call sites at `8d657e` / `5d5bb5`, respectively, so
existing muzzle hooks do not make the new optional feature fail its signature
check. Head requests are outside those hooks' muzzle-owner/caller scopes.

## Ownership, fallback and regression boundaries

CE uses its verified local player mapping, native controlled unit, biped/vehicle
lookups, seat, input/cinematic guards and generation. H2 uses its armed observer,
salted local-player/object resolver and verified bounded vehicle-parent reader.
H4 uses its independently verified input/output mapping and salted TLS object
table, rejects special-camera state, and requires player-controlled mode.
All paths additionally require gameplay and active tracked stereo presentation.
Owner identity, parent, seat and generation are checked again after marker read.
There is no cross-frame cached point to leak into a new vehicle or checkpoint.

CE's existing vehicle input and reticle guards now admit both first-person and
following perspectives, still requiring the actual seated local biped/vehicle.
They reject a perspective change during a single packet/read transaction.
On-foot admission and native steering/physics/seat restrictions are unchanged.

The two hooks must both install before admission. Missing/ambiguous signatures,
failed hook creation, missing ownership/markers and guarded access faults keep
this feature native. Faults disable only this feature and are reported by the
worker, with selector, position and missing-head counters. Callback accounting,
module retention and quiescent retirement protect title changes. No logging,
allocation, locks or signature scanning occurs in these detours.

## Verification and limits

Production-detour fixtures exercise all three ABIs, exact position-only writes,
universal trim axes, remote rejection, toggle-off, transition modes, missing and
nonfinite markers, parent/seat/generation changes, exceptions and callback drain.
CE production input fixtures cover steering and reticle ownership under the new
first-person perspective as well as the existing rejection cases.

Release build, all 59 CTest suites (including 516 new production-detour checks),
24 stock/muzzle-hook signature checks and the Reach consistency gate pass.

Headset tests remain required in CE Classic/Anniversary, H2 Classic/Anniversary
and H4, with Halo 3 regression testing. Check driver/passenger/gunner seats,
entry/exit, toggle off/on, checkpoint reload, co-op and title changes. Native
body/weapon visibility, cockpit clipping, animated-anchor comfort and each
vehicle's usable sightline have not been visually verified. This candidate does
not port the existing titles' separate authored-anchor/body-hide/personal-weapon
overrides into the new titles or claim those refinements complete.

Local RE records: `out/vehicle-fp-followup/ce-director-kit.txt`,
`ce-fp-live.txt`, `ce-position2.txt`, `ce-fp-marker.txt`, `h2-mode-kit.txt`,
`h2-selector2.txt`, `h2-fp-eval.txt`, `h2-fp-marker.txt`, `h4-director-kit.txt`,
`h4-selector2.txt`, `h4-fp-retail.txt` and `h4-selector-retail.txt`.
