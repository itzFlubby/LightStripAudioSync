#pragma once

#include "Packet.hpp"

#include <atomic>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

#if defined(_WIN32)
#define NOMINMAX
#include <winsock2.h>
#include <Ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#define INVALID_SOCKET -1
#define SOCKET_ERROR   -1
typedef int SOCKET;
#endif

class DataSender {
    private:
        constexpr static unsigned short UDP_PORT           = 3333;
        constexpr static unsigned short TCP_PORT           = 3334;
        constexpr static unsigned RESEND_ZERO_PACKET_COUNT = 5;

    private:
#if defined(_WIN32)
        WSADATA wsa_data = {};
#endif
        SOCKET udp_socket = INVALID_SOCKET;
        SOCKET tcp_socket = INVALID_SOCKET;

        struct {
            public:
                std::atomic<bool> udp_listen_running   = false;
                std::atomic<bool> udp_send_running     = false;
                std::atomic<bool> udp_send_queued      = false;
                std::atomic<bool> udp_discover_running = false;
                std::atomic<bool> tcp_send_running     = false;
                std::atomic<bool> tcp_send_queued      = false;
        } state;

        struct {
            public:
                std::unique_ptr<std::thread> udp_listen   = nullptr;
                std::unique_ptr<std::thread> udp_send     = nullptr;
                std::unique_ptr<std::thread> udp_discover = nullptr;
                std::unique_ptr<std::thread> tcp_send     = nullptr;
        } thread_instance;

        // UDP
        std::atomic<unsigned> udp_zero_packet_count = 0;
        std::queue<Packet> udp_send_queue           = {};
        std::vector<sockaddr_in> udp_destinations   = {};
        std::mutex udp_send_mutex                   = {};
        std::mutex udp_listen_mutex                 = {}; // Blocks access to destination vector by the listen thread while sending
        int udp_initialize(void);
        bool udp_send(const Packet& packet);
        bool udp_send_raw(const sockaddr* address, const std::vector<uint8_t>& packet);

        // TCP
        std::atomic<unsigned> tcp_zero_packet_count = 0;
        std::queue<Packet> tcp_send_queue           = {};
        std::mutex tcp_send_mutex                   = {};
        int tcp_initialize(void);
        bool tcp_send(const SOCKET client, const Packet& packet);

    public:
        DataSender(void) = default;
        ~DataSender(void);

        int initialize(void);

        int udp_initialize_device(const char* destination_ip);
        static void udp_listen_thread(DataSender* data_sender);
        static void udp_send_thread(DataSender* data_sender);
        static void udp_discover_thread(DataSender* data_sender);
        void udp_enqueue(const Packet& packet);

        static void tcp_send_thread(DataSender* data_sender);
        void tcp_enqueue(const Packet& packet);
};
