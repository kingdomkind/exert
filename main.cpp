#include "config.h"
#include <cassert>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <iostream>
#include <memory>
#include <ostream>
#include <stack>
#include <unordered_map>
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

struct Container { // If Direction is None it will have a Value, otherwise it will have no value
    Split Direction;
    
    std::shared_ptr<Container> Parent = nullptr;
    std::shared_ptr<Container> Left = nullptr;
    std::shared_ptr<Container> Right = nullptr;

    std::shared_ptr<Window> Value = nullptr;
};

struct WM {
    xcb_connection_t* Connection;
    xcb_screen_t* Screen;
    xcb_key_symbols_t* Keysyms;
    std::shared_ptr<Container> FocusedContainer;
    std::shared_ptr<Container> RootContainer;
};

WM WM;

const uint32_t BORDER_WIDTH = 3;
const uint32_t INACTIVE_BORDER_COLOUR = 0xff0000;
const uint32_t ACTIVE_BORDER_COLOUR = 0x0000ff;

void PrintVisibleWindows() {
    std::cout << "Visible Windows: ";
    if (!(WM.RootContainer == nullptr)) {
        std::stack<std::shared_ptr<Container>> Stack;
        Stack.push(WM.RootContainer);

        while (!Stack.empty()) {
            std::shared_ptr<Container> CurrentContainer = Stack.top();

            if (CurrentContainer->Direction == NONE) {
                std::cout << CurrentContainer->Value->Window;
            } else {
                if (CurrentContainer->Right != nullptr) {
                    Stack.push(CurrentContainer->Right);
                } else {
                    std::cerr << "The split direction is not None, but yet there is no right pointer! [EXIT]" << std::endl;
                    exit(EXIT_FAILURE);
                }
                if (CurrentContainer->Left != nullptr) {
                    Stack.push(CurrentContainer->Left);
                } else {
                    std::cerr << "The split direction is not None, but yet there is no left pointer! [EXIT]" << std::endl;
                    exit(EXIT_FAILURE);
                }
            }
        }
    } else {
        std::cerr << "Could not print windows as there is no root container! [EXIT]" << std::endl;
        exit(EXIT_FAILURE);
    }
    std::cout << std::endl;
}

std::shared_ptr<Container> GetContainerFromWindow(xcb_window_t Window) {

    if (!(WM.RootContainer == nullptr)) {
        std::stack<std::shared_ptr<Container>> Stack;
        Stack.push(WM.RootContainer);

        while (!Stack.empty()) {
            std::shared_ptr<Container> CurrentContainer = Stack.top();

            if (CurrentContainer->Direction == NONE) {
                if (CurrentContainer->Value->Window == Window) {
                    return CurrentContainer;
                }
            } else {
                if (CurrentContainer->Right != nullptr) {
                    Stack.push(CurrentContainer->Right);
                } else {
                    std::cerr << "The split direction is not None, but yet there is no right pointer! [EXIT]" << std::endl;
                    exit(EXIT_FAILURE);
                }
                if (CurrentContainer->Left != nullptr) {
                    Stack.push(CurrentContainer->Left);
                } else {
                    std::cerr << "The split direction is not None, but yet there is no left pointer! [EXIT]" << std::endl;
                    exit(EXIT_FAILURE);
                }
            }
        }
    } else {
        std::cerr << "Could not get container from window as there is no root container! [EXIT]" << std::endl;
        exit(EXIT_FAILURE);
    }

    std::cerr << "Could not find the specified container for window: " << Window << " [EXIT] " << std::endl;
    exit(EXIT_FAILURE);
}

void OnEnterNotify(const xcb_generic_event_t* NextEvent) {
    xcb_enter_notify_event_t* Event = (xcb_enter_notify_event_t*) NextEvent;

    if (Event->event != 0) {
        if (WM.FocusedContainer != nullptr) {
            xcb_change_window_attributes(WM.Connection, WM.FocusedContainer->Value->Window, XCB_CW_BORDER_PIXEL, &INACTIVE_BORDER_COLOUR);
        }
        std::cout << "Setting window focus to: " << Event->event << std::endl;
        xcb_set_input_focus(WM.Connection, XCB_INPUT_FOCUS_POINTER_ROOT, Event->event, XCB_CURRENT_TIME);
        WM.FocusedContainer = GetContainerFromWindow(Event->event);
        xcb_change_window_attributes(WM.Connection, WM.FocusedContainer->Value->Window, XCB_CW_BORDER_PIXEL, &ACTIVE_BORDER_COLOUR);
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
        std::cerr << "Failed to get keycode for keysym: " << Keysym << " [EXIT] " << std::endl;
        exit(EXIT_FAILURE);
    }

    xcb_keycode_t PrimaryKeycode = Keycodes[0]; // The first keycode is the main one
    free(Keycodes);
    return PrimaryKeycode;
}

