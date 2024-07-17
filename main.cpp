#include "config.h"
#include <cassert>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <iostream>
#include <memory>
#include <ostream>
#include <set>
#include <unordered_map>
#include <vector>
#include <xcb/xcb.h>
#include <xcb/xcb_keysyms.h>
#include <unistd.h>
#include <xcb/xproto.h>
#include <X11/keysym.h>


enum Split {
    VERTICAL,
    HORIZONTAL,
    NONE,
};

struct Node {
    Split Direction = NONE;
    // unsigned int Ratio; // eg. 0.4 means the left window is smaller
    
    std::unique_ptr<Node> Parent = nullptr;
    std::unique_ptr<Node> Left = nullptr;
    std::unique_ptr<Node> Right = nullptr;
};

struct WM {
    xcb_connection_t* Connection;
    xcb_screen_t* Screen;
    xcb_key_symbols_t* Keysyms;
    xcb_window_t InputWindow;
    //std::vector<std::unique_ptr<Node>> Tree;
    std::set<xcb_window_t> VisibleWindows;
};

WM WM;

const uint32_t BORDER_WIDTH = 3;

void KillWindow(xcb_window_t Window) {
    std::cout << "Attempting to kill window: " << Window << std::endl;
    xcb_kill_client(WM.Connection, Window);
    xcb_flush(WM.Connection);
}

void ExitWM() {
    free(WM.Keysyms);
    xcb_disconnect(WM.Connection);
}

unsigned int KeysymToKeycode(const unsigned int Keysym) {
    xcb_keycode_t* Keycodes = xcb_key_symbols_get_keycode(WM.Keysyms, Keysym);
    if (!Keycodes) {
        std::cerr << "Failed to get keycode for keysym: " << Keysym << std::endl;
        exit(EXIT_FAILURE);
    }

    xcb_keycode_t PrimaryKeycode = Keycodes[0]; // The first keycode is the main one
    free(Keycodes);
    return PrimaryKeycode;
}

unsigned int KeycodeToKeysym(const unsigned int Keycode) {
    xcb_keysym_t KeySym = xcb_key_symbols_get_keysym(WM.Keysyms, Keycode, 0);
    if (!KeySym) {
        std::cerr << "Failed to get keycode for keycode: " << Keycode << std::endl;
        exit(EXIT_FAILURE);
    }
    return KeySym;
}

void OnMapRequest(const xcb_generic_event_t* NextEvent) {
    xcb_map_request_event_t* Event = (xcb_map_request_event_t*)NextEvent;
    uint32_t Parameters[] = {0, 0, 800, 800, BORDER_WIDTH};
    uint32_t AttributesMasks[] = {XCB_EVENT_MASK_ENTER_WINDOW | XCB_EVENT_MASK_FOCUS_CHANGE | 0xff0000};
    uint32_t ConfigureMasks = XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y | XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT | XCB_CONFIG_WINDOW_BORDER_WIDTH;

    xcb_change_window_attributes(WM.Connection, Event->window, XCB_CW_EVENT_MASK | XCB_CW_BORDER_PIXEL, &AttributesMasks); // Before this was checked, test if needed
    xcb_configure_window(WM.Connection, Event->window, ConfigureMasks, Parameters);

    WM.VisibleWindows.insert(Event->window);
    std::cout << "ADDED!"; for (auto it = WM.VisibleWindows.begin(); it != WM.VisibleWindows.end(); ++it) {std::cout << *it << " "; } std::cout << std::endl; // FLAG

    xcb_map_window(WM.Connection, Event->window);
    xcb_flush(WM.Connection);
}

void StartupWM() {
    const uint32_t Masks = XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT | XCB_EVENT_MASK_STRUCTURE_NOTIFY |  XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY | XCB_EVENT_MASK_PROPERTY_CHANGE;
    xcb_change_window_attributes_checked(WM.Connection, WM.Screen->root, XCB_CW_EVENT_MASK, (void*)&Masks); std::cout << "LOG: Changed checked window attributes" << std::endl;
    xcb_ungrab_key(WM.Connection, XCB_GRAB_ANY, WM.Screen->root, XCB_MOD_MASK_ANY); std::cout << "LOG: Reset all grabbed keys" << std::endl;

    for (const auto &Pair : Runtime.Keybinds) {
        xcb_grab_key(WM.Connection, 0, WM.Screen->root, Pair.second.Modifier, Pair.first, XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC);
    }

    xcb_flush(WM.Connection); std::cout << "LOG: Starting up the WM" << std::endl;
}

void OnUnMapNotify(const xcb_generic_event_t* NextEvent) {
    xcb_map_request_event_t* Event = (xcb_map_request_event_t*)NextEvent;
    if (WM.VisibleWindows.count(Event->window)) {
        WM.VisibleWindows.erase(Event->window);
        std::cout << "ERASED! (Unmap)"; for (auto it = WM.VisibleWindows.begin(); it != WM.VisibleWindows.end(); ++it) {std::cout << *it << " "; } std::cout << std::endl; // FLAG
    }
}

