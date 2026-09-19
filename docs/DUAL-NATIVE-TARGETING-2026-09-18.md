# Native acquisition needed by independent firing

The requested behavior covers both normal dual-wield campaign titles, Halo 2
(both renderers) and Halo 3. These findings extend DUAL-SHOT-PIPELINE-2026-09-18.md;
the native evidence is distinct from the in-progress implementation below.

## Current implementation status

The optional H3 implementation uses a guarded, exception-safe temporary target
lease during the original firing callback. It captures ordinary native query
parameters and actual camera owner on the same native thread, then reruns native
acquisition for the selected controller only with a fresh owned dual pair.
Native query and subsequent firing-assist camera consumers receive that ray;
the full native targeting result also reaches direct homing consumers. Restoration
requires unchanged generation, full owner, allocation and written bytes. Native
changes win. No inventory swap or manufactured target is used.

Production fixtures exercise the implementation with native services stubbed:
1,024 checks include nested firing, optional/native exceptions, storage lifetime
and every hook lifecycle failure. Release, all 45 suites, Reach gate and pinned
bindings passed after the lifecycle/menu edits and worker activation. The option
is default-off. No headset
result, H2 implementation completion or final projectile behavior is claimed.

## Halo 3

Official H3EK central acquisition `411390` corresponds to pinned retail
`13B08C`. Both have six arguments: input user, query flags, direction pointer,
signed 16-bit zoom level, control output and targeting output. The result is
0x24 bytes, initialized with three NONE identifiers. The existing ordinary
caller is H3EK `449186`, retail `F42D8`; retail call `F4CC8` returns at `F4CCD`.
The caller can run a second lead-only query, so blindly saving the last query
would lose the ordinary query's flags. No input-user/output-user equivalence
has been assumed.

Acquisition builds a query with H3EK `40DE60`, retail `13C518`. Its first
argument is an effective UNIT, not a weapon. H3EK `A60C60` / retail `355CD4`
resolve that effective unit, including vehicle/controlling-parent cases. The
builder selects primary and secondary inventory roles itself. It retains each
gun's authored targeting parameters and combines parts of their magnetism
envelope. A copied single-weapon query would not reproduce that behavior.

The native camera call during acquisition is `13B1D7 -> 212198`, return
`13B1DC`. The later firing assist calls the same camera dispatcher at `13BC08`,
return `13BC0D`. Its H3EK homolog `640180` and retail dispatcher both return the
native perspective and fill position/direction. Any controller substitution
must preserve its return value and be restricted to the proven native caller
and owned firing/query scope. Global camera substitution would be incorrect.

H3EK acquisition invokes the actual broad search `40E8B0`, retail `13C914`,
which enumerates, sorts and visibility-checks candidates. This is distinct from
the already observed existing-target validator `40F1E0` / `13E32C`. The broad
search uses temporary native sorting state; calling it concurrently on an
arbitrary render/worker thread would be unsafe. No such call is installed.

Retail firing reads targeting at the effective unit's +218; homing branches
can consume it without calling `13BAD0`. The parent traversal at +2A4 means
that writing the local biped's target alone would be insufficient unless the
controlling-parent case is excluded. Private assist arguments alone likewise
would not redirect the direct homing branch. Ownership, restoration and native
thread evidence must be addressed before enabling that design. The guarded lease prototype described
above is now installed by the title worker, with default-off behavior.

Pinned offline check found unique 40-byte entries for `13B08C`, `212198`,
`13BAD0`; unwind extents are respectively `13B6F8`, `2121EE`, `13C3DD`.
All four acquisition/assist camera and call edges listed above were verified
against the pinned B209D845...EFB63 module. Read-only extracts:
out/reload-policy/h3-acquisition-{kit,query-kit,retail}.c and
out/refinement-20260918/h3-query-bindings.py.

## Halo 2

Pinned firing `8E4940` invokes the existing early aim helper `8F0F70`, then
player assist `7596A0` with five arguments, including its mutable direction.
It also has direct native homing branches. Thus the existing early helper is
not, by itself, evidence that the final primary or secondary projectile is
independent. Retail acquisition `759260` is already used by the accepted
controller-melee targeting path; its ABI/result layout differ from Halo 3.
H2EK `4FE420` confirms retail `7596A0`; kit camera `489E10` matches `6D4730`
with native perspective return. Assist calls it at `7597B8` (return `7597BD`).
The direct homing path consumes effective-unit +1D4, after following +260
controlling parents. These fields are separately observed in H2EK `89C960`
and retail firing; the optional adapter excludes controlling parents and seats.

Acquisition uses H2EK `442DD0`, retail `6F0E60`, at `759325` (return `75932A`).
It returns the complete observer location, whose point starts at +0; the native
BSP/cluster data must remain intact. The new detour preserves this pointer and
converges the acquisition direction from that native point to the selected
hand's aim point. The accepted scoped view helper `6C0DF0` consumes the direction
at `75934C`. Firing helper and later camera assist converge from their respective
native origins to that same point. No guessed BSP location is manufactured.

H2EK `4FEC30` / retail `759B70` build native primary/secondary weapon parameters;
central acquisition performs native broad search, sort and visibility checks.
The implementation reruns the existing central trampoline only on the thread
which completed a fresh ordinary local query. It temporarily supplies its own
0x24-byte native target result to the effective unit during firing, restoring
only unchanged owner/generation/storage/bytes. Existing camera-adhesion suppression
and ordinary melee acquisition stay in their accepted path. No duplicate central
query hook or inventory-role swap is used.

The H2 installer verifies five unique entries and six call edges; the offline
verifier additionally checks pinned identity and seven entry unwind records.
H2 uses chained unwind entries, so a short initial entry extent is not the full
native function extent. Optional retirement covers four exact detours/trampolines
and callback ingress, retaining dependencies on failure. The older early-only
implementation remains inert. The shared config/F1 option defaults off.

Latest validation: Release, all 46 CTests and Reach gate pass; 845 H2 and 1,032
H3 production-fixture assertions include nested roles, role-swapped carriers,
native/optional exceptions, stale ownership, storage replacement and each hook
lifecycle failure. Native services are stubbed; final projectiles and headset
appearance remain unverified. No accepted pointer update or package yet.
