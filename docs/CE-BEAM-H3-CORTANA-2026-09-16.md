# CE Anniversary glare and Halo 3 Cortana facing candidate

This candidate targets two tester reports on top of the latest delivered and
published source **35a4d096b1c535582134ab7be211eda739564aff**. Both corrections
are local and require headset testing. Cumulative accepted source remains
**d47a98c**. The user explicitly requested both corrections in one ZIP handoff.
Preserve the CE vehicle steering and seated reticle work, both editions and
all earlier standing/deferred scope. Package only, without installation,
game-folder writes, game launch or publication.

## Test these changes

Keep your existing configuration when updating the DLL and launcher.

1. In CE Anniversary, watch a Forerunner beam tower fire, then turn and walk
   past it. Check for lingering glare while looking away, normal visible beam
   lighting, and agreement between the two eyes. Check an ordinary bright
   scene and CE vehicle steering/crosshairs too.
2. In Halo 3's Cortana mission, approach the object that must be meleed to
   trigger the rescue cutscene. Walk, turn and melee it. Check that facing
   stays steady during the effects, the trigger still works, and the real
   cutscene and return to gameplay face the right direction.
3. Briefly check ordinary Halo 3 movement, aiming, recoil comfort, snap turn
   and recenter. Send the new log with the headset result, preferably with
   the times of any remaining symptom.

No headset success or absence of regressions is claimed by local checks.
The CE report has no synchronized scene capture, and the log does not identify
the particular beam's draw. Its exact symptom attribution remains a test target.

## Tester identity and observations

`HaloMCCVR (17).log` identifies source 35a4d09, compiled September 16 at
03:18:12. This matches the latest candidate source and publication checkpoint.
Steam, SteamVR/OpenXR in Meta compatibility mode **2.17.10**, Oculus-family
headset, **72 Hz**, 2912x2100 game raster. The precise headset model and a
tester-side DLL hash are not present; do not invent either.

Preserved log: `out/test-runs/35a4d09-ce-beam-h3-cortana-20260916/tester.log`.
SHA-256: `D72316A1F4020EF8A73646B25F860294698C201ABC5B2EFB5B142AC3F0996FA7`.
The preexisting all-campaign acceptance log was checked as a comparison; it
does not provide this Cortana-section reproduction. No installed-game or
deployment-backup log is a matching replay of this external tester's session.

The existing CE flare hooks report installed, zero unproven reads/exceptions,
and final cumulative counts of four clipped and 10,206 visible calls. Their
near-plane guard was active; that alone did not cover the newly reported glare.
Halo 3 reports its recoil/shake hook installed and motion blur disabled. There
are **59** facing-realignment notifications, all labeled gameplay-camera exit,
and no notification with a valid scene/shot pair. Many occur inside continuously
reported gameplay, e.g. 08:15:52.069/.278/.324 and 08:21:51.204/.295.
Notifications can coalesce between Present calls, so this is not a full count
of camera callbacks or proof of the exact visible symptom time.

## CE: bound the residual offscreen halo

Halo 3's reference experience is a stable tracked world, without an offscreen
screen-space light expanding across the eye. This deliberately extends CE's
existing optional flare transaction; Original graphics and other titles do
not enter it.

E-CE-FLARE-1 already proves the native projector, both light records, primary
eye ownership and original argument identities. This extension uses those
same verified bindings. In the pinned CE image
`0A12DC561780F449D3F4D0DF10BB8D3BC7BE7840A5BEB2B236F672EB6CD42E6C`, sprite
draw `0x447EA0` computes normalized radius with render width on BOTH axes
(`0x4480EA..0x44813F`). `0x448144..0x4481DB` sets ordinary sprite strength
to zero beyond radius **0.8**, but its size multiplier keeps growing as
`max(1, (radius - 0.2)/0.4)`. The source-centered sprite takes the separate
`0x448535..0x448546` branch, retaining strength 1 beyond that radius.
At radius 1000, native size exceeds 2000x and the source sprite still has
nonzero alpha. This is a demonstrated native arithmetic property, not proof
that every beam artifact comes from that sprite.

The adapter now skips a proven front-facing flare draw only if its source is
outside the eye raster AND beyond that native 0.8-width envelope. Every
in-raster source (including unusual tall rasters) and modest peripheral flare
keeps its original draw. Existing near-plane/nonfinite rejection is retained.
Malformed raster evidence stays stock. No light, bloom setting, beam material,
effect record, native visibility, history or camera ownership is changed.
Cold logs add `offscreen=` to distinguish the new suppression from near clipping.

`tools/re/test_ce_flare_envelope_native.py` executes the actual pinned envelope
and alpha instructions in a read/execute-only Unicorn image: 20 cases, 548
instructions. Radius and sprite inputs are synthetic; GPU work is not executed.
The shipping dispatch fixture covers both records, four raster shapes,
visible/peripheral/offscreen positions, malformed proof, unowned callbacks,
exceptions, recovery and optional-hook retirement.
Evidence outputs: `out/ce-flare-envelope-native-20260916.json`,
`out/ce-flare-draw-native-20260916.asm`, and
`out/ce-beam-effects-20260916.txt`.

## Halo 3: require a real shot before changing facing

The old `ApplyHeadLook` treated every `AuthoredLocked` flag as a valid shot,
even with negative scene/shot IDs. It also treated `Unknown` as an exit.
Those transitions assign the current native camera heading to `g_gameYawRef`
and the current headset heading to `g_headYawRef`. Repeating them during a
scripted effect therefore changes the movement/view reference independently
of headset motion. This is a source-proven path consistent with the tester's
repeated gameplay-facing notifications; raw per-callback IDs were not logged.

The H3-only `Halo3CinematicFacing` state now requires nonnegative scene AND
shot IDs before an automatic entry/cut alignment. Unknown and shotless reads
retain the previous facing state; only a known PlayerControlled observation
after an identified shot emits an exit. State resets with title generation,
preventing a reload from inheriting a false exit. Real entry, scene/shot cuts
and one real exit retain their existing yaw assignments. Manual recenter,
snap turn, head pose, locomotion, melee, cinematic scripts and theatre
qualification are unchanged. No native hook or guessed binding was added.
Cold telemetry reports ignored shotless samples for the next headset test.

Production state tests cover repeated invalid flag pulses, partial IDs, shot
zero, unchanged shots, unknown reads, real cuts/exits and generation changes.
Native investigation also checked H3's motion-blur constants: unlike Reach,
the examined H3 consumer does not divide by its zeroed scale. No Reach-style
motion-blur change or blanket Cortana effect disable was justified or made.
Official H3EK mission scripts confirm the inner-sanctum Cortana sequence;
kit effect readers separately expose rumble, camera impulses and render
effects. These were investigation evidence only, not new retail bindings.

## Verification and handoff

Focused Release tests and the full 35-suite CTest run passed before packaging;
the package workflow repeats the full build/tests and Reach consistency gate
at its committed identity. Exact final identities and archive verification
are recorded in `out/beam-cortana-current-handoff.json` after packaging.
Source ZIP and build ZIP must match. Deliver both and wait for testing.
