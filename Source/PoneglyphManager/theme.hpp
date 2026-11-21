// Poneglyph Manager - themes. 0 = Silver Chrome, 1 = Dark Purple Chrome, 2 = Black Chrome.
// Design rules (Microsoft Fluent 2): darker surfaces = less important, lighter = more important; one accent colour marks the main action;
// text/background contrast kept at or above 7:1.   Typeface: Segoe UI (final One Piece-style font to be decided with the owner).
#pragma once
#include <windows.h>
struct Theme {
    const wchar_t* name; COLORREF bg, sidebar, panelHi, panelLo, border, text, dim, accent, accentText, hot, listAlt;
};
static const int kThemeCount=3;
static const Theme kThemes[kThemeCount]={
 { L"Silver Chrome",      RGB(18,20,24), RGB(24,27,32), RGB(60,66,76),  RGB(44,49,58),  RGB(82,90,102),  RGB(240,244,250), RGB(168,178,192), RGB(126,200,255), RGB(6,18,30),  RGB(70,86,110), RGB(26,29,35) },
 { L"Dark Purple Chrome", RGB(17,10,28), RGB(25,15,41), RGB(82,56,128),  RGB(52,34,82),  RGB(112,84,166), RGB(246,240,255), RGB(194,176,224), RGB(198,146,255), RGB(22,8,38),  RGB(96,66,150), RGB(24,14,38) },
 { L"Black Chrome",       RGB(8,8,10),   RGB(14,14,17), RGB(58,59,64),   RGB(36,37,41),  RGB(96,98,106),  RGB(250,250,252), RGB(176,178,186), RGB(222,225,232), RGB(10,10,12), RGB(70,72,80),  RGB(13,13,16) } };
