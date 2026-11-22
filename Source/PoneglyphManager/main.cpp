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
#include <richedit.h>
#include <dbt.h>
#include <functional>
#include <thread>
#include <mutex>
#include <map>
#include <fstream>
#include <algorithm>
#include <cmath>
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
#include "poneglyph/launchopts.hpp"
#include "controllers.hpp"
#define PG_MANAGER_VERSION "1.0.0"
using namespace pg;

// ------------------------------------------------------------------ small helpers
static std::wstring W(const std::string& s){ if(s.empty()) return L""; int n=MultiByteToWideChar(CP_UTF8,0,s.data(),(int)s.size(),nullptr,0); std::wstring w((size_t)n,L'\0'); MultiByteToWideChar(CP_UTF8,0,s.data(),(int)s.size(),&w[0],n); return w; }
static std::string U(const std::wstring& w){ if(w.empty()) return ""; int n=WideCharToMultiByte(CP_UTF8,0,w.data(),(int)w.size(),nullptr,0,nullptr,nullptr); std::string s((size_t)n,'\0'); WideCharToMultiByte(CP_UTF8,0,w.data(),(int)w.size(),&s[0],n,nullptr,nullptr); return s; }
static std::wstring Crlf(const std::string& s){ std::wstring w=W(s),o; for(size_t i=0;i<w.size();i++){ if(w[i]==L'\n'&&(i==0||w[i-1]!=L'\r')) o+=L"\r\n"; else o+=w[i]; } return o; }

// ------------------------------------------------------------------ state
enum { PG_HOME,PG_ADDONS,PG_PERF,PG_MEM,PG_OPTIONS,PG_BACKUPS,PG_SETTINGS,PG_HELP,PG_COUNT };
static const wchar_t* kPageNames[PG_COUNT]={L"Home",L"Add-ons",L"Performance",L"Memory (4 GB)",L"Game Options",L"Backups",L"Settings",L"Help"};
enum { ID_NAV=100, ID_PROFILE=110, ID_APPBTN=200 };
enum { B_CHOOSE_EXE=200,B_SEARCH_DIR,B_INSTALL_ENGINE,B_REMOVE_ENGINE,B_LAUNCH,B_OPEN_GAME,
       B_SCAN,B_ENABLE,B_DISABLE,B_INSTALL_ZIP,B_REMOVE_PKG,B_OPEN_ADDONS,
       B_MON_START,B_MON_STOP,B_EXPORT_CSV,B_REPORT,B_SAVE_REPORT,B_COMPARE,B_LAUNCH_MON,
       B_LAA_CHECK,B_LAA_ENABLE,B_LAA_RESTORE,
       B_BK_RESTORE,B_BK_OPEN,
       T_ENABLED,T_LOADADDONS,T_FRAMESTATS,C_SUMMARY,C_LOGLEVEL,C_THEME,B_SAVE_SETTINGS,
       B_README,B_OPEN_LOGS,B_SUPPORT,
       H_VOICE,O_FULLSCREEN,O_VOICEJP,O_DECIDE,C_RES,C_MSAA,C_SHADOW,B_OPT_SAVE,ID_MEMCARD };
