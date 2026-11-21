// Poneglyph Manager v1.0 - Windows desktop application (64-bit). Separate from the game process.
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <commdlg.h>
#include <shellapi.h>
#include <shlobj.h>
#include <psapi.h>
#include <tlhelp32.h>
#include <uxtheme.h>
#include <functional>
#include <thread>
#include <mutex>
#include <map>
#include <fstream>
#include <algorithm>
#include "theme.hpp"
#include "resource.h"
#include "poneglyph/util.hpp"
#include "poneglyph/ini.hpp"
#include "poneglyph/log.hpp"
#include "poneglyph/games.hpp"
#include "poneglyph/addons.hpp"
#include "poneglyph/ops.hpp"
#include "poneglyph/report.hpp"
#include "poneglyph/sha256.hpp"
#define PG_MANAGER_VERSION "1.0.0"
using namespace pg;

// ------------------------------------------------------------------ small helpers
static std::wstring W(const std::string& s){ if(s.empty()) return L""; int n=MultiByteToWideChar(CP_UTF8,0,s.data(),(int)s.size(),nullptr,0); std::wstring w((size_t)n,L'\0'); MultiByteToWideChar(CP_UTF8,0,s.data(),(int)s.size(),&w[0],n); return w; }
static std::string U(const std::wstring& w){ if(w.empty()) return ""; int n=WideCharToMultiByte(CP_UTF8,0,w.data(),(int)w.size(),nullptr,0,nullptr,nullptr); std::string s((size_t)n,'\0'); WideCharToMultiByte(CP_UTF8,0,w.data(),(int)w.size(),&s[0],n,nullptr,nullptr); return s; }
static std::wstring Crlf(const std::string& s){ std::wstring w=W(s),o; for(size_t i=0;i<w.size();i++){ if(w[i]==L'\n'&&(i==0||w[i-1]!=L'\r')) o+=L"\r\n"; else o+=w[i]; } return o; }

// ------------------------------------------------------------------ state
enum { PG_HOME,PG_ADDONS,PG_PERF,PG_MEM,PG_BACKUPS,PG_SETTINGS,PG_HELP,PG_COUNT };
static const wchar_t* kPageNames[PG_COUNT]={L"Home",L"Add-ons",L"Performance",L"Memory (4 GB)",L"Backups",L"Settings",L"Help"};
enum { ID_NAV=100, ID_PROFILE=110, ID_APPBTN=200 };
enum { B_CHOOSE_EXE=200,B_SEARCH_DIR,B_INSTALL_ENGINE,B_REMOVE_ENGINE,B_LAUNCH,B_OPEN_GAME,
       B_SCAN,B_ENABLE,B_DISABLE,B_INSTALL_ZIP,B_REMOVE_PKG,B_OPEN_ADDONS,
       B_MON_START,B_MON_STOP,B_EXPORT_CSV,B_REPORT,B_SAVE_REPORT,B_COMPARE,B_LAUNCH_MON,
       B_LAA_CHECK,B_LAA_ENABLE,B_LAA_RESTORE,
       B_BK_RESTORE,B_BK_OPEN,
       T_ENABLED,T_LOADADDONS,T_FRAMESTATS,C_SUMMARY,C_LOGLEVEL,C_THEME,B_SAVE_SETTINGS,
       B_README,B_OPEN_LOGS,B_SUPPORT };
