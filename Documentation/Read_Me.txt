PONEGLYPH MOD LOADER  v1.0.0
for FINAL FANTASY XIII, XIII-2 and LIGHTNING RETURNS (PC, 32-bit Steam versions)
=====================================================================================

WHAT IT IS
Poneglyph has two parts:
  * Poneglyph.exe  - the Manager (64-bit). Installs/removes the engine, manages add-ons, backups, 4 GB flag, reports.
  * Engine\d3d9.dll - the Engine (32-bit). A Direct3D 9 proxy that loads your add-ons (.asi/.dll) in a safe, repeatable order.

REQUIREMENTS
Windows 10 or 11 (64-bit). One of the three games. No Visual C++ redistributable is needed (static runtime).

INSTALL
 1. Extract this folder anywhere you like (not inside the game folder).
 2. Run Poneglyph.exe. On Home, press "1. Find My Game" and pick:
      FINAL FANTASY XIII     ... ffxiiiimg.exe  (white_data\prog\win\bin)
      FINAL FANTASY XIII-2   ... ffxiii2img.exe (alba_data\prog\win\bin)
      LIGHTNING RETURNS      ... LRFF13.exe
 3. Press "2. Install Poneglyph". Any d3d9.dll already there is backed up first.
 4. Add mods: Add-ons > "Install ZIP...", or copy .asi/.dll files into <game folder>\.Poneglyph-Addons.

UNINSTALL
Home > "Uninstall Poneglyph (undo)". Your previous d3d9.dll is restored from the verified backup. Add-ons, settings and saves are kept.

IF YOU USE NOVAFIX OR ANOTHER d3d9.dll
Only one Direct3D 9 proxy can exist per game. Poneglyph will not overwrite it silently: it asks, backs it up, and can put it back.

4 GB FLAG
Memory page. Makes the 32-bit game able to address up to 4 GB (never more). Backup + verification first; "Restore Original" included.

GAME OPTIONS (FFXIII and FFXIII-2)
Resolution, window mode, anti-aliasing, shadow quality, Japanese voice and swapping Confirm/Cancel. They use the games' own start-up options and apply when you press "Play the Game".
Plug your controller in BEFORE starting the game (these games look for controllers only at start-up). Home shows which controllers Poneglyph can see.

WHAT IS INCLUDED / NOT INCLUDED IN 1.0
Included : engine proxy, deterministic add-on loading, crash recovery (auto-disables an add-on that crashed while loading),
           safe ZIP install/uninstall, 4 GB flag with backup, process monitor, optional frame-time statistics, reports, 3 themes.
Not included: in-game overlay, per-callback add-on profiling, shader/renderer replacement, game-specific memory patches.

SAFETY NOTES
 * Add-ons are native code and run inside the game. Only install mods from sources you trust.
 * Modifying game files can break online/copy-protection checks. Single-player only; keep the backups.
 * No telemetry. Poneglyph makes no network connections.

LOGS AND SUPPORT
<game folder>\.Poneglyph\Logs\poneglyph.log, addon_status.ini, and reports from Help > "Create Support Report".

LICENSE: MIT (see LICENSE). Not affiliated with or endorsed by Square Enix, Valve or Nexus Mods.
