# Needler single-shake refinement

Preserves the working 205ff20 cursor and other features. This candidate changes
only Needler shake reload. Supports all six games, both CE/H2 graphics modes,
Steam and Microsoft Store.

Close MCC and the launcher, then replace the DLL and launcher in your existing
Halo_MCC_VR folder. **Keep your existing halomccvr.cfg.** Nothing was installed
automatically. For a fresh setup, use MANUAL-README.txt.

In F1 > Weapon & Aim, enable **Manual Reload** and **Shake to reload needle
weapons**. With a Needler equipped, quickly move the gun hand out and back once
in any direction. **No grip or other button is needed.** The existing minimum
stroke slider controls distance; lower it for a shorter shake. The old two-cycle
and 1.8-second requirement is gone. Let your hand settle before another shake.
Reach's Needle Rifle also qualifies. Halo still needs ammo and decides whether
a reload can occur. Unknown weapon identities keep their previous controls.

Try after firing a few rounds, with grip released. Check a single shake, a
second shake after settling, and your existing cursor, magazines and holsters.
Please include Halo 3 as a regression check when possible. Local build/tests
pass; headset acceptance remains pending. Send HaloMCCVR.log with your result.
