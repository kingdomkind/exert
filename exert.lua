local Castle = require("wm-castle")

local Binds = {
    {"Command", {"ALT", "R"}, "movetoworkspace 5"},
    {"Custom", {"ALT", "SUPER", "L"}, "vscode --no-gpu-accel"},
}

Castle.SetKeybinds(Binds)
