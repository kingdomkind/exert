#pragma once
#include <map>
#include <string>
#include <thread>

struct Keybind {
    unsigned int Modifier;
    std::string Command;
};

struct Runtime {
    // Key is the letter / number / whatever associated with the keybind
    std::multimap<unsigned int, struct Keybind*> Keybinds;
};

const std::string PIPE_PATH = "/tmp/wmking-runtime";
static std::thread ListeningThread;
static Runtime CachedData;

void WritePipe(std::string Message);
std::string ReadPipe();
void CheckAndCreatePipe();
void StartPipeListener();
