#pragma once

#define NOMINMAX

#include "Packet.hpp"

#include <atomic>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

#if defined(_WIN32)
#include <winsock2.h>
#include <Ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

class DataSender {
    private:
        constexpr static unsigned short PORT               = 3333;
        constexpr static unsigned RESEND_ZERO_PACKET_COUNT = 5;

    private:
#if defined(_WIN32)
        WSADATA wsa_data = {};
        SOCKET socket    = INVALID_SOCKET;
#else
        int socket = -1;
#endif

        std::atomic<bool> listen_thread_is_running          = false;
        std::unique_ptr<std::thread> listen_thread_instance = nullptr;

        std::atomic<bool> send_thread_is_running          = false;
        std::atomic<bool> send_thread_has_queued_data     = false;
        std::unique_ptr<std::thread> send_thread_instance = nullptr;

        std::atomic<bool> discover_thread_is_running          = false;
        std::unique_ptr<std::thread> discover_thread_instance = nullptr;

        std::atomic<unsigned> zero_packet_count = 0;

        std::mutex send_queue_mutex   = {};
        std::queue<Packet> send_queue = {};

        std::mutex destination_mutex          = {};
        std::vector<sockaddr_in> destinations = {};

        bool send(const Packet& packet);
        bool send_raw(const sockaddr* address, const std::vector<uint8_t>& packet);

    public:
        DataSender(void) = default;
        ~DataSender(void);

        int initialize(void);
        int initialize_device(const char* destination_ip);

        static void listen_thread(DataSender* data_sender);
        static void send_thread(DataSender* data_sender);
        static void discover_thread(DataSender* data_sender);

        void enqueue(const Packet& packet);
};
