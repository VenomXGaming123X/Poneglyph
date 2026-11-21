/*
 * Poneglyph SDK  -  public header for add-on (ASI / DLL) authors.
 * API version 1.  License: MIT (see LICENSE).
 *
 * An add-on is an ordinary 32-bit (x86) DLL, usually renamed to .asi.
 * Poneglyph loads it with LoadLibrary, so DllMain behaves as usual.
 * To receive the host API, optionally export:
 *
 *     extern "C" __declspec(dllexport) unsigned long __stdcall Poneglyph_AddonInit(const PoneglyphHostApi* host);
 *
 * Return 0 for success; any other value is logged as a failure (the DLL stays loaded; Poneglyph never
 * unloads an add-on, because its code may still be running).
 *
 * Optional metadata: put "<yourfile>.asi.pgmeta" next to the add-on:
 *     [Addon]
 *     Name=My Fix
 *     Version=1.0
 *     Author=Me
 *     Games=ffxiii,ffxiii2,lr13        ; or all
 *     Arch=x86
 *     Dependencies=other.asi
 *     Conflicts=oldfix.asi
 *     LoadOrder=1000
 */
#ifndef PONEGLYPH_SDK_H
#define PONEGLYPH_SDK_H
#include <wchar.h>
#ifdef __cplusplus
extern "C" {
#endif
#define PONEGLYPH_API_VERSION 1
#define PONEGLYPH_LOG_ERROR 0
#define PONEGLYPH_LOG_WARN  1
#define PONEGLYPH_LOG_INFO  2
#define PONEGLYPH_LOG_DEBUG 3
typedef struct PoneglyphHostApi {
    unsigned long   size;              /* sizeof(PoneglyphHostApi) */
    unsigned long   apiVersion;        /* PONEGLYPH_API_VERSION */
    const char*     engineVersion;     /* e.g. "1.0.0" */
    const char*     gameKey;           /* "ffxiii", "ffxiii2", "lr13" or "unknown" */
    const wchar_t*  gameDir;           /* folder of the game executable */
    void (__stdcall *Log)(const char* addonName, int level, const char* message);
} PoneglyphHostApi;
typedef unsigned long (__stdcall *PFN_Poneglyph_AddonInit)(const PoneglyphHostApi*);
#ifdef __cplusplus
}
#endif
#endif
