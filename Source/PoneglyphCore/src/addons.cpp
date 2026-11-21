#include "poneglyph/addons.hpp"
#include "poneglyph/sha256.hpp"
#include "poneglyph/util.hpp"
#include <algorithm>
#include <map>
#include <set>
namespace pg {
const char* addonStatusText(AddonStatus s){
    switch(s){
    case AddonStatus::Ready: return "Ready"; case AddonStatus::Disabled: return "Disabled";
    case AddonStatus::WrongArch: return "Wrong architecture"; case AddonStatus::InvalidPe: return "Not a valid DLL";
    case AddonStatus::Duplicate: return "Duplicate"; case AddonStatus::MissingDependency: return "Missing dependency";
    case AddonStatus::Conflict: return "Conflict"; case AddonStatus::WrongGame: return "Not for this game";
    case AddonStatus::BlockedProxy: return "Blocked (D3D9 proxy)"; case AddonStatus::DependencyCycle: return "Dependency cycle"; }
    return "?";
}
std::string addonConfigKey(const std::string& rel){
    std::string k=toLower(rel); for(auto& c:k){ if(c=='=') c='~'; if(c==';'||c=='#'||c=='[') c='_'; if(c=='\\') c='/'; } return k;
}
static std::vector<std::string> listOf(const std::string& s){
    std::vector<std::string> r; for(auto& p:split(s,',')){ auto t=trim(p); if(!t.empty()) r.push_back(t); } return r;
}
AddonMeta readAddonMeta(const fs::path& f){
    AddonMeta m; fs::path mp=f; mp+=".pgmeta"; IniFile ini; if(!ini.load(mp)) return m;
    m.present=true; m.name=ini.get("Addon","Name"); m.version=ini.get("Addon","Version"); m.author=ini.get("Addon","Author");
    m.description=ini.get("Addon","Description"); m.arch=toLower(ini.get("Addon","Arch","")); m.games=listOf(toLower(ini.get("Addon","Games")));
    m.dependencies=listOf(ini.get("Addon","Dependencies")); m.conflicts=listOf(ini.get("Addon","Conflicts"));
    m.loadOrder=ini.getInt("Addon","LoadOrder",1000); return m;
}
static void walk(const fs::path& root,const fs::path& dir,int depth,int maxDepth,std::vector<fs::path>& out){
    std::error_code ec;
    for(fs::directory_iterator it(dir,fs::directory_options::skip_permission_denied,ec),end;!ec&&it!=end;it.increment(ec)){
        std::error_code e2; auto name=pathToUtf8(it->path().filename());
        if(it->is_directory(e2)){
            if(!name.empty()&&(name[0]=='_'||name[0]=='.')) continue;         // _Disabled, .git ... never scanned
            if(it->is_symlink(e2)) continue;                                  // never follow links out of the folder
            if(depth<maxDepth) walk(root,it->path(),depth+1,maxDepth,out);
        } else if(it->is_regular_file(e2)){
            auto ext=toLower(pathToUtf8(it->path().extension()));
            if(ext==".asi"||ext==".dll") out.push_back(it->path());
        }
    }
}
std::vector<AddonEntry> scanAddons(const fs::path& dir,const IniFile& cfg,const ScanOptions& opt){
    std::vector<AddonEntry> all; std::vector<fs::path> files; std::error_code ec;
    if(!fs::is_directory(dir,ec)) return all;
    walk(dir,dir,0,opt.maxDepth,files);
    auto gi=gameInfo(opt.game);
    for(auto& p:files){
        AddonEntry e; e.path=p; e.rel=pathToUtf8(fs::relative(p,dir,ec)); std::replace(e.rel.begin(),e.rel.end(),'\\','/');
        e.key=addonConfigKey(e.rel); e.fileName=pathToUtf8(p.filename()); e.kind=toLower(pathToUtf8(p.extension())).substr(1);
        e.size=fs::file_size(p,ec); e.meta=readAddonMeta(p);
        e.userEnabled=cfg.getBool("Addons",e.key,true);
        PeParseOptions po; po.exports=true; po.imports=false; po.checksum=false; e.pe=inspectPe(p,po);
        if(opt.computeHash) Sha256::ofFile(p,e.sha256);
        e.compat="Unknown (no metadata - not verified for this game)";
        if(e.meta.present&&!e.meta.games.empty()){
            bool ok=false; for(auto& g:e.meta.games) if(gi&&(g==gi->key||g=="all")) ok=true;
            e.compat=ok?"Declared compatible by its metadata":"Declared for other game(s): "+join(e.meta.games,", ");
        }
        all.push_back(std::move(e));
    }
    std::sort(all.begin(),all.end(),[](const AddonEntry&a,const AddonEntry&b){ if(a.meta.loadOrder!=b.meta.loadOrder) return a.meta.loadOrder<b.meta.loadOrder; return a.key<b.key; });
    // ---- pass 1: individual checks
    std::set<std::string> seenNames;
    for(auto& e:all){
        auto ln=toLower(e.fileName);
        if(!e.pe.valid){ e.status=AddonStatus::InvalidPe; e.reason=e.pe.error; continue; }
        bool proxy=(ln=="d3d9.dll");
        if(!proxy) for(auto& x:e.pe.exports) if(x.name=="Direct3DCreate9"||x.name=="Direct3DCreate9Ex") proxy=true;
        if(proxy){ e.status=AddonStatus::BlockedProxy; e.reason="This file is a Direct3D 9 proxy. Two proxies in one game break each other; Poneglyph will not load it."; continue; }
        if(!e.userEnabled){ e.status=AddonStatus::Disabled; e.reason="Turned off by the user (file untouched)."; continue; }
        bool archOk=opt.requireX86? e.pe.isX86() : true;
        if(!archOk){ e.status=AddonStatus::WrongArch; e.reason="This game is 32-bit (x86) but the add-on is "+e.pe.archName()+"."; continue; }
        if(e.meta.present&&!e.meta.arch.empty()&&e.meta.arch!="any"&&e.meta.arch!="x86"&&opt.requireX86){ e.status=AddonStatus::WrongArch; e.reason="Metadata says arch="+e.meta.arch+"."; continue; }
        if(gi&&e.meta.present&&!e.meta.games.empty()){
            bool ok=false; for(auto& g:e.meta.games) if(g==gi->key||g=="all") ok=true;
            if(!ok){ e.status=AddonStatus::WrongGame; e.reason="Metadata lists other games only: "+join(e.meta.games,", "); continue; }
        }
        if(seenNames.count(ln)){ e.status=AddonStatus::Duplicate; e.reason="Another add-on with the same file name ("+e.fileName+") loads first."; continue; }
        seenNames.insert(ln);
    }
    // ---- pass 2: dependencies (iterate: removing one can orphan another)
    bool changed=true;
    while(changed){
        changed=false;
        std::set<std::string> ready; for(auto& e:all) if(e.status==AddonStatus::Ready) ready.insert(toLower(e.fileName));
        for(auto& e:all) if(e.status==AddonStatus::Ready) for(auto& d:e.meta.dependencies){
            if(!ready.count(toLower(d))){ e.status=AddonStatus::MissingDependency; e.reason="Requires '"+d+"' which is not present and ready."; changed=true; break; }
        }
    }
    // ---- pass 3: conflicts (first in load order wins)
    for(size_t i=0;i<all.size();i++){
        if(all[i].status!=AddonStatus::Ready) continue;
        for(auto& c:all[i].meta.conflicts){
            for(size_t j=0;j<i;j++) if(all[j].status==AddonStatus::Ready && iequals(all[j].fileName,c)){
                all[i].status=AddonStatus::Conflict; all[i].reason="Declares a conflict with '"+all[j].fileName+"', which loads first."; break; }
            if(all[i].status!=AddonStatus::Ready) break;
        }
    }
    // ---- pass 4: topological order (dependencies first, otherwise stable sorted order)
    std::vector<size_t> pending; for(size_t i=0;i<all.size();i++) if(all[i].status==AddonStatus::Ready) pending.push_back(i);
    std::set<std::string> done; int ord=0;
    while(!pending.empty()){
        bool progressed=false;
        for(size_t k=0;k<pending.size();k++){
            auto& e=all[pending[k]]; bool ok=true;
            for(auto& d:e.meta.dependencies) if(!done.count(toLower(d))) ok=false;
            if(ok){ e.order=ord++; done.insert(toLower(e.fileName)); pending.erase(pending.begin()+k); progressed=true; break; }
        }
        if(!progressed){ for(auto i:pending){ all[i].status=AddonStatus::DependencyCycle; all[i].reason="Circular dependency."; } break; }
    }
    if(opt.computeHash){
        std::map<std::string,std::string> first;
        for(auto& e:all) if(!e.sha256.empty()){ auto it=first.find(e.sha256); if(it==first.end()) first[e.sha256]=e.rel; else e.duplicateOfHash=it->second; }
    }
    return all;
}
std::vector<const AddonEntry*> loadList(const std::vector<AddonEntry>& all){
    std::vector<const AddonEntry*> r; for(auto& e:all) if(e.status==AddonStatus::Ready&&e.order>=0) r.push_back(&e);
    std::sort(r.begin(),r.end(),[](const AddonEntry*a,const AddonEntry*b){return a->order<b->order;}); return r;
}
bool writeLoadStatus(const fs::path& file,const std::vector<AddonLoadRecord>& recs,const std::string& game,const std::string& ver){
    IniFile ini; ini.set("Engine","Version",ver); ini.set("Engine","Game",game); ini.set("Engine","Written",nowStamp()); ini.setInt("Engine","Count",(int)recs.size());
    for(size_t i=0;i<recs.size();i++){ auto s="Addon."+std::to_string(i); ini.set(s,"Key",recs[i].key); ini.set(s,"File",recs[i].file); ini.set(s,"Result",recs[i].result); ini.set(s,"Detail",recs[i].detail);
        char b[32]; std::snprintf(b,sizeof b,"%.3f",recs[i].ms); ini.set(s,"LoadMs",b); }
    return ini.save(file);
}
std::vector<AddonLoadRecord> readLoadStatus(const fs::path& file){
    std::vector<AddonLoadRecord> r; IniFile ini; if(!ini.load(file)) return r; int n=ini.getInt("Engine","Count",0);
    for(int i=0;i<n;i++){ auto s="Addon."+std::to_string(i); AddonLoadRecord a; a.key=ini.get(s,"Key"); a.file=ini.get(s,"File"); a.result=ini.get(s,"Result"); a.detail=ini.get(s,"Detail");
        try{ a.ms=std::stod(ini.get(s,"LoadMs","0")); }catch(...){} r.push_back(a); }
    return r;
}
std::string applyCrashRecovery(const fs::path& marker,const fs::path& cfgFile,std::string& msg){
    std::string key; if(!readFileText(marker,key)) return ""; key=trim(key); std::error_code ec; fs::remove(marker,ec);
    if(key.empty()) return "";
    IniFile cfg; cfg.load(cfgFile); cfg.setBool("Addons",key,false); cfg.set("Recovery","LastDisabled",key); cfg.set("Recovery","When",nowStamp());
    cfg.save(cfgFile); msg="Previous start did not finish loading '"+key+"'. It was turned OFF automatically (file kept). Re-enable it in the manager when fixed."; return key;
}
}
