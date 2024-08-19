#pragma once
#include <string>
#include <sys/types.h>
#include <xcb/xproto.h>
#include <unordered_set>
#include <map>
#include <X11/keysym.h>

/* The letter is the key, keybind struct is intended to be used in a multimap */
struct Keybind {
    xcb_mod_mask_t Modifier;
    std::string Command;
};

struct Settings {
    float MonitorPadding = 0; // Size of padding in pixels between monitor edges and windows
    float WindowPadding = 0; // Size of padding in pixels between windows
    float BorderSize = 0; // Size of borders in pixels on windows
    int32_t ActiveTiledWindowBorderColour = -1; // Set to -1 to disable, otherwise set to colour
    int32_t InActiveTiledWindowBorderColour = -1;
    int32_t ActiveFloatingWindowBorderColour = -1;
    int32_t InActiveFloatingWindowBorderColour = -1;
};

/* Stuff we configure */
struct Runtime {
    Settings Settings; // Settings for WM
    std::multimap<unsigned int, struct Keybind> Keybinds; // Key is the letter / number / whatever associated with the keybind
    std::unordered_set<std::string> Monitors; // Settings for monitors
    std::multimap<std::string, std::string> Exports; // Environment Variables
    std::unordered_set<std::string> StartupCommands; // Commands to run at boot
};

