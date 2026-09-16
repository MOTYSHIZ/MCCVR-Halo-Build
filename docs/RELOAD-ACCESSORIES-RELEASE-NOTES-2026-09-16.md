# Reload and holster refinement test

Supports all six titles, CE/H2 in both graphics modes, Steam and Microsoft Store.
This preserves the earlier CE vehicle/crosshair, Anniversary beam-glare and
Halo 3 Cortana-facing changes. New runtime behavior needs headset testing.

## Updating

Close MCC and the launcher. Extract the build ZIP outside the game folder.
Replace the DLL and launcher in your existing Halo_MCC_VR folder with the new
ones. **Keep your existing halomccvr.cfg**; new options receive safe defaults.
For a fresh setup, follow MANUAL-README.txt and use the supplied default config.
Use your usual anti-cheat-disabled launch method. No files were installed by
the packaging process.

## Try the new controls

In **Weapon & Aim**, enable **Manual Reload**. The part appears at your
support-side hip when a supported gun is equipped. Hold support grip there,
bring it just below the gun hand, and release. Release elsewhere to cancel.
Halo still controls ammo, reload animations and automatic reloading.

- Separate sliders adjust magazine grab, insertion and holster radii.
- Holsters offer **slide / draw** and **click** checkboxes. Click switches once
  when you press weapon grip inside the shoulder/hip zone. With both enabled,
  click takes precedence. Release grip before switching again.
- **Shake to reload needle weapons** is optional and off by default. Hold
  weapon grip away from the holster and shake up/down twice within 1.8 seconds.
  Release grip before repeating. Minimum stroke is adjustable.
- Unfamiliar modded held weapons can automatically use a **blue generic reload
  item**. Disable **Generic reload item for unknown weapons** to hide it while
  keeping the reload gesture. It is a fallback, not the mod's own magazine.
- Match each game's **MCC Reload button** and **MCC Switch Weapon button** to
  its controller layout. Both handedness modes are supported.

## What to expect

There are 42 extracted reload-part meshes across all six games. These include
magazines and identified detachable cells/drums. They use simple grey shading,
not native textures. They can show through the gun or nearby scenery. The
native weapon still plays its normal reload and retains its native magazine
until that animation removes it. CE/H2 use the same accessory in both graphics
modes. Shotgun loading gates are deliberately not displayed as magazines.

Modded guns that retain a recognized model identity use its known part;
unfamiliar valid held models use the generic blue item. Invalid/unreadable rigs
keep the gesture without inventing a mesh. Custom ammo types and arbitrary
magazine geometry are not automatically extracted. Shake is restricted to
recognized Needlers and Reach's Needle Rifle; unknown guns keep the pouch
gesture and ordinary controls. Native ammo rules can reject a reload request.

Test pouch pickup/carry/drop/insert, both holster modes, shake, ordinary buttons,
weapon swaps, full/empty ammo, left/right handedness, menus, vehicles and title
switches. Please include stock and modded weapons, both CE/H2 graphics modes,
and Halo 3 as the regression reference. Send HaloMCCVR.log with your result and
identify the title/mod, edition, OpenXR runtime and headset.

Offline build/tests do not establish headset acceptance. Accepted cumulative
source remains d47a98c. Detailed implementation/evidence is included in the
matching source under docs/RELOAD-ACCESSORIES-2026-09-16.md. Isolated Halo-derived
geometry remains artwork of its respective owners; the generic blue item and
surface shading are mod-authored. Original kit tags, textures and executables
are not included.
