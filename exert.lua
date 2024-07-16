local Attempt = require("attempt")

local Binds = {
    {"Command", {"ALT", "R"}, "movetoworkspace 5"},
    {"Custom", {"ALT", "SUPER", "L"}, "vscode --no-gpu-accel"},
}

Attempt.SetKeybinds(Binds)
