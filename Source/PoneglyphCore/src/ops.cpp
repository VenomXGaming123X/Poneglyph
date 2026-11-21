#include "poneglyph/ops.hpp"
#include "poneglyph/sha256.hpp"
#include "poneglyph/util.hpp"
#include "poneglyph/ini.hpp"
#include "poneglyph/zip.hpp"
#include "poneglyph/addons.hpp"
#include <fstream>
#include <algorithm>
namespace pg {
const char* proxyKindText(ProxyKind k){
    switch(k){ case ProxyKind::None: return "none"; case ProxyKind::Poneglyph: return "Poneglyph"; case ProxyKind::NovaFix: return "NovaFix";
    case ProxyKind::Dxvk: return "DXVK"; case ProxyKind::Other: return "unknown/other"; default: return "unreadable"; } }
static bool contains(const std::string& hay,const std::string& needle){ return hay.find(needle)!=std::string::npos; }
ProxyKind classifyD3D9(const fs::path& dll){
    std::error_code ec; if(!fs::exists(dll,ec)) return ProxyKind::None;
    std::ifstream f(dll,std::ios::binary); if(!f) return ProxyKind::Unreadable;
    std::string d((std::istreambuf_iterator<char>(f)),std::istreambuf_iterator<char>());
    if(contains(d,"PONEGLYPH_ENGINE_ID_7A3F")) return ProxyKind::Poneglyph;
    if(contains(d,"NovaFix")||contains(d,"NOVAFIX")) return ProxyKind::NovaFix;
    if(contains(d,"dxvk")||contains(d,"DXVK")) return ProxyKind::Dxvk;
    return ProxyKind::Other;
}
static void ensureDirs(const ProfilePaths& pp){
    std::error_code ec; for(auto& d:{pp.root,pp.addons,pp.logs,pp.backups}) fs::create_directories(d,ec);
}
void writeDefaultConfig(const ProfilePaths& pp,GameId game){
    std::error_code ec; if(fs::exists(pp.config,ec)) return;
    IniFile c; auto gi=gameInfo(game);
    c.set("General","Game",gi?gi->key:"unknown"); c.setBool("General","Enabled",true); c.set("General","LogLevel","info");
    c.setBool("Addons","LoadAddons",true);
    c.setBool("Diagnostics","FrameStats",false); c.setInt("Diagnostics","SummarySeconds",10);
    c.save(pp.config);
}
static IniFile loadManifest(const ProfilePaths& pp){ IniFile m; m.load(pp.backups/"manifest.ini"); return m; }
static bool copyVerified(const fs::path& a,const fs::path& b,std::string& hashOut){
    std::error_code ec; fs::create_directories(b.parent_path(),ec);
    fs::copy_file(a,b,fs::copy_options::overwrite_existing,ec); if(ec) return false;
    std::string h1,h2; if(!Sha256::ofFile(a,h1)||!Sha256::ofFile(b,h2)||h1!=h2) return false; hashOut=h1; return true;
}
OpResult installEngine(const ProfilePaths& pp,GameId game,const fs::path& engine,const InstallOptions& opt,Logger& log){
    OpResult r; std::error_code ec;
    // 1 identify / 2 verify target
    auto gi=gameInfo(game); if(!gi){ r.message="Unknown game profile."; return r; }
    if(!fs::is_directory(pp.gameDir,ec)){ r.message="The game folder does not exist."; return r; }
    // the exe must live in this folder - never install into an arbitrary directory
    if(!fs::is_regular_file(pp.gameDir/gi->exeName,ec)){ r.message=std::string("This folder does not contain ")+gi->exeName+". Choose the folder that holds the game's executable."; return r; }
    PeInfo ep=inspectPe(engine,{true,false,false});
    if(!ep.valid||!ep.isX86()||!ep.isDll){ r.message="The engine file is not a valid 32-bit DLL. Install aborted."; return r; }
    bool hasCreate=false; for(auto& e:ep.exports) if(e.name=="Direct3DCreate9") hasCreate=true;
    if(!hasCreate){ r.message="The engine DLL does not export Direct3DCreate9. Install aborted."; return r; }
    ensureDirs(pp); log.open(pp.logs/"manager.log"); 
    fs::path target=pp.gameDir/"d3d9.dll";
    // 3 existing?
    ProxyKind k=classifyD3D9(target);
    log.log(LogLevel::Info,"installEngine: existing d3d9.dll kind=%s",proxyKindText(k));
    if((k==ProxyKind::NovaFix||k==ProxyKind::Dxvk||k==ProxyKind::Other)&&!opt.replaceForeignProxy){
        r.message=std::string("A ")+proxyKindText(k)+" d3d9.dll is already installed in this game folder. Two Direct3D 9 proxies cannot run together. "
                  "Poneglyph did NOT change anything. If you want to switch, choose 'Replace with backup' - your current file will be saved and can be restored."; return r; }
    if(k==ProxyKind::Unreadable){ r.message="The existing d3d9.dll could not be read. Nothing changed."; return r; }
    // 4 backup
    IniFile man=loadManifest(pp);
    if(k!=ProxyKind::None&&k!=ProxyKind::Poneglyph){
        std::string h; fs::path bak=pp.backups/("d3d9.dll."+nowStampCompact()+".pgbak");
        if(!copyVerified(target,bak,h)){ r.message="Could not back up the existing d3d9.dll. Nothing changed."; return r; }
        man.set("Engine","PreviousBackup",pathToUtf8(bak.filename())); man.set("Engine","PreviousSha256",h); man.set("Engine","PreviousKind",proxyKindText(k));
        r.changed.push_back("Backed up existing d3d9.dll -> Poneglyph/Backups/"+pathToUtf8(bak.filename()));
        log.log(LogLevel::Info,"backup created %s sha=%s",pathToUtf8(bak).c_str(),h.c_str());
    }
    // 6 change via temp file, 7 verify, 8 rollback on failure
    fs::path tmp=pp.gameDir/"d3d9.dll.pgtmp"; std::string newHash;
    if(!copyVerified(engine,tmp,newHash)){ fs::remove(tmp,ec); r.message="Copy of the engine failed verification. Nothing changed."; return r; }
    fs::remove(target,ec); fs::rename(tmp,target,ec);
    if(ec){ fs::remove(tmp,ec);
        // rollback
        if(man.has("Engine","PreviousBackup")){ fs::copy_file(pp.backups/pathFromUtf8(man.get("Engine","PreviousBackup")),target,fs::copy_options::overwrite_existing,ec); }
        r.message="Could not place d3d9.dll (is the game running?). Rolled back."; log.log(LogLevel::Error,"install failed; rolled back"); return r; }
    std::string fin; Sha256::ofFile(target,fin);
    if(fin!=newHash||classifyD3D9(target)!=ProxyKind::Poneglyph){
        fs::remove(target,ec); if(man.has("Engine","PreviousBackup")) fs::copy_file(pp.backups/pathFromUtf8(man.get("Engine","PreviousBackup")),target,fs::copy_options::overwrite_existing,ec);
        r.message="Verification after install failed. Rolled back to the previous state."; log.log(LogLevel::Error,"post-install verify failed"); return r; }
    man.set("Engine","InstalledSha256",newHash); man.set("Engine","InstalledAt",nowStamp()); man.set("Engine","Game",gi->key);
    man.save(pp.backups/"manifest.ini"); writeDefaultConfig(pp,game);
    r.ok=true; r.changed.push_back("Installed Poneglyph d3d9.dll (SHA-256 "+newHash.substr(0,16)+"...)"); r.message="Poneglyph engine installed and verified."; log.log(LogLevel::Info,"engine installed %s",newHash.c_str()); return r;
}
OpResult uninstallEngine(const ProfilePaths& pp,Logger& log){
    OpResult r; std::error_code ec; fs::path target=pp.gameDir/"d3d9.dll"; log.open(pp.logs/"manager.log");
    ProxyKind k=classifyD3D9(target);
    if(k==ProxyKind::None){ r.ok=true; r.message="No d3d9.dll present. Nothing to do."; return r; }
    if(k!=ProxyKind::Poneglyph){ r.message=std::string("The d3d9.dll here is a ")+proxyKindText(k)+" file, not Poneglyph's. It was NOT touched."; return r; }
    IniFile man=loadManifest(pp); std::string prev=man.get("Engine","PreviousBackup"), prevHash=man.get("Engine","PreviousSha256");
    fs::remove(target,ec); if(ec){ r.message="Could not remove d3d9.dll (is the game running?)."; return r; }
    r.changed.push_back("Removed Poneglyph d3d9.dll");
    if(!prev.empty()){
        fs::path b=pp.backups/pathFromUtf8(prev); std::string h; 
        if(fs::exists(b,ec)&&Sha256::ofFile(b,h)&&h==prevHash){ fs::copy_file(b,target,fs::copy_options::overwrite_existing,ec); if(!ec){ r.changed.push_back("Restored your previous d3d9.dll"); } else { r.message="Removed Poneglyph, but restoring the previous file failed. The backup is still in Poneglyph/Backups."; return r; } }
        else { r.message="Removed Poneglyph, but the backup failed its hash check, so it was NOT restored. It is still in Poneglyph/Backups."; return r; }
    }
    man.remove("Engine","InstalledSha256"); man.save(pp.backups/"manifest.ini");
    r.ok=true; r.message="Poneglyph engine removed. Your add-ons, settings and saves were not deleted."; log.log(LogLevel::Info,"engine uninstalled"); return r;
}
OpResult installAddonZip(const ProfilePaths& pp,const fs::path& zipPath,Logger& log){
    OpResult r; ensureDirs(pp); log.open(pp.logs/"manager.log");
    ZipArchive z; std::string err; if(!z.open(zipPath,err)){ r.message="Cannot read the archive: "+err; return r; }
    std::string pkg=pathToUtf8(zipPath.stem()); for(auto& c:pkg) if(!(isalnum((unsigned char)c)||c=='-'||c=='_'||c=='.'||c==' ')) c='_';
    if(pkg.empty()) pkg="package";
    fs::path dest=pp.addons/pathFromUtf8(pkg); std::error_code ec;
    if(fs::exists(dest,ec)){ r.message="A package named '"+pkg+"' is already installed. Remove it first or rename the ZIP."; return r; }
    std::vector<std::string> written;
    if(!z.extractAll(dest,written,err,false)){ r.message=err; log.log(LogLevel::Warn,"zip rejected: %s",err.c_str()); return r; }
    IniFile inst; inst.load(pp.root/"installed.ini"); int n=0;
    for(auto& w:written){ std::string h; Sha256::ofFile(dest/pathFromUtf8(w),h); inst.set("Package."+pkg,"File."+std::to_string(n),pkg+"/"+w); inst.set("Package."+pkg,"Hash."+std::to_string(n),h); n++; }
    inst.setInt("Package."+pkg,"Count",n); inst.set("Package."+pkg,"Installed",nowStamp()); inst.save(pp.root/"installed.ini");
    r.ok=true; r.message="Installed "+std::to_string(n)+" file(s) into Poneglyph/Addons/"+pkg; r.changed=written; log.log(LogLevel::Info,"package %s installed (%d files)",pkg.c_str(),n); return r;
}
std::vector<std::string> listAddonPackages(const ProfilePaths& pp){
    IniFile inst; inst.load(pp.root/"installed.ini"); std::vector<std::string> r;
    for(auto& s:inst.sections()) if(s.rfind("Package.",0)==0) r.push_back(s.substr(8)); return r;
}
OpResult uninstallAddonPackage(const ProfilePaths& pp,const std::string& pkg,Logger& log){
    OpResult r; IniFile inst; inst.load(pp.root/"installed.ini"); std::string sec="Package."+pkg;
    if(!inst.has(sec,"Count")){ r.message="That package was not installed by Poneglyph, so it will not be removed automatically."; return r; }
    int n=inst.getInt(sec,"Count",0); std::error_code ec; int removed=0,kept=0;
    for(int i=0;i<n;i++){
        fs::path f=pp.addons/pathFromUtf8(inst.get(sec,"File."+std::to_string(i))); 
        if(!isInside(pp.addons,f)) continue;
        std::string h; if(!fs::exists(f,ec)) continue;
        Sha256::ofFile(f,h);
        if(h==inst.get(sec,"Hash."+std::to_string(i))){ fs::remove(f,ec); removed++; } else { kept++; r.changed.push_back("Kept (modified by you): "+pathToUtf8(f.filename())); }
    }
    // remove now-empty folders inside the package dir only
    fs::path pd=pp.addons/pathFromUtf8(pkg);
    if(isInside(pp.addons,pd)&&fs::exists(pd,ec)){ std::vector<fs::path> dirs; for(fs::recursive_directory_iterator it(pd,ec),e;it!=e;it.increment(ec)) if(it->is_directory()) dirs.push_back(it->path());
        std::sort(dirs.begin(),dirs.end(),[](const fs::path&a,const fs::path&b){return a.string().size()>b.string().size();}); for(auto& d:dirs) fs::remove(d,ec); fs::remove(pd,ec); }
    for(auto& k:inst.entries(sec)) inst.remove(sec,k.first); inst.save(pp.root/"installed.ini");
    log.log(LogLevel::Info,"package %s removed=%d kept=%d",pkg.c_str(),removed,kept);
    r.ok=true; r.message="Removed "+std::to_string(removed)+" file(s)."+(kept?(" "+std::to_string(kept)+" file(s) you modified were left in place."):""); return r;
}
OpResult setAddonEnabled(const ProfilePaths& pp,const std::string& key,bool en,Logger& log){
    OpResult r; IniFile c; c.load(pp.config); c.setBool("Addons",addonConfigKey(key),en);
    if(!c.save(pp.config)){ r.message="Could not write the configuration."; return r; }
    log.log(LogLevel::Info,"addon %s -> %s",key.c_str(),en?"enabled":"disabled"); r.ok=true; r.message=en?"Enabled (takes effect next game start).":"Disabled. The file was not deleted."; return r;
}
}
