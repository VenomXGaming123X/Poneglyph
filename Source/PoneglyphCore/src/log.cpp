#include "poneglyph/log.hpp"
#include "poneglyph/util.hpp"
#include <cstdarg>
#include <system_error>
namespace pg {
static std::FILE* openAppend(const std::filesystem::path& p){
#ifdef _WIN32
    return _wfopen(p.c_str(),L"ab");
#else
    return std::fopen(p.c_str(),"ab");
#endif
}
bool Logger::open(const std::filesystem::path& file,uint64_t maxBytes,int keep,LogLevel lvl){
    std::lock_guard<std::mutex> g(m_);
    if(f_){ std::fclose(f_); f_=nullptr; }
    file_=file; max_=maxBytes; keep_=keep<1?1:keep; level_=lvl;
    std::error_code ec; if(file.has_parent_path()) fs::create_directories(file.parent_path(),ec);
    f_=openAppend(file); if(!f_) return false;
    std::fseek(f_,0,SEEK_END); long p=std::ftell(f_); size_=p<0?0:(uint64_t)p;
    return true;
}
void Logger::close(){ std::lock_guard<std::mutex> g(m_); if(f_){ std::fclose(f_); f_=nullptr; } }
void Logger::rotate(){
    if(f_){ std::fclose(f_); f_=nullptr; }
    std::error_code ec;
    for(int i=keep_;i>=1;i--){
        fs::path src= i==1? file_ : fs::path(pathToUtf8(file_)+"."+std::to_string(i-1));
        fs::path dst=fs::path(pathToUtf8(file_)+"."+std::to_string(i));
        if(fs::exists(src,ec)){ fs::remove(dst,ec); fs::rename(src,dst,ec); }
    }
    f_=openAppend(file_); size_=0;
}
void Logger::raw(const std::string& line){
    std::lock_guard<std::mutex> g(m_);
    if(!f_) return;
    if(size_+line.size()+1>max_) rotate();
    if(!f_) return;
    std::fwrite(line.data(),1,line.size(),f_); std::fputc('\n',f_); std::fflush(f_);
    size_+=line.size()+1;
}
void Logger::log(LogLevel l,const char* fmt,...){
    if((int)l>(int)level_) return;
    char buf[2048]; va_list ap; va_start(ap,fmt); std::vsnprintf(buf,sizeof buf,fmt,ap); va_end(ap);
    static const char* n[]={"ERROR","WARN ","INFO ","DEBUG"};
    raw("["+nowStamp()+"] "+n[(int)l]+" "+buf);
}
}
