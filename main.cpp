#include "AudioCapture.hpp"
#include "DataSender.hpp"

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

int cleanup_and_exit(int code) {
    if (data_sender) { delete data_sender; }
    if (audio_capture) { delete audio_capture; }
    if (code) { printf("[CRIT] Setup failed!\n"); }
    return code;
}

int main(int argc, char* argv[]) {
    std::string device_name = "";
    bool use_input_device   = false;
    unsigned max_channels   = 0;
    if ((argc >= 2) && (argc <= 4)) {
        device_name = argv[1];
        if (argc == 3) {
            use_input_device = (argv[2] == "I") || (argv[2] == "i");
        } else if (argc == 4) {
            std::string max_channels_str = argv[3];
            if (std::all_of(max_channels_str.begin(), max_channels_str.end(), ::isdigit)) { max_channels = static_cast<unsigned>(std::stoi(max_channels_str)); }
        }
    } else if (argc > 4) {
        printf("Usage: LightStripAudioSync <Device name (opt.)> <Device type 'I'/'O' (opt.)> <Max. channels (opt.))>\n");
        return 1;
    }

    printf("[LightStripAudioSync]\n\n");

    printf("[INFO] Starting data sender...\n");
    data_sender = new DataSender();
    if (!data_sender || data_sender->initialize() != 0) { return cleanup_and_exit(1); }

    printf("[INFO] Starting audio capture...\n");
    audio_capture = new AudioCapture(data_sender, device_name, use_input_device, max_channels);
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
            if (!input.empty()) {
                printf("Unknown command: %s\n", input.c_str());
            } else {
                // Wait a bit to reduce CPU load
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        }
    }

    return cleanup_and_exit(0);
}
