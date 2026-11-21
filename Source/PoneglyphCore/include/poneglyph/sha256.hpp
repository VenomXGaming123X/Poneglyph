// Poneglyph - SHA-256 (independent implementation).
#pragma once
#include <cstdint>
#include <string>
#include <filesystem>
namespace pg {
class Sha256 {
public:
    Sha256();
    void update(const void* data, size_t len);
    std::string hexdigest();            // finalizes
    static std::string ofBuffer(const void* data, size_t len);
    static bool ofFile(const std::filesystem::path& p, std::string& outHex);
private:
    void block(const uint8_t* b);
    uint32_t h_[8]; uint8_t buf_[64]; uint64_t total_; size_t fill_;
};
}
