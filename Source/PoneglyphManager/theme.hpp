// Poneglyph Manager - the two themes required by the Master Plan (section 10).
//   0 = Silver Chrome / dark chrome        1 = Dark Purple Chrome
// Typeface: Segoe UI (body 10 pt, headings Segoe UI Semibold 15 pt). Text colours are chosen for >= 7:1 contrast.
#pragma once
#include <windows.h>
struct Theme {
    const wchar_t* name; COLORREF bg, sidebar, panelHi, panelLo, border, text, dim, accent, accentText, hot, listAlt;
};
static const Theme kThemes[2]={
 { L"Silver Chrome",       RGB(22,23,27), RGB(30,32,37), RGB(92,97,108), RGB(40,42,48), RGB(138,144,156), RGB(236,238,242), RGB(172,177,187), RGB(120,200,255), RGB(8,16,26),  RGB(122,130,146), RGB(32,34,39) },
 { L"Dark Purple Chrome",  RGB(20,12,32), RGB(30,18,48), RGB(108,74,164), RGB(44,28,68), RGB(150,112,208), RGB(244,236,255), RGB(196,178,226), RGB(190,134,255), RGB(18,6,32),  RGB(138,100,196), RGB(30,18,46) } };
