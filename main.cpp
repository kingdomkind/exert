#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <ostream>
#include <stack>
#include <algorithm>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <xcb/xcb.h>
#include <xcb/xcb_keysyms.h>
#include <unistd.h>
#include <xcb/xproto.h>
#include <X11/keysym.h>
#include <xcb/xcb_icccm.h>
#include <xcb/randr.h>

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
    xcb_atom_t NetWmState;
    xcb_atom_t NetWmStateFullscreen;
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
    std::unordered_set<std::string> StartupCommands; // Commands to run at boot
};

const uint32_t BORDER_WIDTH = 0;
const uint32_t INACTIVE_BORDER_COLOUR = 0xff0000;
const uint32_t ACTIVE_BORDER_COLOUR = 0x0000ff;
const uint32_t OFFSCREEN_WINDOW_POSITION[] = {10000, 10000};

const std::string PIPE_PATH = "/tmp/wmking-runtime";

static std::thread ListeningThread;
static Runtime Runtime;
static WM WM;

bool DoesWindowSupportProtocol(xcb_window_t Window, xcb_atom_t Atom) {
  // Get the supported protocols
    xcb_icccm_get_wm_protocols_reply_t Protocols;
    xcb_get_property_cookie_t Cookie = xcb_icccm_get_wm_protocols(WM.Connection, Window, WM.ProtocolsContainer.Protocols);
    if (xcb_icccm_get_wm_protocols_reply(WM.Connection, Cookie, &Protocols, NULL) != 1) {
        return false;
    }

     for (unsigned int i = 0; i < Protocols.atoms_len; i++) {
            if (Protocols.atoms[i] == Atom) {
                xcb_icccm_get_wm_protocols_reply_wipe(&Protocols);
                return true;
            }
     }

    xcb_icccm_get_wm_protocols_reply_wipe(&Protocols);
    return false;
}

void PrintVisibleWindows() {
    std::cout << "Starting Printing Visible Windows" << std::endl;
    std::cout << "Visible Windows: \n" << std::endl;
    for (int i = 0; i < static_cast<int>(WM.Workspaces.size()); i++) {
        std::shared_ptr<Workspace> Workspace = WM.Workspaces[i];
        if (!(Workspace->RootContainer == nullptr)) {
            std::stack<std::shared_ptr<Container>> Stack;
            Stack.push(Workspace->RootContainer);

            while (!Stack.empty()) {
                std::shared_ptr<Container> CurrentContainer = Stack.top();
                Stack.pop();

                std::cout << "Container: " << CurrentContainer << std::endl;
                std::cout << "Workspace " << i << std::endl;
                if (CurrentContainer->Value != nullptr) {
                    std::cout << "Window: " << CurrentContainer->Value->Window << std::endl;
                } else {
                    std::cout << "Window: " << "No Associated Window" << std::endl;
                }
                std::cout << "Direction: " << CurrentContainer->Direction << std::endl;
                std::cout << "Parent: " << CurrentContainer->Parent << std::endl;
                std::cout << "Left Pointer: " << CurrentContainer->Left << std::endl;
                std::cout << "Right Pointer: " << CurrentContainer->Left << std::endl; 
                std::cout << "\n" << std::endl;

                if (CurrentContainer->Right != nullptr) {
                    Stack.push(CurrentContainer->Right);
                }
                if (CurrentContainer->Left != nullptr) {
                    Stack.push(CurrentContainer->Left);
                }
            }
        } else {
            std::cerr << "Could not print windows as there is no root container!" << std::endl;
        }
    }
    std::cout << std::endl;
}