#define WM_APP_DONE (WM_APP+1)
struct Page { std::vector<HWND> buttons; HWND main=nullptr, second=nullptr; };
static HINSTANCE g_inst; static HWND g_hwnd, g_status, g_progress; static Page g_pages[PG_COUNT];
static int g_page=PG_HOME, g_theme=0, g_cur=0, g_dpi=96; static HFONT g_fBody,g_fBold,g_fTitle,g_fSmall;
static HBRUSH g_brBg,g_brSide,g_brEdit; static std::map<int,bool> g_toggle; static int g_summarySecs=10, g_logLevel=2;
static Logger g_mlog, g_oplog; static std::mutex g_mx; static bool g_busy=false;
struct Profile { GameId id; fs::path exe; };
static Profile g_prof[3]={{GameId::FFXIII,{}},{GameId::FFXIII2,{}},{GameId::LR,{}}};
static std::vector<AddonEntry> g_scan; static std::vector<ProcSample> g_samples;
struct Mon { DWORD pid=0; HANDLE h=nullptr; ULONGLONG lastCpu=0, lastTick=0; bool on=false; } g_mon;
static const Theme& T(){ return kThemes[g_theme]; }
static int S(int v){ return MulDiv(v,g_dpi,96); }
static fs::path appDataDir(){ wchar_t p[MAX_PATH]; if(SUCCEEDED(SHGetFolderPathW(nullptr,CSIDL_APPDATA,nullptr,0,p))) return fs::path(p)/L"Poneglyph"; return fs::temp_directory_path()/L"Poneglyph"; }
static fs::path exeDir(){ wchar_t p[MAX_PATH]; GetModuleFileNameW(nullptr,p,MAX_PATH); return fs::path(p).parent_path(); }
static const GameInfo* curGame(){ return gameInfo(g_prof[g_cur].id); }
static bool haveExe(){ return !g_prof[g_cur].exe.empty(); }
static ProfilePaths curPaths(){ return ProfilePaths::forGameDir(g_prof[g_cur].exe.parent_path()); }
static void Msg(const std::string& title,const std::string& body,UINT icon=MB_ICONINFORMATION){ MessageBoxW(g_hwnd,W(body).c_str(),W(title).c_str(),MB_OK|icon); }
static bool Ask(const std::string& title,const std::string& body){ return MessageBoxW(g_hwnd,W(body).c_str(),W(title).c_str(),MB_YESNO|MB_ICONQUESTION|MB_DEFBUTTON2)==IDYES; }
static void SetStatus(const std::string& s,bool busy=false){ SetWindowTextW(g_status,W(s).c_str()); if(g_progress){ ShowWindow(g_progress,busy?SW_SHOW:SW_HIDE); SendMessage(g_progress,PBM_SETMARQUEE,busy?TRUE:FALSE,40); } }
static void SetMain(int pg,const std::string& text){ if(g_pages[pg].main) SetWindowTextW(g_pages[pg].main,Crlf(text).c_str()); }
static void RunAsync(const std::string& msg,std::function<void()> work,std::function<void()> done){
    if(g_busy){ Msg("Please wait","Another operation is still running. It will finish in a moment."); return; }
    g_busy=true; SetStatus(msg,true);
    std::thread([work,done]{ work(); auto* d=new std::function<void()>(done); PostMessageW(g_hwnd,WM_APP_DONE,0,(LPARAM)d); }).detach();
}
// ------------------------------------------------------------------ settings persistence
static void LoadManagerSettings(){
    IniFile i; i.load(appDataDir()/"manager.ini"); g_theme=std::clamp(i.getInt("UI","Theme",0),0,1); g_cur=std::clamp(i.getInt("UI","Profile",0),0,2);
    for(auto& p:g_prof){ auto gi=gameInfo(p.id); auto e=i.get(std::string("Game.")+gi->key,"Exe"); if(!e.empty()) p.exe=pathFromUtf8(e); }
}
static void SaveManagerSettings(){
    IniFile i; i.setInt("UI","Theme",g_theme); i.setInt("UI","Profile",g_cur);
    for(auto& p:g_prof){ auto gi=gameInfo(p.id); if(!p.exe.empty()) i.set(std::string("Game.")+gi->key,"Exe",pathToUtf8(p.exe)); }
    i.save(appDataDir()/"manager.ini");
}
// ------------------------------------------------------------------ status computation (plain language)
struct HomeState { std::string label, text; };
static HomeState ComputeHome(){
    HomeState h; auto gi=curGame(); std::string t; std::string level="Safe";
    t+=std::string("GAME PROFILE: ")+gi->displayName+"\n\n";
    if(!haveExe()){ h.label="Needs attention"; h.text=t+"What was detected: no executable has been chosen for this game yet.\nWhy it matters: Poneglyph keeps separate settings, add-ons, backups and logs for each game, and needs the exact executable to do that safely.\nWhat to do next: click 'Choose Game Executable' and select "+gi->exeName+" (common location: "+gi->commonRelPath+"). Or click 'Search Folder' and pick the game's install folder.\n"; return h; }
    auto pp=curPaths(); auto chk=verifyGameExe(g_prof[g_cur].exe,g_prof[g_cur].id);
    t+="Executable : "+pathToUtf8(g_prof[g_cur].exe)+"\n"; t+="Verification: "+chk.message+"\n";
    if(!chk.ok){ h.label="Needs attention"; h.text=t+"\nWhat to do next: choose the correct executable for this game. Poneglyph will not change anything until the file is verified.\n"; return h; }
    ProxyKind k=classifyD3D9(pp.gameDir/"d3d9.dll"); t+=std::string("Direct3D 9 proxy in game folder: ")+proxyKindText(k)+"\n";
    IniFile cfg; cfg.load(pp.config); bool enabled=cfg.getBool("General","Enabled",true);
    if(k==ProxyKind::None){ level="Needs attention"; t+="  -> Poneglyph engine is not installed. Click 'Install Poneglyph Engine'. Your game files are backed up first.\n"; }
    else if(k==ProxyKind::NovaFix||k==ProxyKind::Dxvk||k==ProxyKind::Other){ level="Needs attention"; t+=std::string("  -> A ")+proxyKindText(k)+" d3d9.dll is installed. Poneglyph and another proxy cannot run together. Poneglyph will not overwrite it unless you choose 'Install' and confirm; your file is saved to the Backups folder and can be restored.\n"; }
    else if(!enabled){ level="Disabled"; t+="  -> Poneglyph is installed but switched off in Settings (pure pass-through).\n"; }
    else t+="  -> Poneglyph engine installed.\n";
    t+=std::string("4 GB flag (Large Address Aware): ")+(chk.pe.laa?"ON":"OFF")+"\n";
    ScanOptions so; so.game=g_prof[g_cur].id; auto all=scanAddons(pp.addons,cfg,so); int ready=0,dis=0,bad=0; for(auto& a:all){ if(a.status==AddonStatus::Ready) ready++; else if(a.status==AddonStatus::Disabled) dis++; else bad++; }
    t+="Add-ons: "+std::to_string(ready)+" ready, "+std::to_string(dis)+" disabled, "+std::to_string(bad)+" with problems (see Add-ons page)\n";
    if(bad&&level=="Safe") level="Needs attention";
    auto recs=readLoadStatus(pp.status); if(!recs.empty()){ t+="\nLast game start (engine report):\n"; for(auto& r:recs){ char b[200]; snprintf(b,sizeof b,"  %-30s %-9s %.1f ms",r.file.c_str(),r.result.c_str(),r.ms); t+=b; if(r.result=="Failed") { t+="  <- "+r.detail; if(level=="Safe") level="Needs attention"; } t+="\n"; } }
    else { IniFile st; if(st.load(pp.status)&&st.has("Engine","Written")) t+="\nLast game start with Poneglyph: "+st.get("Engine","Written")+" - the engine ran and found no add-ons to load.\n"; else t+="\nThe game has not been started with Poneglyph yet, so there are no load results.\n"; }
    if(g_mon.h) t+="\nThe game is running now: changes to engine or add-ons need a game restart ('Restart required').\n";
    t+="\nBackups: "+pathToUtf8(pp.backups)+"\nLogs   : "+pathToUtf8(pp.logs)+"\n";
    h.label=level; h.text="OVERALL STATUS: "+level+"\n\n"+t; return h;
}
static void RefreshHome(){ auto h=ComputeHome(); SetMain(PG_HOME,h.text); SetStatus("Status: "+h.label+"  |  "+curGame()->displayName); }
// ------------------------------------------------------------------ list view helpers
static HWND MakeList(HWND parent,int id,std::initializer_list<std::pair<const wchar_t*,int>> cols){
    HWND lv=CreateWindowExW(0,WC_LISTVIEWW,L"",WS_CHILD|WS_TABSTOP|LVS_REPORT|LVS_SINGLESEL|LVS_SHOWSELALWAYS,0,0,10,10,parent,(HMENU)(INT_PTR)id,g_inst,nullptr);
    ListView_SetExtendedListViewStyle(lv,LVS_EX_FULLROWSELECT|LVS_EX_DOUBLEBUFFER|LVS_EX_LABELTIP); SetWindowTheme(lv,L"",L"");
    int i=0; for(auto& c:cols){ LVCOLUMNW col{}; col.mask=LVCF_TEXT|LVCF_WIDTH; col.pszText=(LPWSTR)c.first; col.cx=c.second; ListView_InsertColumn(lv,i++,&col); } return lv;
}
static void ListSet(HWND lv,int row,int col,const std::string& s){ auto w=W(s); ListView_SetItemText(lv,row,col,(LPWSTR)w.c_str()); }
static int ListAdd(HWND lv,const std::string& s){ auto w=W(s); LVITEMW it{}; it.mask=LVIF_TEXT; it.iItem=ListView_GetItemCount(lv); it.pszText=(LPWSTR)w.c_str(); return ListView_InsertItem(lv,&it); }
// ------------------------------------------------------------------ add-ons page
static HWND LvAddons(){ return g_pages[PG_ADDONS].main; }
static void FillAddonList(){
    HWND lv=LvAddons(); ListView_DeleteAllItems(lv);
    for(auto& a:g_scan){ int r=ListAdd(lv,a.rel); ListSet(lv,r,1,addonStatusText(a.status)); ListSet(lv,r,2,a.order>=0?std::to_string(a.order+1):"-"); ListSet(lv,r,3,a.pe.valid?a.pe.archName():"?");
        ListSet(lv,r,4,formatBytes(a.size)); ListSet(lv,r,5,a.compat); ListSet(lv,r,6,a.reason+(a.duplicateOfHash.empty()?"":" (identical file to "+a.duplicateOfHash+")")); }
}
static void DoScan(){
    if(!haveExe()){ Msg("Choose the game first","Select this game's executable on the Home page first."); return; }
    auto pp=curPaths(); GameId gid=g_prof[g_cur].id; auto result=std::make_shared<std::vector<AddonEntry>>();
    RunAsync("Scanning add-ons...",[=]{ IniFile cfg; cfg.load(pp.config); ScanOptions so; so.game=gid; so.computeHash=true; *result=scanAddons(pp.addons,cfg,so); },
             [=]{ g_scan=*result; FillAddonList(); SetStatus("Scan finished: "+std::to_string(g_scan.size())+" add-on file(s) found in "+pathToUtf8(pp.addons)); SetMain(PG_ADDONS,""); });
}
static int SelRow(HWND lv){ return ListView_GetNextItem(lv,-1,LVNI_SELECTED); }
static void UpdateAddonDetails(){
    int r=SelRow(LvAddons()); if(r<0||r>=(int)g_scan.size()){ SetWindowTextW(g_pages[PG_ADDONS].second,L"Select an add-on to see details."); return; }
    auto& a=g_scan[(size_t)r]; std::string t="File: "+pathToUtf8(a.path)+"\nStatus: "+addonStatusText(a.status)+"\n"; if(!a.reason.empty()) t+="Why: "+a.reason+"\n";
    t+="SHA-256: "+(a.sha256.empty()?std::string("(not computed)"):a.sha256)+"\nSize: "+formatBytes(a.size)+"   Type: "+a.kind+"   Architecture: "+(a.pe.valid?a.pe.archName():"unknown")+"\nCompatibility: "+a.compat+"\n";
    if(a.meta.present){ t+="Metadata: "+a.meta.name+" "+a.meta.version+" by "+a.meta.author+"\n"; if(!a.meta.description.empty()) t+=a.meta.description+"\n"; } else t+="Metadata: none (compatibility is not assumed).\n";
    SetWindowTextW(g_pages[PG_ADDONS].second,Crlf(t).c_str());
}
static void SetEnabledSel(bool en){
    int r=SelRow(LvAddons()); if(r<0||r>=(int)g_scan.size()){ Msg("Select an add-on","Click an add-on in the list first."); return; }
    auto res=setAddonEnabled(curPaths(),g_scan[(size_t)r].rel,en,g_oplog); SetStatus(res.message); DoScan();
}
static void InstallZip(){
    if(!haveExe()){ Msg("Choose the game first","Select this game's executable on the Home page first."); return; }
    wchar_t f[MAX_PATH]={0}; OPENFILENAMEW o{}; o.lStructSize=sizeof o; o.hwndOwner=g_hwnd; o.lpstrFile=f; o.nMaxFile=MAX_PATH; o.lpstrFilter=L"ZIP archives (*.zip)\0*.zip\0\0"; o.Flags=OFN_FILEMUSTEXIST|OFN_PATHMUSTEXIST;
    if(!GetOpenFileNameW(&o)) return; auto pp=curPaths(); fs::path z=f; auto res=std::make_shared<OpResult>();
    RunAsync("Installing add-on package...",[=]{ *res=installAddonZip(pp,z,g_oplog); },[=]{ if(!res->ok) Msg("Not installed",res->message,MB_ICONWARNING); else Msg("Installed",res->message+"\n\nIt is on the list now. Add-ons without metadata are shown as 'compatibility unknown'."); DoScan(); });
}
static void RemovePkg(){
    int r=SelRow(LvAddons()); if(r<0||r>=(int)g_scan.size()){ Msg("Select an add-on","Click an add-on from the package you want to remove."); return; }
    std::string rel=g_scan[(size_t)r].rel; auto sl=rel.find('/'); std::string pkg=sl==std::string::npos?"":rel.substr(0,sl);
    auto pkgs=listAddonPackages(curPaths()); if(pkg.empty()||std::find(pkgs.begin(),pkgs.end(),pkg)==pkgs.end()){ Msg("Not managed by Poneglyph","This file was not installed from a ZIP by Poneglyph, so it will not be deleted automatically. You can still use 'Disable', or delete it yourself in the folder."); return; }
    if(!Ask("Remove package","Remove the package '"+pkg+"'?\n\nOnly files Poneglyph installed (and you did not modify) are deleted. Your own files stay.")) return;
    auto res=uninstallAddonPackage(curPaths(),pkg,g_oplog); Msg("Package removal",res.message); DoScan();
}
// ------------------------------------------------------------------ game / engine
static void ChooseExe(){
    auto gi=curGame(); wchar_t f[MAX_PATH]={0}; OPENFILENAMEW o{}; o.lStructSize=sizeof o; o.hwndOwner=g_hwnd; o.lpstrFile=f; o.nMaxFile=MAX_PATH; std::wstring flt;
    flt=std::wstring(W(gi->exeName)+L'\0'+W(gi->exeName)+L'\0'+L"All executables"+L'\0'+L"*.exe"+L'\0'); o.lpstrFilter=flt.c_str(); o.Flags=OFN_FILEMUSTEXIST|OFN_PATHMUSTEXIST; o.lpstrTitle=L"Choose the game executable";
    if(!GetOpenFileNameW(&o)) return; auto chk=verifyGameExe(f,g_prof[g_cur].id);
    if(!chk.ok){ Msg("This file cannot be used","What was detected: "+chk.message+"\n\nWhy it matters: applying one game's profile to a different file could damage it.\n\nWhat to do: choose "+std::string(gi->exeName)+" from this game's install folder.",MB_ICONWARNING); return; }
    g_prof[g_cur].exe=f; SaveManagerSettings(); g_mlog.log(LogLevel::Info,"profile %s exe set to %s",gi->key,U(f).c_str()); RefreshHome();
}
static void SearchFolder(){
    BROWSEINFOW bi{}; bi.hwndOwner=g_hwnd; bi.lpszTitle=L"Choose the folder where the game is installed (the search looks inside it)"; bi.ulFlags=BIF_RETURNONLYFSDIRS|BIF_NEWDIALOGSTYLE;
    PIDLIST_ABSOLUTE pidl=SHBrowseForFolderW(&bi); if(!pidl) return; wchar_t p[MAX_PATH]; BOOL ok=SHGetPathFromIDListW(pidl,p); CoTaskMemFree(pidl); if(!ok) return;
    auto res=std::make_shared<fs::path>(); GameId gid=g_prof[g_cur].id; fs::path root=p;
    RunAsync("Searching for the game executable...",[=]{ *res=findGameExe(root,gid); },[=]{
        if(res->empty()){ Msg("Not found","No "+std::string(curGame()->exeName)+" was found inside that folder (searched 5 levels deep). Choose the folder that contains the game, or use 'Choose Game Executable'.",MB_ICONWARNING); SetStatus("Search finished: not found"); return; }
        auto chk=verifyGameExe(*res,gid); if(!chk.ok){ Msg("Found, but not usable",chk.message,MB_ICONWARNING); return; } g_prof[g_cur].exe=*res; SaveManagerSettings(); RefreshHome(); });
}
static void InstallEngine(){
    if(!haveExe()){ Msg("Choose the game first","Select this game's executable first."); return; }
    auto chk=verifyGameExe(g_prof[g_cur].exe,g_prof[g_cur].id); if(!chk.ok){ Msg("Cannot install",chk.message,MB_ICONWARNING); return; }
    if(g_mon.h){ Msg("Game is running","Close the game first, then install."); return; }
    fs::path eng=exeDir()/"Engine"/"d3d9.dll"; std::error_code ec; if(!fs::exists(eng,ec)){ Msg("Engine file missing","The file Engine\\d3d9.dll was not found next to Poneglyph.exe ("+pathToUtf8(eng)+"). Re-extract the full Poneglyph package.",MB_ICONERROR); return; }
    auto pp=curPaths(); InstallOptions io; ProxyKind k=classifyD3D9(pp.gameDir/"d3d9.dll");
    if(k==ProxyKind::NovaFix||k==ProxyKind::Dxvk||k==ProxyKind::Other){
        if(!Ask("Replace the existing Direct3D 9 proxy?",std::string("A ")+proxyKindText(k)+" d3d9.dll is already in this game's folder.\n\nTwo proxies cannot run together. If you continue, your current file is copied to Poneglyph\\Backups (verified by SHA-256) and Poneglyph is installed. You can go back at any time with 'Remove Poneglyph Engine'.\n\nContinue?")) return;
        io.replaceForeignProxy=true; }
    auto res=installEngine(pp,g_prof[g_cur].id,eng,io,g_oplog); Msg(res.ok?"Done":"Not installed",res.message+(res.changed.empty()?"":"\n\n"+join(res.changed,"\n")),res.ok?MB_ICONINFORMATION:MB_ICONWARNING); RefreshHome();
}
static void RemoveEngine(){
    if(!haveExe()) return; if(g_mon.h){ Msg("Game is running","Close the game first."); return; }
    if(!Ask("Remove Poneglyph engine","Remove Poneglyph's d3d9.dll from this game's folder?\n\nIf a previous d3d9.dll was backed up, it is restored. Your add-ons, settings and saves are not deleted.")) return;
    auto res=uninstallEngine(curPaths(),g_oplog); Msg(res.ok?"Done":"Not removed",res.message+(res.changed.empty()?"":"\n\n"+join(res.changed,"\n")),res.ok?MB_ICONINFORMATION:MB_ICONWARNING); RefreshHome();
}
static DWORD LaunchGame(){
    if(!haveExe()){ Msg("Choose the game first","Select this game's executable first."); return 0; } auto chk=verifyGameExe(g_prof[g_cur].exe,g_prof[g_cur].id); if(!chk.ok){ Msg("Cannot launch",chk.message,MB_ICONWARNING); return 0; }
    std::wstring exe=g_prof[g_cur].exe.wstring(), dir=g_prof[g_cur].exe.parent_path().wstring(), cmd=L"\""+exe+L"\""; STARTUPINFOW si{}; si.cb=sizeof si; PROCESS_INFORMATION pi{};
    if(!CreateProcessW(exe.c_str(),&cmd[0],nullptr,nullptr,FALSE,0,nullptr,dir.c_str(),&si,&pi)){ Msg("Could not start the game","Windows error "+std::to_string(GetLastError())+".\n\nSome Steam editions must be started through Steam. Start the game from Steam; Poneglyph still loads because it sits next to the game's executable.",MB_ICONWARNING); return 0; }
    CloseHandle(pi.hThread); CloseHandle(pi.hProcess); g_mlog.log(LogLevel::Info,"launched %s pid %lu",U(exe).c_str(),pi.dwProcessId); SetStatus("Game started."); return pi.dwProcessId;
}
// ------------------------------------------------------------------ performance monitor (Level 1) + frame stats (Level 2)
static DWORD FindPid(const std::wstring& exeName){
    HANDLE s=CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,0); if(s==INVALID_HANDLE_VALUE) return 0; PROCESSENTRY32W pe{}; pe.dwSize=sizeof pe; DWORD pid=0;
    if(Process32FirstW(s,&pe)) do{ if(_wcsicmp(pe.szExeFile,exeName.c_str())==0){ pid=pe.th32ProcessID; break; } }while(Process32NextW(s,&pe)); CloseHandle(s); return pid;
}
static ULONGLONG Ft(const FILETIME& f){ return ((ULONGLONG)f.dwHighDateTime<<32)|f.dwLowDateTime; }
static void MonitorStop(){ if(g_mon.on){ KillTimer(g_hwnd,1); g_mon.on=false; } if(g_mon.h){ CloseHandle(g_mon.h); g_mon.h=nullptr; } g_mon.pid=0; }
static void MonitorTick(){
    if(!g_mon.h){ DWORD pid=FindPid(g_prof[g_cur].exe.filename().wstring()); if(!pid){ SetMain(PG_PERF,"Monitoring is ON. Waiting for "+std::string(curGame()->exeName)+" to start...\nSamples so far: "+std::to_string(g_samples.size())); return; }
        g_mon.h=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,FALSE,pid); if(!g_mon.h) return; g_mon.pid=pid; g_mon.lastCpu=0; g_mon.lastTick=0; g_mlog.log(LogLevel::Info,"monitor attached to pid %lu",pid); }
    DWORD ec=0; if(!GetExitCodeProcess(g_mon.h,&ec)||ec!=STILL_ACTIVE){ CloseHandle(g_mon.h); g_mon.h=nullptr; SetMain(PG_PERF,"The game has exited. Samples kept: "+std::to_string(g_samples.size())+". You can export the CSV or view the report."); return; }
    FILETIME c,e,k,u; if(!GetProcessTimes(g_mon.h,&c,&e,&k,&u)) return; ULONGLONG cpu=Ft(k)+Ft(u), now=GetTickCount64(); SYSTEM_INFO si; GetSystemInfo(&si);
    PROCESS_MEMORY_COUNTERS_EX pmc{}; pmc.cb=sizeof pmc; GetProcessMemoryInfo(g_mon.h,(PROCESS_MEMORY_COUNTERS*)&pmc,sizeof pmc);
    if(g_mon.lastTick){ double wall100ns=(double)(now-g_mon.lastTick)*10000.0; double pct=wall100ns>0?(double)(cpu-g_mon.lastCpu)/wall100ns/(double)si.dwNumberOfProcessors*100.0:0; if(pct<0) pct=0;
        ProcSample s; s.time=nowStamp(); s.cpuPct=pct; s.workingSetBytes=pmc.WorkingSetSize; if(g_samples.size()<36000) g_samples.push_back(s);
        char b[256]; snprintf(b,sizeof b,"Monitoring the game (pid %lu)\nCPU (share of all cores): %.1f %%\nWorking set: %s   Private bytes: %s\nSamples: %zu   (one per second; negligible overhead)\n",g_mon.pid,pct,formatBytes(pmc.WorkingSetSize).c_str(),formatBytes(pmc.PrivateUsage).c_str(),g_samples.size()); SetMain(PG_PERF,b); }
    g_mon.lastCpu=cpu; g_mon.lastTick=now;
}
static void MonitorStart(){ if(!haveExe()){ Msg("Choose the game first","Select this game's executable first."); return; } if(g_mon.on) return; g_samples.clear(); g_mon.on=true; SetTimer(g_hwnd,1,1000,nullptr); MonitorTick(); SetStatus("Monitoring started. It only samples once per second while this is on."); }
struct CsvAgg { size_t rows=0, frames=0, spikes=0; double seconds=0, worstP99=0, worstLow1=1e9; FrameSummary last; };
static CsvAgg ReadFrameCsv(const fs::path& f){
    CsvAgg a; std::ifstream in(f); std::string line; bool first=true;
    while(std::getline(in,line)){ if(first){ first=false; continue; } auto c=split(trim(line),','); if(c.size()<12) continue;
        try{ FrameSummary s; s.frames=(size_t)std::stoull(c[1]); s.seconds=std::stod(c[2]); s.fps=std::stod(c[3]); s.avgMs=std::stod(c[4]); s.p50=std::stod(c[5]); s.p95=std::stod(c[6]); s.p99=std::stod(c[7]); s.maxMs=std::stod(c[8]); s.low1Fps=std::stod(c[9]); s.low01Fps=std::stod(c[10]); s.spikes=(size_t)std::stoull(c[11]);
            a.rows++; a.frames+=s.frames; a.seconds+=s.seconds; a.spikes+=s.spikes; a.worstP99=std::max(a.worstP99,s.p99); a.worstLow1=std::min(a.worstLow1,s.low1Fps); a.last=s; }catch(...){} }
    return a;
}
static ReportInput BuildInput(){
    ReportInput in; in.gameName=curGame()->displayName; in.exePath=pathToUtf8(g_prof[g_cur].exe); in.managerVersion=PG_MANAGER_VERSION; auto pp=curPaths();
    if(haveExe()){ in.exe=inspectPe(g_prof[g_cur].exe,{false,false,false}); IniFile cfg; cfg.load(pp.config); ScanOptions so; so.game=g_prof[g_cur].id; so.computeHash=true; in.addons=scanAddons(pp.addons,cfg,so); in.loadRecords=readLoadStatus(pp.status);
        IniFile st; st.load(pp.status); in.engineVersion=st.get("Engine","Version"); in.engineStarted=st.has("Engine","Written"); in.engineLastStart=st.get("Engine","Written"); auto agg=ReadFrameCsv(pp.logs/"framestats.csv"); if(agg.rows){ in.haveFrames=true; in.frames=agg.last; in.frames.frames=agg.frames; in.frames.spikes=agg.spikes; in.frames.seconds=agg.seconds; if(agg.seconds>0) in.frames.fps=(double)agg.frames/agg.seconds; in.frames.p99=agg.worstP99; in.frames.low1Fps=agg.worstLow1; in.note="Frame figures aggregate "+std::to_string(agg.rows)+" summary window(s); p99 and 1% low are the WORST window. Frame time = interval between Present calls (CPU-side), not GPU time."; } }
    in.samples=g_samples; if(!haveExe()) in.warnings.push_back("No game executable selected."); return in;
}
static void ShowReport(){ if(!haveExe()){ Msg("Choose the game first","Select this game's executable first."); return; } auto in=std::make_shared<ReportInput>(); auto txt=std::make_shared<std::string>();
    RunAsync("Building report...",[=]{ *in=BuildInput(); *txt=buildReport(*in); },[=]{ SetMain(PG_PERF,*txt); SetStatus("Report ready."); }); }
