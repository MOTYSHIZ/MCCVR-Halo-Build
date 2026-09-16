# CE haptics: accepted baseline and missing capability, September 15, 2026

The user explicitly accepts source `5ac02f53a7896ffd6b8dff37ddc5bc4700890559`
as the baseline to preserve and reports no weapon/game vibration in either CE
Original or Anniversary. The latest instruction cancels the GitHub/publication
task. Scope is CE vibration through both controllers, followed by a new verified
build/source ZIP pair. No other behavior is requested. Package without
`-Install`; do not launch MCC, touch the installed game folder, or publish.

Halo 3's behavior to match is its existing game-authored motor vibration,
blended into the shared OpenXR haptic amplitude and delivered to both
controllers, including short pulses and sustained rumble. The shared intensity
setting, contact pulses, handedness routing, pause/focus/tracking suppression,
and title-transition clearing must retain their existing behavior.

## Preserved headset evidence

The supplied log is preserved byte-for-byte at
`out/test-runs/5ac02f5-ce-haptics-20260915/user.log`, SHA-256
`A380428D0A26F3BB29C2A723BEFAAFBC7B2695E3AC3EEB5AD24EE16DE7FBB0DB`.
Its source is `5ac02f53a7896ffd6b8dff37ddc5bc4700890559`, PID 32688,
Steam, SteamVR/OpenXR 2.17.9, Oculus-family headset, panel 90 Hz. The exact
headset model and saved vibration intensity are not named in this log.
The accepted artifact DLL SHA-256 is
`55646EFF6AEF8FAD0C16E9FDA67685292637B97E0B6437B7DD670C4E4B840A63`
according to its preserved candidate identity. A DLL hash is not present in the
runtime log, and no installed DLL was inspected or changed for this task.

Relevant new-log facts:

- Lines 23-26 record `XInputGetCapabilities`/`XInputSetState` detours in
  `xinput1_3.dll` and the claimed host `XInputSetState` import-table slot.
  Lines 118/120 record capabilities/SetState detours in `xinput1_4.dll`.
- CE is detected at line 231. It enters Original/Anniversary stereo at line
  267, its native controls verify at line 270, and its independent contact
  adapter installs at line 307.
- CE remains armed and produces eye pairs; line 1040 records 1,251 pairs,
  and line 1043 records a successful two-eye frame. This is not a missing
  camera-ownership result.
- At line 1063 CE contact has published 1,504 samples, but reports zero
  contacts and zero applied melee hits. That interval does not independently
  demonstrate a contact-haptic event.
- The log records no native motor amplitudes, forwarded game-haptic requests,
  or OpenXR vibration return codes. Hook-install messages alone cannot prove
  which individual gun events reached the mod or what the headset felt.

The required comparison includes the actual preserved deployment backup
`out/deploy-backups/ff1b0e4-steam-before-ad7fbf5-20260905-115650860Z/HaloMCCVR.log`,
SHA-256 `8481F6288894B3718F771BDD05F908A20BD5429A9777325772771C1A48F4E5F9`.
It identifies source `1c088372ac5e8dfca2648f9a1147d8478b9dbbe6`, Steam,
SteamVR/OpenXR 2.17.8, Oculus-family, 120 Hz. Its lines 21/24/26 and 120/121
record the same capabilities/SetState detours and host SetState import claim;
line 161 selects Halo 3 and line 240 installs its contact adapter. There is no
new absence of the shared input-hook setup in the CE report.

Also compared:
`out/test-runs/4e01f28-accepted-feedback/user.log`, SHA-256
`BD269C3800377EEDC2369BEA174D3ACA4A0000AEC655B12E8C03176DA2977DB1`.
This is source `4e01f28b3ec5f5f8f533ac66d94978509cbcea54`, Steam,
SteamVR/OpenXR 2.17.8, Oculus-family, 120 Hz. It covers the previously accepted
other-title paths: Halo 3 is selected at line 1812; Halo 4's shared-systems
telemetry changes from haptics denied while loading at line 2883 to haptics
GRANTED in Gameplay at line 2937. Neither prior log contains individual
gun-motor amplitudes. Their headset acceptance belongs to the recorded user
reports, not an inference from hook installation.

## Proven defect in accepted source

At `5ac02f5`, both CE capability declarations omit
`TitleCapability_Haptics`:

1. CE's descriptor in `src/common/title_registry.cpp` declares Stereo,
   RoomScale, ControllerInput, and RuntimeModes.
2. CE's armed lifecycle publication in
   `src/dll/haloce_stereo_core.cpp::HaloCE_Poll` publishes those same four
   capabilities. Granting only the descriptor would leave the live lifecycle
   gate closed; granting only the lifecycle would leave the declared title
   capability inconsistent. The descriptor is not a second live haptic gate:
   `Game_HasTitleCapability` uses resolved runtime ownership and its arm mask.

The existing `src/dll/input.cpp::ProcessSetState` sees the denied capability
and calls `VR_SetGameHaptics(0.0f)` instead of forwarding the two native motor
values. Independently, `src/dll/vr.cpp::ApplyControllerHaptics` clears game
request/peak and both contact peaks whenever that same capability is denied.
This is sufficient to prevent all CE OpenXR vibration regardless of graphics
mode or native amplitude. It does not depend on a speculative engine binding.

The minimum correction is to add only Haptics to the CE descriptor and its
already-armed lifecycle publication. Pre-arm/admission stays controller-input
only. `Game_AutoVrTick` already publishes CE's native Gameplay/Vehicle/Paused/
Cutscene/Dead/Unsupported modes; no new lifecycle or rendering policy is needed.

