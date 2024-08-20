#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <iostream>
#include <memory>
#include <ostream>
#include <stack>
#include <algorithm>
#include <string>
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
#include "shared.h"
#include "config.h"

/* The split direction of a container */
enum Split {
    VERTICAL, // 0
    HORIZONTAL, // 1
    NONE, // 2
};

/* Struct that allows us to easily specify an X and Y co-ord */
struct Coordinate {
    float X;
    float Y;
};

/* Enums that specify the segment of a window */
enum WindowSegment {
    LEFT, // Remaining 2/4 middle left
    RIGHT, // Remaining 2/4 middle right
    UP, // Top 1/4 of the window
    DOWN, // Bottom 1/4 of the window
};

/* This is used as a return type, so we can return related information about windows */
struct WindowMetadata {
    std::shared_ptr<struct Container> Container;
    int Workspace = -1;
};

/* The struct associated with each window we manage */
struct Window {
    xcb_window_t Window;
    
    // Only if the window is floating (not tiled)
    bool Floating = false;
    Coordinate Position; // scale of 0 to 1
    Coordinate Size; // Width, Height, scale of 0 to 1
};

/* Each window struct has an associated Container. This is because we have a tree structure of containers, that define how windows should be split and positioned
A container can either define a split, or can be a "holding" struct for a window - they cannot do both.
If Direction is None it will have a Value / associated window (and no left / right pointer), otherwise it will have no value (and have left / right pointers) */
struct Container {
    Split Direction;
    float Ratio = 0.5; // Must be between 0 and 1 
    
    std::shared_ptr<Container> Parent = nullptr;
    std::shared_ptr<Container> Left = nullptr;
    std::shared_ptr<Container> Right = nullptr;

    std::shared_ptr<Window> Value = nullptr;
};

/* The struct that defines each workspace. Each workspace has a root container, which represents the root node of the heirarchy tree */
struct Workspace {
    std::shared_ptr<Container> RootContainer = nullptr;
    std::unordered_set<std::shared_ptr<Container>> FloatingContainers;
    std::shared_ptr<Container> FullscreenContainer = nullptr; // If there is a window that is fullscreened on the workspace
};

/* The struct containing information about monitors */
struct Monitor {
    xcb_randr_output_t Output; // Each monitor has a unique output identifier, essentially an id
    std::string Name; // Usually the name of the port the monitor is connected to (eg. DP-2)
    int X;
    int Y;
    int Width;
    int Height;
    int ActiveWorkspace = -1; // Workspace being displayed on the monitor
};

/* Protocols that we support / need */
struct Protocols {
    xcb_atom_t Protocols;
    xcb_atom_t DeleteWindow;
    xcb_atom_t NetWmState;
    xcb_atom_t NetWmStateFullscreen;
    xcb_atom_t NetWmWindowTypeDialog;
    xcb_atom_t NetWmWindowTypeUtility;
    xcb_atom_t NetWmWindowTypeSplash;
    xcb_atom_t NetWmWindowType;
};

/* The main Window manager structure for information */
struct WM {
    xcb_connection_t* Connection; // Reference to the x11 server connection
    xcb_screen_t* Screen; // The root display
    xcb_key_symbols_t* Keysyms; // Gets the key symbols for the connected keyboard
    std::shared_ptr<Container> FocusedContainer; // The container of the current window that is being hovered over
    std::vector<std::shared_ptr<Monitor>> Monitors; // All the monitors
    std::vector<std::shared_ptr<Workspace>> Workspaces; // All the workspace structs. The index refers to which workspace it is (eg. index 0 is workspace 0);

    Protocols ProtocolsContainer; // The previously mentioned protocols
};

const float OFFSCREEN_WINDOW_MULTIPLIER = 1.5;
const float RESIZE_INCREMEMNT = 0.01;

static WM WM;

// ! UTILITY FUNCTIONS
xcb_atom_t GetAtom(std::string AtomName) {
    xcb_intern_atom_reply_t* Atom = xcb_intern_atom_reply(WM.Connection, xcb_intern_atom(WM.Connection, 0, strlen(AtomName.c_str()), AtomName.c_str()), nullptr);
    if (!Atom) {
        std::cerr << "Failed to get Atom: " << AtomName << " [EXIT]" << std::endl;
    }
    xcb_atom_t ReturnAtom = Atom->atom;
    free(Atom); 
    return ReturnAtom;
}