#define WM_APP_DONE (WM_APP+1)
struct Page { std::vector<HWND> buttons; HWND main=nullptr, second=nullptr, card=nullptr; };
static HINSTANCE g_inst; static HWND g_hwnd, g_status, g_progress; static Page g_pages[PG_COUNT];
static int g_page=PG_HOME, g_theme=0, g_cur=0, g_dpi=96; static HFONT g_fBody,g_fBold,g_fTitle,g_fSmall,g_fText,g_fMono,g_fHead; static int g_nextStep=0; static wchar_t g_faceBody[LF_FACESIZE]=L"Segoe UI",g_faceMono[LF_FACESIZE]=L"Consolas",g_faceHead[LF_FACESIZE]=L"Segoe UI";
static int g_laaState=-1; static LaunchOptions g_opt; static std::vector<ControllerInfo> g_ctl;
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
static bool IsHeaderLine(const std::wstring& l){
    size_t i=0; while(i<l.size()&&l[i]==L' ') i++; if(i>=l.size()) return false; std::wstring t=l.substr(i);
    int up=0,lo=0; for(wchar_t c:t){ if(c>=L'A'&&c<=L'Z') up++; else if(c>=L'a'&&c<=L'z') lo++; } if(lo==0&&up>=4) return true;
    int tu=0,tl=0; for(wchar_t c:t){ if(c==L' '||c==L'(') break; if(c>=L'A'&&c<=L'Z') tu++; else if(c>=L'a'&&c<=L'z') tl++; } return tu>=4&&tl==0;
}
static void PutText(HWND h,const std::string& text,bool mono=false,bool firstIsTitle=false){
    if(!h) return; std::wstring w=W(text), body; body.reserve(w.size()); for(wchar_t c:w){ if(c==L'\r') continue; body+=(c==L'\n')?L'\r':c; }
    SETTEXTEX st{ST_DEFAULT,1200}; SendMessageW(h,EM_SETTEXTEX,(WPARAM)&st,(LPARAM)body.c_str()); SendMessageW(h,EM_SETBKGNDCOLOR,0,(LPARAM)T().listAlt);
    CHARFORMAT2W cf{}; cf.cbSize=sizeof cf; cf.dwMask=CFM_COLOR|CFM_FACE|CFM_SIZE|CFM_BOLD; cf.dwEffects=0; cf.crTextColor=T().text; cf.yHeight=MulDiv((mono?11:12)*20,g_dpi,96); lstrcpyW(cf.szFaceName,mono?g_faceMono:g_faceBody);
    SendMessageW(h,EM_SETSEL,0,-1); SendMessageW(h,EM_SETCHARFORMAT,SCF_SELECTION,(LPARAM)&cf);
    PARAFORMAT2 pf{}; pf.cbSize=sizeof pf; pf.dwMask=PFM_SPACEAFTER|PFM_LINESPACING|PFM_STARTINDENT|PFM_RIGHTINDENT; pf.dySpaceAfter=40; pf.bLineSpacingRule=5; pf.dyLineSpacing=24; pf.dxStartIndent=240; pf.dxRightIndent=200;
    SendMessageW(h,EM_SETPARAFORMAT,0,(LPARAM)&pf);
    LONG pos=0; size_t a=0; bool first=true;
    while(a<=body.size()){ size_t e=body.find(L'\r',a); if(e==std::wstring::npos) e=body.size(); std::wstring line=body.substr(a,e-a); LONG len=(LONG)line.size();
        auto color=[&](LONG from,LONG to,COLORREF c,bool bold,int pt){ CHARFORMAT2W f{}; f.cbSize=sizeof f; f.dwMask=CFM_COLOR|CFM_BOLD|(pt?CFM_SIZE:0); f.dwEffects=bold?CFE_BOLD:0; f.crTextColor=c; if(pt) f.yHeight=MulDiv(pt*20,g_dpi,96); SendMessageW(h,EM_SETSEL,from,to); SendMessageW(h,EM_SETCHARFORMAT,SCF_SELECTION,(LPARAM)&f); };
        if(len>0){
            if(first&&firstIsTitle) color(pos,pos+len,T().header,true,mono?12:15);
            else if(IsHeaderLine(line)) color(pos,pos+len,T().header,true,0);
            else { bool rule=len>=10; for(wchar_t c:line) if(c!=L'='&&c!=L'-') rule=false; if(rule) color(pos,pos+len,T().border,false,0); }
            size_t lb=line.find(L'['), rb=line.find(L']'); if(lb!=std::wstring::npos&&rb!=std::wstring::npos&&rb>lb&&line.find(L"[ ")==lb){ std::wstring tok=line.substr(lb,rb-lb+1); COLORREF c=T().dim; if(tok.find(L"DONE")!=std::wstring::npos) c=T().ok; else if(tok.find(L"TO DO")!=std::wstring::npos) c=T().accent; else if(tok.find(L"NEEDS")!=std::wstring::npos) c=T().warn; color(pos+(LONG)lb,pos+(LONG)rb+1,c,true,0); }
        }
        first=false; pos+=len+1; if(e>=body.size()) break; a=e+1; }
    SendMessageW(h,EM_SETSEL,0,0); SendMessageW(h,WM_VSCROLL,SB_TOP,0);
}
static void SetMain(int pg,const std::string& text){ PutText(g_pages[pg].main,text,pg==PG_PERF,pg==PG_HOME); }
static void RunAsync(const std::string& msg,std::function<void()> work,std::function<void()> done){
    if(g_busy){ Msg("Please wait","Another operation is still running. It will finish in a moment."); return; }
    g_busy=true; SetStatus(msg,true);
    std::thread([work,done]{ work(); auto* d=new std::function<void()>(done); PostMessageW(g_hwnd,WM_APP_DONE,0,(LPARAM)d); }).detach();
}
// ------------------------------------------------------------------ settings persistence
static void LoadManagerSettings(){
    IniFile i; i.load(appDataDir()/"manager.ini"); g_theme=std::clamp(i.getInt("UI","Theme",0),0,kThemeCount-1); g_cur=std::clamp(i.getInt("UI","Profile",0),0,2);
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
    HomeState h; auto gi=curGame(); std::string title=gi->displayName, t;
    const std::string DONE="[ DONE ]", TODO="[ TO DO ]", WAIT="[ waiting ]", WARN="[ NEEDS YOU ]";
    if(!haveExe()){
        g_nextStep=B_CHOOSE_EXE; h.label="Let's begin";
        h.text="WELCOME!\n\nLet's get "+title+" ready. It takes three small steps.\nNothing on your computer changes until you press a button AND say Yes.\n\n"
               "STEP 1 - Find your game          "+TODO+"\n   Press the bright button  'Find My Game'.\n   A small window opens. Look for the file called  "+gi->exeName+"  and double-click it.\n   (It is inside the game's folder, here: "+gi->commonRelPath+")\n   Cannot find it? Press 'Search a Folder for My Game' and choose the game's main folder.\n\n"
               "STEP 2 - Install Poneglyph        "+WAIT+"\nSTEP 3 - Play the game            "+WAIT+"\n"; return h; }
    auto pp=curPaths(); auto chk=verifyGameExe(g_prof[g_cur].exe,g_prof[g_cur].id);
    if(!chk.ok){ g_nextStep=B_CHOOSE_EXE; h.label="Needs attention";
        h.text="STEP 1 - Find your game          "+WARN+"\n\nThe file that was chosen cannot be used.\n\nWhat I found : "+chk.message+"\nWhy it matters: Poneglyph only works with the real game file, so it can never harm anything else.\nWhat to do   : press 'Find My Game' and choose  "+gi->exeName+".\n"; return h; }
    ProxyKind k=classifyD3D9(pp.gameDir/"d3d9.dll"); IniFile cfg; cfg.load(pp.config); bool enabled=cfg.getBool("General","Enabled",true);
    ScanOptions so; so.game=g_prof[g_cur].id; auto all=scanAddons(pp.addons,cfg,so); int ready=0,dis=0,bad=0; for(auto& a:all){ if(a.status==AddonStatus::Ready) ready++; else if(a.status==AddonStatus::Disabled) dis++; else bad++; }
    auto recs=readLoadStatus(pp.status); IniFile st; bool started=st.load(pp.status)&&st.has("Engine","Written"); bool failed=false; for(auto& r:recs) if(r.result=="Failed") failed=true;
    bool foreign=(k==ProxyKind::NovaFix||k==ProxyKind::Dxvk||k==ProxyKind::Other); std::string headline;
    t+="STEP 1 - Find your game          "+DONE+"\n   Found and checked: "+pathToUtf8(g_prof[g_cur].exe)+"\n\n";
    if(k==ProxyKind::None){ g_nextStep=B_INSTALL_ENGINE; headline="Next: press 'Install Poneglyph'";
        t+="STEP 2 - Install Poneglyph        "+TODO+"\n   Press the bright button  'Install Poneglyph'.\n   It first saves a safe copy of anything it replaces, and you can undo it any time.\n\nSTEP 3 - Play the game            "+WAIT+"\n"; }
    else if(foreign){ g_nextStep=B_INSTALL_ENGINE; headline="One thing needs your attention";
        t+="STEP 2 - Install Poneglyph        "+WARN+"\n   Another helper file ("+std::string(proxyKindText(k))+") is already installed for this game. Only one can work at a time.\n   Poneglyph will NOT replace it unless you press 'Install Poneglyph' and say Yes. Your file is saved first and can be brought back.\n\nSTEP 3 - Play the game            "+WAIT+"\n"; }
    else { g_nextStep=B_LAUNCH; headline=enabled?(failed||bad?"One thing needs your attention":"Everything looks good"):"Poneglyph is switched off";
        t+="STEP 2 - Install Poneglyph        "+DONE+(enabled?"":"  (but switched OFF in Settings)")+"\n\nSTEP 3 - Play the game            "+TODO+"\n   Press  'Play the Game'  (or start the game from Steam as usual).\n"; }
    t+="\n-------------------------------------------------------------\nMORE INFORMATION (you can ignore this)\n";
    t+=std::string("   4 GB memory setting : ")+(chk.pe.laa?"ON  - the game may use up to 4 GB.":"OFF - you can turn it on in 'Memory (4 GB)'.")+"\n";
    t+="   Mods (add-ons)      : "+std::to_string(ready)+" ready, "+std::to_string(dis)+" switched off, "+std::to_string(bad)+" with a problem (see the Add-ons page)\n";
    g_ctl=DetectControllers(); { std::string cl; for(auto& c:g_ctl) cl+=(cl.empty()?"":", ")+c.name; t+="   Controllers         : "+(g_ctl.empty()?std::string("none detected - plug yours in BEFORE you start the game (these games look for controllers only when they start)"):cl)+"\n"; }
    if(started){ t+="   Last game start     : "+st.get("Engine","Written")+(recs.empty()?" - Poneglyph ran fine; there were no mods to load.":"")+"\n"; for(auto& r:recs){ char b[200]; snprintf(b,sizeof b,"      %-30s %-9s %.1f ms",r.file.c_str(),r.result.c_str(),r.ms); t+=b; if(r.result=="Failed") t+="   <- "+r.detail; t+="\n"; } }
    else t+="   Last game start     : none yet\n";
    if(g_mon.h) t+="   The game is running now. Changes you make will work after you restart the game.\n";
    t+="   Safety copies saved : "+pathToUtf8(pp.backups)+"\n   Log files           : "+pathToUtf8(pp.logs)+"\n";
    h.label=headline; h.text=headline+"\n=============================================================\n\n"+t; return h;
}
static void RefreshHome(){ auto h=ComputeHome(); SetMain(PG_HOME,h.text); SetStatus(h.label+"   |   "+curGame()->displayName); for(HWND b:g_pages[PG_HOME].buttons) InvalidateRect(b,nullptr,TRUE); }
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
    int r=SelRow(LvAddons()); if(r<0||r>=(int)g_scan.size()){ PutText(g_pages[PG_ADDONS].second,"Select an add-on in the list to see its details."); return; }
    auto& a=g_scan[(size_t)r]; std::string t="File: "+pathToUtf8(a.path)+"\nStatus: "+addonStatusText(a.status)+"\n"; if(!a.reason.empty()) t+="Why: "+a.reason+"\n";
    t+="SHA-256: "+(a.sha256.empty()?std::string("(not computed)"):a.sha256)+"\nSize: "+formatBytes(a.size)+"   Type: "+a.kind+"   Architecture: "+(a.pe.valid?a.pe.archName():"unknown")+"\nCompatibility: "+a.compat+"\n";
    if(a.meta.present){ t+="Metadata: "+a.meta.name+" "+a.meta.version+" by "+a.meta.author+"\n"; if(!a.meta.description.empty()) t+=a.meta.description+"\n"; } else t+="Metadata: none (compatibility is not assumed).\n";
    PutText(g_pages[PG_ADDONS].second,t);
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
        if(res->empty()){ Msg("Not found","No "+std::string(curGame()->exeName)+" was found inside that folder (searched 5 levels deep). Choose the folder that contains the game, or use 'Find My Game'.",MB_ICONWARNING); SetStatus("Search finished: not found"); return; }
        auto chk=verifyGameExe(*res,gid); if(!chk.ok){ Msg("Found, but not usable",chk.message,MB_ICONWARNING); return; } g_prof[g_cur].exe=*res; SaveManagerSettings(); RefreshHome(); });
}
static void LaaEnable(bool ask);
static void InstallEngine(){
    if(!haveExe()){ Msg("Please do Step 1 first","Press 'Find My Game' first, so Poneglyph knows which game to set up."); return; }
    auto chk=verifyGameExe(g_prof[g_cur].exe,g_prof[g_cur].id); if(!chk.ok){ Msg("This game file cannot be used",chk.message,MB_ICONWARNING); return; }
    if(g_mon.h){ Msg("The game is running","Please close the game first, then press this button again."); return; }
    fs::path eng=exeDir()/"Engine"/"d3d9.dll"; std::error_code ec; if(!fs::exists(eng,ec)){ Msg("A Poneglyph file is missing","Poneglyph's own file  Engine\\d3d9.dll  is not next to Poneglyph.exe.\n\nPlease unzip the WHOLE Poneglyph download again (all files together, in one folder) and try again.\n\nLooked here: "+pathToUtf8(eng),MB_ICONERROR); return; }
    auto pp=curPaths(); InstallOptions io; ProxyKind k=classifyD3D9(pp.gameDir/"d3d9.dll");
    if(k==ProxyKind::NovaFix||k==ProxyKind::Dxvk||k==ProxyKind::Other){
        if(!Ask("Another helper file is already installed",std::string("This game already has a file named d3d9.dll made by: ")+proxyKindText(k)+".\n\nWHY THIS MATTERS\nOnly one such file can work at a time.\n\nIF YOU PRESS YES\nYour current file is copied to a safe backup folder first (and checked to be an exact copy). Then Poneglyph is installed.\n\nHOW TO UNDO\nPress 'Uninstall Poneglyph (undo)' and your old file comes back.\n\nInstall Poneglyph anyway?")) return;
        io.replaceForeignProxy=true; }
    auto res=installEngine(pp,g_prof[g_cur].id,eng,io,g_oplog);
    if(!res.ok){ Msg("Poneglyph was NOT installed",res.message+"\n\nIf anything had been changed, it was put back the way it was.",MB_ICONWARNING); RefreshHome(); return; }
    std::string what; for(auto& c:res.changed) what+="  - "+c+"\n";
    Msg("Poneglyph is installed!","All done. Poneglyph is now set up for "+std::string(curGame()->displayName)+".\n\nWHAT WAS CHANGED\n"+what+"\nNOTHING ELSE on your computer was touched.\n\nTO UNDO ANYTHING\nPress 'Uninstall Poneglyph (undo)' on the Home page.");
    RefreshHome();
    auto pe=inspectPe(g_prof[g_cur].exe,{false,false,false});
    if(pe.valid&&pe.isX86()&&!pe.laa){
        if(Ask("One more optional step: the 4 GB setting","Would you like to turn on the \"4 GB setting\" for this game now?\n\nWHAT IT IS\nThis game is an older 32-bit program. By default Windows lets it use only 2 GB of memory. The 4 GB setting is a single switch inside the game's .exe file that lets it use up to 4 GB.\n\nWHY YOU MAY WANT IT\nMods, bigger textures and long play sessions can make the game run out of memory and crash. More room helps prevent that.\n\nWHAT IT DOES NOT DO\nIt does not make the game faster, and it never goes above 4 GB.\n\nIS IT SAFE?\nBefore changing anything, Poneglyph saves an exact copy of the game's .exe and checks the copy. It changes only that one switch. You can undo it any time: Memory (4 GB) page, then 'Restore Original'. (A few copy-protected editions may refuse a changed .exe. If the game will not start, use Restore Original.)\n\nYES = turn it on now      NO = skip it (you can do it later)")) LaaEnable(false);
    }
}
static void RemoveEngine(){
    if(!haveExe()) return; if(g_mon.h){ Msg("Game is running","Close the game first."); return; }
    if(!Ask("Remove Poneglyph engine","Remove Poneglyph's d3d9.dll from this game's folder?\n\nIf a previous d3d9.dll was backed up, it is restored. Your add-ons, settings and saves are not deleted.")) return;
    auto res=uninstallEngine(curPaths(),g_oplog); Msg(res.ok?"Done":"Not removed",res.message+(res.changed.empty()?"":"\n\n"+join(res.changed,"\n")),res.ok?MB_ICONINFORMATION:MB_ICONWARNING); RefreshHome();
}
static DWORD LaunchGame(){
    if(!haveExe()){ Msg("Choose the game first","Select this game's executable first."); return 0; } auto chk=verifyGameExe(g_prof[g_cur].exe,g_prof[g_cur].id); if(!chk.ok){ Msg("Cannot launch",chk.message,MB_ICONWARNING); return 0; }
    std::wstring exe=g_prof[g_cur].exe.wstring(), dir=g_prof[g_cur].exe.parent_path().wstring(), cmd=L"\""+exe+L"\"";
    { IniFile c; c.load(curPaths().config); auto o=loadLaunchOptions(c); auto bad=validateLaunchOptions(o); if(!bad.empty()) Msg("Game Options need fixing",join(bad,"\n")+"\n\nThe game will start with its normal settings. Open 'Game Options' to fix this.",MB_ICONWARNING); else { auto args=buildLaunchArgs(g_prof[g_cur].id,o); if(!args.empty()){ cmd+=L" "+W(joinArgs(args)); g_mlog.log(LogLevel::Info,"launch options: %s",joinArgs(args).c_str()); } } }
    STARTUPINFOW si{}; si.cb=sizeof si; PROCESS_INFORMATION pi{};
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
        ProcSample s; s.time=nowStamp(); s.cpuPct=pct; s.workingSetBytes=pmc.WorkingSetSize; if(g_samples.size()<36000) g_samples.push_back(s); if(g_page==PG_MEM&&g_pages[PG_MEM].card) InvalidateRect(g_pages[PG_MEM].card,nullptr,TRUE);
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
    if(!haveExe()){ SetMain(PG_MEM,std::string(kLaaText)+"\nNo executable selected for this game yet."); return; } auto p=inspectPe(g_prof[g_cur].exe,{false,false,true}); g_laaState=p.valid?(p.laa?1:0):-1; std::string t="CURRENT STATE\nFile: "+pathToUtf8(g_prof[g_cur].exe)+"\n";
    if(!p.valid) t+="Could not read the file: "+p.error+"\n"; else { t+="Architecture: "+p.archName()+"\n4 GB flag: "+std::string(p.laa?"ON - already Large Address Aware. Nothing to do.":"OFF - you can enable it with the button above.")+"\n"; }
    auto pp=curPaths(); std::error_code ec; int n=0; if(fs::is_directory(pp.backups,ec)) for(auto& e:fs::directory_iterator(pp.backups,ec)) if(e.path().extension()==".pgbak"&&e.path().filename().string().rfind(g_prof[g_cur].exe.filename().string(),0)==0) n++;
    t+="Backups of this executable: "+std::to_string(n)+"\n\n"+kLaaText; SetMain(PG_MEM,t); if(g_pages[PG_MEM].card) InvalidateRect(g_pages[PG_MEM].card,nullptr,TRUE);
}
static void LaaEnable(bool ask){
    if(!haveExe()){ Msg("Choose the game first","Select this game's executable first."); return; } if(g_mon.h){ Msg("Game is running","Close the game first."); return; }
    auto p=inspectPe(g_prof[g_cur].exe,{false,false,false}); if(p.valid&&p.laa){ Msg("Already enabled","This executable is already Large Address Aware. No change needed."); return; }
    if(ask&&!Ask("Enable the 4 GB flag?","This changes ONE flag in "+std::string(curGame()->exeName)+". A verified backup is made first and you can restore it any time.\n\nIt allows up to 4 GB of address space on 64-bit Windows. It does NOT allow more than 4 GB and does not add RAM or speed.\n\nSome copy-protected editions may reject a modified executable; if the game does not start, use 'Restore Original'.\n\nContinue?")) return;
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
    HWND lv=g_pages[PG_BACKUPS].main; ListView_DeleteAllItems(lv); if(!haveExe()){ PutText(g_pages[PG_BACKUPS].second,"Choose a game first."); return; }
    auto pp=curPaths(); std::error_code ec; if(!fs::is_directory(pp.backups,ec)){ PutText(g_pages[PG_BACKUPS].second,"No backups yet. Poneglyph saves a safe copy automatically before it changes any game file."); return; }
    for(auto& e:fs::directory_iterator(pp.backups,ec)){ if(!e.is_regular_file(ec)) continue; auto n=pathToUtf8(e.path().filename()); int r=ListAdd(lv,n); ListSet(lv,r,1,formatBytes(fs::file_size(e.path(),ec)));
        std::string kind=e.path().extension()==".pgbak"?(n.rfind("d3d9.dll",0)==0?"Previous d3d9.dll":"Game executable"):"Record"; ListSet(lv,r,2,kind); }
    PutText(g_pages[PG_BACKUPS].second,"Folder: "+pathToUtf8(pp.backups)+"\nSelect a 'Game executable' backup and click 'Restore Selected'. To get your previous d3d9.dll back, use 'Uninstall Poneglyph (undo)' on the Home page - it verifies and restores it for you.");
}
static void RestoreSelectedBackup(){
    HWND lv=g_pages[PG_BACKUPS].main; int r=SelRow(lv); if(r<0){ Msg("Select a backup","Click a backup in the list first."); return; } wchar_t b[260]; ListView_GetItemText(lv,r,0,b,260); fs::path bk=curPaths().backups/b;
    if(bk.filename().string().rfind(g_prof[g_cur].exe.filename().string()+".",0)!=0){ Msg("Use the Home page","Only game-executable backups are restored here. For the previous d3d9.dll use 'Uninstall Poneglyph (undo)' on Home."); return; }
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
"WELCOME TO PONEGLYPH\n\nPoneglyph helps your game run mods safely. You do not need to know anything technical. Follow the three steps on the Home page.\n\n"
"THE THREE STEPS\n  1. 'Find My Game'  - choose the game's main file (for example ffxiii2img.exe).\n  2. 'Install Poneglyph' - Poneglyph saves safety copies first, then installs. It also offers an optional 4 GB memory setting.\n  3. 'Play the Game' - or start the game from Steam as you always do.\n\n"
"TO ADD A MOD\n  Go to 'Add-ons', press 'Install ZIP...' and choose the mod's .zip file. Poneglyph checks it is safe and 32-bit before installing. You can switch any mod off and on again without deleting it.\n\n"
"TO UNDO EVERYTHING\n  Home page, press 'Uninstall Poneglyph (undo)'. Your old files come back. Your saves and mods are never deleted.\n\n"
"WHAT THE WORDS ON HOME MEAN\n  Everything looks good - nothing to do.     One thing needs your attention - read the lines on Home.     Poneglyph is switched off - turn it on in Settings.\n\n"
"IF THE GAME WILL NOT START\n  Poneglyph turns a mod off by itself if the last start crashed while loading it. You can also use 'Uninstall Poneglyph (undo)'. Then press Help > 'Create Support Report' and send the file to the person helping you, together with Logs\\poneglyph.log.\n\n"
"GAME OPTIONS (FFXIII and FFXIII-2 only)\n  Resolution, window mode, anti-aliasing, shadow quality, Japanese voice and swapping Confirm/Cancel. They are applied when you start the game with 'Play the Game'.\n\nSHOWN ON HOME\n  Poneglyph shows which controllers are plugged in. Plug yours in BEFORE starting the game: these games only look for controllers when they start.\n\nNOT AVAILABLE YET (planned, one at a time, each tested first)\n  Controller hot-plug and vibration inside the game, ReShade, DXVK, frame limiter, text-language choice.\n";
// ------------------------------------------------------------------ game options (launch options) page
static const int kMsaa[5]={0,2,4,8,16}, kShadow[6]={0,512,1024,2048,4096,8192};
struct ResPreset { int w,h; const wchar_t* label; };
static const ResPreset kRes[6]={{0,0,L"Game default"},{1280,720,L"1280 x 720"},{1600,900,L"1600 x 900"},{1920,1080,L"1920 x 1080"},{2560,1440,L"2560 x 1440"},{3840,2160,L"3840 x 2160"}};
static int ResIndex(){ for(int i=0;i<6;i++) if(kRes[i].w==g_opt.width&&kRes[i].h==g_opt.height) return i; return -1; }
static void RefreshOptionButtons(){
    auto set=[&](int id,const std::wstring& t){ for(int pg:{PG_OPTIONS,PG_HOME}) for(HWND h:g_pages[pg].buttons) if(GetDlgCtrlID(h)==id){ SetWindowTextW(h,t.c_str()); InvalidateRect(h,nullptr,TRUE); } };
    int ri=ResIndex(); std::wstring rl=ri>=0?std::wstring(kRes[ri].label):std::to_wstring(g_opt.width)+L" x "+std::to_wstring(g_opt.height);
    set(C_RES,L"Resolution: "+rl); set(C_MSAA,g_opt.msaa?L"Anti-aliasing: "+std::to_wstring(g_opt.msaa)+L"x":std::wstring(L"Anti-aliasing: Off")); set(C_SHADOW,g_opt.shadow?L"Shadow quality: "+std::to_wstring(g_opt.shadow):std::wstring(L"Shadow quality: Game default"));
    set(H_VOICE,g_opt.voiceJP?L"Voice: Japanese":L"Voice: English");
    g_toggle[O_FULLSCREEN]=g_opt.fullscreen; g_toggle[O_VOICEJP]=g_opt.voiceJP; g_toggle[O_DECIDE]=g_opt.decideReverse;
    for(HWND h:g_pages[PG_OPTIONS].buttons) InvalidateRect(h,nullptr,TRUE);
}
static void LoadOptionsFromProfile(){ g_opt=LaunchOptions(); if(haveExe()){ IniFile c; c.load(curPaths().config); g_opt=loadLaunchOptions(c); } RefreshOptionButtons(); }
static bool SaveOptions(bool quiet){
    if(!haveExe()){ Msg("Please do Step 1 first","Press 'Find My Game' on the Home page first."); return false; }
    auto bad=validateLaunchOptions(g_opt); if(!bad.empty()){ Msg("These options cannot be saved",join(bad,"\n"),MB_ICONWARNING); return false; }
    auto pp=curPaths(); std::error_code ec; fs::create_directories(pp.root,ec); IniFile c; c.load(pp.config); saveLaunchOptions(c,g_opt);
    if(!c.save(pp.config)){ Msg("Could not save","The file could not be written: "+pathToUtf8(pp.config),MB_ICONWARNING); return false; }
    g_mlog.log(LogLevel::Info,"game options saved for %s",curGame()->key); if(!quiet) SetStatus("Game options saved. They are used the next time you press 'Play the Game'."); return true;
}
static void ShowOptionsText(){
    std::string t;
    if(!launchOptionsSupported(g_prof[g_cur].id)) t="GAME OPTIONS\n\nLightning Returns does not read these launch options, so Poneglyph does not offer them for this game. Nothing is guessed: when a safe, tested way exists for this game, it will appear on this page.\n";
    else t="GAME OPTIONS\n\nThese are the same start-up options the game's own launcher uses. Poneglyph does not change any game file or add anything to the game. It simply starts the game with the choices you make here.\n\nHOW TO USE\n  1. Pick your choices with the buttons above.\n  2. Press 'Save Game Options'.\n  3. Start the game with 'Play the Game' on the Home page.\n\nIMPORTANT\n  These options apply only when the game is started from Poneglyph. If you start it from Steam, Steam's own launcher decides.\n\nWHAT EACH ONE DOES\n  Resolution - the size of the picture. 'Game default' lets the game decide.\n  Full screen - OFF means the game runs in a window.\n  Anti-aliasing - smoother edges (2x to 16x). Higher looks nicer but needs a stronger graphics card.\n  Shadow quality - sharper shadows at higher numbers (uses more video memory).\n  Japanese voice - uses the Japanese voice-over. It works only if your copy contains Japanese audio. Some editions have only Japanese, some have both. Choosing a language in Steam's Properties does not change the voice - this option does.\n  Swap Confirm / Cancel - exchanges the two buttons.\n\nNOT GUESSED\n  The presets are the safe choices. Other sizes are accepted by the game's start-up options but have not been tested by us. If the picture looks wrong, set Resolution back to 'Game default'.\n";
    PutText(g_pages[PG_OPTIONS].main,t);
}
// ------------------------------------------------------------------ chrome drawing
static COLORREF Mix(COLORREF a,COLORREF b,double t){ return RGB((int)(GetRValue(a)*(1-t)+GetRValue(b)*t),(int)(GetGValue(a)*(1-t)+GetGValue(b)*t),(int)(GetBValue(a)*(1-t)+GetBValue(b)*t)); }
static void VGrad(HDC dc,const RECT& r,COLORREF top,COLORREF bot){
    TRIVERTEX v[2]={{r.left,r.top,(COLOR16)(GetRValue(top)<<8),(COLOR16)(GetGValue(top)<<8),(COLOR16)(GetBValue(top)<<8),0},{r.right,r.bottom,(COLOR16)(GetRValue(bot)<<8),(COLOR16)(GetGValue(bot)<<8),(COLOR16)(GetBValue(bot)<<8),0}};
    GRADIENT_RECT g{0,1}; GradientFill(dc,v,2,&g,1,GRADIENT_FILL_RECT_V);
}
static void Fill(HDC dc,const RECT& r,COLORREF c){ HBRUSH b=CreateSolidBrush(c); FillRect(dc,&r,b); DeleteObject(b); }
// Action button: dark rounded face with a visible border. primary = filled with the accent colour; outline = accent border + accent text.
static void Chrome(HDC dc,RECT r,bool pressed,bool selected,bool disabled,bool primary=false,bool outline=false){
    int rad=S(12); HRGN rg=CreateRoundRectRgn(r.left,r.top,r.right+1,r.bottom+1,rad,rad); SelectClipRgn(dc,rg); bool hi=selected||primary;
    COLORREF base=hi?T().accent:T().panelLo; COLORREF top=Mix(base,RGB(255,255,255),hi?0.14:0.10), bot=Mix(base,RGB(0,0,0),hi?0.10:0.12);
    if(pressed){ top=Mix(top,RGB(0,0,0),0.22); bot=Mix(bot,RGB(0,0,0),0.22); }
    if(disabled){ top=Mix(top,T().bg,0.65); bot=Mix(bot,T().bg,0.65); }
    VGrad(dc,r,top,bot); SelectClipRgn(dc,nullptr);
    HBRUSH bb=CreateSolidBrush(hi?Mix(T().accent,RGB(255,255,255),0.35):(outline?T().accent:T().border)); FrameRgn(dc,rg,bb,1,1); DeleteObject(bb); DeleteObject(rg);
}
static void DrawIconKind(HDC dc,int k,int cx,int cy,int sz,COLORREF c){
    int h=sz/2; HPEN pen=CreatePen(PS_SOLID,std::max(1,S(2)),c); HGDIOBJ op=SelectObject(dc,pen), ob=SelectObject(dc,GetStockObject(NULL_BRUSH));
    switch(k){
    case PG_HOME: { POINT roof[3]={{cx-h,cy},{cx,cy-h},{cx+h,cy}}; Polyline(dc,roof,3); Rectangle(dc,cx-h+S(3),cy,cx+h-S(3),cy+h); Rectangle(dc,cx-S(3),cy+S(3),cx+S(3),cy+h); break; }
    case PG_ADDONS: Rectangle(dc,cx-h,cy-h+S(4),cx+h-S(4),cy+h); Ellipse(dc,cx+h-S(9),cy-h,cx+h+S(1),cy-h+S(10)); break;
    case PG_PERF: Ellipse(dc,cx-h,cy-h,cx+h,cy+h); MoveToEx(dc,cx,cy,nullptr); LineTo(dc,cx+h*2/3,cy-h*2/3); break;
    case PG_MEM: Rectangle(dc,cx-h+S(3),cy-h+S(3),cx+h-S(3),cy+h-S(3)); for(int i=-1;i<=1;i++){ MoveToEx(dc,cx+i*S(5),cy-h,nullptr); LineTo(dc,cx+i*S(5),cy-h+S(3)); MoveToEx(dc,cx+i*S(5),cy+h-S(3),nullptr); LineTo(dc,cx+i*S(5),cy+h); } break;
    case PG_OPTIONS: for(int i=-1;i<=1;i++){ int y=cy+i*S(6); MoveToEx(dc,cx-h,y,nullptr); LineTo(dc,cx+h,y); int kx=cx+(i==0?-S(3):(i<0?S(4):-S(5))); Ellipse(dc,kx-S(2),y-S(2),kx+S(3),y+S(3)); } break;
    case PG_BACKUPS: RoundRect(dc,cx-h,cy-h,cx+h,cy-h/3,S(4),S(4)); RoundRect(dc,cx-h,cy-h/3+S(2),cx+h,cy+h/3+S(2),S(4),S(4)); RoundRect(dc,cx-h,cy+h/3+S(4),cx+h,cy+h,S(4),S(4)); break;
    case PG_SETTINGS: Ellipse(dc,cx-h*2/3,cy-h*2/3,cx+h*2/3,cy+h*2/3); for(int i=0;i<8;i++){ double a=i*3.14159265/4; MoveToEx(dc,cx+(int)(std::cos(a)*h*0.8),cy+(int)(std::sin(a)*h*0.8),nullptr); LineTo(dc,cx+(int)(std::cos(a)*h),cy+(int)(std::sin(a)*h)); } break;
    case PG_HELP: { Ellipse(dc,cx-h,cy-h,cx+h,cy+h); SelectObject(dc,g_fBold); SetTextColor(dc,c); SetBkMode(dc,TRANSPARENT); RECT q={cx-h,cy-h,cx+h,cy+h}; DrawTextW(dc,L"?",1,&q,DT_CENTER|DT_VCENTER|DT_SINGLELINE); break; }
    }
    SelectObject(dc,op); SelectObject(dc,ob); DeleteObject(pen);
}
static void DrawMemCard(DRAWITEMSTRUCT* d){
    HDC dc=d->hDC; RECT r=d->rcItem; Fill(dc,r,T().listAlt); SetBkMode(dc,TRANSPARENT);
    RECT tb=r; tb.bottom=tb.top+S(36); VGrad(dc,tb,Mix(T().accent,RGB(255,255,255),0.10),Mix(T().accent,T().bg,0.35)); SelectObject(dc,g_fHead); SetTextColor(dc,T().accentText); RECT tt=tb; tt.left+=S(16); DrawTextW(dc,L"Large Address Aware   (the 4 GB setting)",-1,&tt,DT_LEFT|DT_VCENTER|DT_SINGLELINE);
    std::wstring st=g_laaState==1?L"Status:  ON  -  the game may use up to 4096 MB of memory space.":(g_laaState==0?L"Status:  OFF  -  the game is limited to 2048 MB. Use 'Enable 4 GB Flag' below to turn it on (a backup is made first).":L"Status:  unknown  -  choose your game on the Home page first.");
    RECT sr={r.left+S(16),tb.bottom+S(8),r.right-S(16),tb.bottom+S(34)}; SelectObject(dc,g_fBold); SetTextColor(dc,g_laaState==1?T().ok:(g_laaState==0?T().warn:T().dim)); DrawTextW(dc,st.c_str(),-1,&sr,DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS);
    int limit=g_laaState==1?4096:2048; ULONGLONG ws=g_samples.empty()?0:g_samples.back().workingSetBytes; double used=(double)ws/1048576.0;
    RECT bar={r.left+S(16),sr.bottom+S(8),r.right-S(16),sr.bottom+S(26)}; Fill(dc,bar,T().sidebar);
    if(ws>0){ double f=std::min(1.0,used/limit); RECT fl=bar; fl.right=bar.left+(int)((bar.right-bar.left)*f); Fill(dc,fl,T().accent); }
    HBRUSH fr=CreateSolidBrush(T().border); FrameRect(dc,&bar,fr); DeleteObject(fr);
    wchar_t buf[220]; if(ws>0) swprintf(buf,220,L"Game memory in use right now: %.0f MB   (limit %d MB, %.0f%% used)",used,limit,100.0*used/limit); else lstrcpyW(buf,L"Press 'Start Monitoring' on the Performance page while the game runs to see live memory here.");
    RECT lr={r.left+S(16),bar.bottom+S(4),r.right-S(16),r.bottom-S(2)}; SelectObject(dc,g_fSmall); SetTextColor(dc,T().dim); DrawTextW(dc,buf,-1,&lr,DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS);
}
static void DrawOwner(DRAWITEMSTRUCT* d){
    int id=(int)d->CtlID; HDC dc=d->hDC; RECT r=d->rcItem; if(id==ID_MEMCARD){ DrawMemCard(d); return; }
    bool pressed=(d->itemState&ODS_SELECTED)!=0, dis=(d->itemState&ODS_DISABLED)!=0; wchar_t txt[160]; GetWindowTextW(d->hwndItem,txt,160);
    bool nav=id>=ID_NAV&&id<ID_NAV+PG_COUNT, tab=id>=ID_PROFILE&&id<ID_PROFILE+3, tog=g_toggle.count(id)>0; SetBkMode(dc,TRANSPARENT);
    if(tog){ Fill(dc,r,T().bg); RECT bx={r.left+S(4),r.top+(r.bottom-r.top-S(22))/2,r.left+S(26),0}; bx.bottom=bx.top+S(22); bool on=g_toggle[id];
        HBRUSH bg=CreateSolidBrush(on?T().accent:T().sidebar); HPEN pen=CreatePen(PS_SOLID,1,on?T().accent:T().frame); HGDIOBJ op=SelectObject(dc,pen),ob=SelectObject(dc,bg); RoundRect(dc,bx.left,bx.top,bx.right,bx.bottom,S(7),S(7)); SelectObject(dc,op); SelectObject(dc,ob); DeleteObject(pen); DeleteObject(bg);
        if(on){ HPEN cp=CreatePen(PS_SOLID,S(3),T().accentText); HGDIOBJ o2=SelectObject(dc,cp); MoveToEx(dc,bx.left+S(5),bx.top+S(11),nullptr); LineTo(dc,bx.left+S(9),bx.top+S(16)); LineTo(dc,bx.left+S(17),bx.top+S(6)); SelectObject(dc,o2); DeleteObject(cp); }
        RECT tr=r; tr.left=bx.right+S(10); SelectObject(dc,g_fBody); SetTextColor(dc,T().text); DrawTextW(dc,txt,-1,&tr,DT_LEFT|DT_VCENTER|DT_SINGLELINE); if(d->itemState&ODS_FOCUS){ RECT f=r; InflateRect(&f,-1,-1); DrawFocusRect(dc,&f); } return; }
    if(nav){ int idx=id-ID_NAV; bool sel=idx==g_page; Fill(dc,r,T().sidebar); if(sel||pressed) Chrome(dc,r,pressed&&!sel,sel,false);
        DrawIconKind(dc,idx,r.left+S(28),(r.top+r.bottom)/2,S(20),sel?T().accentText:T().header);
        RECT tr=r; tr.left+=S(54); SelectObject(dc,sel?g_fHead:g_fBody); SetTextColor(dc,sel?T().accentText:T().text); DrawTextW(dc,txt,-1,&tr,DT_LEFT|DT_VCENTER|DT_SINGLELINE); return; }
    if(tab){ bool sel=(id-ID_PROFILE)==g_cur; Fill(dc,r,Mix(T().panelHi,T().panelLo,0.5));
        COLORREF fill=sel?Mix(T().sidebar,T().accent,0.22):T().sidebar; HBRUSH fb=CreateSolidBrush(fill); HPEN bp=CreatePen(PS_SOLID,sel?std::max(2,S(2)):1,sel?T().accent:T().border); HGDIOBJ op=SelectObject(dc,bp),ob=SelectObject(dc,fb);
        RoundRect(dc,r.left+1,r.top+1,r.right-1,r.bottom-1,S(12),S(12)); SelectObject(dc,op); SelectObject(dc,ob); DeleteObject(bp); DeleteObject(fb);
        RECT tr=r; InflateRect(&tr,-S(8),-S(2)); SelectObject(dc,sel?g_fHead:g_fBody); SetTextColor(dc,sel?T().text:T().dim);
        RECT calc=tr; DrawTextW(dc,txt,-1,&calc,DT_CALCRECT|DT_WORDBREAK|DT_CENTER); int hh=calc.bottom-calc.top; tr.top+=((tr.bottom-tr.top)-hh)/2; DrawTextW(dc,txt,-1,&tr,DT_CENTER|DT_WORDBREAK); return; }
    bool fillNext=(id==g_nextStep)||id==B_SAVE_SETTINGS||id==B_OPT_SAVE; bool key=(id==B_CHOOSE_EXE||id==B_LAUNCH||id==B_INSTALL_ENGINE);
    Fill(dc,r,T().bg); Chrome(dc,r,pressed,false,dis,fillNext,key&&!fillNext);
    SelectObject(dc,(fillNext||key)?g_fHead:g_fBody); SetTextColor(dc,dis?T().dim:(fillNext?T().accentText:(key?T().accent:T().text))); RECT tr=r; InflateRect(&tr,-S(8),0);
    DrawTextW(dc,txt,-1,&tr,DT_CENTER|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS); if(d->itemState&ODS_FOCUS){ RECT f=r; InflateRect(&f,-S(3),-S(3)); DrawFocusRect(dc,&f); }
}
static LRESULT CALLBACK HeaderProc(HWND h,UINT m,WPARAM w,LPARAM l,UINT_PTR,DWORD_PTR){
    if(m==WM_ERASEBKGND) return 1;
    if(m==WM_PAINT){ PAINTSTRUCT ps; HDC dc=BeginPaint(h,&ps); RECT rc; GetClientRect(h,&rc); VGrad(dc,rc,T().panelHi,T().panelLo); SetBkMode(dc,TRANSPARENT); SelectObject(dc,g_fHead); SetTextColor(dc,T().header);
        int n=Header_GetItemCount(h); HPEN pen=CreatePen(PS_SOLID,1,T().border); HGDIOBJ op=SelectObject(dc,pen);
        for(int i=0;i<n;i++){ RECT ir; Header_GetItemRect(h,i,&ir); wchar_t buf[128]={0}; HDITEMW hi{}; hi.mask=HDI_TEXT; hi.pszText=buf; hi.cchTextMax=128; Header_GetItem(h,i,&hi); RECT tr=ir; tr.left+=S(8); tr.right-=S(4); DrawTextW(dc,buf,-1,&tr,DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS); MoveToEx(dc,ir.right-1,ir.top+2,nullptr); LineTo(dc,ir.right-1,ir.bottom-2); }
        SelectObject(dc,op); DeleteObject(pen); EndPaint(h,&ps); return 0; }
    if(m==WM_NCDESTROY) RemoveWindowSubclass(h,HeaderProc,1);
    return DefSubclassProc(h,m,w,l);
}
// ------------------------------------------------------------------ construction and layout
enum { ID_LIST_ADDONS=300, ID_LIST_BACKUPS=301, ID_EDIT=310 };
static HWND MakeBtn(int page,int id,const wchar_t* text){ HWND b=CreateWindowExW(0,L"BUTTON",text,WS_CHILD|WS_TABSTOP|BS_OWNERDRAW,0,0,10,10,g_hwnd,(HMENU)(INT_PTR)id,g_inst,nullptr); g_pages[page].buttons.push_back(b); return b; }
static HWND MakeRich(int id){ return CreateWindowExW(0,MSFTEDIT_CLASS,L"",WS_CHILD|WS_VSCROLL|ES_MULTILINE|ES_READONLY|ES_AUTOVSCROLL|ES_NOHIDESEL,0,0,10,10,g_hwnd,(HMENU)(INT_PTR)id,g_inst,nullptr); }
static void ThemeLists(){
    for(int pg:{PG_ADDONS,PG_BACKUPS}){ HWND lv=g_pages[pg].main; if(!lv) continue; ListView_SetBkColor(lv,T().listAlt); ListView_SetTextBkColor(lv,T().listAlt); ListView_SetTextColor(lv,T().text); }
}
static bool FontExists(const wchar_t* face){
    bool found=false; LOGFONTW lf{}; lf.lfCharSet=DEFAULT_CHARSET; lstrcpynW(lf.lfFaceName,face,LF_FACESIZE); HDC dc=GetDC(nullptr);
    EnumFontFamiliesExW(dc,&lf,[](const LOGFONTW*,const TEXTMETRICW*,DWORD,LPARAM p)->int{ *(bool*)p=true; return 0; },(LPARAM)&found,0); ReleaseDC(nullptr,dc); return found;
}
static void PickFace(wchar_t* out,std::initializer_list<const wchar_t*> c,const wchar_t* fallback){ for(auto f:c) if(FontExists(f)){ lstrcpyW(out,f); return; } lstrcpyW(out,fallback); }
static void ApplyFonts(){
    if(g_fBody){ DeleteObject(g_fBody); DeleteObject(g_fBold); DeleteObject(g_fTitle); DeleteObject(g_fSmall); DeleteObject(g_fText); DeleteObject(g_fMono); DeleteObject(g_fHead); }
    PickFace(g_faceHead,{L"Bahnschrift",L"Segoe UI Variable Display",L"Segoe UI Semibold"},L"Segoe UI"); PickFace(g_faceBody,{L"Segoe UI Variable Text",L"Segoe UI"},L"Tahoma"); PickFace(g_faceMono,{L"Cascadia Mono",L"Cascadia Code",L"Consolas"},L"Courier New");
    auto mk=[&](const wchar_t* face,int pt,int wt){ return CreateFontW(-MulDiv(pt,g_dpi,72),0,0,0,wt,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH|FF_DONTCARE,face); };
    g_fBody=mk(g_faceBody,11,FW_NORMAL); g_fBold=mk(g_faceBody,11,FW_SEMIBOLD); g_fTitle=mk(g_faceHead,24,FW_SEMIBOLD); g_fHead=mk(g_faceHead,11,FW_SEMIBOLD); g_fSmall=mk(g_faceBody,10,FW_NORMAL); g_fText=mk(g_faceBody,12,FW_NORMAL); g_fMono=mk(g_faceMono,11,FW_NORMAL);
    EnumChildWindows(g_hwnd,[](HWND c,LPARAM)->BOOL{ SendMessageW(c,WM_SETFONT,(WPARAM)g_fBody,TRUE); return TRUE; },0);
    for(int pg:{PG_ADDONS,PG_BACKUPS}){ HWND lv=g_pages[pg].main; if(lv) SendMessageW(ListView_GetHeader(lv),WM_SETFONT,(WPARAM)g_fBold,TRUE); }
    SendMessageW(g_status,WM_SETFONT,(WPARAM)g_fSmall,TRUE);
}
static void ShowPage(int p);
static void ApplyTheme(){
    if(g_brBg){ DeleteObject(g_brBg); DeleteObject(g_brSide); DeleteObject(g_brEdit); } g_brBg=CreateSolidBrush(T().bg); g_brSide=CreateSolidBrush(T().sidebar); g_brEdit=CreateSolidBrush(T().listAlt);
    ThemeLists(); HICON ic=(HICON)LoadImageW(g_inst,MAKEINTRESOURCEW(g_theme==1?IDI_PG_PURPLE:IDI_PG_SILVER),IMAGE_ICON,0,0,LR_DEFAULTSIZE); if(ic) SendMessageW(g_hwnd,WM_SETICON,ICON_BIG,(LPARAM)ic);
    HICON sm=(HICON)LoadImageW(g_inst,MAKEINTRESOURCEW(g_theme==1?IDI_PG_PURPLE:IDI_PG_SILVER),IMAGE_ICON,GetSystemMetrics(SM_CXSMICON),GetSystemMetrics(SM_CYSMICON),0); if(sm) SendMessageW(g_hwnd,WM_SETICON,ICON_SMALL,(LPARAM)sm);
    RefreshSettingButtons(); ShowPage(g_page); RedrawWindow(g_hwnd,nullptr,nullptr,RDW_INVALIDATE|RDW_ERASE|RDW_ALLCHILDREN|RDW_UPDATENOW);
}
static void ShowPage(int p){
    g_page=p; for(int i=0;i<PG_COUNT;i++){ int sh=i==p?SW_SHOW:SW_HIDE; for(HWND b:g_pages[i].buttons) ShowWindow(b,sh); if(g_pages[i].main) ShowWindow(g_pages[i].main,sh); if(g_pages[i].second) ShowWindow(g_pages[i].second,sh); if(g_pages[i].card) ShowWindow(g_pages[i].card,sh); }
    switch(p){ case PG_HOME: LoadOptionsFromProfile(); RefreshHome(); break; case PG_ADDONS: if(haveExe()&&g_scan.empty()) DoScan(); else FillAddonList(); UpdateAddonDetails(); break;
      case PG_PERF: if(!g_mon.on) SetMain(PG_PERF,"PERFORMANCE\n\nThis page watches how hard the game works your computer. It does this from OUTSIDE the game, once per second, and never touches the game.\n\n  Start Monitoring - begin watching (then play).\n  View Report - a summary you can send to whoever is helping you.\n  Export CSV - saves the numbers to a file.\n"); break;
      case PG_MEM: LaaCheck(); break; case PG_OPTIONS: LoadOptionsFromProfile(); ShowOptionsText(); break; case PG_BACKUPS: FillBackups(); break;
      case PG_SETTINGS: LoadSettingsFromProfile(); SetMain(PG_SETTINGS,"SETTINGS\n\nThese settings are stored for each game in that game's Poneglyph folder (poneglyph.ini) and apply the next time the game starts.\n\n  Poneglyph engine OFF - Poneglyph only passes the game's graphics straight to Windows. Nothing else runs.\n  Load add-ons OFF - no mods (.asi/.dll) are loaded.\n  Frame statistics - a tiny recorder writes one summary line every few seconds. OFF by default; there is no per-frame logging.\n  Theme - Silver Chrome, Dark Purple Chrome or Black Chrome.\n"); break;
      case PG_HELP: SetMain(PG_HELP,kHelpText); break; default: break; }
    for(int i=0;i<PG_COUNT;i++) if(HWND n=GetDlgItem(g_hwnd,ID_NAV+i)) InvalidateRect(n,nullptr,TRUE); InvalidateRect(g_hwnd,nullptr,FALSE);
}
static void Layout(){
    if(!g_hwnd) return; RECT rc; GetClientRect(g_hwnd,&rc); int hh=S(72), sw=S(220), sh=S(34), pad=S(18);
    int y=hh+S(16); for(int i=0;i<PG_COUNT;i++){ SetWindowPos(GetDlgItem(g_hwnd,ID_NAV+i),nullptr,S(12),y,sw-S(24),S(44),SWP_NOZORDER); y+=S(50); }
    int tabsX=sw+pad, gapT=S(10), tabW=(rc.right-tabsX-pad-2*gapT)/3, tabH=S(42); for(int i=0;i<3;i++) SetWindowPos(GetDlgItem(g_hwnd,ID_PROFILE+i),nullptr,tabsX+i*(tabW+gapT),(hh-tabH)/2,tabW,tabH,SWP_NOZORDER);
    SetWindowPos(g_status,nullptr,sw+pad,rc.bottom-sh,rc.right-sw-pad-S(190),sh,SWP_NOZORDER); SetWindowPos(g_progress,nullptr,rc.right-S(180),rc.bottom-sh+S(10),S(164),S(14),SWP_NOZORDER);
    RECT c={sw+pad,hh+pad,rc.right-pad,rc.bottom-sh-S(8)}; int gap=S(10), bh=S(44);
    for(int i=0;i<PG_COUNT;i++){ auto& pg=g_pages[i]; int bw=(i==PG_HOME||i==PG_OPTIONS)?(c.right-c.left-2*gap)/3:S(236); int x=c.left,yy=c.top; int col=0;
        for(HWND b:pg.buttons){ if((i==PG_HOME||i==PG_OPTIONS)?(col==3):(x+bw>c.right&&x>c.left)){ x=c.left; yy+=bh+gap; col=0; } SetWindowPos(b,nullptr,x,yy,bw,bh,SWP_NOZORDER); x+=bw+gap; col++; }
        int top=yy+(pg.buttons.empty()?0:bh+S(14)); int bottom=c.bottom;
        if(pg.card){ int ch=S(150); SetWindowPos(pg.card,nullptr,c.left,top,c.right-c.left,ch,SWP_NOZORDER); top+=ch+S(12); }
        if(pg.main&&pg.second){ int split=top+(bottom-top)*58/100; SetWindowPos(pg.main,nullptr,c.left,top,c.right-c.left,split-top,SWP_NOZORDER); SetWindowPos(pg.second,nullptr,c.left,split+S(10),c.right-c.left,bottom-split-S(10),SWP_NOZORDER); }
        else if(pg.main) SetWindowPos(pg.main,nullptr,c.left,top,c.right-c.left,bottom-top,SWP_NOZORDER); }
    InvalidateRect(g_hwnd,nullptr,TRUE);
}
static void BuildUi(){
    g_status=CreateWindowExW(0,L"STATIC",L"Ready",WS_CHILD|WS_VISIBLE|SS_LEFTNOWORDWRAP|SS_CENTERIMAGE,0,0,10,10,g_hwnd,(HMENU)9001,g_inst,nullptr);
    g_progress=CreateWindowExW(0,PROGRESS_CLASSW,L"",WS_CHILD|PBS_MARQUEE,0,0,10,10,g_hwnd,(HMENU)9002,g_inst,nullptr); SetWindowTheme(g_progress,L"",L"");
    for(int i=0;i<PG_COUNT;i++) CreateWindowExW(0,L"BUTTON",kPageNames[i],WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_OWNERDRAW,0,0,10,10,g_hwnd,(HMENU)(INT_PTR)(ID_NAV+i),g_inst,nullptr);
    for(int i=0;i<3;i++) CreateWindowExW(0,L"BUTTON",W(gameInfo(g_prof[i].id)->displayName).c_str(),WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_OWNERDRAW,0,0,10,10,g_hwnd,(HMENU)(INT_PTR)(ID_PROFILE+i),g_inst,nullptr);
    // Home: row 1 = the three main actions, row 2 = helpers, row 3 = voice
    MakeBtn(PG_HOME,B_CHOOSE_EXE,L"Find My Game"); MakeBtn(PG_HOME,B_LAUNCH,L"Play the Game"); MakeBtn(PG_HOME,B_INSTALL_ENGINE,L"Install Poneglyph");
    MakeBtn(PG_HOME,B_SEARCH_DIR,L"Search a Folder for My Game"); MakeBtn(PG_HOME,B_OPEN_GAME,L"Open Game Folder"); MakeBtn(PG_HOME,B_REMOVE_ENGINE,L"Uninstall Poneglyph (Undo)"); MakeBtn(PG_HOME,H_VOICE,L"Voice: English"); g_pages[PG_HOME].main=MakeRich(ID_EDIT);
    MakeBtn(PG_ADDONS,B_SCAN,L"Scan Add-ons"); MakeBtn(PG_ADDONS,B_ENABLE,L"Enable"); MakeBtn(PG_ADDONS,B_DISABLE,L"Disable"); MakeBtn(PG_ADDONS,B_INSTALL_ZIP,L"Install ZIP..."); MakeBtn(PG_ADDONS,B_REMOVE_PKG,L"Remove Package"); MakeBtn(PG_ADDONS,B_OPEN_ADDONS,L"Open Add-ons Folder");
    g_pages[PG_ADDONS].main=MakeList(g_hwnd,ID_LIST_ADDONS,{{L"Add-on",S(230)},{L"Status",S(150)},{L"Order",S(60)},{L"Architecture",S(110)},{L"Size",S(80)},{L"Compatibility",S(260)},{L"Details",S(420)}}); g_pages[PG_ADDONS].second=MakeRich(ID_EDIT+1);
    MakeBtn(PG_PERF,B_MON_START,L"Start Monitoring"); MakeBtn(PG_PERF,B_MON_STOP,L"Stop Monitoring"); MakeBtn(PG_PERF,B_LAUNCH_MON,L"Launch + Monitor"); MakeBtn(PG_PERF,B_EXPORT_CSV,L"Export CSV"); MakeBtn(PG_PERF,B_REPORT,L"View Report"); MakeBtn(PG_PERF,B_SAVE_REPORT,L"Save Report"); MakeBtn(PG_PERF,B_COMPARE,L"Compare Frame Data..."); g_pages[PG_PERF].main=MakeRich(ID_EDIT+2);
    MakeBtn(PG_MEM,B_LAA_CHECK,L"Check Status"); MakeBtn(PG_MEM,B_LAA_ENABLE,L"Enable 4 GB Flag"); MakeBtn(PG_MEM,B_LAA_RESTORE,L"Restore Original"); g_pages[PG_MEM].card=CreateWindowExW(0,L"STATIC",L"",WS_CHILD|SS_OWNERDRAW,0,0,10,10,g_hwnd,(HMENU)(INT_PTR)ID_MEMCARD,g_inst,nullptr); g_pages[PG_MEM].main=MakeRich(ID_EDIT+3);
    MakeBtn(PG_OPTIONS,C_RES,L"Resolution: Game default"); MakeBtn(PG_OPTIONS,C_MSAA,L"Anti-aliasing: Off"); MakeBtn(PG_OPTIONS,C_SHADOW,L"Shadow quality: Game default"); MakeBtn(PG_OPTIONS,O_FULLSCREEN,L"Full screen"); MakeBtn(PG_OPTIONS,O_VOICEJP,L"Japanese voice"); MakeBtn(PG_OPTIONS,O_DECIDE,L"Swap Confirm / Cancel"); MakeBtn(PG_OPTIONS,B_OPT_SAVE,L"Save Game Options"); g_pages[PG_OPTIONS].main=MakeRich(ID_EDIT+7);
    MakeBtn(PG_BACKUPS,B_BK_RESTORE,L"Restore Selected"); MakeBtn(PG_BACKUPS,B_BK_OPEN,L"Open Backups Folder"); g_pages[PG_BACKUPS].main=MakeList(g_hwnd,ID_LIST_BACKUPS,{{L"Backup file",S(520)},{L"Size",S(110)},{L"Type",S(220)}}); g_pages[PG_BACKUPS].second=MakeRich(ID_EDIT+4);
    MakeBtn(PG_SETTINGS,T_ENABLED,L"Poneglyph engine ON"); MakeBtn(PG_SETTINGS,T_LOADADDONS,L"Load add-ons"); MakeBtn(PG_SETTINGS,T_FRAMESTATS,L"Frame statistics (light)"); MakeBtn(PG_SETTINGS,C_SUMMARY,L"Summary every 10 s"); MakeBtn(PG_SETTINGS,C_LOGLEVEL,L"Log detail: Normal"); MakeBtn(PG_SETTINGS,C_THEME,L"Theme"); MakeBtn(PG_SETTINGS,B_SAVE_SETTINGS,L"Save Settings"); g_pages[PG_SETTINGS].main=MakeRich(ID_EDIT+5);
    MakeBtn(PG_HELP,B_README,L"Open Read_Me"); MakeBtn(PG_HELP,B_OPEN_LOGS,L"Open Logs Folder"); MakeBtn(PG_HELP,B_SUPPORT,L"Create Support Report"); g_pages[PG_HELP].main=MakeRich(ID_EDIT+6);
    for(int i=0;i<PG_COUNT;i++){ for(HWND b:g_pages[i].buttons) ShowWindow(b,SW_HIDE); if(g_pages[i].main) ShowWindow(g_pages[i].main,SW_HIDE); if(g_pages[i].second) ShowWindow(g_pages[i].second,SW_HIDE); if(g_pages[i].card) ShowWindow(g_pages[i].card,SW_HIDE); }
    for(int pg:{PG_ADDONS,PG_BACKUPS}) SetWindowSubclass(ListView_GetHeader(g_pages[pg].main),HeaderProc,1,0);
    g_toggle[T_ENABLED]=true; g_toggle[T_LOADADDONS]=true; g_toggle[T_FRAMESTATS]=false; g_toggle[O_FULLSCREEN]=false; g_toggle[O_VOICEJP]=false; g_toggle[O_DECIDE]=false;
}
static void OpenFolder(const fs::path& p){ std::error_code ec; fs::create_directories(p,ec); ShellExecuteW(g_hwnd,L"open",p.wstring().c_str(),nullptr,nullptr,SW_SHOW); }
static void SwitchProfile(int i){ g_cur=i; g_scan.clear(); g_samples.clear(); MonitorStop(); SaveManagerSettings(); ShowPage(g_page); for(int k=0;k<3;k++) InvalidateRect(GetDlgItem(g_hwnd,ID_PROFILE+k),nullptr,TRUE); }
static void OnCommand(int id){
    if(id>=ID_NAV&&id<ID_NAV+PG_COUNT){ ShowPage(id-ID_NAV); return; } if(id>=ID_PROFILE&&id<ID_PROFILE+3){ SwitchProfile(id-ID_PROFILE); return; }
    switch(id){
    case B_CHOOSE_EXE: ChooseExe(); break; case B_SEARCH_DIR: SearchFolder(); break; case B_INSTALL_ENGINE: InstallEngine(); break; case B_REMOVE_ENGINE: RemoveEngine(); break;
    case B_LAUNCH: LaunchGame(); break; case B_OPEN_GAME: if(haveExe()) OpenFolder(g_prof[g_cur].exe.parent_path()); break;
    case H_VOICE: if(!launchOptionsSupported(g_prof[g_cur].id)){ Msg("Not available for this game","Lightning Returns does not read this start-up option, so Poneglyph cannot switch its voice this way. Use the game's own Voice setting in its menu (and restart the game after changing it)."); break; }
                  g_opt.voiceJP=!g_opt.voiceJP; if(SaveOptions(true)) SetStatus(g_opt.voiceJP?"Voice set to Japanese. It applies the next time you press 'Play the Game'. (Needs Japanese audio in your copy.)":"Voice set to English. It applies the next time you press 'Play the Game'."); RefreshOptionButtons(); break;
    case O_FULLSCREEN: g_opt.fullscreen=!g_opt.fullscreen; RefreshOptionButtons(); break; case O_VOICEJP: g_opt.voiceJP=!g_opt.voiceJP; RefreshOptionButtons(); break; case O_DECIDE: g_opt.decideReverse=!g_opt.decideReverse; RefreshOptionButtons(); break;
    case C_RES: { int i=ResIndex(); i=(i<0)?0:(i+1)%6; g_opt.width=kRes[i].w; g_opt.height=kRes[i].h; RefreshOptionButtons(); break; }
    case C_MSAA: { int i=0; for(int k=0;k<5;k++) if(kMsaa[k]==g_opt.msaa) i=k; g_opt.msaa=kMsaa[(i+1)%5]; RefreshOptionButtons(); break; }
    case C_SHADOW: { int i=0; for(int k=0;k<6;k++) if(kShadow[k]==g_opt.shadow) i=k; g_opt.shadow=kShadow[(i+1)%6]; RefreshOptionButtons(); break; }
    case B_OPT_SAVE: if(!launchOptionsSupported(g_prof[g_cur].id)){ Msg("Not available for this game","Lightning Returns does not read these start-up options."); break; } SaveOptions(false); break;
    case B_SCAN: DoScan(); break; case B_ENABLE: SetEnabledSel(true); break; case B_DISABLE: SetEnabledSel(false); break; case B_INSTALL_ZIP: InstallZip(); break; case B_REMOVE_PKG: RemovePkg(); break; case B_OPEN_ADDONS: if(haveExe()) OpenFolder(curPaths().addons); break;
    case B_MON_START: MonitorStart(); break; case B_MON_STOP: MonitorStop(); SetStatus("Monitoring stopped."); break; case B_LAUNCH_MON: MonitorStart(); LaunchGame(); break;
    case B_EXPORT_CSV: ExportCsv(); break; case B_REPORT: ShowReport(); break; case B_SAVE_REPORT: SaveReport(); break; case B_COMPARE: CompareFrames(); break;
    case B_LAA_CHECK: LaaCheck(); break; case B_LAA_ENABLE: LaaEnable(true); break; case B_LAA_RESTORE: LaaRestore(); break;
    case B_BK_RESTORE: RestoreSelectedBackup(); break; case B_BK_OPEN: if(haveExe()) OpenFolder(curPaths().backups); break;
    case T_ENABLED: case T_LOADADDONS: case T_FRAMESTATS: g_toggle[id]=!g_toggle[id]; for(HWND h:g_pages[PG_SETTINGS].buttons) InvalidateRect(h,nullptr,TRUE); break;
    case C_SUMMARY: { int i=0; for(int k=0;k<4;k++) if(kSummary[k]==g_summarySecs) i=k; g_summarySecs=kSummary[(i+1)%4]; RefreshSettingButtons(); break; }
    case C_LOGLEVEL: g_logLevel=(g_logLevel+1)%4; RefreshSettingButtons(); break;
    case C_THEME: g_theme=(g_theme+1)%kThemeCount; SaveManagerSettings(); ApplyTheme(); break;
    case B_SAVE_SETTINGS: SaveSettings(); break;
    case B_README: { fs::path r=exeDir()/"Read_Me.txt"; ShellExecuteW(g_hwnd,L"open",r.wstring().c_str(),nullptr,nullptr,SW_SHOW); break; }
    case B_OPEN_LOGS: if(haveExe()) OpenFolder(curPaths().logs); else OpenFolder(appDataDir()); break; case B_SUPPORT: if(haveExe()) SaveReport(true); else Msg("Please do Step 1 first","Press 'Find My Game' on the Home page first."); break;
    }
}
static LRESULT OnNotify(LPARAM l){
    NMHDR* h=(NMHDR*)l; if(h->idFrom!=ID_LIST_ADDONS&&h->idFrom!=ID_LIST_BACKUPS) return 0;
    if(h->code==LVN_ITEMCHANGED&&h->idFrom==ID_LIST_ADDONS){ UpdateAddonDetails(); return 0; }
    if(h->code==NM_CUSTOMDRAW){ auto* cd=(NMLVCUSTOMDRAW*)l;
        if(cd->nmcd.dwDrawStage==CDDS_PREPAINT) return CDRF_NOTIFYITEMDRAW;
        if(cd->nmcd.dwDrawStage==CDDS_ITEMPREPAINT){ bool sel=ListView_GetItemState(h->hwndFrom,(int)cd->nmcd.dwItemSpec,LVIS_SELECTED)!=0; int row=(int)cd->nmcd.dwItemSpec;
            cd->clrText=T().text; cd->clrTextBk=sel?T().hot:(row%2?Mix(T().listAlt,T().panelLo,0.30):T().listAlt); if(sel){ cd->nmcd.uItemState&=~(CDIS_SELECTED|CDIS_FOCUS); cd->clrText=RGB(255,255,255); } return CDRF_NEWFONT; } }
    return 0;
}
static void FrameAround(HDC dc,HWND h,COLORREF c){ if(!h||!IsWindowVisible(h)) return; RECT r; GetWindowRect(h,&r); MapWindowPoints(nullptr,g_hwnd,(POINT*)&r,2); InflateRect(&r,1,1); HBRUSH b=CreateSolidBrush(c); FrameRect(dc,&r,b); DeleteObject(b); }
static void PaintMain(HDC dc,RECT rc){
    FillRect(dc,&rc,g_brBg); int hh=S(72), sw=S(220); RECT side={0,hh,sw,rc.bottom}; VGrad(dc,side,T().sidebar,Mix(T().sidebar,T().bg,0.55)); RECT hd={0,0,rc.right,hh}; VGrad(dc,hd,T().panelHi,T().panelLo);
    HPEN p=CreatePen(PS_SOLID,1,T().frame); HGDIOBJ op=SelectObject(dc,p); MoveToEx(dc,0,hh,nullptr); LineTo(dc,rc.right,hh); SelectObject(dc,op); DeleteObject(p);
    HPEN p2=CreatePen(PS_SOLID,1,T().border); op=SelectObject(dc,p2); MoveToEx(dc,sw,hh,nullptr); LineTo(dc,sw,rc.bottom); SelectObject(dc,op); DeleteObject(p2);
    SetBkMode(dc,TRANSPARENT); SelectObject(dc,g_fTitle); SetTextColor(dc,T().text); RECT tr={S(20),S(6),sw,S(44)}; DrawTextW(dc,L"PONEGLYPH",-1,&tr,DT_LEFT|DT_SINGLELINE|DT_VCENTER);
    SelectObject(dc,g_fSmall); SetTextColor(dc,T().header); RECT sr={S(22),S(44),sw,S(66)}; DrawTextW(dc,L"Mod Loader   v" PG_MANAGER_VERSION,-1,&sr,DT_LEFT|DT_SINGLELINE);
    for(int i=0;i<PG_COUNT;i++){ FrameAround(dc,g_pages[i].main,T().border); FrameAround(dc,g_pages[i].second,T().border); FrameAround(dc,g_pages[i].card,T().border); }
    HBRUSH fb=CreateSolidBrush(T().frame); FrameRect(dc,&rc,fb); DeleteObject(fb);   // the bright outline of the whole window
}
static LRESULT CALLBACK WndProc(HWND h,UINT m,WPARAM w,LPARAM l){
    switch(m){
    case WM_CREATE: g_hwnd=h; g_dpi=GetDpiForWindow(h); BuildUi(); ApplyFonts(); ApplyTheme(); Layout(); ShowPage(PG_HOME); return 0;
    case WM_SIZE: Layout(); return 0;
    case WM_GETMINMAXINFO: { auto* mm=(MINMAXINFO*)l; mm->ptMinTrackSize.x=S(1000); mm->ptMinTrackSize.y=S(660); return 0; }
    case WM_DPICHANGED: g_dpi=HIWORD(w); { RECT* r=(RECT*)l; SetWindowPos(h,nullptr,r->left,r->top,r->right-r->left,r->bottom-r->top,SWP_NOZORDER|SWP_NOACTIVATE); } ApplyFonts(); Layout(); ShowPage(g_page); return 0;
    case WM_ERASEBKGND: return 1;
    case WM_PAINT: { PAINTSTRUCT ps; HDC dc=BeginPaint(h,&ps); RECT rc; GetClientRect(h,&rc); HDC mem=CreateCompatibleDC(dc); HBITMAP bm=CreateCompatibleBitmap(dc,rc.right,rc.bottom); HGDIOBJ ob=SelectObject(mem,bm); PaintMain(mem,rc); BitBlt(dc,0,0,rc.right,rc.bottom,mem,0,0,SRCCOPY); SelectObject(mem,ob); DeleteObject(bm); DeleteDC(mem); EndPaint(h,&ps); return 0; }
    case WM_DRAWITEM: DrawOwner((DRAWITEMSTRUCT*)l); return TRUE;
    case WM_CTLCOLOREDIT: case WM_CTLCOLORSTATIC: { HDC dc=(HDC)w; HWND c=(HWND)l; if(c==g_status){ SetTextColor(dc,T().dim); SetBkColor(dc,T().bg); return (LRESULT)g_brBg; } SetTextColor(dc,T().text); SetBkColor(dc,T().listAlt); return (LRESULT)g_brEdit; }
    case WM_COMMAND: if(HIWORD(w)==BN_CLICKED||HIWORD(w)==0) OnCommand(LOWORD(w)); return 0;
    case WM_NOTIFY: return OnNotify(l);
    case WM_DEVICECHANGE: if(w==DBT_DEVNODES_CHANGED||w==DBT_DEVICEARRIVAL||w==DBT_DEVICEREMOVECOMPLETE) SetTimer(h,2,1200,nullptr); return TRUE;
    case WM_TIMER: if(w==1) MonitorTick(); else if(w==2){ KillTimer(h,2); if(g_page==PG_HOME&&!g_busy) RefreshHome(); } return 0;
    case WM_APP_DONE: { auto* d=(std::function<void()>*)l; g_busy=false; SetStatus("Ready"); if(*d) (*d)(); delete d; return 0; }
    case WM_CLOSE: if(g_busy&&!Ask("Operation running","An operation is still running. Closing now could interrupt it. Close anyway?")) return 0; MonitorStop(); SaveManagerSettings(); DestroyWindow(h); return 0;
    case WM_DESTROY: g_mlog.log(LogLevel::Info,"manager closed"); PostQuitMessage(0); return 0;
    }
    return DefWindowProcW(h,m,w,l);
}
int WINAPI wWinMain(HINSTANCE hi,HINSTANCE,LPWSTR,int show){
    g_inst=hi; typedef BOOL(WINAPI* PFN)(DPI_AWARENESS_CONTEXT); if(auto f=(PFN)GetProcAddress(GetModuleHandleW(L"user32.dll"),"SetProcessDpiAwarenessContext")) f(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED); INITCOMMONCONTROLSEX ic{sizeof ic,ICC_LISTVIEW_CLASSES|ICC_PROGRESS_CLASS|ICC_STANDARD_CLASSES}; InitCommonControlsEx(&ic);
    if(!LoadLibraryW(L"Msftedit.dll")){ MessageBoxW(nullptr,L"Windows' text component (Msftedit.dll) could not be loaded, so Poneglyph cannot show its pages.",L"Poneglyph",MB_OK|MB_ICONERROR); return 1; }
    std::error_code ec; fs::create_directories(appDataDir()/"Logs",ec); g_mlog.open(appDataDir()/"Logs"/"manager.log",512*1024,3); g_oplog.open(appDataDir()/"Logs"/"operations.log",512*1024,3);
    g_mlog.log(LogLevel::Info,"==== Poneglyph Manager %s started ====",PG_MANAGER_VERSION); LoadManagerSettings();
    WNDCLASSEXW wc{sizeof wc}; wc.lpfnWndProc=WndProc; wc.hInstance=hi; wc.hCursor=LoadCursorW(nullptr,IDC_ARROW); wc.lpszClassName=L"PoneglyphManagerWnd"; wc.hIcon=LoadIconW(hi,MAKEINTRESOURCEW(IDI_PG_SILVER)); wc.style=CS_HREDRAW|CS_VREDRAW; RegisterClassExW(&wc);
    HWND h=CreateWindowExW(0,wc.lpszClassName,L"Poneglyph Mod Loader",WS_OVERLAPPEDWINDOW|WS_CLIPCHILDREN,CW_USEDEFAULT,CW_USEDEFAULT,1320,820,nullptr,nullptr,hi,nullptr); if(!h) return 1;
    ShowWindow(h,show); UpdateWindow(h); MSG m; while(GetMessageW(&m,nullptr,0,0)>0){ if(!IsDialogMessageW(h,&m)){ TranslateMessage(&m); DispatchMessageW(&m); } } return (int)m.wParam;
}
