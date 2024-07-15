#pragma once
#include <algorithm>
#include <set>
#include <string>
#include <vector>

std::vector<std::string> SplitStringAtElement(std::string String, std::string Element);

template <typename T> void RemoveElementFromVector(std::vector<T> &Vector, const T &Element) {
    auto Found = std::find(Vector.begin(), Vector.end(), Element);
    if (Found != Vector.end()) {
        Vector.erase(Found);
    }
}

template <typename T> void RemoveElementFromSet(std::set<T> &Set, const T &Element) {
    auto Found = std::find(Set.begin(), Set.end(), Element);
    if (Found != Set.end()) {
        Set.erase(Found);
    }
}