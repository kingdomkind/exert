#include "config.h"
#include <cassert>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <iostream>
#include <memory>
#include <ostream>
#include <unordered_map>
#include <unordered_set>
#include <xcb/xcb.h>
#include <xcb/xcb_keysyms.h>
#include <unistd.h>
#include <xcb/xproto.h>
#include <X11/keysym.h>

enum SplitLine {X,Y};

struct Inequality {
    SplitLine Axis;
    float Position;
};

struct Window {
    xcb_window_t Window;
    std::array<std::shared_ptr<Inequality>, 2> Inequalities;
};

struct WM {
    xcb_connection_t* Connection;
    xcb_screen_t* Screen;
    xcb_key_symbols_t* Keysyms;
    std::shared_ptr<Window> FocusedWindow;
    std::unordered_set<std::shared_ptr<Window>> VisibleWindows;
};

WM WM;

const uint32_t BORDER_WIDTH = 3;
const uint32_t INACTIVE_BORDER_COLOUR = 0xff0000;
const uint32_t ACTIVE_BORDER_COLOUR = 0x0000ff;

uint32_t Offset = 0;

void PrintVisibleWindows() {
    std::cout << "Visible Windows: ";
    for (auto WindowStruct: WM.VisibleWindows) {
        std::cout << WindowStruct->Window << " ";
    }
    std::cout << std::endl;
}

std::shared_ptr<Window> GetWindowStructFromWindow(xcb_window_t Window) {
    for (auto WindowStruct: WM.VisibleWindows) {
        if (WindowStruct->Window == Window) {
            return WindowStruct;
        }
    }
    std::cerr << "Could not find the specified window struct for window: " << Window << std::endl;
    exit(EXIT_FAILURE);
}

void OnEnterNotify(const xcb_generic_event_t* NextEvent) {
    xcb_enter_notify_event_t* Event = (xcb_enter_notify_event_t*) NextEvent;

    if (Event->event != 0) {
        if (WM.FocusedWindow != nullptr) {
            xcb_change_window_attributes(WM.Connection, WM.FocusedWindow->Window, XCB_CW_BORDER_PIXEL, &INACTIVE_BORDER_COLOUR);
        }
        std::cout << "Setting window focus to: " << Event->event << std::endl;
        xcb_set_input_focus(WM.Connection, XCB_INPUT_FOCUS_POINTER_ROOT, Event->event, XCB_CURRENT_TIME);
        WM.FocusedWindow = GetWindowStructFromWindow(Event->event);
        xcb_change_window_attributes(WM.Connection, WM.FocusedWindow->Window, XCB_CW_BORDER_PIXEL, &ACTIVE_BORDER_COLOUR);
        xcb_flush(WM.Connection);
    } else {
        std::cout << "Did not set focus to 0 (is it root?)" << std::endl;
    }
}

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

void StartupWM() {
    const uint32_t Masks = XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT | XCB_EVENT_MASK_STRUCTURE_NOTIFY |  XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY | XCB_EVENT_MASK_PROPERTY_CHANGE;
    xcb_change_window_attributes_checked(WM.Connection, WM.Screen->root, XCB_CW_EVENT_MASK, (void*)&Masks); std::cout << "Changed checked window attributes" << std::endl;
    xcb_ungrab_key(WM.Connection, XCB_GRAB_ANY, WM.Screen->root, XCB_MOD_MASK_ANY); std::cout << "Reset all grabbed keys" << std::endl;

    for (const auto &Pair : Runtime.Keybinds) {
        xcb_grab_key(WM.Connection, 0, WM.Screen->root, Pair.second.Modifier, Pair.first, XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC);
    }

    xcb_flush(WM.Connection); std::cout << "Starting up the WM" << std::endl;
}

