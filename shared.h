#pragma once
#include <string>
#include <xcb/xproto.h>
#include <unordered_set>
#include <map>
#include <X11/keysym.h>

/* The letter is the key, keybind struct is intended to be used in a multimap */
struct Keybind {
    xcb_mod_mask_t Modifier;
    std::string Command;
};

/* Stuff we configure */
struct Runtime {
    std::multimap<unsigned int, struct Keybind> Keybinds; // Key is the letter / number / whatever associated with the keybind
    std::unordered_set<std::string> Monitors; // Settings for monitors
    std::multimap<std::string, std::string> Exports; // Environment Variables
    std::unordered_set<std::string> StartupCommands; // Commands to run at boot
};

