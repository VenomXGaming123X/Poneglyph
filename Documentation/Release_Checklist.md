# Release checklist (do these on Windows before publishing)
1. `cmake -S . -B build\x86 -A Win32` + build Release; `cmake ... -A x64` + build Release. Fix any compiler error first.
2. `ctest --test-dir build\x86 -C Release` : core_tests AND proxy_runtime must pass.
3. `pg_tool verify build\x86\Source\PoneglyphEngine\Release\d3d9.dll C:\Windows\SysWOW64\d3d9.dll` : RESULT: PASS.
4. Manual game matrix (see TEST_REPORT.md section 4): each game x {clean install, NovaFix present, remove engine, 4 GB flag, add-on crash}.
5. Scan the built binaries with your AV and upload to VirusTotal; note false positives (hooking proxies are often flagged).
6. Code-sign Poneglyph.exe and d3d9.dll if you have a certificate (reduces SmartScreen/AV warnings).
7. ARTWORK NOTE: Resources/Icons contains artwork supplied by the project owner (One Piece characters). Publish it only if you
   hold the rights; otherwise replace Resources/Icons/poneglyph_silver.ico and poneglyph_purple.ico (the build works without them).
8. Tag v1.0.0, attach the CI artifact `Poneglyph_v1.0.0_windows` to the GitHub release, upload the same zip to Nexus.
