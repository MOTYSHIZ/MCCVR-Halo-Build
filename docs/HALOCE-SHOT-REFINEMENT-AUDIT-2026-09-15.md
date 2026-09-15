# CE controller-shot refinement audit - September 15, 2026

This audit supports retaining the existing controller-shot adapter. No shot
binding or behavior change is justified by the latest log.

## Runtime evidence

The preserved prior log is
`out/test-runs/be2140f-ce-partial-failed-20260915/HaloMCCVR-user.log`, source
`be2140fc440462c8ac1441186d28e62370ebe074`, Steam. At 11:53:06.256 it reports:

`CE aim gen=1 installed=1 observed=52 applied=52 stock=0 assist=52`

Those totals persist through its last shot report at 11:53:18.393. The user's
associated report, preserved in `HALOCE-BE2140F-TEST-2026-09-15.md`, confirms
Anniversary tracked hands and controller-directed bullets despite the separate
world-render failure. This is specific positive evidence for the shot path;
it does not accept that failed build or every weapon/renderer combination.

The latest source `a2b526a5d1a387a6b7cd4af1ed70856378b3cffc` log,
`out/test-runs/a2b526a-ce-refinement-feedback-20260915/ce-user.log`, reports
installed aim with `observed=0 applied=0 stock=0 assist=0` throughout. A source
comparison from `be2140f` to `a2b526a` shows no change in
`haloce_first_person.cpp` or `haloce_controls.cpp`; the shot binding group is
also unchanged.

`aimObserved` increments upon the exact native modern/legacy callsite while
the aim feature has current title/generation ownership, **before** checking
the local player, weapon, on-foot state, or camera/controller receipt. A
failed admission after that point increments `stock`. Consequently zero
observations do not establish that those admission guards are rejecting fire.
The supplied log does not record trigger firing input or another independent
shot count. Absence of firing is compatible with its zeros, but is not proved.
The log alone cannot distinguish no calls from an unobserved alternate native
fire path; neither justifies changing a previously reached binding.

## Native call graph and scope

E-CE-FP-2 in `HALOCE-FIRST-PERSON-EVIDENCE.md` records the official HCEEK
trigger `0x008EFD90` and its modern `0x008CD510` / legacy `0x008CD400`
adjustment helpers. Retained retail disassembly in
`out/ce-fp-trigger-disasm.txt` independently confirms:

- Trigger `0xB7A374` selects modern or legacy using native weapon flags.
- Modern call `0xB7A791 -> 0xB00880` returns at `0xB7A796`.
- Legacy call `0xB7A853 -> 0xB00740` returns at `0xB7A858`.
- The trigger then calls player assist `0xB67B00` at `0xB7A8B0` when its
  native player branch applies. `out/ce-fp-assist-disasm.txt` confirms the
  downstream director query `0xB67BDF -> 0xB14F14`, returning at `0xB67BE4`.

These are the exact guarded return addresses in the adapter, verified by the
generated `first_person_aim` signature, instruction, and relative-call
contracts. Its modern and legacy ABI argument order agrees with the native
instructions. The original helper executes with a private controller direction
and its optional unit-facing overwrite disabled; the result is copied back
into the trigger's direction buffer. The downstream player-assist query also
receives that controller direction so it cannot restore the old director aim.

The existing `tools/re/test_ce_shot_native.py` exercises 32 pinned native
modern/legacy cases: direction preservation/overwrite, native origin projection,
and inherited velocity. The preserved result is
`out/ce-fp-final-native-shot-20260915.json`. Its engine origin/collision/velocity
services are explicit stubs; this is ABI/math evidence, not a live trigger
test. No new tests were run for this source-only audit.

## Remaining player-facing limits

The adapter changes the on-foot local weapon's direction. The engine retains
the native fire origin, projection, collision clamp, authored offset, inherited
velocity, spread, and downstream targeting behavior. It does not promise a
projectile starts at the tracked controller/barrel position or that every
close-range muzzle/reticle combination is aligned. Seated, remote, stale,
unowned, and otherwise inadmissible shooters keep native behavior. The current
graphics/weapon/HUD refinements still need headset testing across weapons and
both renderers. No speculative shot rewrite was made.