static void SaveReport(bool openFolder=false){
    if(!haveExe()) return; auto pp=curPaths(); auto txt=std::make_shared<std::string>(); fs::path out=pp.logs/("report_"+nowStampCompact()+".txt");
    RunAsync("Saving report...",[=]{ *txt=buildReport(BuildInput()); writeFileAtomic(out,*txt); },[=]{ SetMain(PG_PERF,*txt); SetStatus("Saved: "+pathToUtf8(out)); if(openFolder) ShellExecuteW(g_hwnd,L"open",pp.logs.wstring().c_str(),nullptr,nullptr,SW_SHOW); });
}
static void ExportCsv(){
    if(g_samples.empty()){ Msg("Nothing to export","Start monitoring while the game runs first."); return; } auto pp=curPaths(); std::error_code ec; fs::create_directories(pp.logs,ec); fs::path out=pp.logs/("process_"+nowStampCompact()+".csv");
    std::ofstream o(out,std::ios::binary); o<<"timestamp,cpu_percent_of_all_cores,working_set_mb\n"; char b[160]; for(auto& s:g_samples){ snprintf(b,sizeof b,"%s,%.2f,%.1f\n",s.time.c_str(),s.cpuPct,(double)s.workingSetBytes/1048576.0); o<<b; }
    SetStatus("Exported "+std::to_string(g_samples.size())+" samples to "+pathToUtf8(out)); ShellExecuteW(g_hwnd,L"open",pp.logs.wstring().c_str(),nullptr,nullptr,SW_SHOW);
}
static void CompareFrames(){
    if(!haveExe()){ Msg("Choose the game first","Select this game's executable first."); return; } wchar_t f[MAX_PATH]={0}; OPENFILENAMEW o{}; o.lStructSize=sizeof o; o.hwndOwner=g_hwnd; o.lpstrFile=f; o.nMaxFile=MAX_PATH; o.lpstrFilter=L"Frame statistics (*.csv)\0*.csv\0\0"; o.lpstrTitle=L"Choose the BASELINE framestats CSV (e.g. from before your change)"; o.Flags=OFN_FILEMUSTEXIST;
    if(!GetOpenFileNameW(&o)) return; auto base=ReadFrameCsv(f), cur=ReadFrameCsv(curPaths().logs/"framestats.csv"); if(!base.rows||!cur.rows){ Msg("No data","Both files need at least one summary row. Enable 'Frame statistics' in Settings and play a session first."); return; }
    auto line=[&](const char* n,const CsvAgg& a){ char b[300]; snprintf(b,sizeof b,"%-9s frames %-8zu avg %.1f FPS   worst p99 %.1f ms   worst 1%% low %.1f FPS   spikes %zu   windows %zu\n",n,a.frames,a.seconds>0?(double)a.frames/a.seconds:0.0,a.worstP99,a.worstLow1,a.spikes,a.rows); return std::string(b); };
    std::string t="BEFORE / AFTER COMPARISON\n\n"+line("Baseline",base)+line("Current",cur)+"\nOnly compare runs of the SAME scene, settings, add-ons and duration, repeated several times. A single run is not evidence of an improvement; Poneglyph makes no automatic claim.\n"; SetMain(PG_PERF,t);
}
// ------------------------------------------------------------------ memory (LAA)
static const char* kLaaText=
"WHAT THIS DOES\nThe three games are 32-bit programs. A 32-bit program normally can use 2 GB of address space. The 'Large Address Aware' (4 GB) flag is one setting in the program's header. When it is ON and you use 64-bit Windows, the game may address up to 4 GB.\n\n"
"WHAT IT DOES NOT DO\n- It does not turn the game into a 64-bit game.\n- It does not give the game more than 4 GB. Nothing can, for a 32-bit program.\n- It does not use more of your installed RAM beyond that limit, and it does not add speed.\n\n"
"SAFETY\nBefore any change Poneglyph copies the executable byte-for-byte (SHA-256 checked), changes ONLY that flag (and the file checksum if it was in use), verifies the result, and keeps 'Restore Original' available. It refuses any file that is not this game's executable, is not 32-bit, or is a DLL. It never runs third-party patchers.\n";
static void LaaCheck(){
    if(!haveExe()){ SetMain(PG_MEM,std::string(kLaaText)+"\nNo executable selected for this game yet."); return; } auto p=inspectPe(g_prof[g_cur].exe,{false,false,true}); std::string t="CURRENT STATE\nFile: "+pathToUtf8(g_prof[g_cur].exe)+"\n";
    if(!p.valid) t+="Could not read the file: "+p.error+"\n"; else { t+="Architecture: "+p.archName()+"\n4 GB flag: "+std::string(p.laa?"ON - already Large Address Aware. Nothing to do.":"OFF - you can enable it with the button above.")+"\n"; }
    auto pp=curPaths(); std::error_code ec; int n=0; if(fs::is_directory(pp.backups,ec)) for(auto& e:fs::directory_iterator(pp.backups,ec)) if(e.path().extension()==".pgbak"&&e.path().filename().string().rfind(g_prof[g_cur].exe.filename().string(),0)==0) n++;
    t+="Backups of this executable: "+std::to_string(n)+"\n\n"+kLaaText; SetMain(PG_MEM,t);
}
static void LaaEnable(){
    if(!haveExe()){ Msg("Choose the game first","Select this game's executable first."); return; } if(g_mon.h){ Msg("Game is running","Close the game first."); return; }
    auto p=inspectPe(g_prof[g_cur].exe,{false,false,false}); if(p.valid&&p.laa){ Msg("Already enabled","This executable is already Large Address Aware. No change needed."); return; }
    if(!Ask("Enable the 4 GB flag?","This changes ONE flag in "+std::string(curGame()->exeName)+". A verified backup is made first and you can restore it any time.\n\nIt allows up to 4 GB of address space on 64-bit Windows. It does NOT allow more than 4 GB and does not add RAM or speed.\n\nSome copy-protected editions may reject a modified executable; if the game does not start, use 'Restore Original'.\n\nContinue?")) return;
    auto rep=std::make_shared<LaaReport>(); auto exe=g_prof[g_cur].exe; auto bd=curPaths().backups; std::vector<std::string> names{curGame()->exeName};
    RunAsync("Patching with backup...",[=]{ *rep=patchLaa(exe,bd,names); },[=]{ bool ok=rep->result==LaaResult::Patched||rep->result==LaaResult::AlreadyLaa; Msg(ok?"Done":"Not changed",rep->message,ok?MB_ICONINFORMATION:MB_ICONWARNING); g_mlog.log(LogLevel::Info,"LAA: %s",rep->message.c_str()); LaaCheck(); RefreshHome(); });
}
static fs::path LatestBackupFor(const fs::path& exe,const fs::path& dir){
    fs::path best; fs::file_time_type bt{}; std::error_code ec; if(!fs::is_directory(dir,ec)) return best;
    for(auto& e:fs::directory_iterator(dir,ec)) if(e.path().extension()==".pgbak"&&e.path().filename().string().rfind(exe.filename().string()+".",0)==0){ auto t=fs::last_write_time(e.path(),ec); if(best.empty()||t>bt){ best=e.path(); bt=t; } }
    return best;
}
static void LaaRestore(){
    if(!haveExe()) return; if(g_mon.h){ Msg("Game is running","Close the game first."); return; } auto b=LatestBackupFor(g_prof[g_cur].exe,curPaths().backups);
    if(b.empty()){ Msg("No backup found","There is no Poneglyph backup of this executable, so there is nothing to restore."); return; }
    if(!Ask("Restore original executable","Restore "+pathToUtf8(b.filename())+" over the game executable? The backup's SHA-256 is verified first.")) return;
    auto r=restoreFromBackup(b,g_prof[g_cur].exe); Msg(r.result==LaaResult::Patched?"Restored":"Not restored",r.message,r.result==LaaResult::Patched?MB_ICONINFORMATION:MB_ICONWARNING); LaaCheck(); RefreshHome();
}
// ------------------------------------------------------------------ backups page
static void FillBackups(){
    HWND lv=g_pages[PG_BACKUPS].main; ListView_DeleteAllItems(lv); if(!haveExe()){ SetWindowTextW(g_pages[PG_BACKUPS].second,L"Choose a game first."); return; }
    auto pp=curPaths(); std::error_code ec; if(!fs::is_directory(pp.backups,ec)){ SetWindowTextW(g_pages[PG_BACKUPS].second,L"No backups yet. Poneglyph creates them automatically before it changes any game file."); return; }
    for(auto& e:fs::directory_iterator(pp.backups,ec)){ if(!e.is_regular_file(ec)) continue; auto n=pathToUtf8(e.path().filename()); int r=ListAdd(lv,n); ListSet(lv,r,1,formatBytes(fs::file_size(e.path(),ec)));
        std::string kind=e.path().extension()==".pgbak"?(n.rfind("d3d9.dll",0)==0?"Previous d3d9.dll":"Game executable"):"Record"; ListSet(lv,r,2,kind); }
    SetWindowTextW(g_pages[PG_BACKUPS].second,Crlf("Folder: "+pathToUtf8(pp.backups)+"\nSelect a 'Game executable' backup and click 'Restore Selected'. To get your previous d3d9.dll back, use 'Remove Poneglyph Engine' on the Home page - it verifies and restores it for you.").c_str());
}
static void RestoreSelectedBackup(){
    HWND lv=g_pages[PG_BACKUPS].main; int r=SelRow(lv); if(r<0){ Msg("Select a backup","Click a backup in the list first."); return; } wchar_t b[260]; ListView_GetItemText(lv,r,0,b,260); fs::path bk=curPaths().backups/b;
    if(bk.filename().string().rfind(g_prof[g_cur].exe.filename().string()+".",0)!=0){ Msg("Use the Home page","Only game-executable backups are restored here. For the previous d3d9.dll use 'Remove Poneglyph Engine' on Home."); return; }
    if(!Ask("Restore backup","Restore this backup over the game executable?")) return; auto res=restoreFromBackup(bk,g_prof[g_cur].exe); Msg(res.result==LaaResult::Patched?"Restored":"Not restored",res.message); RefreshHome(); FillBackups();
}
// ------------------------------------------------------------------ settings page
static const int kSummary[4]={5,10,30,60}; static const char* kLogNames[4]={"Errors only","Warnings","Normal","Detailed"}; static const char* kLogKeys[4]={"error","warn","info","debug"};
static void RefreshSettingButtons(){
    auto set=[&](int id,const std::wstring& t){ for(HWND h:g_pages[PG_SETTINGS].buttons) if(GetDlgCtrlID(h)==id){ SetWindowTextW(h,t.c_str()); InvalidateRect(h,nullptr,TRUE); } };
    int si=0; for(int i=0;i<4;i++) if(kSummary[i]==g_summarySecs) si=i;
    set(C_SUMMARY,L"Summary every "+std::to_wstring(g_summarySecs)+L" s"); set(C_LOGLEVEL,std::wstring(L"Log detail: ")+W(kLogNames[g_logLevel])); set(C_THEME,std::wstring(L"Theme: ")+T().name); (void)si;
}
static void LoadSettingsFromProfile(){
    g_toggle[T_ENABLED]=true; g_toggle[T_LOADADDONS]=true; g_toggle[T_FRAMESTATS]=false; g_summarySecs=10; g_logLevel=2; if(haveExe()){ IniFile c; c.load(curPaths().config); g_toggle[T_ENABLED]=c.getBool("General","Enabled",true); g_toggle[T_LOADADDONS]=c.getBool("Addons","LoadAddons",true); g_toggle[T_FRAMESTATS]=c.getBool("Diagnostics","FrameStats",false);
        g_summarySecs=c.getInt("Diagnostics","SummarySeconds",10); auto lv=toLower(c.get("General","LogLevel","info")); for(int i=0;i<4;i++) if(lv==kLogKeys[i]) g_logLevel=i; }
    RefreshSettingButtons(); for(HWND h:g_pages[PG_SETTINGS].buttons) InvalidateRect(h,nullptr,TRUE);
}
static void SaveSettings(){
    if(!haveExe()){ Msg("Choose the game first","Select this game's executable first."); return; } auto pp=curPaths(); std::error_code ec; fs::create_directories(pp.root,ec); IniFile c; c.load(pp.config); c.set("General","Game",curGame()->key);
    c.setBool("General","Enabled",g_toggle[T_ENABLED]); c.set("General","LogLevel",kLogKeys[g_logLevel]); c.setBool("Addons","LoadAddons",g_toggle[T_LOADADDONS]); c.setBool("Diagnostics","FrameStats",g_toggle[T_FRAMESTATS]); c.setInt("Diagnostics","SummarySeconds",g_summarySecs);
    if(!c.save(pp.config)) Msg("Could not save","The settings file could not be written: "+pathToUtf8(pp.config),MB_ICONWARNING); else { SetStatus("Settings saved for "+std::string(curGame()->displayName)+". They apply the next time the game starts."); g_mlog.log(LogLevel::Info,"settings saved for %s",curGame()->key); RefreshHome(); }
}
static const char* kHelpText=
"QUICK START (5 steps)\n1. Home > 'Choose Game Executable' (or 'Search Folder') and pick your game. Repeat for each game you own using the three tabs at the top.\n2. Home > 'Install Poneglyph Engine'. If another d3d9.dll (for example NovaFix) is already there, you are asked first and your file is backed up.\n3. Add-ons > 'Install ZIP...' to add a mod, or copy .asi/.dll files into the game's Poneglyph\\Addons folder, then 'Scan Add-ons'.\n4. Home > 'Launch Game' (or start it from Steam).\n5. Something wrong? Home > 'Remove Poneglyph Engine' returns the game to how it was.\n\n"
"STATUS WORDS\nSafe - nothing needs attention.   Needs attention - read the explanation on Home.   Disabled - switched off in Settings.   Restart required - the game is running; restart it for changes.\n\n"
"IF THE GAME WILL NOT START\nPoneglyph turns off an add-on automatically if the previous start crashed while loading it. You can also set Settings > 'Poneglyph engine' OFF (pure pass-through) or remove the engine on Home. Send Poneglyph\\Logs\\poneglyph.log and a support report when asking for help.\n\n"
"WHAT IS NOT INCLUDED IN v1.0\nIn-game overlay, per-callback add-on timing, shader/renderer replacement and game-specific memory patches. Only verified features are shipped.\n";
// ------------------------------------------------------------------ chrome drawing
static COLORREF Mix(COLORREF a,COLORREF b,double t){ return RGB((int)(GetRValue(a)*(1-t)+GetRValue(b)*t),(int)(GetGValue(a)*(1-t)+GetGValue(b)*t),(int)(GetBValue(a)*(1-t)+GetBValue(b)*t)); }
static void VGrad(HDC dc,const RECT& r,COLORREF top,COLORREF bot){
    TRIVERTEX v[2]={{r.left,r.top,(COLOR16)(GetRValue(top)<<8),(COLOR16)(GetGValue(top)<<8),(COLOR16)(GetBValue(top)<<8),0},{r.right,r.bottom,(COLOR16)(GetRValue(bot)<<8),(COLOR16)(GetGValue(bot)<<8),(COLOR16)(GetBValue(bot)<<8),0}};
    GRADIENT_RECT g{0,1}; GradientFill(dc,v,2,&g,1,GRADIENT_FILL_RECT_V);
}
static void Chrome(HDC dc,RECT r,bool pressed,bool selected,bool disabled){
    int rad=S(10); HRGN rg=CreateRoundRectRgn(r.left,r.top,r.right+1,r.bottom+1,rad,rad); SelectClipRgn(dc,rg);
    COLORREF top=selected?T().accent:(pressed?T().panelLo:T().panelHi), bot=selected?Mix(T().accent,T().panelLo,0.6):(pressed?T().panelHi:T().panelLo); if(disabled){ top=Mix(top,T().bg,0.6); bot=Mix(bot,T().bg,0.6); }
    VGrad(dc,r,top,bot); RECT hl=r; hl.bottom=hl.top+1; HBRUSH hb=CreateSolidBrush(Mix(top,RGB(255,255,255),0.35)); FillRect(dc,&hl,hb); DeleteObject(hb); SelectClipRgn(dc,nullptr);
    HBRUSH bb=CreateSolidBrush(selected?T().accent:T().border); FrameRgn(dc,rg,bb,1,1); DeleteObject(bb); DeleteObject(rg);
}
static void DrawOwner(DRAWITEMSTRUCT* d){
    int id=(int)d->CtlID; HDC dc=d->hDC; RECT r=d->rcItem; bool pressed=(d->itemState&ODS_SELECTED)!=0, dis=(d->itemState&ODS_DISABLED)!=0; wchar_t txt[160]; GetWindowTextW(d->hwndItem,txt,160);
    bool nav=id>=ID_NAV&&id<ID_NAV+PG_COUNT, tab=id>=ID_PROFILE&&id<ID_PROFILE+3, tog=g_toggle.count(id)>0&&id>=T_ENABLED&&id<=T_FRAMESTATS;
    SetBkMode(dc,TRANSPARENT);
    if(tog){ HBRUSH b=CreateSolidBrush(T().bg); FillRect(dc,&r,b); DeleteObject(b); RECT bx={r.left+S(4),r.top+(r.bottom-r.top-S(22))/2,r.left+S(26),0}; bx.bottom=bx.top+S(22); bool on=g_toggle[id];
        HBRUSH bg=CreateSolidBrush(on?T().accent:T().sidebar); HPEN pen=CreatePen(PS_SOLID,1,T().border); HGDIOBJ op=SelectObject(dc,pen),ob=SelectObject(dc,bg); RoundRect(dc,bx.left,bx.top,bx.right,bx.bottom,S(6),S(6)); SelectObject(dc,op); SelectObject(dc,ob); DeleteObject(pen); DeleteObject(bg);
        if(on){ HPEN cp=CreatePen(PS_SOLID,S(3),T().accentText); HGDIOBJ o2=SelectObject(dc,cp); MoveToEx(dc,bx.left+S(5),bx.top+S(11),nullptr); LineTo(dc,bx.left+S(9),bx.top+S(16)); LineTo(dc,bx.left+S(17),bx.top+S(6)); SelectObject(dc,o2); DeleteObject(cp); }
        RECT tr=r; tr.left=bx.right+S(10); SelectObject(dc,g_fBody); SetTextColor(dc,T().text); DrawTextW(dc,txt,-1,&tr,DT_LEFT|DT_VCENTER|DT_SINGLELINE); if(d->itemState&ODS_FOCUS){ RECT f=r; InflateRect(&f,-1,-1); DrawFocusRect(dc,&f); } return; }
    if(nav){ bool sel=(id-ID_NAV)==g_page; if(sel) Chrome(dc,r,pressed,true,false); else { HBRUSH b=CreateSolidBrush(T().sidebar); FillRect(dc,&r,b); DeleteObject(b); if(pressed) Chrome(dc,r,true,false,false); }
        RECT tr=r; tr.left+=S(16); SelectObject(dc,sel?g_fBold:g_fBody); SetTextColor(dc,sel?T().accentText:T().text); DrawTextW(dc,txt,-1,&tr,DT_LEFT|DT_VCENTER|DT_SINGLELINE); return; }
    bool sel=tab&&(id-ID_PROFILE)==g_cur; Chrome(dc,r,pressed,sel,dis); SelectObject(dc,(sel||tab)?g_fBold:g_fBody); SetTextColor(dc,dis?T().dim:(sel?T().accentText:T().text)); RECT tr=r; InflateRect(&tr,-S(6),0);
    DrawTextW(dc,txt,-1,&tr,DT_CENTER|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS); if(d->itemState&ODS_FOCUS){ RECT f=r; InflateRect(&f,-S(3),-S(3)); DrawFocusRect(dc,&f); }
}
static LRESULT CALLBACK HeaderProc(HWND h,UINT m,WPARAM w,LPARAM l,UINT_PTR,DWORD_PTR){
    if(m==WM_ERASEBKGND) return 1;
    if(m==WM_PAINT){ PAINTSTRUCT ps; HDC dc=BeginPaint(h,&ps); RECT rc; GetClientRect(h,&rc); VGrad(dc,rc,T().panelHi,T().panelLo); SetBkMode(dc,TRANSPARENT); SelectObject(dc,g_fBold); SetTextColor(dc,T().text);
        int n=Header_GetItemCount(h); HPEN pen=CreatePen(PS_SOLID,1,T().border); HGDIOBJ op=SelectObject(dc,pen);
        for(int i=0;i<n;i++){ RECT ir; Header_GetItemRect(h,i,&ir); wchar_t buf[128]={0}; HDITEMW hi{}; hi.mask=HDI_TEXT; hi.pszText=buf; hi.cchTextMax=128; Header_GetItem(h,i,&hi); RECT tr=ir; tr.left+=S(8); tr.right-=S(4); DrawTextW(dc,buf,-1,&tr,DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS); MoveToEx(dc,ir.right-1,ir.top+2,nullptr); LineTo(dc,ir.right-1,ir.bottom-2); }
        SelectObject(dc,op); DeleteObject(pen); EndPaint(h,&ps); return 0; }
    if(m==WM_NCDESTROY) RemoveWindowSubclass(h,HeaderProc,1);
    return DefSubclassProc(h,m,w,l);
}
// ------------------------------------------------------------------ construction and layout
enum { ID_LIST_ADDONS=300, ID_LIST_BACKUPS=301, ID_EDIT=310 };
static HWND MakeBtn(int page,int id,const wchar_t* text){ HWND b=CreateWindowExW(0,L"BUTTON",text,WS_CHILD|WS_TABSTOP|BS_OWNERDRAW,0,0,10,10,g_hwnd,(HMENU)(INT_PTR)id,g_inst,nullptr); g_pages[page].buttons.push_back(b); return b; }
static HWND MakeEdit(int id){ return CreateWindowExW(0,L"EDIT",L"",WS_CHILD|WS_BORDER|WS_VSCROLL|ES_MULTILINE|ES_READONLY|ES_AUTOVSCROLL,0,0,10,10,g_hwnd,(HMENU)(INT_PTR)id,g_inst,nullptr); }
static void ThemeLists(){
    for(int pg:{PG_ADDONS,PG_BACKUPS}){ HWND lv=g_pages[pg].main; if(!lv) continue; ListView_SetBkColor(lv,T().listAlt); ListView_SetTextBkColor(lv,T().listAlt); ListView_SetTextColor(lv,T().text); }
}
static void ApplyFonts(){
    if(g_fBody){ DeleteObject(g_fBody); DeleteObject(g_fBold); DeleteObject(g_fTitle); DeleteObject(g_fSmall); }
    auto mk=[&](int pt,int wt){ return CreateFontW(-MulDiv(pt,g_dpi,72),0,0,0,wt,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH|FF_DONTCARE,L"Segoe UI"); };
    g_fBody=mk(10,FW_NORMAL); g_fBold=mk(10,FW_SEMIBOLD); g_fTitle=mk(17,FW_SEMIBOLD); g_fSmall=mk(9,FW_NORMAL);
    EnumChildWindows(g_hwnd,[](HWND c,LPARAM)->BOOL{ SendMessageW(c,WM_SETFONT,(WPARAM)g_fBody,TRUE); return TRUE; },0);
    for(int pg:{PG_ADDONS,PG_BACKUPS}){ HWND lv=g_pages[pg].main; if(lv) SendMessageW(ListView_GetHeader(lv),WM_SETFONT,(WPARAM)g_fBold,TRUE); }
    SendMessageW(g_status,WM_SETFONT,(WPARAM)g_fSmall,TRUE);
}
static void ApplyTheme(){
    if(g_brBg){ DeleteObject(g_brBg); DeleteObject(g_brSide); DeleteObject(g_brEdit); } g_brBg=CreateSolidBrush(T().bg); g_brSide=CreateSolidBrush(T().sidebar); g_brEdit=CreateSolidBrush(T().listAlt);
    ThemeLists(); HICON ic=(HICON)LoadImageW(g_inst,MAKEINTRESOURCEW(g_theme==0?IDI_PG_SILVER:IDI_PG_PURPLE),IMAGE_ICON,0,0,LR_DEFAULTSIZE); if(ic) SendMessageW(g_hwnd,WM_SETICON,ICON_BIG,(LPARAM)ic);
    HICON sm=(HICON)LoadImageW(g_inst,MAKEINTRESOURCEW(g_theme==0?IDI_PG_SILVER:IDI_PG_PURPLE),IMAGE_ICON,GetSystemMetrics(SM_CXSMICON),GetSystemMetrics(SM_CYSMICON),0); if(sm) SendMessageW(g_hwnd,WM_SETICON,ICON_SMALL,(LPARAM)sm);
    RefreshSettingButtons(); RedrawWindow(g_hwnd,nullptr,nullptr,RDW_INVALIDATE|RDW_ERASE|RDW_ALLCHILDREN|RDW_UPDATENOW);
}
static void ShowPage(int p){
    g_page=p; for(int i=0;i<PG_COUNT;i++){ int sh=i==p?SW_SHOW:SW_HIDE; for(HWND b:g_pages[i].buttons) ShowWindow(b,sh); if(g_pages[i].main) ShowWindow(g_pages[i].main,sh); if(g_pages[i].second) ShowWindow(g_pages[i].second,sh); }
    switch(p){ case PG_HOME: RefreshHome(); break; case PG_ADDONS: if(haveExe()&&g_scan.empty()) DoScan(); else FillAddonList(); UpdateAddonDetails(); break; case PG_MEM: LaaCheck(); break; case PG_BACKUPS: FillBackups(); break; case PG_SETTINGS: LoadSettingsFromProfile(); SetMain(PG_SETTINGS,"These settings are stored per game in that game's Poneglyph\\poneglyph.ini and apply the next time the game starts.\n\n- Poneglyph engine OFF = the proxy only forwards to Windows' Direct3D 9 (nothing else runs).\n- Load add-ons OFF = no .asi/.dll add-ons are loaded.\n- Frame statistics = a tiny hook records frame times and a summary line is written every few seconds. OFF by default; there is no per-frame logging."); break; case PG_HELP: SetMain(PG_HELP,kHelpText); break; default: break; }
    for(int i=0;i<PG_COUNT;i++) if(HWND n=GetDlgItem(g_hwnd,ID_NAV+i)) InvalidateRect(n,nullptr,TRUE); InvalidateRect(g_hwnd,nullptr,FALSE);
}
static void Layout(){
    if(!g_hwnd) return; RECT rc; GetClientRect(g_hwnd,&rc); int hh=S(68), sw=S(210), sh=S(34), pad=S(16);
    int y=hh+S(14); for(int i=0;i<PG_COUNT;i++){ SetWindowPos(GetDlgItem(g_hwnd,ID_NAV+i),nullptr,S(12),y,sw-S(24),S(44),SWP_NOZORDER); y+=S(48); }
    int tabsX=sw+pad, tabW=std::min<int>(S(250),(int)((rc.right-tabsX-pad)/3-S(6))); for(int i=0;i<3;i++) SetWindowPos(GetDlgItem(g_hwnd,ID_PROFILE+i),nullptr,tabsX+i*(tabW+S(8)),S(14),tabW,S(40),SWP_NOZORDER);
    SetWindowPos(g_status,nullptr,sw+pad,rc.bottom-sh,rc.right-sw-pad-S(190),sh,SWP_NOZORDER); SetWindowPos(g_progress,nullptr,rc.right-S(180),rc.bottom-sh+S(10),S(164),S(14),SWP_NOZORDER);
    RECT c={sw+pad,hh+pad,rc.right-pad,rc.bottom-sh-S(8)}; int bw=S(196), bh=S(38), gap=S(8);
    for(int i=0;i<PG_COUNT;i++){ auto& pg=g_pages[i]; int x=c.left,yy=c.top; for(HWND b:pg.buttons){ if(x+bw>c.right&&x>c.left){ x=c.left; yy+=bh+gap; } SetWindowPos(b,nullptr,x,yy,bw,bh,SWP_NOZORDER); x+=bw+gap; }
        int top=yy+(pg.buttons.empty()?0:bh+S(12)); int bottom=c.bottom;
        if(pg.main&&pg.second){ int split=top+(bottom-top)*58/100; SetWindowPos(pg.main,nullptr,c.left,top,c.right-c.left,split-top,SWP_NOZORDER); SetWindowPos(pg.second,nullptr,c.left,split+S(8),c.right-c.left,bottom-split-S(8),SWP_NOZORDER); }
        else if(pg.main) SetWindowPos(pg.main,nullptr,c.left,top,c.right-c.left,bottom-top,SWP_NOZORDER); }
    InvalidateRect(g_hwnd,nullptr,TRUE);
}
static void BuildUi(){
    g_status=CreateWindowExW(0,L"STATIC",L"Ready",WS_CHILD|WS_VISIBLE|SS_LEFTNOWORDWRAP|SS_CENTERIMAGE,0,0,10,10,g_hwnd,(HMENU)9001,g_inst,nullptr);
    g_progress=CreateWindowExW(0,PROGRESS_CLASSW,L"",WS_CHILD|PBS_MARQUEE,0,0,10,10,g_hwnd,(HMENU)9002,g_inst,nullptr); SetWindowTheme(g_progress,L"",L"");
    for(int i=0;i<PG_COUNT;i++) CreateWindowExW(0,L"BUTTON",kPageNames[i],WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_OWNERDRAW,0,0,10,10,g_hwnd,(HMENU)(INT_PTR)(ID_NAV+i),g_inst,nullptr);
    for(int i=0;i<3;i++) CreateWindowExW(0,L"BUTTON",W(gameInfo(g_prof[i].id)->displayName).c_str(),WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_OWNERDRAW,0,0,10,10,g_hwnd,(HMENU)(INT_PTR)(ID_PROFILE+i),g_inst,nullptr);
    auto& H=g_pages[PG_HOME]; MakeBtn(PG_HOME,B_CHOOSE_EXE,L"Choose Game Executable"); MakeBtn(PG_HOME,B_SEARCH_DIR,L"Search Folder"); MakeBtn(PG_HOME,B_INSTALL_ENGINE,L"Install Poneglyph Engine"); MakeBtn(PG_HOME,B_REMOVE_ENGINE,L"Remove Poneglyph Engine"); MakeBtn(PG_HOME,B_LAUNCH,L"Launch Game"); MakeBtn(PG_HOME,B_OPEN_GAME,L"Open Game Folder"); H.main=MakeEdit(ID_EDIT);
    auto& A=g_pages[PG_ADDONS]; MakeBtn(PG_ADDONS,B_SCAN,L"Scan Add-ons"); MakeBtn(PG_ADDONS,B_ENABLE,L"Enable"); MakeBtn(PG_ADDONS,B_DISABLE,L"Disable"); MakeBtn(PG_ADDONS,B_INSTALL_ZIP,L"Install ZIP..."); MakeBtn(PG_ADDONS,B_REMOVE_PKG,L"Remove Package"); MakeBtn(PG_ADDONS,B_OPEN_ADDONS,L"Open Add-ons Folder");
    A.main=MakeList(g_hwnd,ID_LIST_ADDONS,{{L"Add-on",S(230)},{L"Status",S(150)},{L"Order",S(60)},{L"Architecture",S(110)},{L"Size",S(80)},{L"Compatibility",S(260)},{L"Details",S(420)}}); A.second=MakeEdit(ID_EDIT+1);
    MakeBtn(PG_PERF,B_MON_START,L"Start Monitoring"); MakeBtn(PG_PERF,B_MON_STOP,L"Stop Monitoring"); MakeBtn(PG_PERF,B_LAUNCH_MON,L"Launch + Monitor"); MakeBtn(PG_PERF,B_EXPORT_CSV,L"Export CSV"); MakeBtn(PG_PERF,B_REPORT,L"View Report"); MakeBtn(PG_PERF,B_SAVE_REPORT,L"Save Report"); MakeBtn(PG_PERF,B_COMPARE,L"Compare Frame Data..."); g_pages[PG_PERF].main=MakeEdit(ID_EDIT+2);
    MakeBtn(PG_MEM,B_LAA_CHECK,L"Check Status"); MakeBtn(PG_MEM,B_LAA_ENABLE,L"Enable 4 GB Flag"); MakeBtn(PG_MEM,B_LAA_RESTORE,L"Restore Original"); g_pages[PG_MEM].main=MakeEdit(ID_EDIT+3);
    MakeBtn(PG_BACKUPS,B_BK_RESTORE,L"Restore Selected"); MakeBtn(PG_BACKUPS,B_BK_OPEN,L"Open Backups Folder"); g_pages[PG_BACKUPS].main=MakeList(g_hwnd,ID_LIST_BACKUPS,{{L"Backup file",S(520)},{L"Size",S(110)},{L"Type",S(220)}}); g_pages[PG_BACKUPS].second=MakeEdit(ID_EDIT+4);
    MakeBtn(PG_SETTINGS,T_ENABLED,L"Poneglyph engine ON"); MakeBtn(PG_SETTINGS,T_LOADADDONS,L"Load add-ons"); MakeBtn(PG_SETTINGS,T_FRAMESTATS,L"Frame statistics (light)"); MakeBtn(PG_SETTINGS,C_SUMMARY,L"Summary every 10 s"); MakeBtn(PG_SETTINGS,C_LOGLEVEL,L"Log detail: Normal"); MakeBtn(PG_SETTINGS,C_THEME,L"Theme"); MakeBtn(PG_SETTINGS,B_SAVE_SETTINGS,L"Save Settings"); g_pages[PG_SETTINGS].main=MakeEdit(ID_EDIT+5);
    MakeBtn(PG_HELP,B_README,L"Open Read_Me"); MakeBtn(PG_HELP,B_OPEN_LOGS,L"Open Logs Folder"); MakeBtn(PG_HELP,B_SUPPORT,L"Create Support Report"); g_pages[PG_HELP].main=MakeEdit(ID_EDIT+6);
    for(int i=0;i<PG_COUNT;i++){ for(HWND b:g_pages[i].buttons) ShowWindow(b,SW_HIDE); if(g_pages[i].main) ShowWindow(g_pages[i].main,SW_HIDE); if(g_pages[i].second) ShowWindow(g_pages[i].second,SW_HIDE); }
    for(int pg:{PG_ADDONS,PG_BACKUPS}) SetWindowSubclass(ListView_GetHeader(g_pages[pg].main),HeaderProc,1,0);
    g_toggle[T_ENABLED]=true; g_toggle[T_LOADADDONS]=true; g_toggle[T_FRAMESTATS]=false;
}
static void OpenFolder(const fs::path& p){ std::error_code ec; fs::create_directories(p,ec); ShellExecuteW(g_hwnd,L"open",p.wstring().c_str(),nullptr,nullptr,SW_SHOW); }
static void SwitchProfile(int i){ g_cur=i; g_scan.clear(); g_samples.clear(); MonitorStop(); SaveManagerSettings(); ShowPage(g_page); for(int k=0;k<3;k++) InvalidateRect(GetDlgItem(g_hwnd,ID_PROFILE+k),nullptr,TRUE); }
static void OnCommand(int id){
    if(id>=ID_NAV&&id<ID_NAV+PG_COUNT){ ShowPage(id-ID_NAV); return; } if(id>=ID_PROFILE&&id<ID_PROFILE+3){ SwitchProfile(id-ID_PROFILE); return; }
    switch(id){
    case B_CHOOSE_EXE: ChooseExe(); break; case B_SEARCH_DIR: SearchFolder(); break; case B_INSTALL_ENGINE: InstallEngine(); break; case B_REMOVE_ENGINE: RemoveEngine(); break;
    case B_LAUNCH: LaunchGame(); break; case B_OPEN_GAME: if(haveExe()) OpenFolder(g_prof[g_cur].exe.parent_path()); break;
    case B_SCAN: DoScan(); break; case B_ENABLE: SetEnabledSel(true); break; case B_DISABLE: SetEnabledSel(false); break; case B_INSTALL_ZIP: InstallZip(); break; case B_REMOVE_PKG: RemovePkg(); break; case B_OPEN_ADDONS: if(haveExe()) OpenFolder(curPaths().addons); break;
    case B_MON_START: MonitorStart(); break; case B_MON_STOP: MonitorStop(); SetStatus("Monitoring stopped."); break; case B_LAUNCH_MON: MonitorStart(); LaunchGame(); break;
    case B_EXPORT_CSV: ExportCsv(); break; case B_REPORT: ShowReport(); break; case B_SAVE_REPORT: SaveReport(); break; case B_COMPARE: CompareFrames(); break;
    case B_LAA_CHECK: LaaCheck(); break; case B_LAA_ENABLE: LaaEnable(); break; case B_LAA_RESTORE: LaaRestore(); break;
    case B_BK_RESTORE: RestoreSelectedBackup(); break; case B_BK_OPEN: if(haveExe()) OpenFolder(curPaths().backups); break;
    case T_ENABLED: case T_LOADADDONS: case T_FRAMESTATS: g_toggle[id]=!g_toggle[id]; for(HWND h:g_pages[PG_SETTINGS].buttons) InvalidateRect(h,nullptr,TRUE); break;
    case C_SUMMARY: { int i=0; for(int k=0;k<4;k++) if(kSummary[k]==g_summarySecs) i=k; g_summarySecs=kSummary[(i+1)%4]; RefreshSettingButtons(); break; }
    case C_LOGLEVEL: g_logLevel=(g_logLevel+1)%4; RefreshSettingButtons(); break;
    case C_THEME: g_theme=1-g_theme; SaveManagerSettings(); ApplyTheme(); break;
    case B_SAVE_SETTINGS: SaveSettings(); break;
    case B_README: { fs::path r=exeDir()/"Read_Me.txt"; ShellExecuteW(g_hwnd,L"open",r.wstring().c_str(),nullptr,nullptr,SW_SHOW); break; }
    case B_OPEN_LOGS: if(haveExe()) OpenFolder(curPaths().logs); else OpenFolder(appDataDir()); break; case B_SUPPORT: if(haveExe()) SaveReport(true); else Msg("Choose the game first","Select this game's executable first."); break;
    }
}
static LRESULT OnNotify(LPARAM l){
    NMHDR* h=(NMHDR*)l; if(h->idFrom!=ID_LIST_ADDONS&&h->idFrom!=ID_LIST_BACKUPS) return 0;
    if(h->code==LVN_ITEMCHANGED&&h->idFrom==ID_LIST_ADDONS){ UpdateAddonDetails(); return 0; }
    if(h->code==NM_CUSTOMDRAW){ auto* cd=(NMLVCUSTOMDRAW*)l;
        if(cd->nmcd.dwDrawStage==CDDS_PREPAINT) return CDRF_NOTIFYITEMDRAW;
        if(cd->nmcd.dwDrawStage==CDDS_ITEMPREPAINT){ bool sel=ListView_GetItemState(h->hwndFrom,(int)cd->nmcd.dwItemSpec,LVIS_SELECTED)!=0; int row=(int)cd->nmcd.dwItemSpec;
            cd->clrText=T().text; cd->clrTextBk=sel?T().hot:(row%2?Mix(T().listAlt,T().panelLo,0.35):T().listAlt); if(sel){ cd->nmcd.uItemState&=~(CDIS_SELECTED|CDIS_FOCUS); cd->clrText=RGB(255,255,255); } return CDRF_NEWFONT; } }
    return 0;
}
static void PaintMain(HDC dc,RECT rc){
    HBRUSH b=g_brBg; FillRect(dc,&rc,b); int hh=S(68), sw=S(210); RECT side={0,hh,sw,rc.bottom}; FillRect(dc,&side,g_brSide); RECT hd={0,0,rc.right,hh}; VGrad(dc,hd,T().panelHi,T().panelLo);
    HPEN p=CreatePen(PS_SOLID,1,T().border); HGDIOBJ op=SelectObject(dc,p); MoveToEx(dc,0,hh,nullptr); LineTo(dc,rc.right,hh); MoveToEx(dc,sw,hh,nullptr); LineTo(dc,sw,rc.bottom); SelectObject(dc,op); DeleteObject(p);
    SetBkMode(dc,TRANSPARENT); SelectObject(dc,g_fTitle); SetTextColor(dc,T().text); RECT tr={S(18),S(8),sw,S(40)}; DrawTextW(dc,L"PONEGLYPH",-1,&tr,DT_LEFT|DT_SINGLELINE); SelectObject(dc,g_fSmall); SetTextColor(dc,Mix(T().text,T().dim,0.4)); RECT sr={S(20),S(40),sw,S(64)}; DrawTextW(dc,L"Mod Loader  v" PG_MANAGER_VERSION,-1,&sr,DT_LEFT|DT_SINGLELINE);
}
static LRESULT CALLBACK WndProc(HWND h,UINT m,WPARAM w,LPARAM l){
    switch(m){
    case WM_CREATE: g_hwnd=h; g_dpi=GetDpiForWindow(h); BuildUi(); ApplyFonts(); ApplyTheme(); Layout(); ShowPage(PG_HOME); return 0;
    case WM_SIZE: Layout(); return 0;
    case WM_GETMINMAXINFO: { auto* mm=(MINMAXINFO*)l; mm->ptMinTrackSize.x=S(1000); mm->ptMinTrackSize.y=S(640); return 0; }
    case WM_DPICHANGED: g_dpi=HIWORD(w); { RECT* r=(RECT*)l; SetWindowPos(h,nullptr,r->left,r->top,r->right-r->left,r->bottom-r->top,SWP_NOZORDER|SWP_NOACTIVATE); } ApplyFonts(); Layout(); return 0;
    case WM_ERASEBKGND: return 1;
    case WM_PAINT: { PAINTSTRUCT ps; HDC dc=BeginPaint(h,&ps); RECT rc; GetClientRect(h,&rc); HDC mem=CreateCompatibleDC(dc); HBITMAP bm=CreateCompatibleBitmap(dc,rc.right,rc.bottom); HGDIOBJ ob=SelectObject(mem,bm); PaintMain(mem,rc); BitBlt(dc,0,0,rc.right,rc.bottom,mem,0,0,SRCCOPY); SelectObject(mem,ob); DeleteObject(bm); DeleteDC(mem); EndPaint(h,&ps); return 0; }
    case WM_DRAWITEM: DrawOwner((DRAWITEMSTRUCT*)l); return TRUE;
    case WM_CTLCOLOREDIT: case WM_CTLCOLORSTATIC: { HDC dc=(HDC)w; HWND c=(HWND)l; if(c==g_status){ SetTextColor(dc,T().dim); SetBkColor(dc,T().bg); return (LRESULT)g_brBg; } SetTextColor(dc,T().text); SetBkColor(dc,T().listAlt); return (LRESULT)g_brEdit; }
    case WM_COMMAND: if(HIWORD(w)==BN_CLICKED||HIWORD(w)==0) OnCommand(LOWORD(w)); return 0;
    case WM_NOTIFY: return OnNotify(l);
    case WM_TIMER: if(w==1) MonitorTick(); return 0;
    case WM_APP_DONE: { auto* d=(std::function<void()>*)l; g_busy=false; SetStatus("Ready"); if(*d) (*d)(); delete d; return 0; }
    case WM_CLOSE: if(g_busy&&!Ask("Operation running","An operation is still running. Closing now could interrupt it. Close anyway?")) return 0; MonitorStop(); SaveManagerSettings(); DestroyWindow(h); return 0;
    case WM_DESTROY: g_mlog.log(LogLevel::Info,"manager closed"); PostQuitMessage(0); return 0;
    }
    return DefWindowProcW(h,m,w,l);
}
int WINAPI wWinMain(HINSTANCE hi,HINSTANCE,LPWSTR,int show){
    g_inst=hi; typedef BOOL(WINAPI* PFN)(DPI_AWARENESS_CONTEXT); if(auto f=(PFN)GetProcAddress(GetModuleHandleW(L"user32.dll"),"SetProcessDpiAwarenessContext")) f(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED); INITCOMMONCONTROLSEX ic{sizeof ic,ICC_LISTVIEW_CLASSES|ICC_PROGRESS_CLASS|ICC_STANDARD_CLASSES}; InitCommonControlsEx(&ic);
    std::error_code ec; fs::create_directories(appDataDir()/"Logs",ec); g_mlog.open(appDataDir()/"Logs"/"manager.log",512*1024,3); g_oplog.open(appDataDir()/"Logs"/"operations.log",512*1024,3);
    g_mlog.log(LogLevel::Info,"==== Poneglyph Manager %s started ====",PG_MANAGER_VERSION); LoadManagerSettings();
    WNDCLASSEXW wc{sizeof wc}; wc.lpfnWndProc=WndProc; wc.hInstance=hi; wc.hCursor=LoadCursorW(nullptr,IDC_ARROW); wc.lpszClassName=L"PoneglyphManagerWnd"; wc.hIcon=LoadIconW(hi,MAKEINTRESOURCEW(IDI_PG_SILVER)); wc.style=CS_HREDRAW|CS_VREDRAW; RegisterClassExW(&wc);
    HWND h=CreateWindowExW(0,wc.lpszClassName,L"Poneglyph Mod Loader",WS_OVERLAPPEDWINDOW|WS_CLIPCHILDREN,CW_USEDEFAULT,CW_USEDEFAULT,1280,800,nullptr,nullptr,hi,nullptr); if(!h) return 1;
    ShowWindow(h,show); UpdateWindow(h); MSG m; while(GetMessageW(&m,nullptr,0,0)>0){ if(!IsDialogMessageW(h,&m)){ TranslateMessage(&m); DispatchMessageW(&m); } } return (int)m.wParam;
}
