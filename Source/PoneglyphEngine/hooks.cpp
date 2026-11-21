// Poneglyph Engine - optional lightweight frame-time sampler.
// Method: patch the Present slot of the device's vtable. The hook does only QueryPerformanceCounter + one
// array store. A background thread aggregates every N seconds. Measures the interval between consecutive Present
// calls (CPU-side frame time). It is not GPU time and is only active when Diagnostics/FrameStats=1.
#include "engine.h"
#include "poneglyph/stats.hpp"
#include "poneglyph/util.hpp"
#include <atomic>
#include <vector>
#include <cstdio>
namespace pgengine {
namespace {
constexpr size_t kRing=16384;                       // power of two
float g_ring[kRing]; std::atomic<unsigned long> g_head{0}; LARGE_INTEGER g_last{}, g_freq{};
void* g_patchedVtables[8]; int g_nPatched=0;
typedef HRESULT (STDMETHODCALLTYPE* PFN_CreateDevice)(void*,UINT,UINT,HWND,DWORD,void*,void**);
typedef HRESULT (STDMETHODCALLTYPE* PFN_Present)(void*,const RECT*,const RECT*,HWND,const void*);
typedef HRESULT (STDMETHODCALLTYPE* PFN_PresentEx)(void*,const RECT*,const RECT*,HWND,const void*,DWORD);
PFN_CreateDevice g_origCreateDevice=nullptr, g_origCreateDeviceEx=nullptr; PFN_Present g_origPresent=nullptr; PFN_PresentEx g_origPresentEx=nullptr;
inline void Tick(){
    LARGE_INTEGER n; QueryPerformanceCounter(&n);
    if(g_last.QuadPart){ double ms=(double)(n.QuadPart-g_last.QuadPart)*1000.0/(double)g_freq.QuadPart; if(ms<10000.0){ unsigned long h=g_head.load(std::memory_order_relaxed); g_ring[h&(kRing-1)]=(float)ms; g_head.store(h+1,std::memory_order_release); } }
    g_last=n;
}
HRESULT STDMETHODCALLTYPE HookPresent(void* self,const RECT* a,const RECT* b,HWND w,const void* c){ Tick(); return g_origPresent(self,a,b,w,c); }
HRESULT STDMETHODCALLTYPE HookPresentEx(void* self,const RECT* a,const RECT* b,HWND w,const void* c,DWORD f){ Tick(); return g_origPresentEx(self,a,b,w,c,f); }
bool PatchSlot(void** vtbl,int idx,void* hook,void** origOut){
    for(int i=0;i<g_nPatched;i++) if(g_patchedVtables[i]==(void*)(vtbl+idx)) return true;   // already patched (shared vtable)
    DWORD old; if(!VirtualProtect(&vtbl[idx],sizeof(void*),PAGE_READWRITE,&old)) return false;
    if(!*origOut) *origOut=vtbl[idx]; if(vtbl[idx]!=hook) vtbl[idx]=hook; DWORD tmp; VirtualProtect(&vtbl[idx],sizeof(void*),old,&tmp);
    if(g_nPatched<8) g_patchedVtables[g_nPatched++]=(void*)(vtbl+idx); return true;
}
void HookDevice(void* dev,bool ex){
    if(!dev) return; void** vt=*(void***)dev;
    if(PatchSlot(vt,17,(void*)HookPresent,(void**)&g_origPresent)) g_log.log(pg::LogLevel::Debug,"Present hooked");
    if(ex) PatchSlot(vt,121,(void*)HookPresentEx,(void**)&g_origPresentEx);
}
HRESULT STDMETHODCALLTYPE HookCreateDevice(void* self,UINT a,UINT t,HWND w,DWORD f,void* p,void** out){
    HRESULT hr=g_origCreateDevice(self,a,t,w,f,p,out); if(SUCCEEDED(hr)&&out&&*out) HookDevice(*out,false); return hr; }
HRESULT STDMETHODCALLTYPE HookCreateDeviceEx(void* self,UINT a,UINT t,HWND w,DWORD f,void* p,void* m,void** out){
    typedef HRESULT (STDMETHODCALLTYPE* F)(void*,UINT,UINT,HWND,DWORD,void*,void*,void**); HRESULT hr=((F)g_origCreateDeviceEx)(self,a,t,w,f,p,m,out); if(SUCCEEDED(hr)&&out&&*out) HookDevice(*out,true); return hr; }
// sampler thread
HANDLE g_thread=nullptr, g_stop=nullptr; wchar_t g_csv[MAX_PATH]; int g_secs=10; unsigned long g_tail=0; bool g_started=false;
void Flush(){
    unsigned long h=g_head.load(std::memory_order_acquire); unsigned long n=h-g_tail; if(n==0) return; if(n>kRing){ g_tail=h-kRing; n=kRing; }
    std::vector<double> v; v.reserve(n); for(unsigned long i=g_tail;i<h;i++) v.push_back((double)g_ring[i&(kRing-1)]); g_tail=h;
    auto s=pg::summarizeFrames(v); bool fresh=GetFileAttributesW(g_csv)==INVALID_FILE_ATTRIBUTES;
    WIN32_FILE_ATTRIBUTE_DATA fa; if(!fresh&&GetFileAttributesExW(g_csv,GetFileExInfoStandard,&fa)&&fa.nFileSizeLow>512*1024){ std::wstring old=std::wstring(g_csv)+L".1"; MoveFileExW(g_csv,old.c_str(),MOVEFILE_REPLACE_EXISTING); fresh=true; }
    FILE* f=_wfopen(g_csv,L"ab"); if(!f) return; if(fresh) fprintf(f,"%s\n",pg::summaryCsvHeader().c_str()); fprintf(f,"%s\n",pg::summaryCsvLine(pg::nowStamp(),s).c_str()); fclose(f);
}
DWORD WINAPI SamplerMain(LPVOID){ SetThreadPriority(GetCurrentThread(),THREAD_PRIORITY_BELOW_NORMAL); while(WaitForSingleObject(g_stop,(DWORD)g_secs*1000)==WAIT_TIMEOUT) Flush(); Flush(); return 0; }
}
void InstallD3DHooks(void* d3d,bool ex){
    if(!d3d) return; QueryPerformanceFrequency(&g_freq); void** vt=*(void***)d3d;
    if(ex) PatchSlot(vt,20,(void*)HookCreateDeviceEx,(void**)&g_origCreateDeviceEx);
    PatchSlot(vt,16,(void*)HookCreateDevice,(void**)&g_origCreateDevice);   // IDirect3D9::CreateDevice (index 16 in both interfaces)
}
void StartFrameSampler(const wchar_t* csv,int secs){
    if(g_started) return; g_started=true; lstrcpynW(g_csv,csv,MAX_PATH); g_secs=secs; g_stop=CreateEventW(nullptr,TRUE,FALSE,nullptr);
    if(g_stop) g_thread=CreateThread(nullptr,0,SamplerMain,nullptr,0,nullptr);
}
void StopFrameSampler(bool wait){
    if(!g_started) return; if(wait&&g_stop&&g_thread){ SetEvent(g_stop); WaitForSingleObject(g_thread,3000); } else Flush();
}
}