There is no missing virtual motor advertisement:
`ProcessGetCaps` already advertises both vibration motors as `0xFFFF` when it
fabricates slot zero, independently of VR startup and title transitions.
The existing application path merges game rumble into each of the two hand
amplitudes, merges each hand's contact peak, applies configured intensity, and
routes both physical OpenXR subaction paths through the existing handedness
mapping. It retains peak-hold for gun pulses between VR samples. No additional
trigger-generated pulse or camera/renderer change is indicated by this defect.

## Native CE vibration evidence and limits

Read-only inspection uses the already pinned official HCEEK
`out/deps/re-tools/inputs/halo_tag_test.exe`, SHA-256
`FC9E2B6193C6F6D9FF988278B0D39A983747F3FDBDECA7CAFADD28F1F0C53E73`,
and retail `out/deps/re-tools/inputs/halo1.dll`, SHA-256
`0A12DC561780F449D3F4D0DF10BB8D3BC7BE7840A5BEB2B236F672EB6CD42E6C`.
These are the identities in `HALOCE-EVIDENCE-MANIFEST.json`. The addresses
below document cold inspection only; none is a new runtime hook or binding.
Kit addresses are VAs with image base `0x400000`; retail addresses are RVAs.

Official HCEEK contains `player_vibrate.c`, authored `low frequency vibrate`
and `high frequency vibrate` tag fields, and the
`player_effect_set_max_vibrate` script name. Its player vibration allocator
at VA `0x594BF0` allocates `0x82C` bytes. VA `0x594C90` accepts an authored
vibration definition, copies its `0x3C` bytes into one of eight active
envelope slots in a `0x208`-byte per-player record, and scales the two bands.
That establishes that the CE engine has authored game vibration.

The kit's engine output setter at VA `0x5BDB00` is a `ret` stub. Do not cite
the kit as proof of actual controller output. Its separate native XInput
wrapper at VA `0x5C68D0` has a real two-WORD SetState path, but the presence
of that wrapper does not establish that the stubbed producer invokes it.

Retail retains the matching player vibration implementation: allocator
`0xB9B390` allocates `0x82C`; `0xB9B518` copies/scales a `0x3C` authored
definition into eight slots of `0x208` per player. `0xB9B400` evaluates
each local player's vibration through `0xB9B69C`, maps that player's
controller, and at `0xB9B4B3` calls `0x6A520` with the two motor WORDs.
This producer is independent of the two rendering backends.

The MCC output bridge `0x6A520` reads the host service at `0x2E3B9A0`.
Subject to native pause/controller-state checks, it packs the low/high motor
WORDs into an output record, preserves the controller index, and calls the
host virtual method at offset `0x138` at `0x6A58C`. This is a real retail
game-vibration output route; the kit's stub is not retail's behavior.

A separate retail path dynamically resolves `XInputSetState` at `0x8E6C6`
and `0xC0E7C9`, both storing the result at `0x2EAB970`. The native wrapper
`0xC0E71C` invokes it and is called by controller-loop `0xAD950C`. The
resolver names `xinput9_1_0.dll`. The supplied log does not record that DLL
being hooked, so this standalone input path must not be asserted to be the
active MCC-host route. The mod already supports that DLL if it is loaded.

The cold trace establishes native authored vibration and its MCC host callback.
It does not observe the live host callback's resolved target, prove that every
tested gun generated a nonzero event, or measure actual OpenXR output. The
two missing mod capabilities are proven blockers; runtime restoration remains
for the user's headset test. No engine field, native pause state, motor
preference, or title file is written to bypass a native gate.

Reproducible cold inspection scripts and their same-named text output are
preserved under `out/ce-haptics-native-*-20260915.py` / `.txt`; the useful
final route is `out/ce-haptics-native-host-route-20260915.txt`, with initial
identity/string checks in `out/ce-haptics-native-survey-20260915.txt`.

## Candidate validation

Before the correction, the descriptor regression failed once and the real CE
worker failed to publish haptics in both renderer modes; all pre-existing CE
runtime assertions passed. The production haptic fixture passed its Halo 3
reference and failed 24 CE capability/output assertions. Logs are preserved as
`out/ce-haptics-baseline-tests.log` and
`out/ce-haptics-pipeline-baseline-corrected-tests.log`.

After the two grants, Release builds, all **29 CTest suites**, and the Reach
consistency gate pass. The production haptic fixture completes **339 checks
with zero failures**. It compiles six current shipping function bodies, the
actual CE/Halo 3 capability expressions, and real runtime ownership/amplitude
helpers against captured OpenXR endpoints. The actual CE worker additionally
verifies pre-arm, armed and expired publication in both renderer modes.
Coverage includes both endpoints, either native motor band, short pulses,
sustained/zero rumble, contact merging, handedness, intensity and existing
title/focus/pause/tracking denial. No shared production function is modified.

Final packaging reruns Release checks from the committed source. Exact commit,
manifest/ZIP hashes and byte-for-byte archive verification are recorded in
`out/ce-haptics-current-handoff.json` after successful packaging. The verifier
requires the source archive to equal `git archive` for the embedded DLL commit,
and checks every staged build file, archive CRC and SHA-256 sidecar.

The candidate is not headset-accepted by this document. Preserve accepted
`5ac02f5` until the user tests the new ZIP. Target checks are firing and damage
in both CE graphics modes through both controllers, ordinary pause/resume and
title switching, and a quick Halo 3 vibration regression.