unsigned int KeycodeToKeysym(const unsigned int Keycode) {
    xcb_keysym_t KeySym = xcb_key_symbols_get_keysym(WM.Keysyms, Keycode, 0);
    if (!KeySym) {
        std::cerr << "Failed to get keycode for keycode: " << Keycode << " [EXIT] " << std::endl;
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

void UpdateWindowToCurrentSplits(std::shared_ptr<Container> TargetContainer) {

    std::cout << TargetContainer->Parent << " " << TargetContainer->Left << " " << TargetContainer->Right << " " << TargetContainer->Value->Window << std::endl;

    if (TargetContainer->Value == nullptr) {
        std::cerr << "Target container has no value -- cannot proceed in positioning and sizing it! [EXIT]" << std::endl;
        exit(EXIT_FAILURE);
    }

    std::stack<std::shared_ptr<Container>*> Stack;
    uint32_t X, Y, Width, Height;
    X = 0; Y = 0; Width = 1280; Height = 800;
    std::cout << "LIterally nothing can go wrong here" << std::endl;

    // Copy Target Container Properties that we need
    std::shared_ptr<Container>* CurrentContainer = &TargetContainer;
    std::cout << "Pre-Mid" << std::endl;

    while (true) {
        if (CurrentContainer->get()->Parent == nullptr) {
            std::cout << "We break" << std::endl;
            break;
        }
        std::cout << "Achievement get: how did we get here?" << std::endl;
        CurrentContainer = &CurrentContainer->get()->Parent;
        Stack.push(CurrentContainer);
    }

    std::cout << "Mid" << std::endl;

    while (!Stack.empty()) {
        std::shared_ptr<Container> TopContainer = *Stack.top();
        Stack.pop();

        if (TopContainer->Direction == VERTICAL) {
            Width = Width * 0.5;

            if (TopContainer->Right == *Stack.top()) {
                X += Width;
            }
        } else {
            Height = Height * 0.5;

            if (TopContainer->Right == *Stack.top()) {
                Y += Height;
            } 
        }
    }

    uint32_t Parameters[] = {X, Y, Width, Height, BORDER_WIDTH};
    xcb_configure_window(WM.Connection, TargetContainer->Value->Window, XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y | XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT | XCB_CONFIG_WINDOW_BORDER_WIDTH, Parameters);
    xcb_flush(WM.Connection);
    std::cout << "Updated Window " << TargetContainer->Value->Window << " to current splits, PosX: " << X << ", PosY: " << Y << ", Width: " << Width << ", Height: " << Height << std::endl;
}


Coordinate GetCursorPosition() {
    xcb_query_pointer_reply_t* Position = xcb_query_pointer_reply(WM.Connection, xcb_query_pointer(WM.Connection, WM.Screen->root), nullptr);
    if (Position) {
        Coordinate CursorPosition = {};
        CursorPosition.X = Position->root_x;
        CursorPosition.Y = Position->root_y;
        free(Position);
        return CursorPosition;
    } else {
        std::cerr << "Failed to get the cursor position! [EXIT]" << std::endl;
        exit(EXIT_FAILURE);
    }
}

WindowSegment GetWindowSegmentCursorIsIn(xcb_window_t Window) {
    xcb_get_geometry_reply_t* WindowGeometry = xcb_get_geometry_reply(WM.Connection, xcb_get_geometry(WM.Connection, Window), NULL);
    Coordinate CursorPosition = GetCursorPosition();
    Coordinate AccountOffset = {};
    AccountOffset.X = CursorPosition.X - WindowGeometry->x;
    AccountOffset.Y = CursorPosition.Y - WindowGeometry->y;

    float RatioX = AccountOffset.X / WindowGeometry->width;
    float RatioY = AccountOffset.Y / WindowGeometry->height;

    std::cout << "Offset Y: " << AccountOffset.Y << ", Length: " << WindowGeometry->height << std::endl;
    std::cout << "RatioX Segment Cursor: " << RatioX << ", RatioY Segment Cursor: " << RatioY << std::endl; 

    if (RatioY < 0.25) {
        return UP;
    } else if (RatioY > 0.75) {
        return DOWN;
    }

    if (RatioX < 0.5) { // TODO ADD RATIO Y SUPPORT
        return LEFT;
    } else {
        return RIGHT;
    }
}

void OnMapRequest(const xcb_generic_event_t* NextEvent) {
    xcb_map_request_event_t* Event = (xcb_map_request_event_t*)NextEvent;

    std::shared_ptr<Window> NewWindow = std::make_shared<Window>();
    NewWindow->Window = Event->window;

    std::shared_ptr<Container> NewContainer = std::make_shared<Container>();
    NewContainer->Direction = NONE;
    NewContainer->Parent = nullptr;
    NewContainer->Value = NewWindow;

    if (!(WM.RootContainer == nullptr)) { // Need to create a split, this isn't the first window opened
        if (!(WM.FocusedContainer == nullptr)) { // Create window size & splits based on the focused window
            xcb_get_geometry_reply_t* FocusedWindowGeometry = xcb_get_geometry_reply(WM.Connection, xcb_get_geometry(WM.Connection, WM.FocusedContainer->Value->Window), NULL);

            if (FocusedWindowGeometry) {
                std::shared_ptr<Window> FocusedWindow = WM.FocusedContainer->Value;
                WindowSegment Section = GetWindowSegmentCursorIsIn(WM.FocusedContainer->Value->Window);

                std::shared_ptr<Container> NewFocusedContainer = std::make_shared<Container>();
                NewFocusedContainer->Direction = NONE;
                NewFocusedContainer->Parent = nullptr;
                NewFocusedContainer->Value = FocusedWindow;
                NewFocusedContainer->Parent = WM.FocusedContainer;

                WM.FocusedContainer->Value = nullptr;

                if (Section == UP || Section == DOWN) {
                    WM.FocusedContainer->Direction = HORIZONTAL;
                } else {
                    WM.FocusedContainer->Direction = VERTICAL;
                }

                switch (Section) {
                    case RIGHT: {
                        WM.FocusedContainer->Right = NewContainer;
                        break;
                    }
                    case LEFT: {
                        WM.FocusedContainer->Left = NewContainer;
                        break;
                    }
                    case DOWN: {
                        WM.FocusedContainer->Right = NewContainer;
                        break;
                    }
                    case UP: {
                        WM.FocusedContainer->Left = NewContainer;
                        break;
                    }
                }
                WM.FocusedContainer = NewFocusedContainer;
                UpdateWindowToCurrentSplits(WM.FocusedContainer);

            } else {
                std::cerr << "Focused window is " << WM.FocusedContainer->Value->Window << "but was unable to get the window geometry!" << " [EXIT] " <<  std::endl;
                exit(EXIT_FAILURE);
            }
        } else {
            std::cerr << "Unable to create window as the focused window is nullptr, yet there are windows opened!" << " [EXIT] " << std::endl;
            exit(EXIT_FAILURE);
        }
    } else { // First window opened
        std::cout << "No root, making new root" << std::endl;
        WM.RootContainer = NewContainer;
    }

    uint32_t EventMasks[] = {XCB_EVENT_MASK_ENTER_WINDOW | XCB_EVENT_MASK_FOCUS_CHANGE};
    xcb_change_window_attributes(WM.Connection, Event->window, XCB_CW_EVENT_MASK, &EventMasks);
    xcb_change_window_attributes(WM.Connection, Event->window, XCB_CW_BORDER_PIXEL, &INACTIVE_BORDER_COLOUR);
    UpdateWindowToCurrentSplits(NewContainer);

    std::cout << "ADDED! " << Event->window << std::endl;  // FLAG
    //PrintVisibleWindows();

    xcb_map_window(WM.Connection, Event->window);
    xcb_flush(WM.Connection);
}

void RemoveWindowStructFromWM(xcb_window_t Window) {
    /* TODO
    bool Found = false;
    for (auto WindowStruct: WM.VisibleWindows) { // This inherently checks that the Window is a Window we manage
        if (WindowStruct->Window == Window) {
            Found = true;
            WM.VisibleWindows.erase(WindowStruct);
            std::cout << "ERASED! " << Window << std::endl;
            PrintVisibleWindows();
            if (WM.FocusedWindow->Window == Window) {
                WM.FocusedWindow = nullptr;
                std::cout << "Focused Window was deleted, setting to nullptr" << std::endl;
            }
            break;
        }
    } */

    // TODO! Redo removal checking, previous implementation was inherently flawed and could not work
    /*
        The previous idea was that if a splitline was only referenced once, then it was no longer needed as there needed to be atleast two windows referencing it for them to be
        side by side.
        This idea worked well in 1 dimension, but moving into 2d with the X and Y that a splitline could be referenced by multiple windows but they could be stacked ontop of eachother.
        This means that imagining we had a 3x3 grid of windows, if we remove the middle column, the windows on the left and right hand side would not expand towards the middle as their
        splitlines still have more than 1 reference, as there would be a total of 3 references for each of the split lines.
    */
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
    {"KillActive", []() { if (!(WM.FocusedContainer == nullptr)) { KillWindow(WM.FocusedContainer->Value->Window); } else { std::cerr << "Attempted to kill focused container - which is nullptr!" << " [EXIT] " << std::endl;; exit(EXIT_FAILURE); } }},
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