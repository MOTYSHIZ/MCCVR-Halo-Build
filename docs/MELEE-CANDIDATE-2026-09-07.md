# Physical and gesture melee test candidate

The user's September 7 request is to compile and deliver the current work in
progress now. This handoff supersedes the earlier checkpoint's packaging hold;
it does not claim the remaining physical-melee work is complete or accepted.
The accepted baseline remains ad7fbf5. Both MCC editions use this build.

In F1, True physical melee and Gesture melee are separate experimental toggles,
independent of World collision. Both default off. Required swing speed defaults
to 5.00 m/s. True physical melee takes priority when both modes are enabled.
Gesture melee resolves the active native controller melee binding per title.
Native contact adapters are present for H2, H3, ODST, Reach, and H4; CE is excluded.

Known unfinished work: fully unarmed native damage selection and correct
secondary-weapon damage response selection. Existing native builders can use
the primary weapon response for either hand and can skip unarmed hit queries.
H2 gesture axis/held bindings decline with diagnostics. Network-host behavior
and all new headset behavior remain unverified. The current worktree also
contains the preserved contact, H2, visibility and alignment refinements; this
is a snapshot of that work, not an isolated or accepted melee release.

Packaging updates the obsolete fixed-right-shoulder dispatcher consistency
check to the active-binding gesture route, while keeping native binding proof
verification and the existing geometry/velocity guards. Build, tests and the
Reach gate must pass for the exact committed source before delivery. The build
ZIP includes this note and its manifest; the source ZIP is git archive of that
same commit. No installation, game launch or accepted-pointer change occurs.
