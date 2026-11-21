// Poneglyph - minimal ZIP reader with path-traversal and zip-bomb protection.
#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include <filesystem>
namespace pg {
struct ZipEntry { std::string name; uint16_t method=0; uint32_t crc=0; uint64_t csize=0, usize=0; uint64_t localOffset=0; bool isDir=false; bool encrypted=false; bool safe=true; std::string unsafeReason; };
struct ZipLimits { uint64_t maxTotalBytes=2ull<<30; uint64_t maxFileBytes=1ull<<30; uint32_t maxEntries=20000; uint32_t maxRatio=1000; };
bool inflateRaw(const uint8_t* in,size_t inLen,std::vector<uint8_t>& out,size_t expectedSize,size_t maxOut);
uint32_t crc32Buf(const uint8_t* d,size_t n,uint32_t crc=0);
class ZipArchive {
public:
    bool open(const std::filesystem::path& p,std::string& err);
    const std::vector<ZipEntry>& entries() const { return e_; }
    bool readEntry(const ZipEntry& e,std::vector<uint8_t>& out,std::string& err);
    // Extracts every safe entry below destRoot. Returns false (and extracts nothing) if ANY entry is unsafe
    // unless skipUnsafe is true. Lists written relative paths in 'written'.
    bool extractAll(const std::filesystem::path& destRoot,std::vector<std::string>& written,std::string& err,
                    bool skipUnsafe=false,const ZipLimits& lim=ZipLimits());
private:
    std::filesystem::path path_; std::vector<ZipEntry> e_;
};
}
