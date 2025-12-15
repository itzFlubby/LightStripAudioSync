#pragma once

#include <atomic>
#include <stdint.h>
#include <stdio.h>
#include <thread>
#include <vector>
#include <winsock2.h>
#include <Ws2tcpip.h>

// Link with ws2_32.lib
#pragma comment(lib, "Ws2_32.lib")

class DataSender {
    private:
        constexpr static size_t magnitudes_size       = 2;
        constexpr static unsigned short port          = 3333;
        constexpr static const char* discover_message = "DISCOVER_LIGHTSTRIP_AUDIOSYNC_DEVICE";
        constexpr static const char* register_message = "REGISTER_LIGHTSTRIP_AUDIOSYNC_DEVICE";

    private:
        WSADATA wsa_data                      = {};
        SOCKET socket                         = {};
        std::vector<sockaddr_in> destinations = {};

        std::atomic<bool> discover_thread_running = false;
        std::thread* discover_thread_instance     = nullptr;

        std::atomic<bool> listen_thread_running = false;
        std::thread* listen_thread_instance     = nullptr;

        uint8_t last_magnitudes[magnitudes_size] = {};
        unsigned identical_magnitudes_count      = 0;

    public:
        DataSender(void);
        ~DataSender(void);

        int initialize(void);
        int initialize_device(PCSTR destination_ip);

        static void discover_thread(DataSender* data_sender, std::atomic<bool>* running);
        static void listen_thread(DataSender* data_sender, std::atomic<bool>* running);

        int send_discover(void);
        int send_magnitudes(uint8_t left, uint8_t right);
};
