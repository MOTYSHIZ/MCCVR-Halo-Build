# Supplied playthrough: weapon coverage and damage blackout

Latest user instruction: damage blackout investigation is deferred until the
user explicitly requests it again. The damage notes below preserve findings
already made; they are not an active research task. Continue weapon contact.

Preserved log: out/test-runs/d77c9dd-20260910-weapon-contact-damage/user.log.
SHA-256: C59FD0A1C70619697F039F570993BE53246591DCBE9C9604CAC6418AD0B11167.
The attachment and preserved copy match. Source identity is
d77c9ddecc7c08a4cfde5fae2f9a358be82c0b16, Steam, SteamVR/OpenXR 2.17.9,
Oculus-family, 120 Hz. The log does not name an exact headset model. This is a
Halo 4 run; it does not establish the same failure in other titles.

The 1,752 world-contact windows contain 302,624 authored-bound publications
and 346,328 hand-only fallbacks. Grouping each window by its latest held-model
checksum gives the following evidence. These are two-second aggregates, not
per-impact or per-fallback cause records; swaps can mix models within a window.

| Held checksum | In current H4 catalog | Hand-only fallbacks |
| --- | --- | ---: |
| 0x1A04081C | No | 154,058 |
| 0x181D0903 | No | 62,191 |
| 0x1112060D | No | 54,722 |
| 0x19140D1C | No | 36,690 |
| 0x1304071C | No | 35,238 |

These five unknown models account for 342,899 fallbacks. This strongly supports
the catalog gap as the leading explanation for the user's missing weapon
contact. The accepted code explicitly falls back to hand-only when a checksum
has no bounds entry. Other failures (invalid transforms and publication order)
remain distinguishable possibilities for the remaining windows.

Next implementation should obtain bounds from the actual loaded equipped model,
with title-specific evidence for each cache layout and safe shape reseeding on
weapon changes. The H4EK editing-format compression block at +0x80 is not yet
proof of that loaded layout. H3/ODST/Reach previously failed after editing-format
offsets were treated as retail offsets; do not repeat this. A larger fixed
catalog does not fulfill the user's request for automatic custom-model coverage.

The damage black/fade report is unresolved. Only the startup interval records
two uncaptured eyes; subsequent capture telemetry does not show sustained loss
of eye capture. The existing shader bridge suppresses only the independently
proven motion-suck shader 47668A1953271934. The log provides no precise damage
timestamp or captured offending shader. Do not claim the blackout is that known
effect or disable unrelated screen effects speculatively.
