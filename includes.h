#pragma once
#include <map>
#include <memory>
#include <string>
#include <thread>
#include <vector>
#include <xcb/randr.h>
#include <xcb/xcb_keysyms.h>
#include <xcb/xproto.h>
#include <X11/keysym.h>


struct Keybind {
    xcb_mod_mask_t Modifier;
    std::string Command;
};

enum Split {
    VERTICAL, // 0
    HORIZONTAL, // 1
    NONE, // 2
};
enum WindowSegment {
    LEFT, // Remaining 2/4 middle left
    RIGHT, // Remaining 2/4 middle right
    UP, // Top 1/4 of the window
    DOWN, // Bottom 1/4 of the window
};

struct Coordinate {
    float X;
    float Y;
};

struct Window {
    xcb_window_t Window;
};

struct Protocols {
    xcb_atom_t Protocols;
    xcb_atom_t DeleteWindow;
};

struct Container { // If Direction is None it will have a Value (and no left / right pointer), otherwise it will have no value (and have left / right pointers)
    Split Direction;
    
    std::shared_ptr<Container> Parent = nullptr;
    std::shared_ptr<Container> Left = nullptr;
    std::shared_ptr<Container> Right = nullptr;

    std::shared_ptr<Window> Value = nullptr;
};

struct Workspace {
    std::shared_ptr<Container> RootContainer = nullptr;
};

struct Monitor {
    xcb_randr_output_t Output;
    std::string Name;
    int X;
    int Y;
    int Width;
    int Height;

    int ActiveWorkspace = -1;
};

struct WindowMetadata {
    std::shared_ptr<struct Container> Container;
    int Workspace = -1;
};

struct WM {
    xcb_connection_t* Connection;
    xcb_screen_t* Screen;
    xcb_key_symbols_t* Keysyms;
    std::shared_ptr<Container> FocusedContainer;
    std::vector<std::shared_ptr<Monitor>> Monitors;
    std::vector<std::shared_ptr<Workspace>> Workspaces;

    Protocols ProtocolsContainer;
};

struct Runtime {
    // Key is the letter / number / whatever associated with the keybind
    std::multimap<unsigned int, struct Keybind> Keybinds;
    std::multimap<std::string, std::string> Exports; // Environment Variables
    std::string StartupCommands; // Commands to run at boot
};

const uint32_t BORDER_WIDTH = 0;
const uint32_t INACTIVE_BORDER_COLOUR = 0xff0000;
const uint32_t ACTIVE_BORDER_COLOUR = 0x0000ff;
const uint32_t OFFSCREEN_WINDOW_POSITION[] = {10000, 10000};

const std::string PIPE_PATH = "/tmp/wmking-runtime";

static std::thread ListeningThread;
static Runtime Runtime;
static WM WM;

void CheckAndCreatePipe();
void StartPipeListener();