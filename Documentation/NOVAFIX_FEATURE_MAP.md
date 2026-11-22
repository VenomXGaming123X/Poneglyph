# NovaFix feature map vs Poneglyph  (built from what is visible inside NovaFix's d3d9.dll text and the three game programs)

Status key:  DONE = built + unit-tested here | NEEDS WINDOWS TEST = built, awaiting your test | PLANNED = approved, not built | RESEARCH = need more evidence first

| NovaFix feature (seen in its DLL) | How it works | Poneglyph status |
|---|---|---|
| Resolution (width/height) | Game launch options `-Width= -Height=` (confirmed in FFXIII + XIII-2 programs and the FFXIII launcher) | NEEDS WINDOWS TEST (Game Options page) - FFXIII, FFXIII-2. Lightning Returns: not a launch option |
| Window mode / Borderless / Fullscreen | `-FullScreenMode=Force` for full screen (confirmed). Borderless is NovaFix's own window handling | Full screen vs window: NEEDS WINDOWS TEST. Borderless: PLANNED (needs a hook) |
| Anti-aliasing (MSAA) | `-MSAA=2/4/8/16` (confirmed) | NEEDS WINDOWS TEST |
| Shadow resolution | `-Shadow=512..8192` (confirmed) | NEEDS WINDOWS TEST |
| Japanese voice | `-VoiceJPMode` (confirmed in FFXIII and XIII-2). Edition decides if Japanese audio exists | NEEDS WINDOWS TEST (Home button + Game Options) |
| Swap Confirm/Cancel | `-DecideButtonReverse` (confirmed) | NEEDS WINDOWS TEST |
| Interface/text language | XIII-2 has `-Lan=` (values not yet known). NovaFix lists English, German, French, Italian, Korean, Spanish, Traditional Chinese, Japanese | RESEARCH - need the accepted `-Lan=` values (a short test on XIII-2 will tell) |
| "Language/audio files" | NovaFix does NOT contain language or audio files - it only sets the options above. Audio comes from your Steam install (Western editions of FFXIII have both; Asian edition is Japanese only; LR may need the Japanese voice pack) | Nothing to extract. Poneglyph explains this in the UI |
| VSync / frame-rate limit / high-FPS correctness | NovaFix hooks Direct3D | PLANNED (opt-in, off by default) |
| Texture filtering / high-res UI | NovaFix hooks Direct3D | PLANNED |
| FPS counter + F10 in-game menu (Dear ImGui overlay) | Per-frame overlay inside the game | PLANNED - biggest piece; costs performance by nature, so opt-in |
| Controller: detect + show | Manager silently lists Xbox (XInput), PS3/PS4/PS5 and generic pads (Windows HID list) | NEEDS WINDOWS TEST (Home page) |
| Controller: hot-plug, "fast reconnect", vibration | XIII + XIII-2 read pads through DirectInput8 (they import dinput8.dll, NOT xinput). NovaFix reaches into each game's own memory to rebuild its device list (it logs "contract mismatch" checks) | PLANNED/RESEARCH - needs per-game reverse engineering. LR imports xinput1_3.dll (XInput) |
| Controller button prompts (Automatic / Controller / Keyboard) | NovaFix patches the game's prompt mode | PLANNED/RESEARCH |
| PS3/PS4/PS5/generic pad -> game | Needs an input translation layer (DirectInput/XInput emulation) | PLANNED - only testable with a PlayStation pad; owner has Xbox |
| ReShade support | Chain-load / compatibility | PLANNED (priority) |
| DXVK support | Renderer switch (NovaFix ships Install-DXVK scripts) | PLANNED (priority) |
| 4 GB patch ("Crash Patch.exe") | Sets Large Address Aware flag | DONE + verified on Windows; Yes/No prompt after install |
| Movie crash fixes (XIII Nautilus, XIII-2 aspect) | Game-specific hooks | RESEARCH |
| Add-on loader | Loads .asi/.dll | DONE + verified on Windows (deterministic order, crash recovery) |
| Steam cloud / launcher wrappers | Misc | not requested |

## Findings that matter
* XIII and XIII-2 use DirectInput for controllers, so they only look for pads at start-up: plug the pad in BEFORE starting the game. The Manager now shows what it sees.
* Launch options only apply when the game is started with them (Poneglyph's "Play the Game"). A plain Steam launch uses the game's own launcher.
* Lightning Returns does not read these launch options.