bool DoesWindowSupportProtocol(xcb_window_t Window, xcb_atom_t Atom) {
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

                if (CurrentContainer->Right != nullptr) { Stack.push(CurrentContainer->Right); }
                if (CurrentContainer->Left != nullptr) { Stack.push(CurrentContainer->Left); }
            }
        } else {
            std::cerr << "Could not print windows as there is no root container!" << std::endl;
        }
    }
    std::cout << std::endl;
}

std::shared_ptr<WindowMetadata> GetWorkspaceAndContainerFromWindow_PossibleNullptr(xcb_window_t Window) {
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

        for (auto CurrentContainer: Workspace->FloatingContainers) {
            if (CurrentContainer->Value->Window == Window) {
                Metadata->Container = CurrentContainer;
                Metadata->Workspace = i;
                return Metadata;
            }
        }
    }
    std::cerr << "Could not find the specified container for window: " << Window << ", note that this may be because we do not manage this client" << std::endl;
    return nullptr;
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

std::shared_ptr<Monitor> GetMonitorFromWorkspace_PossibleNullptr(int Workspace) {
    for (auto Monitor: WM.Monitors) {
        if (Monitor->ActiveWorkspace == Workspace) {
            return Monitor;
        }
    }
    return nullptr;
}

void SendWindowToFront(xcb_window_t Window) {
    uint32_t Parameters[] = { XCB_STACK_MODE_ABOVE };
    xcb_configure_window(WM.Connection, Window, XCB_CONFIG_WINDOW_STACK_MODE, Parameters);
    xcb_flush(WM.Connection);
}


unsigned int GetActiveWorkspaceEnsureValid(std::shared_ptr<Monitor> MonitorToCheck) {
    int ActiveWorkspace = MonitorToCheck->ActiveWorkspace;
    if (ActiveWorkspace != -1) {
        std::cout << "Active workspace of Monitor: " << MonitorToCheck->Name << " is " << ActiveWorkspace << std::endl;
        return ActiveWorkspace;
    } else {
        std::cerr << "Active workspace of Monitor: " << MonitorToCheck->Name << " is -1, which is invalid! [EXIT]" << std::endl;
        exit(EXIT_FAILURE);
    }
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

void UpdateWindowToCurrentSplits(std::shared_ptr<Container> TargetContainer) {
    std::cout << "Updating to current splits: " << TargetContainer->Parent << " " << TargetContainer->Left << " " << TargetContainer->Right << " " << TargetContainer->Value->Window << std::endl;
    std::shared_ptr<Monitor> Monitor = GetMonitorFromWorkspace_PossibleNullptr(GetWorkspaceAndContainerFromWindow_PossibleNullptr(TargetContainer->Value->Window)->Workspace);

    if (Monitor == nullptr) { // Workspace is off screen
        std::cout << "Monitor is nullptr, and so is offscreen" << std::endl;
        xcb_get_geometry_reply_t* WindowGeometry = xcb_get_geometry_reply(WM.Connection, xcb_get_geometry(WM.Connection, TargetContainer->Value->Window), NULL);
        const uint32_t POS_TO_MOVE[] = {static_cast<uint32_t>(WindowGeometry->x), static_cast<uint32_t>(static_cast<uint32_t>(WindowGeometry->y) + (GetActiveMonitor()->Height * OFFSCREEN_WINDOW_MULTIPLIER))};
        xcb_configure_window(WM.Connection, TargetContainer->Value->Window, XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y, &POS_TO_MOVE);
        xcb_flush(WM.Connection);
        return;
    }

    if (TargetContainer->Value == nullptr) {
        std::cerr << "Target container has no value -- cannot proceed in positioning and sizing it! [EXIT]" << std::endl;
        exit(EXIT_FAILURE);
    }

    uint32_t X, Y, Width, Height;
    X = Monitor->X; Y = Monitor->Y; Width = Monitor->Width; Height = Monitor->Height;

    // Ensure that the window isn't fullscreened
    if (WM.Workspaces[GetActiveWorkspaceEnsureValid(Monitor)]->FullscreenContainer != TargetContainer) {
        if (TargetContainer->Value->Floating != true) {
            X += Runtime.Settings.MonitorPadding; Y += Runtime.Settings.MonitorPadding; Width -= (Runtime.Settings.MonitorPadding*2); Height -= (Runtime.Settings.MonitorPadding*2);
            X -= (Runtime.Settings.WindowPadding/2); Y -= (Runtime.Settings.WindowPadding/2); Width += (Runtime.Settings.WindowPadding); Height += (Runtime.Settings.WindowPadding);
            std::shared_ptr<Container>* CurrentContainer = &TargetContainer;
            std::stack<std::shared_ptr<Container>> Stack;
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
                    if (TopContainer->Right == Stack.top()) { X += Width * (TopContainer->Ratio); Width *= (1-TopContainer->Ratio); } else { Width *= (TopContainer->Ratio); }
                } else {
                    if (TopContainer->Right == Stack.top()) { Y += Height * (TopContainer->Ratio); Height *= (1-TopContainer->Ratio); } else { Height *= (TopContainer->Ratio); }
                }
            }
            X += (Runtime.Settings.WindowPadding/2); Y += (Runtime.Settings.WindowPadding/2); Width -= Runtime.Settings.WindowPadding; Height -= Runtime.Settings.WindowPadding;
        } else { // Window is floating
            X += (Width * TargetContainer->Value->Position.X); Y += (Height * TargetContainer->Value->Position.Y); Width *= TargetContainer->Value->Size.X; Height *= TargetContainer->Value->Size.Y;
        }
        std::cout << "Iter " << TargetContainer->Value->Window << " to current splits, PosX: " << X << ", PosY: " << Y << ", Width: " << Width << ", Height: " << Height << std::endl;
    } else {
        std::cout << "Setting fullscreened window to max res" << std::endl;
    }

    uint32_t BorderWidth = TargetContainer->Value->Floating ? Runtime.Settings.FloatingWindowBorderSize : Runtime.Settings.TiledWindowBorderSize;
    uint32_t Parameters[] = {X, Y, Width-(2*BorderWidth), Height-(2*BorderWidth), BorderWidth};
    xcb_configure_window(WM.Connection, TargetContainer->Value->Window, XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y | XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT | XCB_CONFIG_WINDOW_BORDER_WIDTH, Parameters);
    xcb_flush(WM.Connection);
    std::cout << "Updated Window " << TargetContainer->Value->Window << " to current splits, PosX: " << X << ", PosY: " << Y << ", Width: " << Width << ", Height: " << Height << std::endl;
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

    if (RatioY < 0.25) { return UP; } else if (RatioY > 0.75) { return DOWN; }
    if (RatioX < 0.5) { return LEFT; } else { return RIGHT; }
}

