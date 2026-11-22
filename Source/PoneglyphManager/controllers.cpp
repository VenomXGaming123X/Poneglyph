#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <cstdio>
#include <set>
#include "controllers.hpp"
namespace {
typedef DWORD (WINAPI* PFN_XInputGetState)(DWORD,void*);
std::string hex4(unsigned v){ char b[8]; snprintf(b,sizeof b,"%04X",v&0xFFFF); return b; }
}
std::vector<ControllerInfo> DetectControllers(){
    std::vector<ControllerInfo> r;
    HMODULE x=LoadLibraryW(L"xinput1_4.dll"); if(!x) x=LoadLibraryW(L"xinput1_3.dll");
    if(x){ auto f=(PFN_XInputGetState)GetProcAddress(x,"XInputGetState");
        if(f) for(DWORD i=0;i<4;i++){ unsigned char st[16]={0}; if(f(i,st)==ERROR_SUCCESS) r.push_back({"Xbox controller (player "+std::to_string(i+1)+")","Xbox"}); }
        FreeLibrary(x); }
    UINT n=0; GetRawInputDeviceList(nullptr,&n,sizeof(RAWINPUTDEVICELIST));
    if(n){ std::vector<RAWINPUTDEVICELIST> l(n);
        if(GetRawInputDeviceList(l.data(),&n,sizeof(RAWINPUTDEVICELIST))!=(UINT)-1){
            std::set<std::string> seen;
            for(UINT i=0;i<n;i++){
                if(l[i].dwType!=RIM_TYPEHID) continue; RID_DEVICE_INFO info{}; info.cbSize=sizeof info; UINT sz=sizeof info;
                if(GetRawInputDeviceInfoW(l[i].hDevice,RIDI_DEVICEINFO,&info,&sz)==(UINT)-1) continue;
                if(info.hid.usUsagePage!=1||(info.hid.usUsage!=4&&info.hid.usUsage!=5)) continue;   // joystick / gamepad only
                unsigned vid=info.hid.dwVendorId, pid=info.hid.dwProductId; if(vid==0x045E) continue; // Microsoft pads are reported through XInput above
                std::string key=hex4(vid)+":"+hex4(pid); if(seen.count(key)) continue; seen.insert(key);
                std::string name,fam="Generic";
                if(vid==0x054C){ fam="PlayStation";
                    if(pid==0x0268) name="PlayStation 3 controller (DualShock 3)"; else if(pid==0x05C4||pid==0x09CC) name="PlayStation 4 controller (DualShock 4)";
                    else if(pid==0x0CE6) name="PlayStation 5 controller (DualSense)"; else if(pid==0x0DF2) name="PlayStation 5 controller (DualSense Edge)"; else name="PlayStation controller (ID "+hex4(pid)+")"; }
                else name="Game controller (ID "+hex4(vid)+":"+hex4(pid)+")";
                r.push_back({name,fam}); } } }
    return r;
}
