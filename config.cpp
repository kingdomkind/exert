#include <cerrno>
#include <cstdlib>
#include <iostream>
#include <ostream>
#include <string>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <filesystem>
#include "config.h"
/*#include "utils.h"


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

    for (Keybind Key : CachedData.Keybinds) {
        std::cout << "Binds: ";
        for (std::string temp : Key.Binds) {
            std::cout << " Key:" << temp;
        }
        std::cout << "" << std::endl;
        std::cout << "Command: " << Key.Command << std::endl;
    } 
} */


void WritePipe(std::string Message) {
    std::cout << "Opening Pipe" << std::endl;
    int FileDescriptor = open(PIPE_PATH.c_str(), O_WRONLY); // Hangs until a reader connects
    if (FileDescriptor == -1) {
        std::cerr << "Failed to open pipe!" << std::endl;
        exit(EXIT_FAILURE);
    }

    std::cout << "Writing to Pipe" << std::endl; 
    write(FileDescriptor, Message.c_str(), Message.size());
    close(FileDescriptor);
}

std::string ReadPipe() {
    std::cout << "Opening Pipe" << std::endl;
    int FileDescriptor = open(PIPE_PATH.c_str(), O_RDONLY);
    if (FileDescriptor == -1) {
        std::cerr << "Failed to open pipe!" << std::endl;
        exit(EXIT_FAILURE);
    }

    std::cout << "Reading from Pipe" << std::endl;

    std::string Result;
    char Buffer[1024];
    int BytesRead;
    while ((BytesRead = read(FileDescriptor, Buffer, sizeof(Buffer))) > 0) {
        Result.append(Buffer, BytesRead);
    }

    if (BytesRead == -1) {
        std::cerr << "Failed to read from pipe!" << std::endl;
        close(FileDescriptor);
        exit(EXIT_FAILURE);
    }

    close(FileDescriptor);
    return Result;
}

void CheckAndCreatePipe() {
    if (!std::filesystem::exists(PIPE_PATH)) {
        if (mkfifo(PIPE_PATH.c_str(), 0666) == -1) {
            std::cerr << "Failed to create named pipe!" << std::endl;
            exit(EXIT_FAILURE);
        }
        std::cout << "Created Named Pipe" << std::endl;
    }
}

void StartPipeListener() {
    ListeningThread = std::thread([]() {
        while (true) {
            std::string Message = ReadPipe();
            int EqualsPosition = Message.find('=');
            std::string Identifier = Message.substr(0, EqualsPosition);
            std::string Argument = Message.substr(EqualsPosition + 1);

            if (Identifier == "KEYBIND") {
                //ParseAndCreateKeybinds(Argument);
            }
        }
    });
}