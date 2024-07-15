#include <cassert>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <algorithm>
#include <xcb/xcb.h>
#include <xcb/xcb_keysyms.h>
#include <unistd.h>
#include <xcb/xproto.h>
#include <xkbcommon/xkbcommon-keysyms.h>
#include <xkbcommon/xkbcommon.h>
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
    uint32_t Parameters[5] = {0, 0, 800, 800, 3};
    uint32_t Masks[1] = {XCB_EVENT_MASK_ENTER_WINDOW | XCB_EVENT_MASK_FOCUS_CHANGE};

    xcb_change_window_attributes_checked(WM.Connection, Event->window, XCB_CW_EVENT_MASK, Masks);
    xcb_configure_window(WM.Connection, Event->window, XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y | XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT | XCB_CONFIG_WINDOW_BORDER_WIDTH,
        Parameters);
    xcb_map_window(WM.Connection, Event->window);
    xcb_flush(WM.Connection);
}

/*
void OnUnmapNotify(const XUnmapEvent& NextEvent) {
    if (WM.Clients.count(NextEvent.window) == 0) {
        std::cout << "LOG: Ignored unmap request on a window that isn't our client" << std::endl;
    } else {
        UnFrameWindow(NextEvent.window);
    }
}

void OnConfigureRequest(const XConfigureRequestEvent& NextEvent) {
    // Mirror Changes
    XWindowChanges Changes;
    Changes.x = NextEvent.x;
    Changes.y = NextEvent.y;
    Changes.width = NextEvent.width;
    Changes.height = NextEvent.height;
    Changes.border_width = NextEvent.border_width;
    Changes.sibling = NextEvent.above;
    Changes.stack_mode = NextEvent.detail;

    if (WM.Clients.count(NextEvent.window)) {
        const Window Frame = WM.Clients[NextEvent.window];
        XConfigureWindow(WM.RootDisplay, Frame, NextEvent.value_mask, &Changes);
    }

    // Send Request
    XConfigureWindow(WM.RootDisplay, NextEvent.window, NextEvent.value_mask, &Changes);
    std::cout << "Resize " << NextEvent.window << " to W:" << NextEvent.width << ", H:" << NextEvent.height << std::endl;
}

int OnXError(Display* Display, XErrorEvent* Error) {
    std::cerr << "X ERROR: " << Error->error_code << std::endl;
    return 0;
}

int OnOtherWMDetected(Display* Display, XErrorEvent* Error) {
    assert(Error->error_code == BadAccess);
    std::cerr << "Another window manager / X client is already running!" << std::endl;
    exit(EXIT_FAILURE);
    return 0;
}
*/
void StartupWM() {
    const uint32_t Masks = XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT | XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY;
    xcb_change_window_attributes_checked(WM.Connection, WM.Screen->root, XCB_CW_EVENT_MASK, (void*)Masks);
    xcb_ungrab_key(WM.Connection, XCB_GRAB_ANY, WM.Screen->root, XCB_MOD_MASK_ANY); // Reset to known state

    //xcb_grab_key(WM.Connection, 1, WM.Screen->root, XCB_MOD_MASK_1, XKB_KEY_q, XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC );
    //xcb_grab_key(WM.Connection, 1, WM.Screen->root, XCB_MOD_MASK_1, XKB_KEY_space, XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC );
    xcb_flush(WM.Connection);

    std::cout << "LOG: Starting up the WM" << std::endl;
}

void RunEventLoop() {
    std::cout << "LOG: Running the event loop" << std::endl;
    while (true) {
        xcb_generic_event_t* NextEvent = xcb_wait_for_event(WM.Connection);
        std::cout << "Recieved Event: " << NextEvent->response_type << std::endl;

        switch (NextEvent->response_type & ~0x80) {
            case XCB_MAP_REQUEST: { OnMapRequest(NextEvent); break; }
            /*case XCB_CREATE_NOTIFY: { OnCreateNotify(NextEvent); break; }
            case XCB_CONFIGURE_REQUEST: { OnConfigureRequest(NextEvent); break; }
            case XCB_MAP_REQUEST: { OnMapRequest(NextEvent); break; }
            case XCB_UNMAP_NOTIFY: { OnUnmapNotify(NextEvent); break; }
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