std::shared_ptr<WindowMetadata> GetWorkspaceAndContainerFromWindow(xcb_window_t Window) {
    std::shared_ptr<WindowMetadata> Metadata = std::make_shared<WindowMetadata>();
    for (int i = 0; i < static_cast<int>(WM.Workspaces.size()); i++) {
        std::shared_ptr<Workspace> Workspace = WM.Workspaces[i];
        if (!(Workspace->RootContainer == nullptr)) {
            std::stack<std::shared_ptr<Container>> Stack;
            Stack.push(Workspace->RootContainer);

            while (!Stack.empty()) {
                std::shared_ptr<Container> CurrentContainer = Stack.top();
                Stack.pop();

                if (CurrentContainer->Direction == NONE) {
                    if (CurrentContainer->Value->Window == Window) {
                        Metadata->Container = CurrentContainer;
                        Metadata->Workspace = i;
                        return Metadata;
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
        }
    }
    std::cerr << "Could not find the specified container for window: " << Window << ", note that this may be because we do not manage this client" << std::endl;
    return nullptr;
}

void OnEnterNotify(const xcb_generic_event_t* NextEvent) {
    xcb_enter_notify_event_t* Event = (xcb_enter_notify_event_t*) NextEvent;

    if (Event->event != 0) {
        if (WM.FocusedContainer != nullptr) {
            xcb_change_window_attributes(WM.Connection, WM.FocusedContainer->Value->Window, XCB_CW_BORDER_PIXEL, &INACTIVE_BORDER_COLOUR);
        }
        std::cout << "Setting window focus to: " << Event->event << std::endl;
        xcb_set_input_focus(WM.Connection, XCB_INPUT_FOCUS_POINTER_ROOT, Event->event, XCB_CURRENT_TIME);
        WM.FocusedContainer = GetWorkspaceAndContainerFromWindow(Event->event)->Container;
        xcb_change_window_attributes(WM.Connection, WM.FocusedContainer->Value->Window, XCB_CW_BORDER_PIXEL, &ACTIVE_BORDER_COLOUR);
        xcb_flush(WM.Connection);
    } else {
        std::cout << "Did not set focus to 0 (is it root?)" << std::endl;
    }
    std::cout << "Finished setting focus" << std::endl;
}

void KillWindow(xcb_window_t Window) {
    if (DoesWindowSupportProtocol(Window, WM.ProtocolsContainer.DeleteWindow)) {
        std::cout << "Soft killing window: " << Window << std::endl;
        xcb_client_message_event_t Event;
        std::memset(&Event, 0, sizeof(Event));
        Event.response_type = XCB_CLIENT_MESSAGE;
        Event.window = Window;
        Event.type = WM.ProtocolsContainer.Protocols;
        Event.format = 32;
        Event.data.data32[0] = WM.ProtocolsContainer.DeleteWindow;
        Event.data.data32[1] = XCB_CURRENT_TIME;

        xcb_send_event(WM.Connection, false, Window, XCB_EVENT_MASK_NO_EVENT, (const char*)&Event);
        xcb_flush(WM.Connection);
    } else {
        std::cout << "Hard killing window: " << Window << std::endl;
        xcb_kill_client(WM.Connection, Window);
        xcb_flush(WM.Connection);
    }
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

std::shared_ptr<Monitor> GetMonitorFromWorkspace(int Workspace) {
    for (auto Monitor: WM.Monitors) {
        if (Monitor->ActiveWorkspace == Workspace) {
            return Monitor;
        }
    }
    return nullptr;
}

void UpdateWindowToCurrentSplits(std::shared_ptr<Container> TargetContainer) {

    std::cout << "Updating to current splits: " << TargetContainer->Parent << " " << TargetContainer->Left << " " << TargetContainer->Right << " " << TargetContainer->Value->Window << std::endl;
    std::shared_ptr<Monitor> Monitor = GetMonitorFromWorkspace(GetWorkspaceAndContainerFromWindow(TargetContainer->Value->Window)->Workspace);

    if (TargetContainer->Value == nullptr) {
        std::cerr << "Target container has no value -- cannot proceed in positioning and sizing it! [EXIT]" << std::endl;
        exit(EXIT_FAILURE);
    }

    std::stack<std::shared_ptr<Container>> Stack;
    uint32_t X, Y, Width, Height;
    X = Monitor->X; Y = Monitor->Y; Width = Monitor->Width; Height = Monitor->Height;

    std::shared_ptr<Container>* CurrentContainer = &TargetContainer;
    while (true) {
        Stack.push(*CurrentContainer);
        std::cout << "Pushed " << CurrentContainer->get() << " onto the stack" << std::endl;
        if (CurrentContainer->get()->Parent == nullptr) { break; }
        CurrentContainer = &CurrentContainer->get()->Parent;
    }

    while (Stack.size() > 1) { // Don't iterate over the base container
        std::shared_ptr<Container> TopContainer = Stack.top();
        Stack.pop();

        if (TopContainer->Direction == VERTICAL) {
            Width = Width * 0.5;

            if (TopContainer->Right == Stack.top()) {
                X += Width;
            }
        } else {
            Height = Height * 0.5;

            if (TopContainer->Right == Stack.top()) {
                Y += Height;
            } 
        }
        std::cout << "Iter " << TargetContainer->Value->Window << " to current splits, PosX: " << X << ", PosY: " << Y << ", Width: " << Width << ", Height: " << Height << std::endl;
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


std::shared_ptr<Monitor> GetActiveMonitor() {
    Coordinate CursorPosition = GetCursorPosition();
    for (std::shared_ptr<Monitor> Monitor: WM.Monitors) {
        int UpperBoundX = Monitor->Width + Monitor->X;
        int UpperBoundY = Monitor->Height + Monitor->Y;
        if ((Monitor->X <= CursorPosition.X) && (CursorPosition.X <= UpperBoundX) && (Monitor->Y <= CursorPosition.Y) && (CursorPosition.Y <= UpperBoundY)) {
            std::cout << "Returning Active Monitor is: " << Monitor->Name << std::endl;
            return Monitor;
        }
    }

    std::cout << "No Active Monitor was found somehow! [EXIT]" << std::endl;
    exit(EXIT_FAILURE);
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

    if (RatioX < 0.5) {
        return LEFT;
    } else {
        return RIGHT;
    }
}

unsigned int GetActiveWorkspaceChecked(std::shared_ptr<Monitor> MonitorToCheck) {
    int ActiveWorkspace = MonitorToCheck->ActiveWorkspace;
    if (ActiveWorkspace != -1) {
        std::cout << "Active workspace of Monitor: " << MonitorToCheck->Name << " is " << ActiveWorkspace << std::endl;
        return ActiveWorkspace;
    } else {
        std::cerr << "Active workspace of Monitor: " << MonitorToCheck->Name << " is -1, which is invalid! [EXIT]" << std::endl;
        exit(EXIT_FAILURE);
    }
}

void OnMapRequest(const xcb_generic_event_t* NextEvent) {
    std::cout << "Map request recieved" << std::endl;
    xcb_map_request_event_t* Event = (xcb_map_request_event_t*)NextEvent;

    std::shared_ptr<Window> NewWindow = std::make_shared<Window>();
    NewWindow->Window = Event->window;

    std::shared_ptr<Container> NewContainer = std::make_shared<Container>();
    NewContainer->Direction = NONE;
    NewContainer->Parent = nullptr;
    NewContainer->Value = NewWindow;

    std::shared_ptr<Workspace> ActiveWorkspace = WM.Workspaces[GetActiveWorkspaceChecked(GetActiveMonitor())];

    if (!(ActiveWorkspace->RootContainer == nullptr)) { // Need to create a split, this isn't the first window opened
        if (!(WM.FocusedContainer == nullptr)) { // Create window size & splits based on the focused window
            xcb_get_geometry_reply_t* FocusedWindowGeometry = xcb_get_geometry_reply(WM.Connection, xcb_get_geometry(WM.Connection, WM.FocusedContainer->Value->Window), NULL);

            if (FocusedWindowGeometry) {
                WindowSegment Section = GetWindowSegmentCursorIsIn(WM.FocusedContainer->Value->Window);

                std::shared_ptr<Container> NewFocusedContainer = std::make_shared<Container>();
                NewFocusedContainer->Direction = NONE;
                NewFocusedContainer->Value = WM.FocusedContainer->Value;
                NewFocusedContainer->Parent = WM.FocusedContainer;
                NewContainer->Parent = WM.FocusedContainer;

                WM.FocusedContainer->Value = nullptr;

                if (Section == UP || Section == DOWN) {
                    WM.FocusedContainer->Direction = HORIZONTAL;
                } else {
                    WM.FocusedContainer->Direction = VERTICAL;
                }

                if (Section == RIGHT || Section == DOWN) {
                    WM.FocusedContainer->Right = NewContainer;
                    WM.FocusedContainer->Left = NewFocusedContainer;
                } else {
                    WM.FocusedContainer->Left = NewContainer;
                    WM.FocusedContainer->Right = NewFocusedContainer;
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
        ActiveWorkspace->RootContainer = NewContainer;
    }

    uint32_t EventMasks[] = {XCB_EVENT_MASK_ENTER_WINDOW | XCB_EVENT_MASK_FOCUS_CHANGE};
    xcb_change_window_attributes(WM.Connection, Event->window, XCB_CW_EVENT_MASK, &EventMasks);
    xcb_change_window_attributes(WM.Connection, Event->window, XCB_CW_BORDER_PIXEL, &INACTIVE_BORDER_COLOUR);
    UpdateWindowToCurrentSplits(NewContainer);

    std::cout << "ADDED! " << Event->window << std::endl;
    PrintVisibleWindows();

    xcb_map_window(WM.Connection, Event->window);
    xcb_flush(WM.Connection);
}

void RemoveContainerFromWM(std::shared_ptr<Container> ToBeRemoved, int Workspace) {

    std::cout << "Removing container from WM" << std::endl;
    if (!(ToBeRemoved->Parent == nullptr)) {
        std::shared_ptr<Container> PromotionContainer; // We choose the other window to be promoted
        if (ToBeRemoved->Parent->Left == ToBeRemoved) {
            PromotionContainer = ToBeRemoved->Parent->Right;
        } else {
            PromotionContainer = ToBeRemoved->Parent->Left;
        }

        if (ToBeRemoved->Parent->Parent != nullptr) { // Swapping the parent container to be the promotion container
            if (ToBeRemoved->Parent->Parent->Left == ToBeRemoved->Parent ) {
                ToBeRemoved->Parent->Parent->Left = PromotionContainer;
            } else {
                ToBeRemoved->Parent->Parent->Right = PromotionContainer;
            }
            PromotionContainer->Parent = ToBeRemoved->Parent->Parent;
        } else { // Do the same thing, but no need to modify the parent's parent, as the parent of promotion container is already the root container
            ToBeRemoved->Parent = PromotionContainer;
            WM.Workspaces[Workspace]->RootContainer = PromotionContainer;
            PromotionContainer->Parent = nullptr;
        }

        std::cout << "After reconfigurement" << std::endl;
        PrintVisibleWindows();

	// Update the splits for all affected windows
        std::stack<std::shared_ptr<Container>> Stack;
        Stack.push(PromotionContainer);
        while (!Stack.empty()) {
            std::shared_ptr<Container> CurrentContainer = Stack.top();
	    std::cout << "Iterating removal stack over: " << CurrentContainer << std::endl;
            Stack.pop();

	    if (CurrentContainer == ToBeRemoved) {
		std::cerr << "Impossible - This container should no longer exist! [EXIT]" << std::endl;
		exit(EXIT_FAILURE);
	    }
            if (CurrentContainer->Direction == NONE) {
                UpdateWindowToCurrentSplits(CurrentContainer);
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
        WM.Workspaces[Workspace]->RootContainer = nullptr;
        std::cout << "Root container was deleted, setting to nullptr" << std::endl;    
    }

    if (WM.FocusedContainer == ToBeRemoved) {
        WM.FocusedContainer = nullptr;
        std::cout << "Focused Container was deleted, setting to nullptr" << std::endl;    
    }
}

void OnUnMapNotify(const xcb_generic_event_t* NextEvent) {
    xcb_map_request_event_t* Event = (xcb_map_request_event_t*)NextEvent;
    auto Result = GetWorkspaceAndContainerFromWindow(Event->window);
    if (Result != nullptr) {
        RemoveContainerFromWM(Result->Container, Result->Workspace);
    } // We don't error, as it can fail as unmap can be called on clients we haven't set up
}

void OnDestroyNotify(const xcb_generic_event_t* NextEvent) {
    xcb_map_request_event_t* Event = (xcb_map_request_event_t*)NextEvent;
    auto Result = GetWorkspaceAndContainerFromWindow(Event->window);
    if (Result != nullptr) {
        RemoveContainerFromWM(Result->Container, Result->Workspace);
    }
}

void EnsureValidWorkspacesBetweenIndicesInclusive(int LowerBound, int UpperBound) {
    for (int i = LowerBound; i <= UpperBound; i++) {
        if (static_cast<int>(WM.Workspaces.size()-1) < i) { // Current Index doesn't exist -- create a new one
            std::shared_ptr<Workspace> NewWorkspace = std::make_shared<Workspace>();
            WM.Workspaces.push_back(NewWorkspace);
            std::cout << "Created Workspace at Index " << i << ", ensuring a valid range" << std::endl;
        }
    }
}

void SetWorkspaceToMonitor(unsigned int TargetWorkspace, std::shared_ptr<Monitor> TargetMonitor) {
    unsigned int PreviousWorkspace = GetActiveWorkspaceChecked(TargetMonitor);
    std::shared_ptr<Monitor> PreviousMonitor = GetMonitorFromWorkspace(TargetWorkspace);

    if (PreviousMonitor != nullptr) {
        TargetMonitor->ActiveWorkspace = -1;
        PreviousMonitor->ActiveWorkspace = PreviousWorkspace;
        std::cout << "Swapping workspaces, set Previous monitor from workspace " << TargetMonitor << " to workspace " << PreviousWorkspace << std::endl;
    }

    if (!(WM.Workspaces[PreviousWorkspace]->RootContainer == nullptr)) {
        std::stack<std::shared_ptr<Container>> Stack;
        Stack.push(WM.Workspaces[PreviousWorkspace]->RootContainer);

        while (!Stack.empty()) {
            std::shared_ptr<Container> CurrentContainer = Stack.top();
            Stack.pop();

            if (CurrentContainer->Direction == NONE) {
                if (PreviousMonitor == nullptr) { // We are not stealing the workspace from another monitor, so set window positions elsewhere
                    xcb_configure_window(WM.Connection, CurrentContainer->Value->Window, XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y, &OFFSCREEN_WINDOW_POSITION);
                    xcb_flush(WM.Connection);
                } else { // We are stealing the workspace from another monitor, so update splits since the other monitor has the old workspace now
                    UpdateWindowToCurrentSplits(CurrentContainer);
                }
            } else {
                if (CurrentContainer->Right != nullptr) { Stack.push(CurrentContainer->Right); }
                if (CurrentContainer->Left != nullptr) { Stack.push(CurrentContainer->Left); }
            }
        }
    }
    std::cout << "Moved previous workspace " << PreviousWorkspace << std::endl;

    // Now we can set the target workspace to the target monitor
    EnsureValidWorkspacesBetweenIndicesInclusive(WM.Workspaces.size(), TargetWorkspace); // Incase we swap to a workspace that doesn't yet exist    
    TargetMonitor->ActiveWorkspace = TargetWorkspace;
    if (!(WM.Workspaces[TargetWorkspace]->RootContainer == nullptr)) {
        std::stack<std::shared_ptr<Container>> Stack;
        Stack.push(WM.Workspaces[TargetWorkspace]->RootContainer);

        while (!Stack.empty()) {
            std::shared_ptr<Container> CurrentContainer = Stack.top();
            Stack.pop();

            if (CurrentContainer->Direction == NONE) {
                UpdateWindowToCurrentSplits(CurrentContainer);
            } else {
                if (CurrentContainer->Right != nullptr) { Stack.push(CurrentContainer->Right); }
                if (CurrentContainer->Left != nullptr) { Stack.push(CurrentContainer->Left); }
            }
        }
    }
    std::cout << "Set Monitor: " << TargetMonitor << ", to workspace: " << TargetMonitor->ActiveWorkspace << " (should be the same as " << TargetWorkspace << ")" << std::endl;
}


std::unordered_map<std::string, std::function<void(const std::string &Arguments)>> InternalCommand = {
    {"KillActive", [](const std::string &Arguments) { if (!(WM.FocusedContainer == nullptr)) { KillWindow(WM.FocusedContainer->Value->Window); } else { std::cerr << "Focused window does not exist, cannot kill it" << std::endl;}}},
    {"ExitWM", [](const std::string &Arguments) { ExitWM(); }},
    {"SetFocusedMonitorToWorkspace", [](const std::string &Arguments){SetWorkspaceToMonitor(std::stoi(Arguments), GetActiveMonitor()); }},
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
                    size_t SpacePosition = SubCommand.find(' ');
                    std::string CommandName = SubCommand.substr(0, SpacePosition);
                    std::string Arguments = (SpacePosition != std::string::npos) ? SubCommand.substr(SpacePosition + 1) : "";
                    auto Found = InternalCommand.find(CommandName);
                    if (Found != InternalCommand.end()) {
                        std::cout << "Executing Internal Command: " << CommandName << std::endl; 
                        Found->second(Arguments);
                    } else {
                        std::cerr << "No matching function to call for: " << CommandName << std::endl;
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

void OnClientMessage(const xcb_generic_event_t* NextEvent) {
    xcb_client_message_event_t* Event = (xcb_client_message_event_t*)NextEvent;
    std::cout << "Entered Client Message" << std::endl;
    if (Event->type == WM.ProtocolsContainer.NetWmState) {
        if (Event->data.data32[1] == WM.ProtocolsContainer.NetWmStateFullscreen || Event->data.data32[2] == WM.ProtocolsContainer.NetWmStateFullscreen) {
            std::cout << "Ignoring fullscreen request for window: " << Event->window << std::endl;
            return;
        }
    } else {
        std::cout << "Failed" << std::endl;
    }
}

void RunEventLoop() {
    std::cout << "Running the event loop" << std::endl;

    while (true) {
        xcb_generic_event_t* NextEvent = xcb_wait_for_event(WM.Connection);
        switch (NextEvent->response_type & ~0x80) {
            case XCB_MAP_REQUEST: { OnMapRequest(NextEvent); break; }
            case XCB_KEY_PRESS: { OnKeyPress(NextEvent); break; }
            case XCB_UNMAP_NOTIFY: { OnUnMapNotify(NextEvent); break; }
            case XCB_DESTROY_NOTIFY: { OnDestroyNotify(NextEvent); break; }
            case XCB_ENTER_NOTIFY: { OnEnterNotify(NextEvent); break; }
            case XCB_CLIENT_MESSAGE: {  }
            default: { break; }
        }
    }
}

void AssignFreeWorkspaceToMonitor(std::shared_ptr<Monitor> Monitor) {
    std::vector<int> ClaimedWorkspaces;
    for (auto &MonitorLoop: WM.Monitors) {
        if (MonitorLoop->ActiveWorkspace != -1) {
            ClaimedWorkspaces.push_back(MonitorLoop->ActiveWorkspace);
	    std::cout << "Added " << MonitorLoop->ActiveWorkspace << " to claimed workspaces!" << std::endl;
        }
    }

    for (int i = 0; i < static_cast<int>(WM.Workspaces.size()); i++) {
        auto Found = std::find(ClaimedWorkspaces.begin(), ClaimedWorkspaces.end(), i);
        if (Found == ClaimedWorkspaces.end()) { // Allocates any spare workspaces
            Monitor->ActiveWorkspace = i;
            std::cout << "Assigned Monitor: " << Monitor->Name << ", Pre-existing Workspace: " << i << " (Should be same as " << Monitor->ActiveWorkspace << ")" << std::endl;
            return;
        }
    }

    // No spare workspaces, create new one and allocate that
    std::shared_ptr<Workspace> NewWorkspace = std::make_shared<Workspace>();
    WM.Workspaces.push_back(NewWorkspace);
    Monitor->ActiveWorkspace = WM.Workspaces.size() - 1;

    std::cout << "Assigned Monitor: " << Monitor->Name << ", NEW Workspace: " << WM.Workspaces.size() - 1
    << " (Should be same as " << Monitor->ActiveWorkspace << ")" << std::endl;
}

void InitialiseMonitors() {
    xcb_randr_get_screen_resources_current_cookie_t ResourcesCookie = xcb_randr_get_screen_resources_current(WM.Connection, WM.Screen->root);
    xcb_randr_get_screen_resources_current_reply_t* ResourcesReply = xcb_randr_get_screen_resources_current_reply(WM.Connection, ResourcesCookie, nullptr);

    if (!ResourcesReply) {
        std::cerr << "Failed to get screen resources! [EXIT]" << std::endl;
        exit(EXIT_FAILURE);
    }

    int NumberOfOutputs = xcb_randr_get_screen_resources_current_outputs_length(ResourcesReply);
    xcb_randr_output_t* Outputs = xcb_randr_get_screen_resources_current_outputs(ResourcesReply);

    for (int i = 0; i < NumberOfOutputs; i++) {
        xcb_randr_output_t Output = Outputs[i];
        
        xcb_randr_get_output_info_cookie_t InformationCookie = xcb_randr_get_output_info(WM.Connection, Output, XCB_CURRENT_TIME);
        xcb_randr_get_output_info_reply_t* InformationReply = xcb_randr_get_output_info_reply(WM.Connection, InformationCookie, nullptr);

        if (!InformationReply) {
            std::cerr << "Failed to get info for Output: " << Output << " [EXIT] " << std::endl;
            exit(EXIT_FAILURE);
        }

        if (InformationReply->crtc != XCB_NONE) {
            xcb_randr_get_crtc_info_cookie_t CRTCCookie = xcb_randr_get_crtc_info(WM.Connection, InformationReply->crtc, XCB_CURRENT_TIME);
            xcb_randr_get_crtc_info_reply_t* CRTCReply = xcb_randr_get_crtc_info_reply(WM.Connection, CRTCCookie, nullptr);

            if (CRTCReply) {
                std::shared_ptr<Monitor> NewMonitor = std::make_shared<Monitor>();
                NewMonitor->Output = Output;
                NewMonitor->Name = std::string((char*)xcb_randr_get_output_info_name(InformationReply), xcb_randr_get_output_info_name_length(InformationReply));
                NewMonitor->X = CRTCReply->x;
                NewMonitor->Y = CRTCReply->y;
                NewMonitor->Width = CRTCReply->width;
                NewMonitor->Height = CRTCReply->height;

                WM.Monitors.push_back(NewMonitor);

                std::cout << "Name: " << NewMonitor->Name << ", Output: " << NewMonitor->Output << ", X: " << NewMonitor->X << ", Y: " 
                << NewMonitor->Y << ", Width: " << NewMonitor->Width << ", Height: " << NewMonitor->Height << std::endl;

                free(CRTCReply);
                AssignFreeWorkspaceToMonitor(NewMonitor);
            }
        } else {
            std::cerr << "Output: " << Output << " has no crtc, skipping!" << std::endl;
        }

        free(InformationReply);
    }

    free(ResourcesReply);
}

void GetAtom(std::string AtomName, xcb_atom_t* ToAssignTo) {
    xcb_intern_atom_reply_t* Atom = xcb_intern_atom_reply(WM.Connection, xcb_intern_atom(WM.Connection, 0, strlen(AtomName.c_str()), AtomName.c_str()), nullptr);
    ToAssignTo = &Atom->atom;
    free(Atom); 
}

int main() {
    /* EXPORTS */
    Runtime.Exports.insert({"XCURSOR_SIZE", "24"});
    Runtime.Exports.insert({"GTK_THEME", "Adwaita:dark"});

    for (auto Pair: Runtime.Exports) {
        setenv(Pair.first.c_str(), Pair.second.c_str(), 1);
    }

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

    InitialiseMonitors();

    Runtime.StartupCommands.insert("dunst");
    Runtime.StartupCommands.insert("flameshot");
    Runtime.StartupCommands.insert("picom");

    /* STARTUP COMMANDS */
    for (auto Command: Runtime.StartupCommands) {
        if (fork() == 0) {
            std::cout << "Executing: " << Command << std::endl;
            execl("/bin/sh", "/bin/sh", "-c", Command.c_str(), (void *)NULL);
        }
    }

    /* KEYBINDS */
    Runtime.Keybinds.insert({KeysymToKeycode(XK_space), {XCB_MOD_MASK_4, "rofi -show drun"}});
    Runtime.Keybinds.insert({KeysymToKeycode(XK_m), {XCB_MOD_MASK_4, "exert-command ExitWM"}});
    Runtime.Keybinds.insert({KeysymToKeycode(XK_c), {XCB_MOD_MASK_4, "exert-command KillActive"}});
    Runtime.Keybinds.insert({KeysymToKeycode(XK_d), {XCB_MOD_MASK_4, "librewolf"}});
    Runtime.Keybinds.insert({KeysymToKeycode(XK_f), {XCB_MOD_MASK_4, "armcord"}});
    Runtime.Keybinds.insert({KeysymToKeycode(XK_q), {XCB_MOD_MASK_4, "kitty"}});
    Runtime.Keybinds.insert({KeysymToKeycode(XK_z), {XCB_MOD_MASK_4, "vscodium"}});
    Runtime.Keybinds.insert({KeysymToKeycode(XK_w), {XCB_MOD_MASK_4, "virt-manager"}});
    Runtime.Keybinds.insert({KeysymToKeycode(XK_x), {XCB_MOD_MASK_4, "thunar"}});
    Runtime.Keybinds.insert({KeysymToKeycode(XK_e), {XCB_MOD_MASK_4, "notify-send \"$(date)\""}});
    Runtime.Keybinds.insert({KeysymToKeycode(XK_Insert), {XCB_MOD_MASK_4, "flameshot gui"}});
    Runtime.Keybinds.insert({KeysymToKeycode(XK_Page_Up), {XCB_MOD_MASK_4, "kitty -o allow_remote_control=yes sh -c \"sudo vpn_handler up\""}});
    Runtime.Keybinds.insert({KeysymToKeycode(XK_Page_Down), {XCB_MOD_MASK_4, "kitty -o allow_remote_control=yes sh -c \"sudo vpn_handler down\""}});

    // Workspaces
    Runtime.Keybinds.insert({KeysymToKeycode(XK_1), {XCB_MOD_MASK_4, "exert-command SetFocusedMonitorToWorkspace 0"}});
    Runtime.Keybinds.insert({KeysymToKeycode(XK_2), {XCB_MOD_MASK_4, "exert-command SetFocusedMonitorToWorkspace 1"}});
    Runtime.Keybinds.insert({KeysymToKeycode(XK_3), {XCB_MOD_MASK_4, "exert-command SetFocusedMonitorToWorkspace 2"}});
    Runtime.Keybinds.insert({KeysymToKeycode(XK_4), {XCB_MOD_MASK_4, "exert-command SetFocusedMonitorToWorkspace 3"}});
    Runtime.Keybinds.insert({KeysymToKeycode(XK_5), {XCB_MOD_MASK_4, "exert-command SetFocusedMonitorToWorkspace 4"}});
    Runtime.Keybinds.insert({KeysymToKeycode(XK_6), {XCB_MOD_MASK_4, "exert-command SetFocusedMonitorToWorkspace 5"}});
    Runtime.Keybinds.insert({KeysymToKeycode(XK_7), {XCB_MOD_MASK_4, "exert-command SetFocusedMonitorToWorkspace 6"}});
    Runtime.Keybinds.insert({KeysymToKeycode(XK_8), {XCB_MOD_MASK_4, "exert-command SetFocusedMonitorToWorkspace 7"}});
    Runtime.Keybinds.insert({KeysymToKeycode(XK_9), {XCB_MOD_MASK_4, "exert-command SetFocusedMonitorToWorkspace 8"}});

    // Get Protocols
    GetAtom("WM_PROTOCOLS", &WM.ProtocolsContainer.Protocols);
    GetAtom("WM_DELETE_WINDOW", &WM.ProtocolsContainer.DeleteWindow);
    GetAtom("_NET_WM_STATE", &WM.ProtocolsContainer.NetWmState);
    GetAtom("_NET_WM_STATE_FULLSCREEN", &WM.ProtocolsContainer.NetWmStateFullscreen);


    StartupWM();
    RunEventLoop();
    
    return EXIT_SUCCESS;
}
