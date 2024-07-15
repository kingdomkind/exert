#pragma once
#include <set>
#include <string>
#include <thread>
#include <vector>

struct Keybind {
    std::set<unsigned int> Binds;
    std::string Command;
    bool AlreadyPressed = false;
};

struct Runtime {
    std::vector<Keybind*> Keybinds;
};

const std::string PIPE_PATH = "/tmp/wmking-runtime";
static std::thread ListeningThread;
static Runtime CachedData;

void WritePipe(std::string Message);
std::string ReadPipe();
void CheckAndCreatePipe();
void StartPipeListener();
