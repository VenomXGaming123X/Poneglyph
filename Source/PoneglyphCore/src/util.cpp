#include "poneglyph/util.hpp"
#include <algorithm>
#include <chrono>
#include <ctime>
#include <fstream>
#include <sstream>
#include <cstdio>
namespace pg {
std::string toLower(std::string s){ for(auto& c:s) if(c>='A'&&c<='Z') c=(char)(c+32); return s; }
std::string trim(const std::string& s){
    size_t a=0,b=s.size();
    while(a<b && (s[a]==' '||s[a]=='\t'||s[a]=='\r'||s[a]=='\n')) a++;
    while(b>a && (s[b-1]==' '||s[b-1]=='\t'||s[b-1]=='\r'||s[b-1]=='\n')) b--;
    return s.substr(a,b-a);
}
std::vector<std::string> split(const std::string& s,char sep){
    std::vector<std::string> r; std::string cur;
    for(char c:s){ if(c==sep){ r.push_back(cur); cur.clear(); } else cur+=c; }
    r.push_back(cur); return r;
}
std::string join(const std::vector<std::string>& v,const std::string& sep){
    std::string r; for(size_t i=0;i<v.size();i++){ if(i) r+=sep; r+=v[i]; } return r;
}
bool iequals(const std::string& a,const std::string& b){ return toLower(a)==toLower(b); }
static std::string fmtTime(const char* f){
    auto t=std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    std::tm tmv{};
#ifdef _WIN32
    localtime_s(&tmv,&t);
#else
    localtime_r(&t,&tmv);
#endif
    char b[64]; std::strftime(b,sizeof b,f,&tmv); return b;
}
std::string nowStamp(){ return fmtTime("%Y-%m-%d %H:%M:%S"); }
std::string nowStampCompact(){ return fmtTime("%Y%m%d_%H%M%S"); }
std::string pathToUtf8(const fs::path& p){
    auto u=p.u8string(); return std::string(u.begin(),u.end());
}
fs::path pathFromUtf8(const std::string& s){
#if (defined(__cplusplus)&&__cplusplus>=202002L)||(defined(_MSVC_LANG)&&_MSVC_LANG>=202002L)
    return fs::path(std::u8string(s.begin(),s.end()));
#else
    return fs::u8path(s);
#endif
}
bool readFileText(const fs::path& p,std::string& out){
    std::ifstream f(p,std::ios::binary); if(!f) return false;
    std::ostringstream ss; ss<<f.rdbuf(); out=ss.str(); return true;
}
bool writeFileAtomic(const fs::path& p,const std::string& data){
    std::error_code ec;
    if(p.has_parent_path()) fs::create_directories(p.parent_path(),ec);
    fs::path tmp=p; tmp+=".pgtmp";
    { std::ofstream f(tmp,std::ios::binary|std::ios::trunc); if(!f) return false;
      f.write(data.data(),(std::streamsize)data.size()); f.flush(); if(!f) return false; }
    fs::rename(tmp,p,ec);
    if(ec){ fs::remove(tmp,ec); return false; }
    return true;
}
std::string formatBytes(uint64_t n){
    const char* u[]={"B","KB","MB","GB"}; double v=(double)n; int i=0;
    while(v>=1024.0 && i<3){ v/=1024.0; i++; }
    char b[48]; if(i==0) std::snprintf(b,sizeof b,"%llu B",(unsigned long long)n);
    else std::snprintf(b,sizeof b,"%.2f %s",v,u[i]);
    return b;
}
bool isSafeRelativePath(const std::string& rel){
    if(rel.empty()||rel.size()>1024) return false;
    if(rel[0]=='/'||rel[0]=='\\') return false;
    if(rel.size()>=2 && rel[1]==':') return false;
    for(unsigned char c:rel){ if(c<32) return false; if(c=='<'||c=='>'||c=='"'||c=='|'||c=='?'||c=='*'||c==':') return false; }
    std::string n=rel; std::replace(n.begin(),n.end(),'\\','/');
    for(auto& part:split(n,'/')){
        if(part==".."||part==".") { if(part=="..") return false; }
        if(!part.empty() && (part.back()==' '||part.back()=='.') && part!=".") return false; // Windows trailing dot/space tricks
        std::string base=toLower(part); auto dot=base.find('.'); if(dot!=std::string::npos) base=base.substr(0,dot);
        static const char* rsv[]={"con","prn","aux","nul","com1","com2","com3","com4","com5","com6","com7","com8","com9","lpt1","lpt2","lpt3","lpt4","lpt5","lpt6","lpt7","lpt8","lpt9"};
        for(auto r:rsv) if(base==r) return false;
    }
    return true;
}
bool isInside(const fs::path& root,const fs::path& child){
    std::error_code ec;
    fs::path r=fs::weakly_canonical(root,ec); if(ec) r=root.lexically_normal();
    fs::path c=fs::weakly_canonical(child,ec); if(ec) c=child.lexically_normal();
    auto ri=r.begin(), ci=c.begin();
    for(;ri!=r.end();++ri,++ci){
        if(ci==c.end()) return false;
        if(ri->empty()) continue;
#ifdef _WIN32
        if(toLower(pathToUtf8(*ri))!=toLower(pathToUtf8(*ci))) return false;
#else
        if(*ri!=*ci) return false;
#endif
    }
    return true;
}
}
