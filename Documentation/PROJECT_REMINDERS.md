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

## G. DECISIONS / NOTES FROM UPDATE 3
 - Home button order (owner): row 1 = Find My Game | Play the Game | Install Poneglyph (highlighted text); row 2 = Search a Folder for My Game | Open Game Folder | Uninstall Poneglyph (Undo).
 - Owner likes the 4 GB page design (status card + memory bar) - implemented with REAL numbers only.
 - Owner wants smaller tabs, fashionable fonts, richer chrome colours, bright (white/silver) frame in every theme - applied.
 - Owner asked for language selection on Home: Voice (English/Japanese) is on Home. TEXT language needs the -Lan= values (XIII-2) - RESEARCH.
 - Controller priority stays #1: silent detection done (Manager); hot-plug/vibration in-game needs per-game reverse engineering (see NOVAFIX_FEATURE_MAP.md).
 - REMINDER STILL OPEN: Donation box, sound effects, One Piece font, more owner ideas, artwork rights, WebView2 UI decision.

## H. OWNER'S LIST FROM UPDATE 4 REQUEST  (status as of this update)
 DONE in Update 4 (safe, tested):
  [x] Folders renamed: <game bin>\.Poneglyph  and  <game bin>\.Poneglyph-Addons  (all 3 games). Old "Poneglyph" folder is MOVED automatically (never overwritten).
  [x] Removed the "=====" and "-----" lines from the UI text and reports.
  [x] Lightning Returns voice message: no more error popup - the Voice button is greyed out with "Voice: set inside the game".
  [x] Home tells you when a newer Poneglyph engine came with a download ("A Poneglyph update is ready").
 WAITING FOR OWNER'S APPROVAL OF THE DESIGN PREVIEW (then built):
  [ ] Welcome note "Hisashiburi dana... Mugiwara" - 5 seconds, OK button, auto-closes, One Piece-style background.
  [ ] Modern UI: clean cards instead of text blocks, next-step card, themed title-bar buttons (minimise / maximise / close) and themed window border.
  [ ] Dropdowns on every choice (Resolution etc.), glass look, hover highlight in the theme colour on menu items and tabs.
  [ ] Font size -2 pt; Settings: font (5 Google Fonts, sans-serif default), size (4 choices), bold on/off.
  [ ] Replace "Black Chrome" with the Elbaf-inspired futuristic theme (preview: "Elbaf Aurora").
  [ ] "Where can I find my game folder?" help on Home.
 PENDING - REMIND THE OWNER (do not forget):
  [ ] "Other Games" in the side panel (older -> modern games; "your sidekick" scales it).
  [ ] Help > "Check for Update" with GitHub. Design: public Releases of ONE fixed repository only (owner + repo pinned in the program), HTTPS, SHA-256 checked, asks before installing, checks at most once a day when idle. No GitHub password or token is needed. Owner should turn on 2-step login on the GitHub account and protect the main branch.
  [ ] "Nakama / Friend" room in the side panel (unique feature) - owner will send Source Code + Master Plan to sync. Needs a security design first.
  [ ] "Donate" below Help: PayPal. Poneglyph must NOT collect card/account details itself. Plan: show PayPal's own secure page inside the Poneglyph window (no separate browser), owner's PayPal account.
  [ ] Button sound effects (owner says which).  [ ] One Piece-style font - discuss.
  [ ] Controller priorities (Xbox/PS3/PS4/PS5/generic), ReShade, DXVK, resolution (launch options done for XIII/XIII-2), frame limiter etc. - see NOVAFIX_FEATURE_MAP.md.
  [ ] Artwork rights before publishing the One Piece icons.

## I. UPDATE 6 - APPROVED DESIGN BUILT (owner: "PERFECT, I LOVE IT")
 BUILT (awaiting Windows test):
  [x] Welcome note "Hisashiburi dana... Mugiwara": 5 seconds, OK button, closes by itself, sunset/straw-hat drawing. (Setting: Settings > Show welcome note)
  [x] Elbaf Aurora theme replaces Black Chrome (themes: Silver Chrome, Dark Purple Chrome, Elbaf Aurora).
  [x] Own title bar + themed window border + themed minimise / maximise / close buttons.
  [x] Hover highlight in the theme colour on menu items, tabs, buttons, switches.
  [x] Dropdowns (30% see-through) for Resolution, Anti-aliasing, Shadow quality, Theme, Font, Text size, Summary, Log detail.
  [x] Home: next-step card, step cards with pills, "Where can I find my game folder?", More information cards (no ==== / ---- lines).
  [x] Settings: Font (Inter, Exo 2, Rajdhani, Chakra Petch, Nunito), Text size (Small/Medium/Large/Extra large), Bold, Show welcome note. Default text is 1-2 pt smaller.
  [x] Fonts are embedded from Google Fonts (SIL OFL) when the build can download them; otherwise Segoe UI is used.
 STILL PENDING: Other Games, Check for Update, Nakama, Donate, sound effects, One Piece font, controller/ReShade/DXVK/frame-limiter work, text language (-Lan=).
