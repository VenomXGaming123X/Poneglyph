// Poneglyph Engine - Direct3D 9 export forwarding.
// Rules (Master Plan sections 3 and 12):
//  * the genuine DLL is loaded by FULL system path, never by the bare name "d3d9.dll";
//  * the handle must differ from this module (no self-loading / recursion);
//  * an unresolved function is NEVER called - the export returns a safe failure value instead.
#include "engine.h"
extern "C" FARPROC pg_thunk_tab[17]={0};
namespace pgengine {
static HMODULE g_real=nullptr;
static FARPROC g_fn[IDX_COUNT];
static INIT_ONCE g_once=INIT_ONCE_STATIC_INIT;
static const char* kNames[IDX_COUNT]={"Direct3DCreate9","Direct3DCreate9Ex","Direct3DCreate9On12","Direct3DCreate9On12Ex",
 "D3DPERF_BeginEvent","D3DPERF_EndEvent","D3DPERF_GetStatus","D3DPERF_QueryRepeatFrame","D3DPERF_SetMarker","D3DPERF_SetOptions",
 "D3DPERF_SetRegion","DebugSetLevel","DebugSetMute","Direct3D9EnableMaximizedWindowedModeShim","Direct3DShaderValidatorCreate9","PSGPError","PSGPSampleTexture"};
static BOOL CALLBACK LoadRealOnce(PINIT_ONCE,PVOID,PVOID*){
    wchar_t sysdir[MAX_PATH]; UINT n=GetSystemDirectoryW(sysdir,MAX_PATH);   // SysWOW64 for a 32-bit process on 64-bit Windows
    if(n==0||n>=MAX_PATH-16){ g_log.log(pg::LogLevel::Error,"GetSystemDirectory failed (%lu)",GetLastError()); return TRUE; }
    wchar_t full[MAX_PATH]; lstrcpyW(full,sysdir); lstrcatW(full,L"\\d3d9.dll");
    g_real=LoadLibraryW(full);
    if(!g_real){ g_log.log(pg::LogLevel::Error,"Could not load the genuine system d3d9.dll (error %lu).",GetLastError()); return TRUE; }
    if(g_real==g_self){ g_log.log(pg::LogLevel::Error,"Refusing to forward to myself (self-load detected)."); g_real=nullptr; return TRUE; }
    wchar_t got[MAX_PATH]; DWORD gl=GetModuleFileNameW(g_real,got,MAX_PATH);
    if(gl==0||lstrcmpiW(got,full)!=0){ g_log.log(pg::LogLevel::Error,"Loaded module path does not match the system path - not forwarding."); FreeLibrary(g_real); g_real=nullptr; return TRUE; }
    int resolved=0;
    for(int i=0;i<IDX_COUNT;i++){ g_fn[i]=GetProcAddress(g_real,kNames[i]); if(g_fn[i]) resolved++; else g_log.log(i<2?pg::LogLevel::Error:pg::LogLevel::Info,"export %s not present in system d3d9.dll",kNames[i]); }
    for(int i=0;i<IDX_COUNT;i++) pg_thunk_tab[i]=g_fn[i];
    g_log.log(pg::LogLevel::Info,"Forwarding to genuine d3d9.dll (full path load), %d/%d exports resolved.",resolved,(int)IDX_COUNT);
    if(!g_fn[IDX_Direct3DCreate9]){ g_log.log(pg::LogLevel::Error,"Direct3DCreate9 unresolved - the game cannot start through this proxy."); }
    return TRUE;
}
bool EnsureRealD3D9(){ InitOnceExecuteOnce(&g_once,LoadRealOnce,nullptr,nullptr); return g_real!=nullptr&&g_fn[IDX_Direct3DCreate9]!=nullptr; }
FARPROC RealProc(int i){ EnsureRealD3D9(); return (i>=0&&i<IDX_COUNT)?g_fn[i]:nullptr; }
}
using namespace pgengine;
// ------------------------------------------------------------------------ typed exports
extern "C" {
void* WINAPI Direct3DCreate9(UINT sdk){
    InitializeOnce();
    typedef void*(WINAPI* F)(UINT); F f=(F)RealProc(IDX_Direct3DCreate9);
    if(!f){ return nullptr; }
    void* d=f(sdk); if(d) OnD3D9ObjectCreated(d,false); return d;
}
HRESULT WINAPI Direct3DCreate9Ex(UINT sdk,void** out){
    InitializeOnce();
    typedef HRESULT(WINAPI* F)(UINT,void**); F f=(F)RealProc(IDX_Direct3DCreate9Ex);
    if(out) *out=nullptr; if(!f||!out) return E_NOTIMPL;
    HRESULT hr=f(sdk,out); if(SUCCEEDED(hr)&&*out) OnD3D9ObjectCreated(*out,true); return hr;
}
void* WINAPI Direct3DCreate9On12(UINT sdk,void* args,UINT n){
    InitializeOnce(); typedef void*(WINAPI* F)(UINT,void*,UINT); F f=(F)RealProc(IDX_Direct3DCreate9On12);
    if(!f) return nullptr; void* d=f(sdk,args,n); if(d) OnD3D9ObjectCreated(d,false); return d;
}
HRESULT WINAPI Direct3DCreate9On12Ex(UINT sdk,void* args,UINT n,void** out){
    InitializeOnce(); typedef HRESULT(WINAPI* F)(UINT,void*,UINT,void**); F f=(F)RealProc(IDX_Direct3DCreate9On12Ex);
    if(out) *out=nullptr; if(!f||!out) return E_NOTIMPL; HRESULT hr=f(sdk,args,n,out); if(SUCCEEDED(hr)&&*out) OnD3D9ObjectCreated(*out,true); return hr;
}
DWORD WINAPI D3DPERF_BeginEvent(DWORD col,LPCWSTR name){ typedef DWORD(WINAPI* F)(DWORD,LPCWSTR); F f=(F)RealProc(IDX_D3DPERF_BeginEvent); return f?f(col,name):(DWORD)-1; }
int   WINAPI D3DPERF_EndEvent(void){ typedef int(WINAPI* F)(void); F f=(F)RealProc(IDX_D3DPERF_EndEvent); return f?f():-1; }
DWORD WINAPI D3DPERF_GetStatus(void){ typedef DWORD(WINAPI* F)(void); F f=(F)RealProc(IDX_D3DPERF_GetStatus); return f?f():0; }
BOOL  WINAPI D3DPERF_QueryRepeatFrame(void){ typedef BOOL(WINAPI* F)(void); F f=(F)RealProc(IDX_D3DPERF_QueryRepeatFrame); return f?f():FALSE; }
void  WINAPI D3DPERF_SetMarker(DWORD col,LPCWSTR name){ typedef void(WINAPI* F)(DWORD,LPCWSTR); F f=(F)RealProc(IDX_D3DPERF_SetMarker); if(f) f(col,name); }
void  WINAPI D3DPERF_SetOptions(DWORD o){ typedef void(WINAPI* F)(DWORD); F f=(F)RealProc(IDX_D3DPERF_SetOptions); if(f) f(o); }
void  WINAPI D3DPERF_SetRegion(DWORD col,LPCWSTR name){ typedef void(WINAPI* F)(DWORD,LPCWSTR); F f=(F)RealProc(IDX_D3DPERF_SetRegion); if(f) f(col,name); }
}
// ------------------------------------------------------------------------ thunks for exports with undocumented signatures
// A thunk jumps straight into the real function (arguments and return address untouched). If the real function
// is unresolved it returns 0 instead of jumping through a null pointer. Games do not use these functions.
static_assert(IDX_DebugSetLevel==11&&IDX_DebugSetMute==12&&IDX_Direct3D9EnableMaximizedWindowedModeShim==13&&
              IDX_Direct3DShaderValidatorCreate9==14&&IDX_PSGPError==15&&IDX_PSGPSampleTexture==16,"thunk table indices changed");
extern "C" void __cdecl pg_thunk_ensure(void){ EnsureRealD3D9(); }
#define PG_THUNK(NAME,N) \
  extern "C" __declspec(naked) void __stdcall NAME(){ \
    __asm { call pg_thunk_ensure } \
    __asm { mov eax, dword ptr pg_thunk_tab[4*N] } \
    __asm { test eax,eax } \
    __asm { jz pg_failed } \
    __asm { jmp eax } \
    __asm { pg_failed: xor eax,eax } \
    __asm { ret } }
PG_THUNK(DebugSetLevel,11)
PG_THUNK(DebugSetMute,12)
PG_THUNK(Direct3D9EnableMaximizedWindowedModeShim,13)
PG_THUNK(Direct3DShaderValidatorCreate9,14)
PG_THUNK(PSGPError,15)
PG_THUNK(PSGPSampleTexture,16)