void UpdateWindowSplitsRecursively(std::shared_ptr<Container> BaseContainer) {
    std::stack<std::shared_ptr<Container>> Stack;
    Stack.push(BaseContainer);
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

void MapWindowToWM(unsigned int WindowToMap) {
    std::shared_ptr<Window> NewWindow = std::make_shared<Window>();
    NewWindow->Window = WindowToMap;
    std::shared_ptr<Container> NewContainer = std::make_shared<Container>();
    NewContainer->Direction = NONE;
    NewContainer->Parent = nullptr;
    NewContainer->Value = NewWindow;
    std::shared_ptr<Workspace> ActiveWorkspace = WM.Workspaces[GetActiveWorkspaceEnsureValid(GetActiveMonitor())];

    // Check if window is a popup or similar, if so map it to the center of the current monitor
    xcb_get_property_reply_t* WindowTypeReply = xcb_get_property_reply(WM.Connection, xcb_get_property(WM.Connection, 0, WindowToMap, WM.ProtocolsContainer.NetWmWindowType, XCB_ATOM_ATOM, 0, 32), nullptr);
    if (WindowTypeReply) {
        if (WindowTypeReply->type == XCB_ATOM_ATOM && WindowTypeReply->format == 32 && WindowTypeReply->length > 0) {
            xcb_atom_t* Types = (xcb_atom_t*)xcb_get_property_value(WindowTypeReply);
            for (int i = 0; i < static_cast<int>(WindowTypeReply->length); i++) {
                if (Types[i] == WM.ProtocolsContainer.NetWmWindowTypeDialog || Types[i] == WM.ProtocolsContainer.NetWmWindowTypeUtility || Types[i] == WM.ProtocolsContainer.NetWmWindowTypeSplash) {
                    std::cout << "Window to map is a popup, mapping it to 1/2 the current monitor in all respects. Window: " << WindowToMap << std::endl;
                    NewWindow->Floating = true;
                    NewWindow->Position = {0.25f, 0.25f};
                    NewWindow->Size = {0.5f, 0.5f};
                    ActiveWorkspace->FloatingContainers.insert(NewContainer);
                    uint32_t EventMasks[] = {XCB_EVENT_MASK_ENTER_WINDOW | XCB_EVENT_MASK_FOCUS_CHANGE};
                    xcb_change_window_attributes(WM.Connection, WindowToMap, XCB_CW_EVENT_MASK, &EventMasks);
                    xcb_change_window_attributes(WM.Connection, WindowToMap, XCB_CW_BORDER_PIXEL, &Runtime.Settings.InActiveFloatingWindowBorderColour);
                    UpdateWindowToCurrentSplits(NewContainer);
                    xcb_map_window(WM.Connection, WindowToMap);
                    xcb_flush(WM.Connection);
                    return;
                }
            }
        }
    }

    bool FullscreenRefreshNeeded = false;

    if (ActiveWorkspace->RootContainer != nullptr) { // Need to create a split, this isn't the first window opened
        if (WM.FocusedContainer != nullptr) { // Create window size & splits based on the focused window

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

            if (ActiveWorkspace->FullscreenContainer == WM.FocusedContainer) {
                std::cout << "Mapping window when there is a window fullscreened, changing focused container to new focused container" << std::endl;
                ActiveWorkspace->FullscreenContainer = NewFocusedContainer;
                FullscreenRefreshNeeded = true;
            }
            WM.FocusedContainer = NewFocusedContainer;
            if (FullscreenRefreshNeeded == false) { UpdateWindowToCurrentSplits(WM.FocusedContainer); }

        } else {
            std::cerr << "Unable to create window as the focused window is nullptr, yet there are windows opened!" << " [EXIT] " << std::endl;
            exit(EXIT_FAILURE);
        }
    } else { // First window opened
        std::cout << "No root, making new root" << std::endl;
        ActiveWorkspace->RootContainer = NewContainer;
    }

    uint32_t EventMasks[] = {XCB_EVENT_MASK_ENTER_WINDOW | XCB_EVENT_MASK_FOCUS_CHANGE};
    xcb_change_window_attributes(WM.Connection, WindowToMap, XCB_CW_EVENT_MASK, &EventMasks);
    xcb_change_window_attributes(WM.Connection, WindowToMap, XCB_CW_BORDER_PIXEL, &Runtime.Settings.InActiveTiledWindowBorderColour);
    UpdateWindowToCurrentSplits(NewContainer);
    if (FullscreenRefreshNeeded == true) { UpdateWindowToCurrentSplits(WM.FocusedContainer); SendWindowToFront(WM.FocusedContainer->Value->Window); } // We map the fullscreened window after so it appears ontop
    std::cout << "ADDED! " << WindowToMap << std::endl;
    PrintVisibleWindows();

    xcb_map_window(WM.Connection, WindowToMap);
    xcb_flush(WM.Connection);
}

void RemoveContainerFromWM(std::shared_ptr<Container> ToBeRemoved, int Workspace) {
    std::cout << "Removing container from WM" << std::endl;
    if (WM.FocusedContainer == ToBeRemoved) {
        WM.FocusedContainer = nullptr;
        std::cout << "Focused Container was deleted, setting to nullptr" << std::endl;    
    }

    if (WM.Workspaces[Workspace]->FullscreenContainer == ToBeRemoved) {
        WM.Workspaces[Workspace]->FullscreenContainer = nullptr;
        std::cout << "Fullscreened Container was deleted, setting to nullptr" << std::endl;    
    }

    if (ToBeRemoved->Value->Floating == true) { // Floating Logic
        WM.Workspaces[Workspace]->FloatingContainers.erase(ToBeRemoved);
        xcb_change_window_attributes(WM.Connection, ToBeRemoved->Value->Window, XCB_CW_BORDER_PIXEL, &Runtime.Settings.InActiveFloatingWindowBorderColour); // Incase the window is planned to be remapped later
        xcb_flush(WM.Connection);

    } else { // Tiling logic
        xcb_change_window_attributes(WM.Connection, ToBeRemoved->Value->Window, XCB_CW_BORDER_PIXEL, &Runtime.Settings.InActiveTiledWindowBorderColour);
        xcb_flush(WM.Connection);

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
            UpdateWindowSplitsRecursively(PromotionContainer);

        } else {
            WM.Workspaces[Workspace]->RootContainer = nullptr;
            std::cout << "Root container was deleted, setting to nullptr" << std::endl;    
        }
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

// ! COMMANDS
void ToggleActiveWindowFloating() {
    if (WM.FocusedContainer->Value->Floating == false) { // Tiling to Floating Logic
        std::shared_ptr<Container> RemovalContainer = WM.FocusedContainer;
        int Workspace = GetWorkspaceAndContainerFromWindow_PossibleNullptr(WM.FocusedContainer->Value->Window)->Workspace;
        RemoveContainerFromWM(WM.FocusedContainer, Workspace);
        RemovalContainer->Parent = nullptr;
        RemovalContainer->Left = nullptr;
        RemovalContainer->Right = nullptr;
        RemovalContainer->Value->Floating = true;
        RemovalContainer->Value->Position = {0.25f, 0.25f};
        RemovalContainer->Value->Size = {0.5f, 0.5f};
        WM.Workspaces[Workspace]->FloatingContainers.insert(RemovalContainer);
        UpdateWindowToCurrentSplits(RemovalContainer);
    }
}

void ChangeActiveWindowSplitDirection() {
    if (WM.FocusedContainer != nullptr) {
        if (WM.FocusedContainer->Value->Floating == true) { return; } 
        if (WM.FocusedContainer->Parent != nullptr) {
            if (WM.FocusedContainer->Parent->Direction == VERTICAL) {
                WM.FocusedContainer->Parent->Direction = HORIZONTAL;
            } else {
                WM.FocusedContainer->Parent->Direction = VERTICAL;
            }
            UpdateWindowSplitsRecursively(WM.FocusedContainer->Parent);
        }
    }
}

void SwapActiveWindowSides() {
    if (WM.FocusedContainer != nullptr) {
        if (WM.FocusedContainer->Value->Floating == true) { return; } 
        if (WM.FocusedContainer->Parent != nullptr) {
            if (WM.FocusedContainer->Parent->Left == WM.FocusedContainer) {
                WM.FocusedContainer->Parent->Left = WM.FocusedContainer->Parent->Right;
                WM.FocusedContainer->Parent->Right = WM.FocusedContainer;
            } else {
                WM.FocusedContainer->Parent->Right = WM.FocusedContainer->Parent->Left;
                WM.FocusedContainer->Parent->Left = WM.FocusedContainer;

            }
            UpdateWindowSplitsRecursively(WM.FocusedContainer->Parent);
        }
    }
}

void MoveActiveWindow() {
    static int WindowToMove = -1;
    if (WindowToMove == -1) {
        if (WM.FocusedContainer != nullptr) {
            WindowToMove = WM.FocusedContainer->Value->Window;
            xcb_unmap_window(WM.Connection, WindowToMove);
            xcb_flush(WM.Connection);
        }
    } else {
        MapWindowToWM(WindowToMove);
        WindowToMove = -1;
    }
}

void ResizeActiveWindow(WindowSegment Direction) {
    std::cout << "Resizing Active window!" << std::endl;

    if (WM.FocusedContainer != nullptr) {
        if (WM.FocusedContainer->Value->Floating == true) { // Floating Logic
            switch (Direction) {
                case LEFT: { WM.FocusedContainer->Value->Size.X = std::clamp(WM.FocusedContainer->Value->Size.X - RESIZE_INCREMEMNT, 0.0f, 1.0f); break; }
                case RIGHT: { WM.FocusedContainer->Value->Size.X = std::clamp(WM.FocusedContainer->Value->Size.X + RESIZE_INCREMEMNT, 0.0f, 1.0f); break; }
                case UP: { WM.FocusedContainer->Value->Size.Y = std::clamp(WM.FocusedContainer->Value->Size.Y - RESIZE_INCREMEMNT, 0.0f, 1.0f); break; }
                case DOWN: { WM.FocusedContainer->Value->Size.Y = std::clamp(WM.FocusedContainer->Value->Size.Y + RESIZE_INCREMEMNT, 0.0f, 1.0f); break; }
            }
            UpdateWindowToCurrentSplits(WM.FocusedContainer);
        } else { // Tiling Logic
            Split TargetSplit;
            if (Direction == LEFT || Direction == RIGHT) { TargetSplit = VERTICAL; } else { TargetSplit = HORIZONTAL; }    
            std::shared_ptr<Container>* TargetContainer = &WM.FocusedContainer;
            while (TargetContainer->get()->Parent != nullptr) {
                TargetContainer = &TargetContainer->get()->Parent;
                if (TargetContainer->get()->Direction == TargetSplit) {
                    if (Direction == RIGHT || Direction == DOWN) {
                        TargetContainer->get()->Ratio = std::clamp(TargetContainer->get()->Ratio + RESIZE_INCREMEMNT, 0.05f, 0.95f);
                    } else {
                        TargetContainer->get()->Ratio = std::clamp(TargetContainer->get()->Ratio - RESIZE_INCREMEMNT, 0.05f, 0.95f);
                    }
                    UpdateWindowSplitsRecursively(*TargetContainer);
                    break;
                }
            }
        }
    } 
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

void SetWorkspaceToMonitor(unsigned int TargetWorkspace, std::shared_ptr<Monitor> TargetMonitor) {
    std::cout << "Started swapping workspaces" << std::endl;
    unsigned int PreviousWorkspace = GetActiveWorkspaceEnsureValid(TargetMonitor);
    std::shared_ptr<Monitor> PreviousMonitor = GetMonitorFromWorkspace_PossibleNullptr(TargetWorkspace);

    EnsureValidWorkspacesBetweenIndicesInclusive(WM.Workspaces.size(), TargetWorkspace); // Incase we swap to a workspace that doesn't yet exist
    TargetMonitor->ActiveWorkspace = TargetWorkspace;
    if (PreviousMonitor != nullptr) { // The target workspace is on another monitor, we're robbing it from them
        PreviousMonitor->ActiveWorkspace = PreviousWorkspace;
        std::cout << "Swapping workspaces, set Previous monitor from workspace " << TargetMonitor << " to workspace " << PreviousWorkspace << std::endl;
    }
    
    if (WM.Workspaces[PreviousWorkspace]->RootContainer != nullptr) {
        UpdateWindowSplitsRecursively(WM.Workspaces[PreviousWorkspace]->RootContainer);
    }
    for (auto FloatingContainer: WM.Workspaces[PreviousWorkspace]->FloatingContainers) {
        UpdateWindowToCurrentSplits(FloatingContainer);
    }
    std::cout << "Moved previous workspace " << PreviousWorkspace << std::endl;

    if (WM.Workspaces[TargetWorkspace]->RootContainer != nullptr) {
        UpdateWindowSplitsRecursively(WM.Workspaces[TargetWorkspace]->RootContainer);
    }
    for (auto FloatingContainer: WM.Workspaces[TargetWorkspace]->FloatingContainers) {
        UpdateWindowToCurrentSplits(FloatingContainer);
    }

    std::cout << "Set Monitor: " << TargetMonitor << ", to workspace: " << TargetMonitor->ActiveWorkspace << " (should be the same as " << TargetWorkspace << ")" << std::endl;
}

void ToggleFullscreen() {
    if (WM.FocusedContainer) {
        int WorkspaceInt = GetWorkspaceAndContainerFromWindow_PossibleNullptr(WM.FocusedContainer->Value->Window)->Workspace;
        std::shared_ptr<Workspace> TargetWorkspace = WM.Workspaces[WorkspaceInt];
        if (TargetWorkspace->FullscreenContainer != nullptr) { // Untoggle fullscreen window
            std::cout << "Untoggling fullscreen for Workspace: " << WorkspaceInt << std::endl;
            auto TargetContainer = TargetWorkspace->FullscreenContainer;
            TargetWorkspace->FullscreenContainer = nullptr;
            UpdateWindowToCurrentSplits(TargetContainer);
        } else { // Fullscreen the focused window
            std::cout << "Toggling fullscreen for Window: " << WM.FocusedContainer->Value->Window << std::endl;
            TargetWorkspace->FullscreenContainer = WM.FocusedContainer;
            std::cout << "Set workspace " << WorkspaceInt << " fullscreen container to " << TargetWorkspace->FullscreenContainer << "(Should be same as " << WM.FocusedContainer << ")" << std::endl; 
            SendWindowToFront(WM.FocusedContainer->Value->Window);
            UpdateWindowToCurrentSplits(WM.FocusedContainer);
        }
    } else {
        std::cerr << "No focused container to fullscreen / unfullscreen" << std::endl;
    }
}

// ! EVENT LOOP FUNCTIONS
void OnEnterNotify(const xcb_generic_event_t* NextEvent) {
    xcb_enter_notify_event_t* Event = (xcb_enter_notify_event_t*) NextEvent;
    if (Event->event != 0) {
        if (WM.FocusedContainer != nullptr) {
            if (WM.FocusedContainer->Value->Floating == true) {
                xcb_change_window_attributes(WM.Connection, WM.FocusedContainer->Value->Window, XCB_CW_BORDER_PIXEL, &Runtime.Settings.InActiveFloatingWindowBorderColour);
            } else {
                xcb_change_window_attributes(WM.Connection, WM.FocusedContainer->Value->Window, XCB_CW_BORDER_PIXEL, &Runtime.Settings.InActiveTiledWindowBorderColour);
            }
        }
        std::cout << "Setting window focus to: " << Event->event << std::endl;
        xcb_set_input_focus(WM.Connection, XCB_INPUT_FOCUS_POINTER_ROOT, Event->event, XCB_CURRENT_TIME);
        WM.FocusedContainer = GetWorkspaceAndContainerFromWindow_PossibleNullptr(Event->event)->Container;
        if (WM.FocusedContainer->Value->Floating == true) {
            xcb_change_window_attributes(WM.Connection, WM.FocusedContainer->Value->Window, XCB_CW_BORDER_PIXEL, &Runtime.Settings.ActiveFloatingWindowBorderColour);
        } else {
            xcb_change_window_attributes(WM.Connection, WM.FocusedContainer->Value->Window, XCB_CW_BORDER_PIXEL, &Runtime.Settings.ActiveTiledWindowBorderColour);
        }
        xcb_flush(WM.Connection);
    } else {
        std::cout << "Did not set focus to 0 (is it root?)" << std::endl;
    }
    std::cout << "Finished setting focus" << std::endl;
}

void OnMapRequest(const xcb_generic_event_t* NextEvent) {
    std::cout << "Map request recieved" << std::endl;
    xcb_map_request_event_t* Event = (xcb_map_request_event_t*)NextEvent;
    MapWindowToWM(Event->window);
}

void OnUnMapNotify(const xcb_generic_event_t* NextEvent) {
    xcb_map_request_event_t* Event = (xcb_map_request_event_t*)NextEvent;
    auto Result = GetWorkspaceAndContainerFromWindow_PossibleNullptr(Event->window);
    if (Result != nullptr) {
        RemoveContainerFromWM(Result->Container, Result->Workspace);
    } // We don't error, as it can fail as unmap can be called on clients we haven't set up
}

void OnDestroyNotify(const xcb_generic_event_t* NextEvent) {
    xcb_map_request_event_t* Event = (xcb_map_request_event_t*)NextEvent;
    auto Result = GetWorkspaceAndContainerFromWindow_PossibleNullptr(Event->window);
    if (Result != nullptr) {
        RemoveContainerFromWM(Result->Container, Result->Workspace);
    }
}

void HandleFullScreenRequest(xcb_generic_event_t* NextEvent) {
    xcb_client_message_event_t* event = (xcb_client_message_event_t*)NextEvent;
    if (event->type == WM.ProtocolsContainer.NetWmState) {
        if (event->data.data32[1] == WM.ProtocolsContainer.NetWmStateFullscreen || event->data.data32[2] == WM.ProtocolsContainer.NetWmStateFullscreen) {
            std::cout << "fullscreen request for window " << event->window << std::endl; /*
            uint32_t values[] = { XCB_STACK_MODE_ABOVE };
            xcb_configure_window(WM.Connection, event->window, XCB_CONFIG_WINDOW_STACK_MODE, values);
            xcb_flush(WM.Connection); */
            return;
        }
    }
}

std::unordered_map<std::string, std::function<void(const std::string &Arguments)>> InternalCommand = { // Only used in on keypress hence why it is here
    {"KillActive", [](const std::string &Arguments) { if (!(WM.FocusedContainer == nullptr)) { KillWindow(WM.FocusedContainer->Value->Window); } else { std::cerr << "Focused window does not exist, cannot kill it" << std::endl;}}},
    {"ExitWM", [](const std::string &Arguments) { ExitWM(); }},
    {"SetFocusedMonitorToWorkspace", [](const std::string &Arguments){ SetWorkspaceToMonitor(std::stoi(Arguments), GetActiveMonitor()); }},
    {"ToggleFullscreen", [](const std::string &Arguments){ ToggleFullscreen(); }},
    {"ResizeActiveWindow", [](const std::string &Arguments) { if (Arguments == "Left") {ResizeActiveWindow(LEFT); } else if (Arguments == "Right") { ResizeActiveWindow(RIGHT); } else if (Arguments == "Up") { ResizeActiveWindow(UP); } else if (Arguments == "Down") {ResizeActiveWindow(DOWN); }}},
    {"MoveActiveWindow", [](const std::string &Arguments){ MoveActiveWindow(); }},
    {"ChangeActiveWindowSplitDirection", [](const std::string &Arguments){ ChangeActiveWindowSplitDirection(); }},
    {"SwapActiveWindowSides", [](const std::string &Arguments){ SwapActiveWindowSides(); }},
    {"ToggleActiveWindowFloating", [](const std::string &Arguments){ ToggleActiveWindowFloating(); }},
};

void OnKeyPress(const xcb_generic_event_t* NextEvent) {
    xcb_key_press_event_t* Event = (xcb_key_press_event_t*)NextEvent;
    xcb_keycode_t Keycode = Event->detail;
    auto TargetRange = Runtime.Keybinds.equal_range(Keycode);
    if (TargetRange.first != TargetRange.second) {
        for (auto Pair = TargetRange.first; Pair != TargetRange.second; ++Pair) {
            if ((Event->state & Pair->second.Modifier) && Keycode == Pair->first) {
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

/* MAIN FUNCTION CALLS */
void StartupWM() {
    const uint32_t Masks = XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT | XCB_EVENT_MASK_STRUCTURE_NOTIFY |  XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY | XCB_EVENT_MASK_PROPERTY_CHANGE;
    xcb_change_window_attributes_checked(WM.Connection, WM.Screen->root, XCB_CW_EVENT_MASK, (void*)&Masks); std::cout << "Changed checked window attributes" << std::endl;
    xcb_ungrab_key(WM.Connection, XCB_GRAB_ANY, WM.Screen->root, XCB_MOD_MASK_ANY); std::cout << "Reset all grabbed keys" << std::endl;

    for (const auto &Pair : Runtime.Keybinds) {
        xcb_grab_key(WM.Connection, 0, WM.Screen->root, Pair.second.Modifier, Pair.first, XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC);
    }

    xcb_flush(WM.Connection); std::cout << "Starting up the WM" << std::endl;
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

void RunEventLoop() {
    std::cout << "Running the event loop" << std::endl;

    while (true) {
        xcb_generic_event_t* NextEvent = xcb_wait_for_event(WM.Connection);
        // std::cout << "Recieved Event: " << (int)NextEvent->response_type << std::endl;
        switch (NextEvent->response_type & ~0x80) {
            case XCB_MAP_REQUEST: { OnMapRequest(NextEvent); break; }
            case XCB_KEY_PRESS: { OnKeyPress(NextEvent); break; }
            case XCB_UNMAP_NOTIFY: { OnUnMapNotify(NextEvent); break; }
            case XCB_DESTROY_NOTIFY: { OnDestroyNotify(NextEvent); break; }
            case XCB_ENTER_NOTIFY: { OnEnterNotify(NextEvent); break; }
            case XCB_CLIENT_MESSAGE: { HandleFullScreenRequest(NextEvent); break; }
            // default: { std::cout << "Ignored Event: " << (int)NextEvent->response_type << std::endl; break; }
        }
    }
}

int main() {
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

    // Convert the keysymbols to their keycodes
    std::multimap<unsigned int, struct Keybind> TempKeybinds;
    for (const auto& Pair : Runtime.Keybinds) {
        TempKeybinds.insert({KeysymToKeycode(Pair.first), Pair.second});
    }
    Runtime.Keybinds.clear();
    for (const auto& Pair : TempKeybinds) {
        Runtime.Keybinds.insert(Pair);
    }

    for (auto Setting: Runtime.Monitors) {
        system(Setting.c_str());
    }
    InitialiseMonitors();

    for (auto Command: Runtime.StartupCommands) {
        if (fork() == 0) {
            std::cout << "Executing: " << Command << std::endl;
            execl("/bin/sh", "/bin/sh", "-c", Command.c_str(), (void *)NULL);
        }
    }

    // Get Protocols
    WM.ProtocolsContainer.Protocols = GetAtom("WM_PROTOCOLS");
    WM.ProtocolsContainer.DeleteWindow = GetAtom("WM_DELETE_WINDOW");
    WM.ProtocolsContainer.NetWmState = GetAtom("_NET_WM_STATE");
    WM.ProtocolsContainer.NetWmStateFullscreen = GetAtom("_NET_WM_STATE_FULLSCREEN");
    WM.ProtocolsContainer.NetWmWindowTypeDialog = GetAtom("_NET_WM_WINDOW_TYPE_DIALOG");
    WM.ProtocolsContainer.NetWmWindowTypeUtility = GetAtom("_NET_WM_WINDOW_TYPE_UTILITY");
    WM.ProtocolsContainer.NetWmWindowTypeSplash = GetAtom("_NET_WM_WINDOW_TYPE_SPLASH");
    WM.ProtocolsContainer.NetWmWindowType = GetAtom("_NET_WM_WINDOW_TYPE");

    StartupWM();
    RunEventLoop();
    return EXIT_SUCCESS;
}
