#include "AudioCapture.hpp"
#include "DataSender.hpp"

#include <stdio.h>
#include <stdlib.h>

// Link with ws2_32.lib
#pragma comment(lib, "Ws2_32.lib")

DataSender* data_sender     = nullptr;
AudioCapture* audio_capture = nullptr;

int cleanup_and_exit(int code) {
    if (data_sender) { delete data_sender; }
    if (audio_capture) { delete audio_capture; }
    if (code) { printf("[CRIT] Setup failed!\n"); }
    return code;
}

int main() {
    printf("[LightStripAudioSync]\n\n");

    printf("[INFO] Starting data sender...\n");
    data_sender = new DataSender();
    if (!data_sender || data_sender->initialize() != 0) { return cleanup_and_exit(1); }

    printf("[INFO] Starting audio capture...\n");
    audio_capture = new AudioCapture(data_sender);
    if (!audio_capture || audio_capture->initialize() != 0) { return cleanup_and_exit(1); }

    char input;
    while (true) {
        std::string input;
        std::getline(std::cin, input);

        if (input == "help" || input == "?") {
            printf("Available commands:\n");
            printf("  help, ?       Show this help message\n");
            printf("  exit, quit    Exit the program\n");
        } else if (input == "exit" || input == "quit" || input == "q") {
            break;
        } else {
            printf("Unknown command: %s\n", input.c_str());
        }
    }

    return cleanup_and_exit(0);
}
