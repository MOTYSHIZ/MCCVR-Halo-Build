# 2cf002b Anniversary prepared HUD rollback

The user confirms CE Original reticle, muzzle flash, gun tracking and overall
behavior are now correct, but reports an immediate crash switching graphics.
The supplied Steam / SteamVR OpenXR 2.17.9 / Oculus-family / 90 Hz log records
the switch at 19:45:21.474, heartbeat expiry at 19:45:22.580 and a presentation
stall at 19:45:22.581. Window-sizing callbacks continue at 19:45:23.685–23.694.
There is no exception address or crash stack. A switch stall is established;
its precise cause is not established by this log alone.

Disable the newly enabled prepared Anniversary HUD target transaction as a
separate rollback before investigating its replacement. Retain its code and
all Original reticle, camera, effects and stock weapon contact improvements.
Earlier manual and unprepared HUD replays remain disabled. This rollback is
not a delivery candidate or an accepted fix, and does not advance the accepted
source pointer. No game is launched or installed.
