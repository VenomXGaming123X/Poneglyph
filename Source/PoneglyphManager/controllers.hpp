// Poneglyph Manager - silent controller detection (runs in the Manager, never inside the game).
#pragma once
#include <string>
#include <vector>
struct ControllerInfo { std::string name; std::string family; };
// Xbox family via XInput; PlayStation 3/4/5 and generic gamepads via the Windows HID device list. Read-only, no drivers installed.
std::vector<ControllerInfo> DetectControllers();
