// pg_proxytest - Windows x86 runtime tests for the engine (Master Plan stages B and C).
// usage: pg_proxytest <path to built d3d9.dll> <path to test_addon.asi>
//        pg_proxytest --child <dir>      (internal: loads <dir>\d3d9.dll and creates a Direct3D 9 object)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <psapi.h>
#include <cstdio>
#include <string>
#include <vector>
#include <filesystem>
#include <fstream>
#include "poneglyph/pe.hpp"
#include "poneglyph/ini.hpp"
#include "poneglyph/addons.hpp"
#include "poneglyph/util.hpp"
using namespace pg;
static int g_fail=0;
static void check(bool c,const std::string& m){ std::printf("  [%s] %s\n",c?"PASS":"FAIL",m.c_str()); if(!c) g_fail++; }
typedef void*(WINAPI* PFN_Create9)(UINT);
static int child(const fs::path& dir){
    HMODULE h=LoadLibraryW((dir/"d3d9.dll").wstring().c_str()); if(!h) return 10;
    auto f=(PFN_Create9)GetProcAddress(h,"Direct3DCreate9"); if(!f) return 11;
    void* d=f(32); if(!d) return 12; typedef ULONG(STDMETHODCALLTYPE* Rel)(void*); void** vt=*(void***)d; ((Rel)vt[2])(d); return 0;
}
static int countD3D9Modules(std::vector<std::wstring>& paths){
    HMODULE mods[1024]; DWORD need=0; int n=0; if(!EnumProcessModules(GetCurrentProcess(),mods,sizeof mods,&need)) return -1;
    for(DWORD i=0;i<need/sizeof(HMODULE);i++){ wchar_t p[MAX_PATH]; if(GetModuleFileNameW(mods[i],p,MAX_PATH)){ std::wstring s=p; auto sl=s.find_last_of(L'\\'); if(_wcsicmp(s.substr(sl+1).c_str(),L"d3d9.dll")==0){ n++; paths.push_back(s); } } }
    return n;
}
int wmain(int argc,wchar_t** argv){
    if(argc>=3&&std::wstring(argv[1])==L"--child") return child(argv[2]);
    if(argc<3){ std::printf("usage: pg_proxytest <d3d9.dll> <test_addon.asi>\n"); return 64; }
    fs::path proxy=argv[1], addon=argv[2]; std::error_code ec;
    wchar_t sys[MAX_PATH]; GetSystemDirectoryW(sys,MAX_PATH); fs::path sysd3d9=fs::path(sys)/L"d3d9.dll";
    fs::path dir=fs::temp_directory_path()/("pg_proxytest_"+std::to_string(GetCurrentProcessId())); fs::remove_all(dir,ec); auto pp=ProfilePaths::forGameDir(dir); fs::create_directories(pp.addons,ec);
    std::printf("== Stage B: proxy ==\n");
    auto pi=inspectPe(proxy,{true,true,false}); auto si=inspectPe(sysd3d9,{true,false,false});
    check(pi.valid&&pi.isX86(),"proxy is x86"); check(si.valid&&si.isX86(),"system d3d9.dll (SysWOW64 view) is x86: "+pathToUtf8(sysd3d9));
    fs::copy_file(proxy,dir/"d3d9.dll",ec); fs::copy_file(addon,pp.addons/"test_addon.asi",ec); { std::ofstream m(pp.addons/"test_addon.asi.pgmeta"); m<<"[Addon]\nName=Test\nGames=all\nArch=x86\n"; }
    { std::ofstream j(pp.addons/"bad.asi",std::ios::binary); j<<"this is not a dll"; }
    // load proxy by full path
    HMODULE hp=LoadLibraryW((dir/"d3d9.dll").wstring().c_str()); check(hp!=nullptr,"proxy loads by full path");
    if(hp){
        // named + ordinal exports resolve in the proxy exactly as in the system DLL
        const char* names[]={"Direct3DCreate9","Direct3DCreate9Ex","D3DPERF_BeginEvent","D3DPERF_EndEvent","D3DPERF_SetMarker","D3DPERF_SetRegion","D3DPERF_QueryRepeatFrame","D3DPERF_GetStatus","D3DPERF_SetOptions"};
        for(auto n:names){ bool inSys=false; unsigned ord=0; for(auto& e:si.exports) if(e.name==n){ inSys=true; ord=e.ordinal; } if(!inSys){ std::printf("  [skip] %s not present in this Windows' d3d9.dll\n",n); continue; }
            check(GetProcAddress(hp,n)!=nullptr,std::string("named export resolves: ")+n); check(GetProcAddress(hp,MAKEINTRESOURCEA(ord))!=nullptr,std::string("ordinal ")+std::to_string(ord)+" resolves ("+n+")"); }
        auto f=(PFN_Create9)GetProcAddress(hp,"Direct3DCreate9"); void* d=f?f(32):nullptr; check(d!=nullptr,"Direct3DCreate9(32) reaches the genuine DLL and returns an object");
        if(d){ typedef ULONG(STDMETHODCALLTYPE* Rel)(void*); void** vt=*(void***)d; ((Rel)vt[2])(d); }
        std::vector<std::wstring> mods; int n=countD3D9Modules(mods); check(n==2,"exactly two d3d9.dll modules loaded (proxy + genuine) - no recursion; found "+std::to_string(n));
        bool realByFullPath=false; for(auto& p:mods) if(_wcsicmp(p.c_str(),sysd3d9.wstring().c_str())==0) realByFullPath=true; check(realByFullPath,"genuine d3d9.dll was loaded from the full system path");
    }
    std::printf("== Stage C: add-on runtime ==\n");
    IniFile st; st.load(pp.status); auto recs=readLoadStatus(pp.status);
    bool loaded=false,skipped=false; for(auto& r:recs){ if(r.file=="test_addon.asi"&&r.result=="Loaded") loaded=true; if(r.file=="bad.asi"&&r.result=="Skipped") skipped=true; }
    check(loaded,"valid x86 add-on loaded (status file)"); check(skipped,"invalid add-on refused (status file)"); check(fs::exists(pp.addons/"init_called.txt"),"Poneglyph_AddonInit was called");
    check(!fs::exists(pp.logs/"loading.marker"),"no stale loading marker after a clean start"); check(fs::exists(pp.logs/"poneglyph.log"),"log file written");
    // crash-recovery: simulate a start that died while loading the add-on
    std::printf("== crash recovery (second process) ==\n");
    { std::ofstream m(pp.logs/"loading.marker"); m<<"test_addon.asi"; } fs::remove(pp.addons/"init_called.txt",ec);
    std::wstring cmd=L"\""+std::wstring(argv[0])+L"\" --child \""+dir.wstring()+L"\""; STARTUPINFOW s{}; s.cb=sizeof s; PROCESS_INFORMATION p{};
    if(CreateProcessW(nullptr,&cmd[0],nullptr,nullptr,FALSE,0,nullptr,nullptr,&s,&p)){ WaitForSingleObject(p.hProcess,30000); DWORD ec2=99; GetExitCodeProcess(p.hProcess,&ec2); CloseHandle(p.hProcess); CloseHandle(p.hThread); check(ec2==0,"child process started and exited cleanly (exit "+std::to_string(ec2)+")"); } else check(false,"could not start child");
    IniFile cfg; cfg.load(pp.config); check(!cfg.getBool("Addons","test_addon.asi",true),"add-on that never finished loading was disabled automatically"); check(!fs::exists(pp.addons/"init_called.txt"),"disabled add-on was not initialised again"); check(fs::exists(pp.addons/"test_addon.asi"),"add-on file was kept (not deleted)");
    std::printf("\n%s (%d failed)\n",g_fail?"RESULT: FAIL":"RESULT: PASS",g_fail); return g_fail?1:0;
}
