// Poneglyph Engine - DLL entry. Does the absolute minimum under the loader lock: remember our handle.
// All real initialisation is deferred to the first Direct3DCreate9* call (outside the loader lock).
#include "engine.h"
BOOL APIENTRY DllMain(HMODULE h,DWORD reason,LPVOID reserved){
    switch(reason){
    case DLL_PROCESS_ATTACH: pgengine::g_self=h; DisableThreadLibraryCalls(h); break;
    case DLL_PROCESS_DETACH: pgengine::OnShutdown(reserved!=nullptr); break;
    }
    return TRUE;
}