void OnMapRequest(const xcb_generic_event_t* NextEvent) {
    xcb_map_request_event_t* Event = (xcb_map_request_event_t*)NextEvent;

    std::shared_ptr<Window> NewWindow = std::make_shared<Window>();
    NewWindow->Window = Event->window;

    // ADD POSITIONING + SPLIT LOGIC HERE!

    uint32_t Parameters[] = {Offset, Offset, 800, 800, BORDER_WIDTH};
    Offset += 40;
    uint32_t EventMasks[] = {XCB_EVENT_MASK_ENTER_WINDOW | XCB_EVENT_MASK_FOCUS_CHANGE};
    uint32_t ConfigureMasks = XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y | XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT | XCB_CONFIG_WINDOW_BORDER_WIDTH;

    xcb_change_window_attributes(WM.Connection, Event->window, XCB_CW_EVENT_MASK, &EventMasks);
    xcb_change_window_attributes(WM.Connection, Event->window, XCB_CW_BORDER_PIXEL, &INACTIVE_BORDER_COLOUR);
    xcb_configure_window(WM.Connection, Event->window, ConfigureMasks, Parameters);

    WM.VisibleWindows.insert(NewWindow);

    std::cout << "ADDED! " << Event->window << std::endl;  // FLAG
    PrintVisibleWindows();

    xcb_map_window(WM.Connection, Event->window);
    xcb_flush(WM.Connection);
}

void RemoveWindowStructFromWM(xcb_window_t Window) {
    for (auto WindowStruct: WM.VisibleWindows) {
        if (WindowStruct->Window == Window) {
            WM.VisibleWindows.erase(WindowStruct);
            std::cout << "ERASED! " << Window << std::endl;
            PrintVisibleWindows();
            return;
        }
    }

    if (WM.FocusedWindow->Window == Window) {
        std::cout << "Focused window has been deleted! Setting to nullptr" << std::endl;
        WM.FocusedWindow = nullptr;
    }
}

void OnUnMapNotify(const xcb_generic_event_t* NextEvent) {
    xcb_map_request_event_t* Event = (xcb_map_request_event_t*)NextEvent;
    RemoveWindowStructFromWM(Event->window);
}

void OnDestroyNotify(const xcb_generic_event_t* NextEvent) {
    xcb_destroy_notify_event_t* Event = (xcb_destroy_notify_event_t*)NextEvent;
    RemoveWindowStructFromWM(Event->window);
}


std::unordered_map<std::string, std::function<void()>> InternalCommand = {
    {"KillActive", []() { KillWindow(WM.FocusedWindow->Window); }},
    {"ExitWM", []() { ExitWM(); }},
};


void OnKeyPress(const xcb_generic_event_t* NextEvent) {
    xcb_key_press_event_t* Event = (xcb_key_press_event_t*)NextEvent;
    xcb_keycode_t Keycode = Event->detail;
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
    std::cout << "Running the event loop" << std::endl;
    if (fork() == 0) { std::cout << "showing kitty" << std::endl; execl("/bin/sh", "/bin/sh", "-c", "kitty", (void *)NULL);}

    while (true) {
        xcb_generic_event_t* NextEvent = xcb_wait_for_event(WM.Connection);
        switch (NextEvent->response_type & ~0x80) {
            case XCB_MAP_REQUEST: { OnMapRequest(NextEvent); break; }
            case XCB_KEY_PRESS: { OnKeyPress(NextEvent); break; }
            case XCB_UNMAP_NOTIFY: { OnUnMapNotify(NextEvent); break; }
            case XCB_DESTROY_NOTIFY: { OnDestroyNotify(NextEvent); break; }
            case XCB_ENTER_NOTIFY: { OnEnterNotify(NextEvent); break; }
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
    std::cout << "Initialised the connection" << std::endl;

    // Create a screen
    WM.Screen = xcb_setup_roots_iterator(xcb_get_setup(WM.Connection)).data;
    if (!WM.Screen) {
        std::cerr << "Failed to get the XCB screen!" << std::endl;
        return EXIT_FAILURE;
    }
    std::cout << "Initialised the screen" << std::endl;

    WM.Keysyms = xcb_key_symbols_alloc(WM.Connection);
    if (!WM.Keysyms) {
        std::cerr << "ERROR: Failed to allocate key symbols" << std::endl;
        return EXIT_FAILURE;
    }
    std::cout << "Initialised the key symbols" << std::endl;

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

    StartupWM();
    RunEventLoop();
    
    return EXIT_SUCCESS;
}