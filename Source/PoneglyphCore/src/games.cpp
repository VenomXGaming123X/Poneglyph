#include "poneglyph/games.hpp"
#include "poneglyph/util.hpp"
namespace pg {
const std::vector<GameInfo>& allGames(){
    static const std::vector<GameInfo> g={
        {GameId::FFXIII ,"ffxiii" ,"FINAL FANTASY XIII"               ,"ffxiiiimg.exe" ,"white_data\\prog\\win\\bin\\ffxiiiimg.exe" ,"FINAL FANTASY XIII"},
        {GameId::FFXIII2,"ffxiii2","FINAL FANTASY XIII-2"             ,"ffxiii2img.exe","alba_data\\prog\\win\\bin\\ffxiii2img.exe" ,"FINAL FANTASY XIII-2"},
        {GameId::LR     ,"lr13"   ,"LIGHTNING RETURNS: FINAL FANTASY XIII","LRFF13.exe","LRFF13.exe"                                ,"LIGHTNING RETURNS FINAL FANTASY XIII"}};
    return g;
}
const GameInfo* gameInfo(GameId id){ for(auto& g:allGames()) if(g.id==id) return &g; return nullptr; }
const GameInfo* gameByKey(const std::string& k){ for(auto& g:allGames()) if(iequals(g.key,k)) return &g; return nullptr; }
GameId identifyByExeName(const std::string& n){ for(auto& g:allGames()) if(iequals(g.exeName,n)) return g.id; return GameId::Unknown; }
std::filesystem::path findGameExe(const fs::path& root,GameId id,int maxDepth){
    auto gi=gameInfo(id); if(!gi) return {};
    std::error_code ec; if(!fs::is_directory(root,ec)) return {};
    fs::path direct=root/pathFromUtf8(std::string(gi->commonRelPath)); 
    // allow either separator style in commonRelPath
    { std::string s=gi->commonRelPath; for(auto& c:s) if(c=='\\') c='/'; fs::path d2=root/pathFromUtf8(s); if(fs::is_regular_file(d2,ec)) return d2; }
    if(fs::is_regular_file(direct,ec)) return direct;
    fs::recursive_directory_iterator it(root,fs::directory_options::skip_permission_denied,ec),end;
    for(;!ec&&it!=end;it.increment(ec)){
        if(it.depth()>maxDepth){ it.disable_recursion_pending(); continue; }
        std::error_code e2; if(!it->is_regular_file(e2)) continue;
        if(iequals(pathToUtf8(it->path().filename()),gi->exeName)) return it->path();
    }
    return {};
}
ExeCheck verifyGameExe(const fs::path& exe,GameId expected){
    ExeCheck c; auto gi=gameInfo(expected);
    std::error_code ec;
    if(!gi){ c.message="Unknown game profile."; return c; }
    if(!fs::is_regular_file(exe,ec)){ c.message="The file does not exist: "+pathToUtf8(exe); return c; }
    c.id=identifyByExeName(pathToUtf8(exe.filename()));
    if(c.id!=expected){ c.message=std::string("This file is not ")+gi->exeName+". Poneglyph will not apply a "+gi->displayName+" profile to a different executable."; return c; }
    c.pe=inspectPe(exe,{false,false,false});
    if(!c.pe.valid){ c.message="The executable could not be verified: "+c.pe.error; return c; }
    if(!c.pe.isX86()||c.pe.isDll){ c.message="Expected a 32-bit (x86) game executable but found: "+c.pe.archName()+"."; return c; }
    c.ok=true; c.message="Verified: "+std::string(gi->exeName)+" ("+c.pe.archName()+", "+(c.pe.laa?"4 GB flag ON":"4 GB flag OFF")+")."; return c;
}
ProfilePaths ProfilePaths::forGameDir(const fs::path& d){
    ProfilePaths p; p.gameDir=d; p.root=d/".Poneglyph"; p.legacyRoot=d/"Poneglyph"; p.addons=d/".Poneglyph-Addons"; p.logs=p.root/"Logs"; p.backups=p.root/"Backups";
    p.config=p.root/"poneglyph.ini"; p.status=p.logs/"addon_status.ini"; return p;
}
}
