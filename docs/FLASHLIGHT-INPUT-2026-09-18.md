# Optional flashlight input suppression

`disable_flashlight_input` defaults off. F1 Controls exposes **Disable flashlight
input**, a title selector, and **MCC Flashlight button**. The six independently
saved button selections must match the player's MCC control layout; this is
gamepad button suppression, not automatic discovery of the native flashlight
action. Right bumper is the initial selection. Suppressing a button also
suppresses other gameplay actions bound to that same button.

The filter runs on the final merged gamepad, including physical-controller-only
input. It does not modify tracked grip samples used for two-hand aiming, weapon
gestures or the F1 chord. Menus, pause and cutscenes retain their inputs. During
gameplay a consumed hold must release before changing the setting can expose a
new press. Other buttons, triggers and stick axes remain unchanged.

Validation: Release build, all 41 CTests, and Reach consistency gate pass.
The input fixture covers every selection in all six titles, default-off behavior,
selected-only suppression, release draining, menus and invalid selections.
Config tests cover six independent selections and persistence. Logs are under
`out/refinement-20260918/*flashlight*`. Headset behavior remains unaccepted.
