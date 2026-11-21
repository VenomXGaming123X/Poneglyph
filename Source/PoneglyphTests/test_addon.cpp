// Tiny x86 add-on used by pg_proxytest to prove the engine's add-on runtime works.
#include <windows.h>
#include "poneglyph_sdk.h"
extern "C" __declspec(dllexport) unsigned long __stdcall Poneglyph_AddonInit(const PoneglyphHostApi* host){
    if(!host||host->apiVersion!=PONEGLYPH_API_VERSION) return 2;
    wchar_t p[MAX_PATH]; HMODULE me=nullptr; GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,(LPCWSTR)&Poneglyph_AddonInit,&me);
    GetModuleFileNameW(me,p,MAX_PATH); wchar_t* s=wcsrchr(p,L'\\'); if(s) *(s+1)=0; lstrcatW(p,L"init_called.txt");
    HANDLE f=CreateFileW(p,GENERIC_WRITE,0,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr); if(f==INVALID_HANDLE_VALUE) return 3; DWORD n; WriteFile(f,"ok",2,&n,nullptr); CloseHandle(f);
    if(host->Log) host->Log("test_addon",PONEGLYPH_LOG_INFO,"hello from the test add-on"); return 0;
}
BOOL APIENTRY DllMain(HMODULE,DWORD,LPVOID){ return TRUE; }
