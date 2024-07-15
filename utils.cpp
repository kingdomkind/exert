#include "utils.h"

std::vector<std::string> SplitStringAtElement(std::string String, std::string Element) {
    std::vector<std::string> Result;
    while (true) {
        int LastFound = String.find(Element);
        if (LastFound == -1) {
            Result.push_back(String);
            break;
        }   
        Result.push_back(String.substr(0, LastFound));
        String = String.substr(LastFound + Element.length());
    }
    return Result;
}