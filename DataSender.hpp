#pragma once

#define NOMINMAX

#include <atomic>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>
#include <winsock2.h>

// Link with ws2_32.lib
#pragma comment(lib, "Ws2_32.lib")

class DataSender {
    private:
        constexpr static unsigned short port       = 3333;
        constexpr static const char* data_discover = "DISCOVER_LIGHTSTRIP_AUDIOSYNC_DEVICE";
        constexpr static const char* data_register = "REGISTER_LIGHTSTRIP_AUDIOSYNC_DEVICE";

    public:
        struct QueuedData {
            public:
                std::shared_ptr<uint8_t[]> data = nullptr;
                size_t data_size                = 0;

                enum class destination_t : uint8_t {
                    broadcast = 0,
                    device
                } destination = destination_t::broadcast;
        };

    private:
        WSADATA wsa_data = {};
        SOCKET socket    = INVALID_SOCKET;

        std::atomic<bool> listen_thread_is_running          = false;
        std::unique_ptr<std::thread> listen_thread_instance = nullptr;

        std::atomic<bool> send_thread_is_running          = false;
        std::atomic<bool> send_thread_has_queued_data     = false;
        std::unique_ptr<std::thread> send_thread_instance = nullptr;

        std::atomic<bool> discover_thread_is_running          = false;
        std::unique_ptr<std::thread> discover_thread_instance = nullptr;

        std::mutex thread_mutex;
        std::queue<QueuedData> send_queue     = {};
        std::vector<sockaddr_in> destinations = {};

        bool send(const uint8_t* data, size_t data_size, sockaddr_in& address);

    public:
        DataSender(void) = default;
        ~DataSender(void);

        int initialize(void);
        int initialize_device(const char* destination_ip);

        static void listen_thread(DataSender* data_sender);
        static void send_thread(DataSender* data_sender);
        static void discover_thread(DataSender* data_sender);

        void enqueue(const uint8_t* data, size_t data_size, const QueuedData::destination_t destination);
};
