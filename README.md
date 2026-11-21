# Poneglyph Mod Loader

Native, single-purpose mod loader for the PC (Steam) versions of **FINAL FANTASY XIII**, **XIII-2** and **LIGHTNING RETURNS: FINAL FANTASY XIII**.

* **Engine** (`d3d9.dll`, x86) - Direct3D 9 proxy; loads `.asi`/`.dll` add-ons deterministically; crash-loop recovery; optional light frame statistics.
* **Manager** (`Poneglyph.exe`, x64) - install/remove with backups, safe ZIP install, add-on policy preview, 4 GB (Large Address Aware) flag with verified backup/restore, process monitor, reports, two themes.
* **Core** (portable C++17) - PE parser, SHA-256, hardened ZIP reader, INI, add-on policy. Unit-tested on Linux and Windows.
* **SDK** - `Source/PoneglyphSDK/poneglyph_sdk.h` for add-on authors.

## Build
```
cmake -S . -B build\x86 -A Win32 && cmake --build build\x86 --config Release   # engine + tools + tests
cmake -S . -B build\x64 -A x64   && cmake --build build\x64 --config Release   # manager
ctest --test-dir build\x86 -C Release --output-on-failure
```
Requires Visual Studio 2022+ (C++ desktop workload) and CMake 3.20+. The build generates the engine's export list (names **and ordinals**) from the genuine `SysWOW64\d3d9.dll`, then verifies the result.

## Status
See `Documentation/TEST_REPORT.md` for exactly what has and has not been verified. Windows binaries must be built and tested on Windows (CI workflow included).

## Layout
`Source/PoneglyphCore | PoneglyphEngine | PoneglyphManager | PoneglyphTools | PoneglyphTests | PoneglyphSDK`, `Documentation/`, `.github/workflows/`.

MIT licensed. Not affiliated with Square Enix, Valve or Nexus Mods.
