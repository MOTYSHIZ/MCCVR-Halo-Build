# CE contact palette adapter

September 15 follow-up: the 22cb813 gun-contact report is addressed by
`HALOCE-WEAPON-CONTACT-REFINEMENT-2026-09-15.md`. Recognized stock graph frames
now append a posed envelope derived from all 21,180 official weapon vertices.
The node-only description below records the original adapter and remains the
explicit fallback for unknown graphs; exact custom/Saber mesh coverage is open.

Halo 3's reference behavior uses the visible hand/weapon pose for contact,
routes both physical hands independently, ignores authored animation as a
physical swing, and changes only the impacted hand's presentation when world
collision blocks its requested position. Contact failure must leave working
hands, camera and rendering available.

## Native evidence reused

`HALOCE-FIRST-PERSON-EVIDENCE.md` E-CE-FP-1 establishes the CE 52-byte native
world-space graph matrices and the graph palette commit before both Original
and Anniversary consumers. The corresponding official animation graph node
hierarchies are preserved in `HALOCE-FIRST-PERSON-TAG-EVIDENCE.json`.
The existing named wrist, weapon and arm-chain binding is authoritative here;
this adapter introduces no native offset, hook, guessed bone or marker.

The contact palette is sampled after `BuildTrackedFirstPersonPalette` produces
the requested visible pose and before a collision response changes that pose.
The root sample is the owning hand's wrist. Remaining points are actual hand
and weapon descendant node positions in stable native node order. Only the
selected main hand owns weapon descendants. The graph root and unrelated
nodes never become contact points or receive hand corrections.

These are **node-based contact samples**, not a proved complete weapon mesh
surface. CE runtime mesh bounds and detailed authored hand surfaces are not
implemented by this helper. Native damage/query ownership and scheduling are
separate from this geometry adapter.

The completed native adapter, HCEEK/retail homologues, simulation scheduling,
damage ownership and explicit limits are documented in
`HALOCE-NATIVE-CONTACT-EVIDENCE-2026-09-15.md` (E-CE-CONTACT-1).

## Transform, motion and routing

The tracking-to-world transform is derived from CE's production
`BuildTrackingFrame` equation. Its axes are the three tracking unit vectors
mapped through that frame; its origin subtracts the reference position from
the native center position. With positional tracking disabled it subtracts
the current head position, matching `BuildControllerMatrix` exactly. Scale
uses CE's current native units per metre.

World palette positions are converted back to tracking metres in each
`contact_melee::Frame`. The frame's rigid-motion controller is the corresponding
**physical** tracked controller, rather than the calibrated visible weapon
carrier. Shared motion processing therefore evaluates the current geometry
under the previous and current controller poses; reload/finger animation or
actor locomotion cannot become a punch.

Physical indices are left zero and right one. Default left-handed presentation
keeps the released semantic hand meshes while moving the weapon to physical
left. Experimental anatomical alignment routes native left-hand descendants
to physical left and native right-hand descendants to physical right; weapon
descendants still belong only to the main hand. Graph/count/masks, route,
tracking mode, mount/scale settings and two-hand state form shape identity.
Module generation, reference revision, tracking-space epoch and graphics-mode
epoch form reference identity, forcing motion reseeding across those changes.

## Applying contact without changing the working hand solver

Both physical-hand translations are staged atomically. Every matching hand or
weapon descendant is translated once; orientation and scale are retained.
The existing CE arm solver rebuilds the corresponding named chain from the
original native authored palette to the corrected wrist. Non-IK chains use
the existing rigid wrist carrier. Floating arms are then collapsed again at
their corrected wrist, avoiding the earlier detached hidden-arm triangles.
Native root/carrier and unrelated nodes remain byte-identical. A malformed
binding, nonfinite input, failed arm solve or correction exceeding the shared
0.75-metre bound leaves the supplied palette untouched.

## Focused checks

`halomccvr_ce_contact_tests` exercises both handedness modes, anatomical routing,
IK and floating hands; exact wrist/descendant world reconstruction; single
weapon translation; unchanged root and unrelated nodes; correction rejection
without partial writes; animation/locomotion exclusion; a real 5 m/s physical
controller displacement; duplicate-eye suppression; identity-change reseeding;
invalid pose/clock/generation/mask rejection; and equality with the actual CE
controller transform at multiple world scales and reference orientations.

These checks establish local math and transport input only. Native contact,
damage and both graphics modes still need the user's headset result.
