# D-pad controls and Quest thumbrest evidence, September 15, 2026

## Accepted baseline and scope

The user headset-accepts source
`7ff9697685e78389dad16126e4d1ec7188a3acbe`, including CE vibration in both
graphics modes. Preserve that runtime and its haptics. The next work is D-pad
controls: a head-gesture radius slider (10-50 cm, existing 30 cm default) and
an optional left-thumbrest hold that redirects the physical right stick to
D-pad directions, with the option off by default. Existing gameplay,
rendering, title recovery and haptic behavior remain the accepted reference.

The supplied log is preserved at
`out/test-runs/7ff9697-dpad-controls-20260915/user.log`, SHA-256
`E48CC1C64CB20FFBAC6076AEAE2B8AD68C6A39213CC9AC0EE6C8B870487B5E62`.
Line 1 identifies the exact source above and PID 28696. Line 2 identifies
Steam; lines 28/30 identify SteamVR/OpenXR 2.17.9 and
`SteamVR/OpenXR : oculus`; line 44 identifies panel 90 Hz. The user identifies
Quest 3, while the log itself does not identify an exact headset model.
Line 348 records the existing head gesture with a constant legacy diagnostic
label naming the right controller; that label does not identify the selected
physical hand. The accepted CE implementation selects physical left and
requires it to be on the left side of the head.

The accepted candidate DLL SHA-256 is
`6B2BAC9936D9606A0EBE76086EDE612422D99A9368F5A472994B448BA3F8A329`,
from `out/ce-haptics-current-handoff.json` and the candidate manifest, not
from the runtime log. No installed DLL or game file was changed for this audit.
The old haptics handoff's awaiting-test status predates this new acceptance.

Delivery remains a new build/source ZIP pair after verification, using
`tools/package-candidate.ps1` without `-Install`, then waiting for testing.
No MCC launch, game-folder change, PR or publication belongs to this task.

## Verified input path

The exact left-thumbrest input is:

```
Action type: XR_ACTION_TYPE_BOOLEAN_INPUT
Input path:  /user/hand/left/input/thumbrest/touch
Profile:     /interaction_profiles/oculus/touch_controller
```

