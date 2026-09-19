# Independent shot pipeline investigation

Halo 3 reference behavior sought: each gun follows its owning controller while
native spread, ammunition, damage and appropriate target validation remain in
the engine. This investigation is incomplete; the old H3 experiment stays off.

The original d77c9dd log is preserved in
`C:/Users/Shadow/.codex/attachments/224dac8d-4d91-4c5e-bb0d-96aad71544b8/pasted-text.txt`.
Its H3 independent-ray success and refusal counts are both zero. Those counters
do not cover all earlier rejection gates and do not establish a firing crash.
The same log contains repeated H2 cleanup failures during H3 and H3 cleanup
failure on re-entry. Existing later lifecycle changes must be considered before
attributing re-entry failure to ray math. No such failure was reproduced here.

Official H3EK `A844A0` and pinned retail `3683A0` establish this sequence:

1. Authored marker results initialize the origin and direction for each barrel.
2. Retail `3524B0`, called at `368B92`, can replace direction with unit `+1AC`,
   project the origin, apply a barrel offset and collision-adjust the origin.
   The old independent-ray detour replaces direction after this helper.
3. Player shots subsequently call `13BAD0` at `368DFE` (return `368E03`).
   H3EK homolog `411F60` validates the targeting/position/direction arguments.
   It obtains the unit camera ray from `640180`, retail `212198`, then evaluates
   the existing target against that ray. It can overwrite the shot direction,
   including through the camera-ray collision result when no target is selected.
4. Native spread and projectile construction occur afterward. Thus a successful
   early-helper redirect alone does **not** prove independent final trajectories.

Retail `13BAD0` calls camera evaluator `212198` at `13BC08` (return `13BC0D`),
and target validator `13E32C` at `13BD0A`. `212198` is the existing camera
dispatcher; changing it globally would affect cameras. H3EK `640180` confirms
the perspective dispatch. Any future override requires a narrow firing scope.

H3EK `40F1E0` / retail `13E32C` validate an existing targeting record, rather
than performing unrestricted target acquisition. Target handle is `+4`, marker
`+8`, strengths `+C/+10`, offset `+14..1C`, flags `+20`. Flag `4` makes the
preceding assist function derive its query direction from the cached target.
Simply changing the camera ray would therefore still be insufficient in that
case. These are observed consumers, not authorization to clear native lock or
homing state. Native acquisition and ownership remain to be traced.

The old `BULLET-PROBE` logged camera and desired wrist positions while claiming
to measure projectile spawn and muzzle. Its diagnostic text is now corrected;
it does not sample either native shot origin or an authored muzzle. No gameplay
behavior changed by that correction.

Read-only decompiles: `out/dual-h3-fire-match.c`,
`out/dual-h3-firing-data-retail.c`, and `out/reload-policy/h3-dual-{downstream,
assist-kit,camera-kit,camera-retail,query-kit,target-validation}.c`.
No ZIP, installed-file change, launch or acceptance-pointer update.
