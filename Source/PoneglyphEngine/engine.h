#pragma once
// Poneglyph Engine - internal interfaces (x86 only).
#ifndef _M_IX86
#error "PoneglyphEngine must be built for x86 (Win32). The three games are 32-bit."
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include "poneglyph/log.hpp"
#include "poneglyph/ini.hpp"
#include "poneglyph/games.hpp"
#define PG_ENGINE_VERSION "1.0.0"
namespace pgengine {
extern HMODULE g_self;                 // this DLL
extern pg::Logger g_log;
// proxy.cpp
void SetRealD3D9Override(const wchar_t* fullPath);   // DXVK (or another d3d9.dll) to chain to instead of the Windows one
bool UsingDxvk();
bool EnsureRealD3D9();                 // loads the genuine system DLL by full path (once); false on failure
FARPROC RealProc(int index);           // nullptr if unavailable - callers MUST check
enum { IDX_Direct3DCreate9, IDX_Direct3DCreate9Ex, IDX_Direct3DCreate9On12, IDX_Direct3DCreate9On12Ex,
       IDX_D3DPERF_BeginEvent, IDX_D3DPERF_EndEvent, IDX_D3DPERF_GetStatus, IDX_D3DPERF_QueryRepeatFrame,
       IDX_D3DPERF_SetMarker, IDX_D3DPERF_SetOptions, IDX_D3DPERF_SetRegion, IDX_DebugSetLevel, IDX_DebugSetMute,
       IDX_Direct3D9EnableMaximizedWindowedModeShim, IDX_Direct3DShaderValidatorCreate9, IDX_PSGPError, IDX_PSGPSampleTexture, IDX_COUNT };
// runtime.cpp
void InitializeOnce();                 // config, recovery, add-on loading. Never called from DllMain.
bool FrameStatsEnabled();
void OnD3D9ObjectCreated(void* d3d9, bool isEx);   // may install lightweight vtable hooks
void OnShutdown(bool processTerminating);
// hooks.cpp
void InstallD3DHooks(void* d3d9, bool isEx);
void StartFrameSampler(const wchar_t* csvPath,int summarySeconds);
void StopFrameSampler(bool wait);
extern const char kEngineId[];
}
