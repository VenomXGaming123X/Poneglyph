// Poneglyph - bounded, rotating, thread-safe text logger.
#pragma once
#include <string>
#include <filesystem>
#include <mutex>
#include <cstdio>
namespace pg {
enum class LogLevel { Error=0, Warn=1, Info=2, Debug=3 };
class Logger {
public:
    Logger()=default;
    ~Logger(){ close(); }
    // maxBytes: rotate when exceeded; keep: number of rotated files kept (log.1..log.N)
    bool open(const std::filesystem::path& file, uint64_t maxBytes=1024*1024, int keep=3, LogLevel lvl=LogLevel::Info);
    void close();
    void setLevel(LogLevel l){ level_=l; }
    void log(LogLevel l,const char* fmt,...)
#if defined(__GNUC__)
      __attribute__((format(printf,3,4)))
#endif
      ;
    void raw(const std::string& line);
    std::filesystem::path path() const { return file_; }
private:
    void rotate();
    std::mutex m_; std::FILE* f_=nullptr; std::filesystem::path file_;
    uint64_t max_=1048576, size_=0; int keep_=3; LogLevel level_=LogLevel::Info;
};
}
