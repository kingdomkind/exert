#include <cassert>
#include <cstdlib>
#include <iostream>
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <unordered_map>

struct WM {
    Display* RootDisplay;
    Window RootWindow;
    std::unordered_map<Window, Window> Clients;
};

WM WM;

void FrameWindow(Window EventWindow) {
    const uint BORDER_WIDTH = 12;
    const ulong BORDER_COLOUR = 0xff0000;
    const ulong BACKGROUND_COLOUR = 0xff0000;

    XWindowAttributes WindowAttributes; // Get the attributes of the window
    assert(XGetWindowAttributes(WM.RootDisplay, EventWindow, &WindowAttributes) != 0);
    
    // Create the frame
    const Window Frame = XCreateSimpleWindow(
        WM.RootDisplay, WM.RootWindow,
        WindowAttributes.x, WindowAttributes.y, WindowAttributes.width, WindowAttributes.height,
        BORDER_WIDTH, BORDER_COLOUR, BACKGROUND_COLOUR);

    XSelectInput(WM.RootDisplay, Frame, SubstructureRedirectMask | SubstructureNotifyMask); // Choose registered events for the frame
    XAddToSaveSet(WM.RootDisplay, EventWindow); // Add client to the save set, if X crashes, it can be restored
    XReparentWindow(WM.RootDisplay, EventWindow, Frame, 0, 0); // Make EventWindow a child of Frame, Last 2 ints are offset of the window in the frame

    XMapWindow(WM.RootDisplay, Frame); // Map Frame
    WM.Clients[EventWindow] = Frame; // Save the Frame handle

    std::cout << "LOG: Framed the window: " << EventWindow << ", in the frame: " << Frame << std::endl;

}

void OnCreateNotify(const XCreateWindowEvent& NextEvent) {}

void OnMapRequest(const XMapRequestEvent& NextEvent) {
    FrameWindow(NextEvent.window);
    XMapWindow(WM.RootDisplay, NextEvent.window);
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

void StartupWM() {
    XSetErrorHandler(&OnOtherWMDetected);
    XSelectInput(WM.RootDisplay, WM.RootWindow, SubstructureRedirectMask | SubstructureNotifyMask);

    // Grab Mod1 + Q for exiting the window manager
    XGrabKey(WM.RootDisplay, XKeysymToKeycode(WM.RootDisplay, XK_q), Mod1Mask, WM.RootWindow, false, GrabModeAsync, GrabModeAsync);

    XSync(WM.RootDisplay, false);
    XSetErrorHandler(&OnXError);
}

void RunEventLoop() {
    while (true) {
        XEvent NextEvent; XNextEvent(WM.RootDisplay, &NextEvent);
        std::cout << "Recieved Event: " << NextEvent.type << std::endl;

        switch (NextEvent.type) {
            case CreateNotify: { OnCreateNotify(NextEvent.xcreatewindow); break; }
            case ConfigureRequest: { OnConfigureRequest(NextEvent.xconfigurerequest); break; }
            case MapRequest: { OnMapRequest(NextEvent.xmaprequest); break; }
            //case UnmapNotify: { OnUnmapNotify(NextEvent.xunmap); break; }
            case KeyPress: {
                std::cout << "It's a keypress!" << std::endl;
                KeySym key = XKeycodeToKeysym(WM.RootDisplay, NextEvent.xkey.keycode, 0);
                if (key == XK_q && (NextEvent.xkey.state & Mod1Mask)) {
                    std::cout << "Exit key combination pressed. Exiting." << std::endl;
                    return; // Exit the event loop
                }
                break;
            }
            default: {std::cerr << "Ignored Event: " << NextEvent.type << std::endl; break; }
        }
    }
}

void InitDisplay(Display*& Display) {
    Display = XOpenDisplay(nullptr);
    if (Display == nullptr) {
        std::cerr << "Failed to open the X display!" << std::endl;
        exit(EXIT_FAILURE);
    }
}

int main() {
    InitDisplay(WM.RootDisplay);
    WM.RootWindow = DefaultRootWindow(WM.RootDisplay);

    StartupWM();
    RunEventLoop();

    return EXIT_SUCCESS;
}