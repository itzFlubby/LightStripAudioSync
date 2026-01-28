#include "AudioCapture.hpp"
#include "DataSender.hpp"
#include "Visualizer.hpp"

#include <algorithm>
#include <chrono>
#include <stdio.h>
#include <stdlib.h>
#if defined(_WIN32)
// Link with ws2_32.lib
#pragma comment(lib, "Ws2_32.lib")
#endif

DataSender* data_sender     = nullptr;
AudioCapture* audio_capture = nullptr;
Visualizer visualizer;

int cleanup_and_exit(int code) {
    if (data_sender) { delete data_sender; }
    if (audio_capture) { delete audio_capture; }
    if (code) { printf("[CRIT] Setup failed!\n"); }
    return code;
}

int main(int argc, char* argv[]) {
    std::string device_name = "";
    int device_id           = -1;
    int max_channels        = -1;

    if ((argc >= 2) && (argc <= 3)) {
        std::string device_name_or_id = argv[1];
        if (std::all_of(device_name_or_id.begin(), device_name_or_id.end(), ::isdigit)) {
            device_id = std::stoi(device_name_or_id);
        } else {
            device_name = device_name_or_id;
        }
        if (argc == 3) {
            std::string max_channels = argv[2];
            if (std::all_of(max_channels.begin(), max_channels.end(), ::isdigit)) {
                max_channels = std::stoi(max_channels);
            } else {
                printf("[WARN] Invalid max channels value: %s (expected a number)\n", max_channels.c_str());
            }
        }
    } else if (argc > 3) {
        printf("Usage: LightStripAudioSync <Device name or id (opt.)> <Max. channels (opt.)>\n");
        return 1;
    }

    printf("[LightStripAudioSync]\n\n");

    printf("[INFO] Starting data sender...\n");
    data_sender = new DataSender();
    if (!data_sender || data_sender->initialize() != 0) { return cleanup_and_exit(1); }

    printf("[INFO] Starting audio capture...\n");
    audio_capture = new AudioCapture(data_sender, device_name, device_id, max_channels);
    if (!audio_capture || audio_capture->initialize() != 0) { return cleanup_and_exit(1); }

    bool visualizer_active = false;
    while (true) {
        if (visualizer_active) {
            visualizer.render(audio_capture);
            continue;
        }
        std::string input;
        std::getline(std::cin, input);

        if (input == "help" || input == "?") {
            printf("Available commands:\n");
            printf("  help, ?       Show this help message\n");
            printf("  visualizer    Opens the visualizer\n");
            printf("  exit, quit    Exit the program\n");
        } else if (input == "visualizer") {
            visualizer_active = true;
        } else if (input == "exit" || input == "quit" || input == "q") {
            break;
        } else {
            if (!input.empty()) {
                printf("Unknown command: \"%s\"\n", input.c_str());
            } else {
                // Wait a bit to reduce CPU load (terminal probably doesn't support input)
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        }
    }

    return cleanup_and_exit(0);
}
