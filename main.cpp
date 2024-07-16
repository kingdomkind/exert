#include <cassert>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <algorithm>
#include <xcb/xcb.h>
#include <xcb/xcb_keysyms.h>
#include <unistd.h>
#include <xcb/xproto.h>
#include <X11/keysym.h>
#include <unordered_map>

struct WM {
    xcb_connection_t* Connection;
    xcb_screen_t* Screen;
};

WM WM;
/*

void UnFrameWindow(Window EventWindow) {
    const Window Frame = WM.Clients[EventWindow];
    XUnmapWindow(WM.RootDisplay, Frame);
    XReparentWindow(WM.RootDisplay, EventWindow, WM.RootWindow, 0, 0);
    XRemoveFromSaveSet(WM.RootDisplay, Frame);
    XDestroyWindow(WM.RootDisplay, Frame);
    WM.Clients.erase(EventWindow);
    XSync(WM.RootDisplay, false); // Ensure synchronization
    std::cout << "LOG: Unframed the window: " << EventWindow << std::endl;

}

void OnCreateNotify(const xcb_generic_event_t* NextEvent) {}

*/

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
    xcb_change_window_attributes_checked(WM.Connection, WM.Screen->root, XCB_CW_EVENT_MASK, (void*)&Masks);
    xcb_ungrab_key(WM.Connection, XCB_GRAB_ANY, WM.Screen->root, XCB_MOD_MASK_ANY); // Reset to known state

    xcb_grab_key(WM.Connection, 1, WM.Screen->root, XCB_MOD_MASK_1, XK_q, XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC);

    xcb_flush(WM.Connection);

    std::cout << "LOG: Starting up the WM" << std::endl;
}

void OnKeyPress(const xcb_generic_event_t* NextEvent) {
    xcb_key_press_event_t* Event = (xcb_key_press_event_t*)NextEvent;
    xcb_key_symbols_t* Keysyms = xcb_key_symbols_alloc(WM.Connection);
    xcb_keysym_t KeySym = xcb_key_symbols_get_keysym(Keysyms, Event->detail, 0);
    xcb_key_symbols_free(Keysyms);
    std::cout << "Pressed " << KeySym << std::endl;
    xcb_disconnect(WM.Connection);
}

void RunEventLoop() {
    std::cout << "LOG: Running the event loop" << std::endl;
    while (true) {
        xcb_generic_event_t* NextEvent = xcb_wait_for_event(WM.Connection);
        std::cout << "Recieved Event: " << NextEvent->response_type << std::endl;

        switch (NextEvent->response_type & ~0x80) {
            case XCB_MAP_REQUEST: { OnMapRequest(NextEvent); break; }
            case XCB_KEY_PRESS: { OnKeyPress(NextEvent); break; }
            /*
            case XCB_KEY_PRESS: {
                std::cout << "It's a keypress!" << std::endl;
                KeySym key = XKeycodeToKeysym(WM.RootDisplay, NextEvent.xkey.keycode, 0);
                if (key == XK_q && (NextEvent.xkey.state & Mod1Mask)) {
                    std::cout << "Exit key combination pressed. Exiting." << std::endl;
                    return; // Exit event loop
                }
                else if (key == XK_space && (NextEvent.xkey.state & Mod1Mask)) {
                    if (fork() == 0) {
                        std::cout << "showing rofi" << std::endl;
			            execl("/bin/sh", "/bin/sh", "-c", "rofi -show run", (void *)NULL);
                    }
                }
                else if (key == XK_c && (NextEvent.xkey.state & Mod1Mask)) {
                    Atom* SupportedProtocols;
                    int NumberOfSupportedProtocols;
                    if (XGetWMProtocols(WM.RootDisplay, NextEvent.xkey.window, &SupportedProtocols, &NumberOfSupportedProtocols) &&
                    (std::find(SupportedProtocols, SupportedProtocols + NumberOfSupportedProtocols, WM.WMDeleteWindow) != SupportedProtocols + NumberOfSupportedProtocols)) {
                        std::cout << "LOG: Gently deleting window " << NextEvent.xkey.window << std::endl;;
                        // 1. Construct message.
                        XEvent Message;
                        memset(&Message, 0, sizeof(Message));
                        Message.xclient.type = ClientMessage;
                        Message.xclient.message_type = WM.WMProtocols;
                        Message.xclient.window = NextEvent.xkey.window;
                        Message.xclient.format = 32;
                        Message.xclient.data.l[0] = WM.WMDeleteWindow;
                        // 2. Send message to window to be closed.
                        XSendEvent(WM.RootDisplay, NextEvent.xkey.window, false, 0, &Message);
                    } else {
                        std::cout << "LOG: Killing window " << NextEvent.xkey.window << std::endl;
                        XSetCloseDownMode(Display *,)
                        XKillClient(WM.RootDisplay, NextEvent.xkey.window);
                    }
                }
                break;
            } */
            default: {std::cerr << "Ignored Event: " << NextEvent->response_type << std::endl; break; }
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

    StartupWM();
    RunEventLoop();
    return EXIT_SUCCESS;
}