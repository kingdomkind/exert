#pragma once
#include "shared.h"

static Runtime Runtime = {
    // * KEYBINDS
    {
        // WM
        {XK_m, {XCB_MOD_MASK_4, "exert-command ExitWM"}},
        {XK_c, {XCB_MOD_MASK_4, "exert-command KillActive"}},
        {XK_f, {XCB_MOD_MASK_4, "exert-command ToggleFullscreen"}},
        {XK_Left, {XCB_MOD_MASK_4, "exert-command ResizeActiveWindow Left"}},
        {XK_Right, {XCB_MOD_MASK_4, "exert-command ResizeActiveWindow Right"}},
        {XK_Up, {XCB_MOD_MASK_4, "exert-command ResizeActiveWindow Up"}},
        {XK_Down, {XCB_MOD_MASK_4, "exert-command ResizeActiveWindow Down"}},
        {XK_x, {XCB_MOD_MASK_4, "exert-command MoveActiveWindow"}},
        {XK_l, {XCB_MOD_MASK_4, "exert-command ChangeActiveWindowSplitDirection"}},
        {XK_k, {XCB_MOD_MASK_4, "exert-command SwapActiveWindowSides"}},
        // Programs
        {XK_space, {XCB_MOD_MASK_4, "rofi -show drun"}},
        {XK_d, {XCB_MOD_MASK_4, "brave"}},
        {XK_q, {XCB_MOD_MASK_4, "alacritty"}},
        {XK_z, {XCB_MOD_MASK_4, "vscodium"}},
        {XK_w, {XCB_MOD_MASK_4, "virt-manager"}},
        {XK_e, {XCB_MOD_MASK_4, "notify-send \"$(date)\""}},
        {XK_Insert, {XCB_MOD_MASK_4, "flameshot gui"}},
        {XK_Page_Up, {XCB_MOD_MASK_CONTROL, "wpctl set-volume @DEFAULT_AUDIO_SINK@ 5%+"}},
        {XK_Page_Down, {XCB_MOD_MASK_CONTROL, "wpctl set-volume @DEFAULT_AUDIO_SINK@ 5%-"}},
        {XK_Page_Up, {XCB_MOD_MASK_4, "wpctl set-default 56"}},
        {XK_Page_Down, {XCB_MOD_MASK_4, "wpctl set-default 43"}},
        {XK_r, {XCB_MOD_MASK_4, "/home/pika/Config/scripts/wallpaper/change-wallpaper.sh"}},
        // Workspaces
        {XK_1, {XCB_MOD_MASK_4, "exert-command SetFocusedMonitorToWorkspace 0"}},
        {XK_2, {XCB_MOD_MASK_4, "exert-command SetFocusedMonitorToWorkspace 1"}},
        {XK_3, {XCB_MOD_MASK_4, "exert-command SetFocusedMonitorToWorkspace 2"}},
        {XK_4, {XCB_MOD_MASK_4, "exert-command SetFocusedMonitorToWorkspace 3"}},
        {XK_5, {XCB_MOD_MASK_4, "exert-command SetFocusedMonitorToWorkspace 4"}},
        {XK_6, {XCB_MOD_MASK_4, "exert-command SetFocusedMonitorToWorkspace 5"}},
        {XK_7, {XCB_MOD_MASK_4, "exert-command SetFocusedMonitorToWorkspace 6"}},
        {XK_8, {XCB_MOD_MASK_4, "exert-command SetFocusedMonitorToWorkspace 7"}},
        {XK_9, {XCB_MOD_MASK_4, "exert-command SetFocusedMonitorToWorkspace 8"}},
    },

    // * MONITOR SETTINGS
    {
        "xrandr --output DP-4 --mode 2560x1080 --rate 74.99 --right-of DP-2",
        "xrandr --output DP-2 --mode 3840x2160 --rate 119.91",
    },

    // * EXPORTS
    {
        {"XDG_CURRENT_DESKTOP", "Exert"},
        {"XCURSOR_SIZE", "24"},
        {"GTK_THEME", "Adwaita:dark"},
    },

    // * STARTUP COMMANDS
    {
        "dunst",
        "flameshot",
        "picom -b --experimental-backends",
        "/home/pika/Config/scripts/wallpaper/change-wallpaper.sh",
        "xset -dpms && xset s off",
    }
};