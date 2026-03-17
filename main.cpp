#include "AudioCapture.hpp"
#include "DataSender.hpp"
#include "logger.hpp"
#include "Visualizer.hpp"

#include <algorithm>
#include <chrono>
#include <stdlib.h>
#if defined(_WIN32)
#pragma comment(lib, "Ws2_32.lib") // Link with ws2_32.lib
#endif

#if defined(_WIN32)
#include <io.h>
#define ISATTY _isatty
#define FILENO _fileno
#else
#include <unistd.h>
#define ISATTY isatty
#define FILENO fileno
#endif

DataSender* data_sender     = nullptr;
AudioCapture* audio_capture = nullptr;
Visualizer visualizer;

int cleanup_and_exit(int code) {
    if (data_sender) { delete data_sender; }
    if (audio_capture) { delete audio_capture; }
    if (code) { log("[CRIT] Setup failed!"); }
    return code;
}

int main(int argc, char* argv[]) {
    // Disable buffering
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);

    bool init_ws            = false;
    std::string device_name = "";
    int device_id           = -1;
    int max_channels        = -1;

    if ((argc >= 2) && (argc <= 4)) {
        if (argc >= 2) { init_ws = (std::string(argv[1]) == "ws"); }
        if (argc >= 3) {
            std::string device_name_or_id = argv[2];
            if (std::all_of(device_name_or_id.begin(), device_name_or_id.end(), ::isdigit)) {
                device_id = std::stoi(device_name_or_id);
            } else {
                device_name = device_name_or_id;
            }
        }
        if (argc >= 4) {
            std::string max_channels_string = argv[3];
            if (std::all_of(max_channels_string.begin(), max_channels_string.end(), ::isdigit)) {
                max_channels = std::stoi(max_channels_string);
            } else {
                log("[WARN] Invalid max channels value: %s (expected a number)", max_channels_string.c_str());
            }
        }
    } else if (argc > 4) {
        log("Usage: LightStripAudioSync <Init websocket (opt.)> <Capture device name or ID (opt.)> <Capture device max. channels (opt.)>");
        return 1;
    }

    log("[LightStripAudioSync]");

    log("[INFO] Starting data sender...");
    data_sender = new DataSender();
    if (!data_sender || data_sender->initialize(init_ws) != 0) { return cleanup_and_exit(1); }

    log("[INFO] Starting audio capture...");
    audio_capture = new AudioCapture(data_sender, device_name, device_id, max_channels);
    if (!audio_capture || audio_capture->initialize() != 0) { return cleanup_and_exit(1); }

    if (!ISATTY(FILENO(stdin))) {
        log("[INFO] Environment doesn't have a terminal, going to sleep...");
        while (true) {
            std::this_thread::sleep_for(std::chrono::seconds(60));
        }
    }

    bool visualizer_active = false;
    while (true) {
        if (visualizer_active) {
            visualizer.render(audio_capture);
            continue;
        }
        std::string input;
        std::getline(std::cin, input);

        if (input == "help" || input == "?") {
            log("Available commands:");
            log("  help, ?       Show this help message");
            log("  visualizer    Opens the visualizer");
            log("  exit, quit    Exit the program");
        } else if (input == "visualizer") {
            visualizer_active = true;
        } else if (input == "exit" || input == "quit" || input == "q") {
            break;
        } else {
            if (!input.empty()) { log("Unknown command: \"%s\"", input.c_str()); }
        }
    }

    return cleanup_and_exit(0);
}
