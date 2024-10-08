#pragma once
#include <string>
#include <sys/types.h>
#include <xcb/xproto.h>
#include <unordered_set>
#include <map>
#include <X11/keysym.h>

#define MOUSE_LEFT_CLICK 1
#define MOUSE_SCROLL_WHEEL_CLICK 2
#define MOUSE_RIGHT_CLICK 3


/* The letter is the key, keybind struct is intended to be used in a multimap */
struct Keybind {
    unsigned int Modifier;
    std::string Command;
};

struct WMSettings {
    float MonitorPadding = 0; // Size of padding in pixels between monitor edges and windows
    float WindowPadding = 0; // Size of padding in pixels between windows
    float TiledWindowBorderSize = 3; // Size of borders in pixels on tiled windows
    float FloatingWindowBorderSize = 3; // Size of borders in pixels on floating windows
    int32_t ActiveTiledWindowBorderColour = -1; // Set to -1 to disable, otherwise set to colour
    int32_t InActiveTiledWindowBorderColour = -1;
    int32_t ActiveFloatingWindowBorderColour = -1;
    int32_t InActiveFloatingWindowBorderColour = -1;
};

/* Stuff we configure */
struct Runtime {
    WMSettings Settings; // Settings for WM
    std::multimap<unsigned int, struct Keybind> Keybinds; // Key is the letter / number / whatever associated with the keybind
    std::multimap<unsigned int, struct Keybind> Mousebinds;
    std::unordered_set<std::string> Monitors; // Settings for monitors
    std::multimap<std::string, std::string> Exports; // Environment Variables
    std::unordered_set<std::string> StartupCommands; // Commands to run at boot
};

