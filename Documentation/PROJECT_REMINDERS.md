# PONEGLYPH - PROJECT REMINDERS & RULES (owner's instructions - do not lose)

## A. RULES FOR THE ASSISTANT
1. NEVER make a change the owner has not approved, and NEVER remove anything without telling the owner first.
2. Everything must be written so a 70-year-old grandmother can use it: plain words, short sentences, one clear next step, explain what/why, ask before changing files.
3. NovaFix features are to be added "precisely and safely": no feature may affect any of the 3 games unless the user turns it on; each change needs backup/rollback and a test.
4. Honesty: never claim something works that was not tested. Report what is verified and what is not.
5. Suggestions are welcome, but present them as suggestions and wait for approval.

## B. PRIORITIES - "without fail", must run better than NovaFix
 P1. CONTROLLER INPUT: Xbox, PS3, PS4, PS5, generic. Shown in the side panel "Add-ons" as e.g. "Enable controller input for Xbox",
     or AUTOMATIC detection when a device is already connected to the PC.  (NovaFix contains this - owner says it is all in its DLL.)
 P2. RESHADE support.
 P3. DXVK support.
 P4. RESOLUTION options (window mode / resolution / render resolution). To be offered AFTER the Poneglyph Engine is applied.
 Then (also from NovaFix): frame-rate limiter + VSync, anti-aliasing (MSAA), texture filtering, high-res UI, high-FPS correctness, FPS counter / F10 in-game menu.

## C. APPROVED CHANGES (in progress / done)
 - UI: modern look, better text colours per theme, bigger/clearer text, friendlier wording.  [Update 2]
 - THIRD THEME: Black Chrome (themes now: Silver Chrome, Dark Purple Chrome, Black Chrome).  [Update 2]
 - 4 GB flag: ask Yes/No right after the Engine installs, with details of why.  [Update 2]
 - Resolution settings are offered after the Engine is applied.  [with P4]
 - Font: change to a more modern, One Piece-anime-inspired font - TO BE DISCUSSED at finalization.

## D. DESIGN PRINCIPLE - THE "DEAD CORPSE" ENGINE
 - Poneglyph must be dormant while the game runs: it wakes only when instructed. Even when performance monitoring or any other feature runs.
 - The Manager (separate process, outside the game's memory) does the monitoring; the in-game engine does as little as possible.
 - Every in-game hook/feature is OFF by default and installed only when the owner switches it on.
 - Owner will bring MODs later; after the engine is finished, analyse/optimise them.

## E. REMINDER - ASK THE OWNER BEFORE FINALIZING (do not forget!)
 [ ] DONATION BOX in the Side Panel of the UI.
 [ ] SOUND EFFECTS on specific buttons (owner will say which).
 [ ] One Piece-anime themed FONT / final look - discuss.
 [ ] Owner has "a lot" more ideas to add - ask for them before release.
 [ ] Artwork rights (One Piece icons) before publishing.
 [ ] Antivirus check / code signing, Nexus page, GitHub release.

## F. OPEN SUGGESTION (needs owner approval): move the Manager's UI to a modern web-style UI (WebView2/HTML) for a true "2026" look:
     animations, custom fonts, sound, donation panel, all easy. Trade-off: needs the Microsoft WebView2 runtime (built into Windows 11, usually present on Windows 10).
