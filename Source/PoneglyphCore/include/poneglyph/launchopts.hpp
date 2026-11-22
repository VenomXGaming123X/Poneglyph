// Poneglyph - per-game launch options (resolution, window mode, anti-aliasing, shadows, voice, button swap).
// These are the SAME command-line options the games' own launcher passes to the game executable
// (verified by scanning ffxiiiimg.exe / ffxiii2img.exe and the FFXIII launcher). No hooking, no game files changed.
// Lightning Returns does not read them, so it is reported as unsupported rather than guessed.
#pragma once
#include <string>
#include <vector>
#include "poneglyph/games.hpp"
#include "poneglyph/ini.hpp"
namespace pg {
struct LaunchOptions {
    bool fullscreen=false;     // -FullScreenMode=Force (otherwise windowed)
    int  width=0, height=0;    // 0 = let the game decide
    int  msaa=0;               // 0 (off), 2, 4, 8, 16
    int  shadow=0;             // 0 (game default), 512, 1024, 2048, 4096, 8192
    bool voiceJP=false;        // -VoiceJPMode  (Japanese voice; needs Japanese audio data in the install)
    bool decideReverse=false;  // -DecideButtonReverse (swap Confirm / Cancel)
};
bool launchOptionsSupported(GameId g);
LaunchOptions loadLaunchOptions(const IniFile& cfg);
void saveLaunchOptions(IniFile& cfg,const LaunchOptions& o);
std::vector<std::string> validateLaunchOptions(const LaunchOptions& o);          // empty = valid
std::vector<std::string> buildLaunchArgs(GameId g,const LaunchOptions& o);       // empty if unsupported or invalid
std::string joinArgs(const std::vector<std::string>& a);
}
