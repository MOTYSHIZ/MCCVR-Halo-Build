# 2cf002b user test: Original confirmed; graphics switch fails; Reach HUD height

Tested source: `2cf002b6b41dd69b2515469bde82cd2b1553d50d`.
Supplied log preserved as
`out/test-runs/2cf002b-ce-reach-feedback-20260915/HaloMCCVR-user.log`, SHA-256
`BF9B95D51EC75A8F99BF040C8FF428D919F47F11BEE845C55F33F2CC85076E5E`.
Steam, SteamVR/OpenXR 2.17.9, Oculus-family headset, 90 Hz; this log does not
identify the exact headset model. Process 26776.

The user explicitly confirms CE Original now works perfectly, naming reticle,
muzzle flash and gun tracking. Preserve that feature-confirmed base. They
report an immediate crash when switching graphics and request full Anniversary
parity, retaining tracking, world contact, physical melee and gun contact.
They also tested Reach first and report that HUD height cannot move, although
the same control works in other titles. They request autonomous completion
through properly packaged new build and matching source ZIPs.

At 19:45:21.474 the log records the graphics-switch gesture. Presentation stops;
camera heartbeat expires at 19:45:22.580 and the stall is logged at 22.581.
Window-sizing callbacks continue at 23.685–23.694. The text log has no exception
address. A matching Windows dump exists at
`C:/Users/Shadow/AppData/Local/CrashDumps/MCC-Win64-Shipping.exe.26776.dmp`,
written 19:45:28. Analyze that dump before attributing the crash.

Before the switch, telemetry records 775 Classic eye pairs without drops,
1,264 native crosshair captures, zero contact exceptions and stock weapon
envelope publication. Zero Anniversary HUD/projection/visibility/muzzle counts
before switching do not establish where its first frame stopped. Contact
queries in this excerpt do not independently prove successful physical hits.

This is scoped Original feature confirmation, not cumulative acceptance of
the failed candidate. Accepted source remains `4e01f28`. Both MCC editions and
all standing/deferred scope remain retained. Package without `-Install`;
deliver both ZIPs in chat, then wait. No installation, launch, game-folder
writes, PR or publication.
