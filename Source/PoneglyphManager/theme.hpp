// Poneglyph Manager - themes (approved design preview).  0 = Silver Chrome, 1 = Dark Purple Chrome, 2 = Elbaf Aurora.
// `frame` is the bright outline around the window; the SAME colours drive the title bar, its three buttons, tabs, menu and dropdowns.
// Elbaf Aurora is an original design (deep forest night, aurora mint, Norse-gold frame) and uses no official artwork.
#pragma once
#include <windows.h>
struct Theme {
    const wchar_t* name; COLORREF bg, sidebar, panelHi, panelLo, border, frame, text, dim, header, accent, accentText, hot, listAlt, ok, warn, danger;
};
static const int kThemeCount=3;
static const Theme kThemes[kThemeCount]={
 { L"Silver Chrome",      RGB(11,16,24), RGB(17,26,42), RGB(28,45,73),  RGB(22,34,56),  RGB(42,60,92),  RGB(207,224,245), RGB(238,243,251), RGB(157,176,202), RGB(127,214,255), RGB(92,195,255),  RGB(4,18,31),  RGB(52,78,120),  RGB(15,23,38), RGB(94,224,154),  RGB(255,193,90),  RGB(255,107,122) },
 { L"Dark Purple Chrome", RGB(13,8,32),  RGB(21,12,48), RGB(41,23,99),  RGB(31,17,70),  RGB(71,41,138), RGB(199,160,255), RGB(246,240,255), RGB(187,167,230), RGB(121,184,255), RGB(181,124,255), RGB(24,6,50),  RGB(100,64,170), RGB(22,12,44), RGB(98,226,154),  RGB(255,203,102), RGB(255,107,138) },
 { L"Elbaf Aurora",      RGB(6,17,13),  RGB(10,27,21), RGB(22,56,42),  RGB(16,42,32),  RGB(43,90,69),  RGB(224,184,104), RGB(244,239,225), RGB(166,189,175), RGB(111,230,196), RGB(240,185,74),  RGB(37,24,0),  RGB(44,96,72),   RGB(11,30,23), RGB(111,230,160), RGB(255,207,112), RGB(255,122,107) } };
