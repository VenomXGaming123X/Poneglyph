# Poneglyph 1.0.0 - verification report (honest status)

## 1. Verified here (Linux sandbox, g++ 17, real files from your archive)
| Area | Result |
|---|---|
| RAR extraction of your project (own RAR5 decoder, CRC-checked) | OK |
| Core unit tests: SHA-256 vectors, INI, safe paths, PE parser, LAA patch/restore/tamper, ZIP (stored+deflate, 6 traversal variants, CRC, bomb), add-on policy (arch/dup/dep/conflict/cycle/proxy/disabled/game), crash recovery, install/uninstall engine with foreign-proxy backup+restore, package install/uninstall, frame stats, log rotation, report | **156 checks, 0 failures** |
| Real binaries: ffxiiiimg.exe (LAA off), ffxiii2img.exe and LRFF13.exe (LAA on), x86 PE32; NovaFix d3d9.dll recognised and classified | OK |
| PE checksum implementation vs independent reference | identical |
| `pg_tool gendef` against NovaFix's d3d9.dll: produces ordinal-exact .def | OK |

## 2. NOT verified (cannot be done in this sandbox - no Windows, no MSVC, no network)
* Engine and Manager compile with MSVC: **not compiled**. Engine was only syntax-checked against a stub header; Manager not at all. Expect to fix small compile errors on first build.
* Engine behaviour inside a real game (forwarding, add-on load, frame hook), Present vtable indices, SEH paths.
* Manager GUI rendering, DPI scaling, dialogs, process monitor.
* Anything on Windows 10 vs 11, Steam editions, AV reaction, SmartScreen.
These are covered by `pg_proxytest` (Stage B/C) and the CI workflow; they must pass on a Windows machine before release.

## 3. Known limits of v1.0 (by design - nothing faked)
Per-callback add-on profiling, in-game overlay, renderer/shader fixes, game-specific memory patches: not implemented.
Frame time = interval between Present calls (CPU-side), not GPU time. Optimisation claims are never made automatically.

## 4. Manual matrix to run (Windows) - mark each
For each of FFXIII, FFXIII-2, LR: clean install; install over NovaFix then Remove (original d3d9.dll byte-identical?); add-on loads; broken add-on is skipped;
add-on that crashes during load is auto-disabled next start; 4 GB flag on/off/restore; game starts and reaches the menu; ZIP with `..\` entries is refused.
