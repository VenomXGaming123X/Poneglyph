// Poneglyph Manager - themes. 0 = Silver Chrome, 1 = Dark Purple Chrome, 2 = Black Chrome.
// Design rules (Microsoft Fluent 2): darker surfaces = less important, lighter = more important; one accent colour marks the main action;
// text/background contrast kept at or above 7:1.  `frame` is the bright outline drawn around the window and content panel.
// Typefaces are chosen at run time from what Windows has: headings Bahnschrift, body Segoe UI Variable / Segoe UI, logs Cascadia Mono / Consolas.
#pragma once
#include <windows.h>
struct Theme {
    const wchar_t* name; COLORREF bg, sidebar, panelHi, panelLo, border, frame, text, dim, header, accent, accentText, hot, listAlt, ok, warn;
};
static const int kThemeCount=3;
static const Theme kThemes[kThemeCount]={
 //                        bg              sidebar         panelHi          panelLo          border           frame             text              dim               header           accent            accentText     hot              listAlt         ok              warn
 { L"Silver Chrome",      RGB(12,16,24),  RGB(18,25,38),  RGB(58,72,96),   RGB(32,42,60),   RGB(84,104,138), RGB(214,224,240), RGB(240,245,252), RGB(160,176,200), RGB(110,205,255), RGB(96,190,255),  RGB(4,16,30),  RGB(52,78,120),  RGB(17,23,34),  RGB(90,220,140), RGB(255,196,90) },
 { L"Dark Purple Chrome", RGB(14,8,28),   RGB(26,13,50),  RGB(96,60,160),   RGB(46,26,84),   RGB(120,86,190), RGB(190,150,255), RGB(246,240,255), RGB(190,170,224), RGB(112,182,255), RGB(176,110,255), RGB(20,6,38),  RGB(100,64,170), RGB(22,12,40),  RGB(96,226,150), RGB(255,200,96) },
 { L"Black Chrome",       RGB(6,6,8),     RGB(13,13,16),  RGB(64,66,74),   RGB(30,31,36),   RGB(100,104,116),RGB(236,238,244), RGB(250,250,252), RGB(170,174,186), RGB(226,230,240), RGB(224,228,238), RGB(8,8,10),   RGB(74,76,88),   RGB(11,11,14),  RGB(100,226,150),RGB(255,204,102) } };
