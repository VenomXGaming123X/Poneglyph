// Poneglyph - safe file operations on a game directory: install/uninstall the engine proxy,
// install add-on ZIP packages, enable/disable add-ons. Every change follows the Master Plan
// sequence: identify -> verify path -> check existing -> backup -> log -> change -> verify -> rollback.
#pragma once
#include <string>
#include <vector>
#include <filesystem>
#include "poneglyph/games.hpp"
#include "poneglyph/log.hpp"
namespace pg {
enum class ProxyKind { None, Poneglyph, NovaFix, Dxvk, Other, Unreadable };
const char* proxyKindText(ProxyKind k);
ProxyKind classifyD3D9(const std::filesystem::path& dll);
struct OpResult { bool ok=false; std::string message; std::vector<std::string> changed; };
struct InstallOptions { bool replaceForeignProxy=false; /* NovaFix/other: only with explicit user consent */ };
// 'engineDll' = the Poneglyph d3d9.dll build to install (must be x86 + export Direct3DCreate9).
OpResult installEngine(const ProfilePaths& pp,GameId game,const std::filesystem::path& engineDll,const InstallOptions& opt,Logger& log);
OpResult uninstallEngine(const ProfilePaths& pp,Logger& log);   // restores the previous d3d9.dll if we backed one up
OpResult installAddonZip(const ProfilePaths& pp,const std::filesystem::path& zip,Logger& log);
OpResult uninstallAddonPackage(const ProfilePaths& pp,const std::string& packageName,Logger& log);
std::vector<std::string> listAddonPackages(const ProfilePaths& pp);
OpResult setAddonEnabled(const ProfilePaths& pp,const std::string& addonKey,bool enabled,Logger& log);
void writeDefaultConfig(const ProfilePaths& pp,GameId game);
}
