# Building Halo MCC VR

The cumulative Release preset includes H2 Classic/Anniversary, H3, ODST, Reach
and H4. Both MCC editions use the same binaries. Local build/test/package commands
write under ignored `out/`; packaging without `-Install` does not deploy or launch.

## Requirements

Windows x64, Visual Studio 2022 Desktop development with C++, CMake 3.24+, Git,
and network access for the first dependency fetch. OpenXR, MinHook and Dear ImGui
are pinned in CMakeLists.txt. The modified MinHook implementation is compiled from
src/common/minhook_lifecycle.c without editing the upstream checkout.

## Build and verify

From a Developer PowerShell in the source directory:

```powershell
cmake --preset release
cmake --build --preset release
ctest --preset release
powershell -NoProfile -ExecutionPolicy Bypass -File tools/check-reach-fp-parity.ps1
```

The preset points to existing dependency checkouts in out/deps. For a fresh clone
or extracted source archive without those directories, clear the local overrides
to allow CMake's pinned dependency fetches:

```powershell
cmake --preset release -DFETCHCONTENT_SOURCE_DIR_OPENXR= -DFETCHCONTENT_SOURCE_DIR_MINHOOK= -DFETCHCONTENT_SOURCE_DIR_IMGUI=
cmake --build --preset release
ctest --preset release
```

Product outputs are out/build/release/Release/HaloMCCVR.dll and
HaloMCCVRLauncher.exe. The config-defaults executable generates the candidate
config from this source. Preserve users' existing saved config when upgrading.
`camscan` and standalone observers are excluded from normal builds; do not build
or run process-memory write diagnostics without explicit authorization.

## Package-only handoff

From a clean committed checkout descending from the accepted source:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/package-candidate.ps1
```

This repeats configure/build/tests and the Reach consistency check, stages the
manifest-backed candidate under out/candidates, and writes a build ZIP, matching
git source ZIP and SHA-256 sidecar. Do not pass `-Install` for an ordinary handoff.
The source archive contains tracked source, documentation and assets at that
commit, excluding dependencies, game binaries, local evidence captures and build
outputs. It contains no .git history; archive rebuilds log source as unknown.
Use the original ZIP/manifest for exact candidate bytes and provenance.

A successful build is not runtime acceptance. Test affected titles and Halo 3 in
the headset, recording the edition/runtime/headset, source and installed DLL hash.
Only the user's explicit acceptance advances docs/CURRENT-STATE.md. Current
feature status is in docs/ACTIVE-WORK-CHECKPOINT.md and the standing refinement
list. Historical per-title architecture narratives live in their evidence docs;
optional feature failures must not tear down a working VR core.