void OnDestroyNotify(const xcb_generic_event_t* NextEvent) {
    xcb_destroy_notify_event_t* Event = (xcb_destroy_notify_event_t*)NextEvent;
    if (WM.VisibleWindows.count(Event->window)) {
        WM.VisibleWindows.erase(Event->window);
        std::cout << "ERASED! (Destroy)"; for (auto it = WM.VisibleWindows.begin(); it != WM.VisibleWindows.end(); ++it) {std::cout << *it << " "; } std::cout << std::endl; // FLAG
    }
}


std::unordered_map<std::string, std::function<void()>> InternalCommand = {
    {"KillActive", []() { KillWindow(WM.InputWindow); }},
    {"ExitWM", []() { ExitWM(); }},
};


void OnKeyPress(const xcb_generic_event_t* NextEvent) {
    xcb_key_press_event_t* Event = (xcb_key_press_event_t*)NextEvent;
    xcb_keycode_t Keycode = Event->detail;
    WM.InputWindow = Event->child;
    std::cout << "Set Input Window to " << WM.InputWindow << std::endl;
    auto TargetRange = Runtime.Keybinds.equal_range(Event->detail);
    if (TargetRange.first != TargetRange.second) {
        for (auto Pair = TargetRange.first; Pair != TargetRange.second; ++Pair) {
            if ((Event->state & Pair->second.Modifier) && Event->detail == Keycode) {
                std::string Prefix = "exert-command";
                std::string Command = Pair->second.Command;
                if (Command.rfind(Prefix, 0) == 0) {
                    std::string SubCommand = Command.substr(Prefix.length() + 1);
                    auto Found = InternalCommand.find(SubCommand);
                    if (Found != InternalCommand.end()) {
                        std::cout << "Executing Internal Command: " << SubCommand << std::endl; 
                        Found->second();
                    } else {
                        std::cerr << "No matching function to call for: " << SubCommand << std::endl;
                    }
                } else if (fork() == 0) {
                    std::cout << "Executing: " << Command << std::endl;
                    execl("/bin/sh", "/bin/sh", "-c", Command.c_str(), (void *)NULL);
                }
                return;
            }
        }
    }
}

void RunEventLoop() {
    std::cout << "LOG: Running the event loop" << std::endl;
    if (fork() == 0) { std::cout << "showing kitty" << std::endl; execl("/bin/sh", "/bin/sh", "-c", "kitty", (void *)NULL);}

    while (true) {
        xcb_generic_event_t* NextEvent = xcb_wait_for_event(WM.Connection);
        switch (NextEvent->response_type & ~0x80) {
            case XCB_MAP_REQUEST: { OnMapRequest(NextEvent); break; }
            case XCB_KEY_PRESS: { OnKeyPress(NextEvent); break; }
            case XCB_UNMAP_NOTIFY: { OnUnMapNotify(NextEvent); break; }
            case XCB_DESTROY_NOTIFY: { OnDestroyNotify(NextEvent); break; }
            default: { break; }
        }
    }
}

int main() {
    // Create a connection
    WM.Connection = xcb_connect(nullptr, nullptr);
    if (xcb_connection_has_error(WM.Connection)) {
        std::cerr << "Failed to open the XCB connection!" << std::endl;
        return EXIT_FAILURE;
    }
    std::cout << "LOG: Initialised the connection" << std::endl;

    // Create a screen
    WM.Screen = xcb_setup_roots_iterator(xcb_get_setup(WM.Connection)).data;
    if (!WM.Screen) {
        std::cerr << "Failed to get the XCB screen!" << std::endl;
        return EXIT_FAILURE;
    }
    std::cout << "LOG: Initialised the screen" << std::endl;

    WM.Keysyms = xcb_key_symbols_alloc(WM.Connection);
    if (!WM.Keysyms) {
        std::cerr << "ERROR: Failed to allocate key symbols" << std::endl;
        return EXIT_FAILURE;
    }
    std::cout << "LOG: Initialised the key symbols" << std::endl;

    Keybind Test = {};
    Test.Modifier = XCB_MOD_MASK_1;
    Test.Command = "rofi -show run";
    Runtime.Keybinds.insert({KeysymToKeycode(XK_space), Test});

    Keybind Test2 = {};
    Test2.Modifier = XCB_MOD_MASK_1;
    Test2.Command = "exert-command ExitWM";
    Runtime.Keybinds.insert({KeysymToKeycode(XK_m), Test2});

    Keybind Test3 = {};
    Test3.Modifier = XCB_MOD_MASK_1;
    Test3.Command = "exert-command KillActive";
    Runtime.Keybinds.insert({KeysymToKeycode(XK_c), Test3});

    //auto DefaultNode = std::make_unique<Node>();
    //7WM.Tree.push_back(DefaultNode);

    StartupWM();
    RunEventLoop();
    
    return EXIT_SUCCESS;
}