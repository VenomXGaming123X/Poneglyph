// Poneglyph - add-on discovery and the deterministic LOAD POLICY shared by the
// manager (preview) and the native engine (actual loading). One implementation,
// so what the manager shows is exactly what the engine does.
#pragma once
#include <string>
#include <vector>
#include <filesystem>
#include "poneglyph/pe.hpp"
#include "poneglyph/ini.hpp"
#include "poneglyph/games.hpp"
namespace pg {
enum class AddonStatus { Ready, Disabled, WrongArch, InvalidPe, Duplicate, MissingDependency, Conflict, WrongGame, BlockedProxy, DependencyCycle };
const char* addonStatusText(AddonStatus s);
struct AddonMeta {
    bool present=false; std::string name, version, author, description, arch;
    std::vector<std::string> games, dependencies, conflicts; int loadOrder=1000;
};
struct AddonEntry {
    std::filesystem::path path; std::string rel, key, fileName, kind;   // rel uses '/', key is lower-case
    uint64_t size=0; std::string sha256; PeInfo pe; AddonMeta meta;
    bool userEnabled=true; AddonStatus status=AddonStatus::Ready; std::string reason;
    int order=-1;               // position in final load list (-1 = not loaded)
    std::string compat;         // "Declared for this game" / "Unknown (no metadata)" / ...
    std::string duplicateOfHash;// manager-only: same SHA-256 as another add-on
};
struct ScanOptions { GameId game=GameId::Unknown; bool requireX86=true; bool computeHash=false; int maxDepth=3; };
std::string addonConfigKey(const std::string& rel);          // INI-safe lower-case key
AddonMeta readAddonMeta(const std::filesystem::path& addonFile);
// Returns ALL discovered add-ons. Entries with status Ready and order>=0 form the load list, already sorted.
std::vector<AddonEntry> scanAddons(const std::filesystem::path& addonsDir,const IniFile& cfg,const ScanOptions& opt);
std::vector<const AddonEntry*> loadList(const std::vector<AddonEntry>& all);

// ---- engine -> manager status exchange
struct AddonLoadRecord { std::string key, file, result, detail; double ms=0; };
bool writeLoadStatus(const std::filesystem::path& file,const std::vector<AddonLoadRecord>& recs,const std::string& game,const std::string& engineVersion);
std::vector<AddonLoadRecord> readLoadStatus(const std::filesystem::path& file);
// ---- crash recovery: a marker left behind means that add-on never finished loading.
std::string applyCrashRecovery(const std::filesystem::path& markerFile,const std::filesystem::path& cfgFile,std::string& message);
}