The already pinned SDK registry
`out/deps/openxr-src/specification/registry/xr.xml` declares the Touch profile
at line 12459, both left/right user paths, and the boolean thumbrest component
at line 12480. Registry SHA-256:
`D014A9D1ACD4BFD32D6056702ED48D929508DCC22BEC2A337920F4FCD18ED420`.
This is an OpenXR 1.0 core profile component; no Touch Pro, Touch Plus, finger
tracking, thumb proximity, or thumbrest-force extension is required.
The public [Khronos OpenXR specification](https://registry.khronos.org/OpenXR/specs/1.0-khr/html/xrspec.html)
also defines this Touch controller component.

The registry also declares the same boolean component for:

- `/interaction_profiles/facebook/touch_controller_pro` at lines 12732/12753,
  supplied by `XR_FB_touch_controller_pro` for this application's OpenXR 1.0
  instance;
- `/interaction_profiles/meta/touch_controller_plus` at lines 12768/12789,
  supplied by `XR_META_touch_controller_plus` for an OpenXR 1.0 instance.

The XML contracts for these three profiles were parsed and checked, with
the exact result preserved in
`out/test-runs/7ff9697-dpad-controls-20260915/thumbrest-registry-check.json`.
Meta's [Touch Plus controller documentation](https://developers.meta.com/horizon/documentation/native/pc/native-touch-plus-controllers/)
independently lists thumbrest touch for both hands and the extension needed
when selecting that newer profile. Adding or requiring the newer profile is
unnecessary for this ordinary Touch binding and is outside this correction.

At the accepted source, `CreateControllerActions` already submits the complete
Touch binding list. It submits the Pro variant only when the runtime advertises
its extension. The new log states that Pro is not advertised (line 29) and that
five profiles were accepted (line 38). That identifies binding setup, not the
actual currently active profile or a working thumbrest sensor.

Read-only inspection of installed SteamVR's
`C:/Program Files (x86)/Steam/steamapps/common/SteamVR/drivers/oculus/resources/input/touch_profile.json`
finds `controller_type: oculus_touch` and an `/input/thumbrest` input source
with `click: false`, `touch: true` at lines 83-87. The exact file is copied to
`out/test-runs/7ff9697-dpad-controls-20260915/steamvr-oculus-touch-profile.json`,
SHA-256 `C5C2C35F96EB479429FA82113B870BE73F84BB238B59ECC990F9D09D4CDC44A8`.
This confirms that the installed driver describes the sensor; it does not
prove that the user's current streaming transport forwards its live state.

## Optional action and failure isolation

Create one optional boolean action before attaching the existing gameplay
action set. Bind it only to the physical left-thumbrest path. Existing
left-handed weapon mode must not exchange this control or the physical right
stick: this is a physical control chord, independent of weapon-hand roles.
Action creation failure leaves only this action unavailable and logged; it
must not fail controller-action creation or VR ownership.

Append the optional binding to the *complete* existing Touch bindings. If
the runtime rejects that attempt, retry the unchanged complete original
bindings and log the thumbrest feature as unavailable for that profile.
Do not submit a separate thumbrest-only list after the original list:
another successful suggestion replaces every earlier suggestion for that
profile. Suggestion acceptance also does not prove the runtime actually uses
the source. These are explicit rules of
[`xrSuggestInteractionProfileBindings`](https://registry.khronos.org/OpenXR/specs/1.0/man/html/xrSuggestInteractionProfileBindings.html).

Sample the boolean after the current successful `xrSyncActions`. A positive
hold requires a successful boolean query, `isActive == XR_TRUE`,
`currentState == XR_TRUE`, current valid input and focused session. Inactive,
failed, unfocused or stale samples release the modifier and clear its D-pad
fields. [`XrActionStateBoolean`](https://registry.khronos.org/OpenXR/specs/1.0/man/html/XrActionStateBoolean.html)
defines `isActive` as the existence of a contributing input source; a true
historical value alone is insufficient. No action query or binding error
should disarm the title camera or end the OpenXR session.

Consume the physical right-stick axes centrally when the enabled thumbrest
modifier owns them, before scope zoom and any title/controller snapshot sees
the axes. Preserve their original values in dedicated D-pad fields and publish
zero ordinary turn axes for that sample. Consuming only inside the XInput
hook would leave native title turn and scope paths able to act on the same
stick. This is a deliberate shared input change and requires a Halo 3
regression as well as the user's target-title test.

## Validation and remaining runtime proof

Implementation and compiled fixtures now pass in the cumulative Release build.
`dpad_action_tests.cpp` executes extracted production action setup, optional
binding fallback, touch sampling and stale/focus/menu clearing. The input suite
checks the configured radius, physical right-stick consumption, direction bits
and unchanged disabled behavior. Core tests check defaults, migration,
clamping, save/load and malformed configuration. All 33 CTest suites and the
Reach consistency gate pass before committed packaging. The same candidate
contains the separately documented all-title opt-in hand-alignment correction.

Record implementation, final candidate identity and test results after the
source is complete. Verification should cover enabled/disabled behavior,
all four directions and neutral, holding/releasing the thumbrest, physical
hand selection under left-handed mode, no simultaneous turning/zooming,
failed/inactive/stale/focus-loss samples, and complete original binding
preservation when the optional action or binding is refused. Keep all existing
D-pad gestures available according to their configured choices.

The registry, vendor documentation and installed driver establish that the
chosen path is valid. They do not establish the user's live thumbrest sensor
delivery through SteamVR or headset acceptance of this new control. The next
headset test must confirm the optional control, legacy D-pad controls, CE's
accepted vibrations and a quick Halo 3 controls/haptics regression.
