#include "poneglyph/ini.hpp"
#include "poneglyph/util.hpp"
namespace pg {
IniFile::Sec* IniFile::find(const std::string& n){ auto l=toLower(n); for(auto& s:secs_) if(toLower(s.name)==l) return &s; return nullptr; }
const IniFile::Sec* IniFile::find(const std::string& n) const { auto l=toLower(n); for(auto& s:secs_) if(toLower(s.name)==l) return &s; return nullptr; }
bool IniFile::load(const std::filesystem::path& p){
    std::string t; if(!readFileText(p,t)) return false; return loadFromString(t);
}
bool IniFile::loadFromString(const std::string& text){
    secs_.clear(); std::string cur="";
    size_t i=0; std::string t=text;
    if(t.size()>=3 && (unsigned char)t[0]==0xEF&&(unsigned char)t[1]==0xBB&&(unsigned char)t[2]==0xBF) i=3;
    while(i<t.size()){
        size_t e=t.find('\n',i); if(e==std::string::npos) e=t.size();
        std::string line=trim(t.substr(i,e-i)); i=e+1;
        if(line.empty()||line[0]==';'||line[0]=='#') continue;
        if(line[0]=='['){ auto r=line.find(']'); if(r!=std::string::npos){ cur=trim(line.substr(1,r-1)); if(!find(cur)) secs_.push_back({cur,{}}); } continue; }
        auto eq=line.find('='); if(eq==std::string::npos) continue;
        set(cur,trim(line.substr(0,eq)),trim(line.substr(eq+1)));
    }
    return true;
}
std::string IniFile::toString() const{
    std::string o;
    for(auto& s:secs_){ if(!s.name.empty()) o+="["+s.name+"]\n"; for(auto& kv:s.kv) o+=kv.first+"="+kv.second+"\n"; o+="\n"; }
    return o;
}
bool IniFile::save(const std::filesystem::path& p) const { return writeFileAtomic(p,toString()); }
bool IniFile::has(const std::string& sec,const std::string& key) const{
    auto s=find(sec); if(!s) return false; auto l=toLower(key); for(auto& kv:s->kv) if(toLower(kv.first)==l) return true; return false;
}
std::string IniFile::get(const std::string& sec,const std::string& key,const std::string& def) const{
    auto s=find(sec); if(!s) return def; auto l=toLower(key); for(auto& kv:s->kv) if(toLower(kv.first)==l) return kv.second; return def;
}
int IniFile::getInt(const std::string& sec,const std::string& key,int def) const{
    if(!has(sec,key)) return def; try{ return std::stoi(get(sec,key)); }catch(...){ return def; }
}
bool IniFile::getBool(const std::string& sec,const std::string& key,bool def) const{
    if(!has(sec,key)) return def; auto v=toLower(get(sec,key));
    if(v=="1"||v=="true"||v=="yes"||v=="on") return true;
    if(v=="0"||v=="false"||v=="no"||v=="off") return false; return def;
}
void IniFile::set(const std::string& sec,const std::string& key,const std::string& val){
    auto s=find(sec); if(!s){ secs_.push_back({sec,{}}); s=&secs_.back(); }
    auto l=toLower(key); for(auto& kv:s->kv) if(toLower(kv.first)==l){ kv.second=val; return; }
    s->kv.push_back({key,val});
}
void IniFile::setInt(const std::string& s,const std::string& k,int v){ set(s,k,std::to_string(v)); }
void IniFile::setBool(const std::string& s,const std::string& k,bool v){ set(s,k,v?"1":"0"); }
bool IniFile::remove(const std::string& sec,const std::string& key){
    auto s=find(sec); if(!s) return false; auto l=toLower(key);
    for(size_t i=0;i<s->kv.size();i++) if(toLower(s->kv[i].first)==l){ s->kv.erase(s->kv.begin()+i); return true; } return false;
}
std::vector<std::string> IniFile::sections() const{ std::vector<std::string> r; for(auto& s:secs_) r.push_back(s.name); return r; }
std::vector<std::pair<std::string,std::string>> IniFile::entries(const std::string& sec) const{ auto s=find(sec); return s?s->kv:std::vector<std::pair<std::string,std::string>>{}; }
}
