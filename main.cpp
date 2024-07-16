#include "config.h"
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <xcb/xcb.h>
#include <xcb/xcb_keysyms.h>
#include <unistd.h>
#include <xcb/xproto.h>
#include <X11/keysym.h>

struct WM {
    xcb_connection_t* Connection;
    xcb_screen_t* Screen;
    xcb_key_symbols_t* Keysyms;
};

WM WM;

void ExitWM() {
    free(WM.Keysyms);
    xcb_disconnect(WM.Connection);
}

unsigned int KeysymToKeycode(const unsigned int Keysym) {
    xcb_keycode_t* Keycodes = xcb_key_symbols_get_keycode(WM.Keysyms, Keysym);
    if (!Keycodes) {
        std::cout << "Failed to get keycode for keysym: " << Keysym << std::endl;
        exit(EXIT_FAILURE);
    }

    xcb_keycode_t PrimaryKeycode = Keycodes[0]; // The first keycode is the main one
    free(Keycodes);
    return PrimaryKeycode;
}

unsigned int KeycodeToKeysym(const unsigned int Keycode) {
    xcb_keysym_t KeySym = xcb_key_symbols_get_keysym(WM.Keysyms, Keycode, 0);
    if (!KeySym) {
        std::cout << "Failed to get keycode for keycode: " << Keycode << std::endl;
        exit(EXIT_FAILURE);
    }
    return KeySym;
}

void OnMapRequest(const xcb_generic_event_t* NextEvent) {
    xcb_map_request_event_t* Event = (xcb_map_request_event_t*)NextEvent;
    uint32_t Parameters[] = {0, 0, 800, 800, 3};
    uint32_t AttributesMasks[] = {XCB_EVENT_MASK_ENTER_WINDOW | XCB_EVENT_MASK_FOCUS_CHANGE};
    uint32_t ConfigureMasks = XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y | XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT | XCB_CONFIG_WINDOW_BORDER_WIDTH;

    xcb_change_window_attributes_checked(WM.Connection, Event->window, XCB_CW_EVENT_MASK, &AttributesMasks);
    xcb_configure_window(WM.Connection, Event->window, ConfigureMasks, Parameters);
    xcb_map_window(WM.Connection, Event->window);
    xcb_flush(WM.Connection);
}

void StartupWM() {
    const uint32_t Masks = XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT | XCB_EVENT_MASK_STRUCTURE_NOTIFY |  XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY | XCB_EVENT_MASK_PROPERTY_CHANGE;
    xcb_change_window_attributes_checked(WM.Connection, WM.Screen->root, XCB_CW_EVENT_MASK, (void*)&Masks); std::cout << "LOG: Changed checked window attributes" << std::endl;
    xcb_ungrab_key(WM.Connection, XCB_GRAB_ANY, WM.Screen->root, XCB_MOD_MASK_ANY); std::cout << "LOG: Reset all grabbed keys" << std::endl;

    //xcb_grab_key(WM.Connection, 0, WM.Screen->root, XCB_MOD_MASK_1, KeysymToKeycode(XK_m), XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC);
    //xcb_grab_key(WM.Connection, 0, WM.Screen->root, XCB_MOD_MASK_1, KeysymToKeycode(XK_space), XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC);

    for (const auto &Pair : CachedData.Keybinds) {
        xcb_grab_key(WM.Connection, 0, WM.Screen->root, Pair.second.Modifier, Pair.first, XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC);
    }

    xcb_flush(WM.Connection); std::cout << "LOG: Starting up the WM" << std::endl;
}

void OnKeyPress(const xcb_generic_event_t* NextEvent) {
    xcb_key_press_event_t* Event = (xcb_key_press_event_t*)NextEvent;
    xcb_keycode_t Keycode = Event->detail;
    xcb_keysym_t KeySym = KeycodeToKeysym(Keycode);
    std::cout << "Pressed " << KeySym << std::endl;
/*
    if ((Event->state & XCB_MOD_MASK_1) && (KeySym == XK_m)) {
        ExitWM();
    } else if ((Event->state & XCB_MOD_MASK_1) && (KeySym == XK_space)) {
        if (fork() == 0) { std::cout << "showing rofi" << std::endl; execl("/bin/sh", "/bin/sh", "-c", "rofi -show run", (void *)NULL);}
    } */

    auto TargetRange = CachedData.Keybinds.equal_range(Event->detail);
    if (TargetRange.first != TargetRange.second) {
        for (auto Pair = TargetRange.first; Pair != TargetRange.second; ++Pair) {
            if ((Event->state & Pair->second.Modifier) && Event->detail == Keycode) {
                if (fork() == 0) {
                    std::cout << "Executing: " << Pair->second.Command << std::endl;
                    execl("/bin/sh", "/bin/sh", "-c", Pair->second.Command.c_str(), (void *)NULL);
                }
                return;
            }
        }
    }
}

void RunEventLoop() {
    std::cout << "LOG: Running the event loop" << std::endl;
    if (fork() == 0) { std::cout << "showing xterm" << std::endl; execl("/bin/sh", "/bin/sh", "-c", "xterm", (void *)NULL);}

    while (true) {
        xcb_generic_event_t* NextEvent = xcb_wait_for_event(WM.Connection);
        std::cout << "Recieved Event: " << NextEvent->response_type << std::endl;

        switch (NextEvent->response_type & ~0x80) {
            case XCB_MAP_REQUEST: { OnMapRequest(NextEvent); break; }
            case XCB_KEY_PRESS: { OnKeyPress(NextEvent); break; }
            default: {std::cout << "Ignored Event: " << NextEvent->response_type << std::endl; break; }
        }
    }
}

int main() {
    // Create a connection
    WM.Connection = xcb_connect(nullptr, nullptr);
    if (xcb_connection_has_error(WM.Connection)) {
        std::cout << "Failed to open the XCB connection!" << std::endl;
        return EXIT_FAILURE;
    }
    std::cout << "LOG: Initialised the connection" << std::endl;

    // Create a screen
    WM.Screen = xcb_setup_roots_iterator(xcb_get_setup(WM.Connection)).data;
    if (!WM.Screen) {
        std::cout << "Failed to get the XCB screen!" << std::endl;
        return EXIT_FAILURE;
    }
    std::cout << "LOG: Initialised the screen" << std::endl;

    WM.Keysyms = xcb_key_symbols_alloc(WM.Connection);
    if (!WM.Keysyms) {
        std::cout << "ERROR: Failed to allocate key symbols" << std::endl;
        return EXIT_FAILURE;
    }
    std::cout << "LOG: Initialised the key symbols" << std::endl;

    Keybind Test = {};
    Test.Modifier = XCB_MOD_MASK_1;
    Test.Command = "rofi -show run";
    CachedData.Keybinds.insert({KeysymToKeycode(XK_space), Test});

    Keybind Test2 = {};
    Test2.Modifier = XCB_MOD_MASK_1;
    Test2.Command = "pkill exert";
    CachedData.Keybinds.insert({KeysymToKeycode(XK_m), Test2}); 

    StartupWM();
    RunEventLoop();
    
    return EXIT_SUCCESS;
}