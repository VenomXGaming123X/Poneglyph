// Poneglyph - small shared helpers (portable C++17).
#pragma once
#include <string>
#include <vector>
#include <filesystem>
#include <cstdint>
namespace pg {
namespace fs = std::filesystem;
std::string toLower(std::string s);
std::string trim(const std::string& s);
std::vector<std::string> split(const std::string& s, char sep);
std::string join(const std::vector<std::string>& v, const std::string& sep);
bool iequals(const std::string& a, const std::string& b);
std::string nowStamp();                       // "2026-11-21 14:03:09"
std::string nowStampCompact();                // "20261121_140309"
std::string pathToUtf8(const fs::path& p);
fs::path pathFromUtf8(const std::string& s);
bool readFileText(const fs::path& p, std::string& out);
bool writeFileAtomic(const fs::path& p, const std::string& data); // temp + rename
std::string formatBytes(uint64_t n);
// Reject unsafe relative paths (.., absolute, drive letters, ADS, reserved chars).
bool isSafeRelativePath(const std::string& rel);
// Returns true if 'child' resolves inside 'root' (lexical, after normalisation).
bool isInside(const fs::path& root, const fs::path& child);
}
