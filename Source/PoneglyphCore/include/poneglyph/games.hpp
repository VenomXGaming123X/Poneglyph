// Poneglyph - the three supported games and their profiles.
#pragma once
#include <string>
#include <vector>
#include <filesystem>
#include "poneglyph/pe.hpp"
namespace pg {
enum class GameId { FFXIII=0, FFXIII2=1, LR=2, Unknown=99 };
struct GameInfo {
    GameId id; const char* key; const char* displayName; const char* exeName; const char* commonRelPath;
    const char* steamFolder;
};
const std::vector<GameInfo>& allGames();
const GameInfo* gameInfo(GameId id);
const GameInfo* gameByKey(const std::string& key);
// Identify by executable file name only (case-insensitive).
GameId identifyByExeName(const std::string& fileName);
// Searches 'root' (depth <= 5) for any supported game executable. Returns full path or empty.
std::filesystem::path findGameExe(const std::filesystem::path& root, GameId id, int maxDepth=5);
struct ExeCheck { bool ok=false; GameId id=GameId::Unknown; std::string message; PeInfo pe; };
// Verifies: file exists, name matches the game, PE32 x86 EXE. Never guesses.
ExeCheck verifyGameExe(const std::filesystem::path& exe, GameId expected);
// Folder layout used for a given game directory (exe directory).
struct ProfilePaths {
    std::filesystem::path gameDir, root, addons, logs, backups, config, status, legacyRoot, renderers;   // root=".Poneglyph", addons=".Poneglyph-Addons" (legacyRoot="Poneglyph", used only for migration)
    static ProfilePaths forGameDir(const std::filesystem::path& dir);
};
}
