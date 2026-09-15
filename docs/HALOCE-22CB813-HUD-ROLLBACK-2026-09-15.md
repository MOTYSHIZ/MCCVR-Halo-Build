# Disable the failed 22cb813 late Anniversary HUD transaction

The September 15 headset test confirms excellent CE world rendering but no
Anniversary HUD. The log records 2,464 completed HUD copies, proving that the
old success counter is insufficient evidence of visible drawing.

Disable only admission of the unprepared late Anniversary HUD adapter before
implementing its replacement. Keep its code, native bindings and earlier manual
replay adapter in place. The earlier manual replay remains disabled too.
Original HUD, reticle capture, camera, native resolution, muzzle projection,
hands and contact are preserved. This isolated rollback is not a candidate
handoff and makes no claim to have diagnosed the native pixel failure.

Replacement work must verify actual GPU drawing and native output state, remain
an optional feature, and retain the working world path when it declines.
