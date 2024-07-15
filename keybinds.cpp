#include "keybinds.h"
#include "utils.h"
#include "config.h"
#include <set>

void ParseAndCreateKeybinds(std::string Argument) {
    Argument = Argument.substr(1, Argument.length()-2); // Remove start and ending brackets
    std::vector<std::string> IndividualKeybinds = SplitStringAtElement(Argument, "][");
    CachedData.Keybinds.clear();

    for (std::string RawKeybind : IndividualKeybinds) {
        Keybind FinishedKeybind;
        int CommaPosition = RawKeybind.find(',');
        std::string KeySelection = RawKeybind.substr(0, CommaPosition);
        FinishedKeybind.Command = RawKeybind.substr(CommaPosition + 1);

        std::set<unsigned int> Binds;
        for (std::string Element: SplitStringAtElement(KeySelection, ".")) {
            Binds.insert(std::stoi(Element));
        }

        FinishedKeybind.Binds = Binds;
        CachedData.Keybinds.push_back(&FinishedKeybind);
    }

/*
    for (Keybind Key : CachedData.Keybinds) {
        std::cout << "Binds: ";
        for (std::string temp : Key.Binds) {
            std::cout << " Key:" << temp;
        }
        std::cout << "" << std::endl;
        std::cout << "Command: " << Key.Command << std::endl;
    } */
}
