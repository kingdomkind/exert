--[[ Syntax

Order: , .
Embody duplicate commands with []

wm-king command KEYBIND=[ALT.Q.L,wm-king\ command\ workspace\ 5][ALT.Q.R,wm-king\ command\ workspace\ 6]
]]


local Castle = {}
local PipePath = "/tmp/wmking-runtime"

---@param Message string
function Castle.WritePipe(Message) 
    local Pipe = io.open(PipePath, "w")

    if Pipe == nil then
        error("Unable to open the named pipe for writing.")
        os.exit(-1)
    end

    Pipe:write(Message)
    Pipe:close()
end

---@return string
function Castle.ReadPipe()
    local Pipe = io.open(PipePath, "r")

    if Pipe == nil then
        error("Unable to open the named pipe for reading.")
        os.exit(-1)
    end

    local Data = Pipe:read("*all")
    Pipe:close()
    return Data
end

---@param Binds table
function Castle.SetKeybinds(Binds)
    local Command = "KEYBIND="
    for Index, Keybind in ipairs(Binds) do
        local String = "["
        if #Keybind > 3 then error("There are more than 3 values in a keybind! Table Index: ".. Index ..", Function Call: ".. debug.getinfo(2, "l").currentline) end
        for Index, Value in ipairs(Keybind) do
            if Index == 2 then
                for NumIndex, SingleKey in ipairs(Value) do
                    String = String..SingleKey.."."
                end
                String = string.sub(String, 1, -2) --> Remove last character (there will be a spare '.')
            elseif Index == 3 then
                String = String..","
                if Keybind[1] == "Command" then
                    String = String.."wm-king "
                elseif Keybind[1] ~= "Custom" then
                    error("Did not recognise bind type of '".. Keybind[1] .."'! Table Index: ".. Index ..", Function Call: ".. debug.getinfo(2, "l").currentline)
                end
                String = String..Value
            end
        end
        String = String.."]"
        Command = Command..String
    end
    print(Command)

    Castle.WritePipe(Command)
end

return Castle