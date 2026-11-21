// Poneglyph - PE (Portable Executable) inspection, Large-Address-Aware
// patching with backup / verify / restore. Pure C++17, no Windows API.
#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <filesystem>
namespace pg {
struct PeExport { uint32_t ordinal=0; std::string name; bool forwarded=false; std::string forwardTarget; };
struct PeInfo {
    bool valid=false;
    std::string error;
    uint16_t machine=0;               // 0x14c = x86, 0x8664 = x64
    uint16_t optMagic=0;              // 0x10b = PE32, 0x20b = PE32+
    bool isDll=false, isExe=false;
    bool laa=false;                   // IMAGE_FILE_LARGE_ADDRESS_AWARE (0x20)
    uint16_t characteristics=0;
    uint32_t timestamp=0;
    uint32_t storedChecksum=0;
    uint32_t calculatedChecksum=0;
    uint64_t fileSize=0;
    uint32_t charactOffset=0;         // file offset of IMAGE_FILE_HEADER.Characteristics
    uint32_t checksumOffset=0;        // file offset of OptionalHeader.CheckSum
    uint16_t subsystem=0;
    uint32_t exportBase=0;
    std::vector<PeExport> exports;
    std::vector<std::string> importDlls;   // lower-case DLL names
    bool is32() const { return optMagic==0x10b; }
    bool is64() const { return optMagic==0x20b; }
    bool isX86() const { return machine==0x14c && optMagic==0x10b; }
    std::string archName() const;
};
struct PeParseOptions { bool exports=true; bool imports=true; bool checksum=true; };
PeInfo inspectPe(const std::filesystem::path& file, const PeParseOptions& opt=PeParseOptions());
uint32_t computePeChecksum(const std::filesystem::path& file, uint32_t checksumOffset, uint64_t* sizeOut=nullptr);

enum class LaaResult { Patched, AlreadyLaa, NotPe32, NotX86, IsDll, FileError, BackupFailed, VerifyFailed, WrongExecutable };
struct LaaReport { LaaResult result=LaaResult::FileError; std::string message; std::filesystem::path backupPath; std::string sha256Before, sha256After; };
// Patches only IMAGE_FILE_LARGE_ADDRESS_AWARE (+ checksum when it was non-zero).
// expectedNames: if non-empty, filename (case-insensitive) must match one of them.
LaaReport patchLaa(const std::filesystem::path& exe, const std::filesystem::path& backupDir,
                   const std::vector<std::string>& expectedNames = {});
// Restores a backup produced by patchLaa. Verifies the hash recorded in the backup name.
LaaReport restoreFromBackup(const std::filesystem::path& backupFile, const std::filesystem::path& target);
}
