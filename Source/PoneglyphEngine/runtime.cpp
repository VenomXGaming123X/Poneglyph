// Poneglyph Engine - runtime: configuration, crash recovery, deterministic add-on loading, status output.
#include "engine.h"
#include "poneglyph/addons.hpp"
#include "poneglyph/util.hpp"
#include "poneglyph_sdk.h"
#include <mutex>
namespace pgengine {
HMODULE g_self=nullptr; pg::Logger g_log;
const char kEngineId[]="PONEGLYPH_ENGINE_ID_7A3F";
static std::once_flag g_init; static bool g_enabled=true, g_frameStats=false; static int g_summarySecs=10;
static pg::ProfilePaths g_pp; static std::string g_gameKey="unknown"; static std::wstring g_gameDirW;
bool FrameStatsEnabled(){ return g_enabled&&g_frameStats; }

// ---- SEH-protected primitives: no C++ objects with destructors in these functions (MSVC C2712).
static HMODULE SafeLoad(const wchar_t* path,DWORD* err,DWORD* exc){
    HMODULE h=nullptr; *err=0; *exc=0;
    __try{ h=LoadLibraryExW(path,nullptr,LOAD_WITH_ALTERED_SEARCH_PATH); if(!h) *err=GetLastError(); }
    __except(*exc=GetExceptionCode(),EXCEPTION_EXECUTE_HANDLER){ h=nullptr; }
    return h;
}
static unsigned long SafeInit(FARPROC fn,const PoneglyphHostApi* api,DWORD* exc){
    unsigned long r=0; *exc=0;
    __try{ r=((PFN_Poneglyph_AddonInit)fn)(api); } __except(*exc=GetExceptionCode(),EXCEPTION_EXECUTE_HANDLER){ r=0xFFFFFFFFul; }
    return r;
}
static void __stdcall HostLog(const char* addon,int level,const char* msg){
    pg::LogLevel l=level<=0?pg::LogLevel::Error:level==1?pg::LogLevel::Warn:level==2?pg::LogLevel::Info:pg::LogLevel::Debug;
    g_log.log(l,"[addon %s] %s",addon?addon:"?",msg?msg:"");
}
static std::wstring widen(const std::string& s){
    if(s.empty()) return L""; int n=MultiByteToWideChar(CP_UTF8,0,s.data(),(int)s.size(),nullptr,0); std::wstring w((size_t)n,L'\0');
    MultiByteToWideChar(CP_UTF8,0,s.data(),(int)s.size(),&w[0],n); return w;
}
static double msSince(const LARGE_INTEGER& a){ LARGE_INTEGER b,f; QueryPerformanceCounter(&b); QueryPerformanceFrequency(&f); return (double)(b.QuadPart-a.QuadPart)*1000.0/(double)f.QuadPart; }

static void LoadAddons(){
    pg::IniFile cfg; cfg.load(g_pp.config);
    if(!cfg.getBool("Addons","LoadAddons",true)){ g_log.log(pg::LogLevel::Info,"Add-on loading is turned off in poneglyph.ini."); return; }
    pg::ScanOptions so; so.game=pg::gameByKey(g_gameKey)?pg::gameByKey(g_gameKey)->id:pg::GameId::Unknown; so.requireX86=true; so.computeHash=false;
    auto all=pg::scanAddons(g_pp.addons,cfg,so);
    std::vector<pg::AddonLoadRecord> recs;
    g_log.log(pg::LogLevel::Info,"Add-on scan: %u file(s) found in %s",(unsigned)all.size(),pg::pathToUtf8(g_pp.addons).c_str());
    for(auto& e:all){ if(e.status==pg::AddonStatus::Ready) continue;
        g_log.log(e.status==pg::AddonStatus::Disabled?pg::LogLevel::Info:pg::LogLevel::Warn,"SKIP %s : %s - %s",e.rel.c_str(),pg::addonStatusText(e.status),e.reason.c_str());
        recs.push_back({e.key,e.rel,e.status==pg::AddonStatus::Disabled?"Disabled":"Skipped",std::string(pg::addonStatusText(e.status))+": "+e.reason,0}); }
    PoneglyphHostApi api{}; api.size=sizeof api; api.apiVersion=PONEGLYPH_API_VERSION; api.engineVersion=PG_ENGINE_VERSION; api.gameKey=g_gameKey.c_str(); api.gameDir=g_gameDirW.c_str(); api.Log=HostLog;
    for(auto* e:pg::loadList(all)){
        // marker: if the process dies while this add-on loads, the next start disables it (recovery path)
        pg::writeFileAtomic(g_pp.logs/"loading.marker",e->key);
        std::wstring wp=e->path.wstring(); LARGE_INTEGER t0; QueryPerformanceCounter(&t0); DWORD err=0,exc=0;
        HMODULE h=SafeLoad(wp.c_str(),&err,&exc); double ms=msSince(t0);
        if(!h){ char d[160]; if(exc) snprintf(d,sizeof d,"exception 0x%08lX during load",exc); else snprintf(d,sizeof d,"LoadLibrary failed, Windows error %lu%s",err,err==193?" (not a valid 32-bit DLL)":err==126?" (a dependency DLL is missing)":"");
            g_log.log(pg::LogLevel::Error,"FAIL %s : %s",e->rel.c_str(),d); recs.push_back({e->key,e->rel,"Failed",d,ms}); std::error_code ec; std::filesystem::remove(g_pp.logs/"loading.marker",ec); continue; }
        std::string detail; FARPROC init=GetProcAddress(h,"Poneglyph_AddonInit"); if(!init) init=GetProcAddress(h,"Poneglyph_AddonInit@4"); if(!init) init=GetProcAddress(h,"_Poneglyph_AddonInit@4"); /* 32-bit __stdcall exports are decorated */ std::string result="Loaded";
        if(init){ DWORD ex2=0; unsigned long rc=SafeInit(init,&api,&ex2); if(ex2){ result="Failed"; detail="exception in Poneglyph_AddonInit"; } else if(rc!=0){ result="Failed"; char d[64]; snprintf(d,sizeof d,"Poneglyph_AddonInit returned %lu",rc); detail=d; } else detail="initialised via SDK"; }
        ms=msSince(t0);
        g_log.log(result=="Loaded"?pg::LogLevel::Info:pg::LogLevel::Error,"%s %s in %.2f ms %s",result=="Loaded"?"LOAD":"FAIL",e->rel.c_str(),ms,detail.c_str());
        recs.push_back({e->key,e->rel,result,detail,ms}); std::error_code ec; std::filesystem::remove(g_pp.logs/"loading.marker",ec);
    }
    pg::writeLoadStatus(g_pp.status,recs,g_gameKey,PG_ENGINE_VERSION);
}
void InitializeOnce(){
    std::call_once(g_init,[]{
        wchar_t me[MAX_PATH]; DWORD n=GetModuleFileNameW(g_self,me,MAX_PATH); if(n==0||n>=MAX_PATH) return;
        std::filesystem::path dir=std::filesystem::path(me).parent_path(); g_pp=pg::ProfilePaths::forGameDir(dir); g_gameDirW=dir.wstring();
        std::error_code ec; std::filesystem::create_directories(g_pp.logs,ec); std::filesystem::create_directories(g_pp.addons,ec);
        pg::IniFile cfg; cfg.load(g_pp.config);
        auto lv=pg::toLower(cfg.get("General","LogLevel","info")); pg::LogLevel L=lv=="debug"?pg::LogLevel::Debug:lv=="warn"?pg::LogLevel::Warn:lv=="error"?pg::LogLevel::Error:pg::LogLevel::Info;
        g_log.open(g_pp.logs/"poneglyph.log",512*1024,3,L);
        wchar_t exe[MAX_PATH]; GetModuleFileNameW(nullptr,exe,MAX_PATH); std::string exeName=pg::pathToUtf8(std::filesystem::path(exe).filename());
        auto gid=pg::identifyByExeName(exeName); if(gid!=pg::GameId::Unknown) g_gameKey=pg::gameInfo(gid)->key; else if(cfg.has("General","Game")) g_gameKey=cfg.get("General","Game");
        g_log.log(pg::LogLevel::Info,"==== Poneglyph Engine %s (%s) starting in %s, game profile '%s' ====",PG_ENGINE_VERSION,kEngineId,exeName.c_str(),g_gameKey.c_str());
        if(gid!=pg::GameId::Unknown){ auto info=pg::gameInfo(gid); if(!pg::iequals(pg::pathToUtf8(std::filesystem::path(exe).filename()),info->exeName)) g_log.log(pg::LogLevel::Warn,"unexpected exe name"); }
        else g_log.log(pg::LogLevel::Warn,"The host executable '%s' is not one of the three supported games. Forwarding still works; add-ons are loaded as configured.",exeName.c_str());
        g_enabled=cfg.getBool("General","Enabled",true); g_frameStats=cfg.getBool("Diagnostics","FrameStats",false); g_summarySecs=cfg.getInt("Diagnostics","SummarySeconds",10); if(g_summarySecs<2) g_summarySecs=2; if(g_summarySecs>300) g_summarySecs=300;
        if(!EnsureRealD3D9()){ g_log.log(pg::LogLevel::Error,"Genuine Direct3D 9 DLL unavailable. Add-ons will NOT be loaded (safe mode)."); return; }
        if(!g_enabled){ g_log.log(pg::LogLevel::Warn,"Poneglyph is disabled in poneglyph.ini (General/Enabled=0): pure pass-through."); return; }
        std::string msg; auto rec=pg::applyCrashRecovery(g_pp.logs/"loading.marker",g_pp.config,msg); if(!rec.empty()) g_log.log(pg::LogLevel::Warn,"RECOVERY: %s",msg.c_str());
        LoadAddons();
        if(g_frameStats) g_log.log(pg::LogLevel::Info,"Frame statistics ON (summary every %d s).",g_summarySecs);
    });
}
void OnD3D9ObjectCreated(void* d3d,bool isEx){ if(FrameStatsEnabled()){ InstallD3DHooks(d3d,isEx); StartFrameSampler((g_pp.logs/"framestats.csv").c_str(),g_summarySecs); } }
void OnShutdown(bool terminating){ if(g_frameStats) StopFrameSampler(!terminating); g_log.log(pg::LogLevel::Info,"==== Poneglyph Engine shutdown (%s) ====",terminating?"process exit":"unload"); g_log.close(); }
}
