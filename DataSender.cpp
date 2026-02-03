#include "DataSender.hpp"

#include <algorithm>
#include <iomanip>
#include <sstream>
#include <stdio.h>
#include <string>

#if defined(_WIN32)
#include <Ws2tcpip.h>
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define ERRNO         WSAGetLastError()
#define SOCKOPT_DTYPE char
#else
#include <errno.h>
#define ERRNO         errno
#define SOCKOPT_DTYPE int
#endif

DataSender::~DataSender(void) {
    int result;

    if (this->thread_instance.udp_listen) {
        this->state.udp_listen_running = false;
        this->thread_instance.udp_listen->join();
    }

    if (this->thread_instance.udp_send) {
        this->state.udp_send_running = false;
        this->thread_instance.udp_send->join();
    }

    if (this->thread_instance.udp_discover) {
        this->state.udp_discover_running = false;
        this->thread_instance.udp_discover->join();
    }

    if (this->thread_instance.tcp_send) {
        this->state.tcp_send_running = false;
        this->thread_instance.tcp_send->join();
    }

#if defined(_WIN32)
    if ((result = closesocket(this->udp_socket)) == SOCKET_ERROR) {
#else
    if ((result = close(this->udp_socket)) == SOCKET_ERROR) {
#endif
        printf("[CRIT] Closing the socket failed with error code %d!\n", ERRNO);

#if defined(_WIN32)
        WSACleanup();
#endif
    }
}

int DataSender::initialize(void) {
    int result = 0;

#if defined(_WIN32)
    // Initialize WinSock
    if ((result = WSAStartup(MAKEWORD(2, 2), &this->wsa_data)) != NO_ERROR) {
        printf("[CRIT] WSAStartup failed with error code %d!\n", result);
        return result;
    }
#endif

    if (this->udp_initialize() != 0) {
        printf("[CRIT] UDP initialization failed!\n");
        return 1;
    }

    if (this->tcp_initialize() != 0) {
        printf("[CRIT] TCP initialization failed!\n");
        return 1;
    }

    return result;
}

int DataSender::udp_initialize(void) {
    int result = 0;

    // Create the socket
    if ((this->udp_socket = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == INVALID_SOCKET) {
        printf("[CRIT] Opening the UDP socket failed with error code %ld!\n", ERRNO);
        return 1;
    }

    // Allow broadcasts by the socket
    bool broadcast = true;
    if (setsockopt(this->udp_socket, SOL_SOCKET, SO_BROADCAST, reinterpret_cast<SOCKOPT_DTYPE*>(&broadcast), sizeof(SOCKOPT_DTYPE)) == SOCKET_ERROR) {
        printf("[CRIT] Enabling UDP broadcast failed with error code %ld!\n", ERRNO);
        return 1;
    }

    // Bind socket to allow listening on the network
    sockaddr_in local_addr     = {};
    local_addr.sin_family      = AF_INET;
    local_addr.sin_port        = htons(UDP_PORT);
    local_addr.sin_addr.s_addr = INADDR_ANY;
    if (bind(this->udp_socket, reinterpret_cast<struct sockaddr*>(&local_addr), sizeof(local_addr)) == SOCKET_ERROR) {
        printf("[CRIT] Binding the UDP socket failed with error code %d!\n", ERRNO);
        return 1;
    }

    // Initialize broadcast address
    if (this->udp_initialize_device("255.255.255.255") != 0) {
        printf("[CRIT] Starting UDP broadcast failed!\n");
        return 1;
    }

    // Create and start the listen thread
    this->state.udp_listen_running   = true;
    this->thread_instance.udp_listen = std::make_unique<std::thread>(std::thread(&udp_listen_thread, this));

    // Create and start the udp_send thread
    this->state.udp_send_running   = true;
    this->thread_instance.udp_send = std::make_unique<std::thread>(std::thread(&udp_send_thread, this));

    // Create and start the discover thread
    this->state.udp_discover_running   = true;
    this->thread_instance.udp_discover = std::make_unique<std::thread>(std::thread(&udp_discover_thread, this));

    return 0;
}

int DataSender::udp_initialize_device(const char* destination_ip) {
    sockaddr_in destination = {};
    destination.sin_family  = AF_INET;
    destination.sin_port    = htons(UDP_PORT);

    // Check if destination_ip is valid
#if defined(_WIN32)
    if (InetPton(AF_INET, destination_ip, &destination.sin_addr.s_addr) == 1) {
#else
    if (inet_pton(AF_INET, destination_ip, &destination.sin_addr) == 1) {
#endif
        this->udp_destinations.push_back(destination);
        this->udp_zero_packet_count = 0; // Force resend when new client connects
    } else {
        printf("[CRIT] Invalid destination IP address %s!\n", destination_ip);
        return 1;
    }
    return 0;
}

void DataSender::udp_listen_thread(DataSender* data_sender) {
    static char buffer[64]     = {};
    sockaddr_in sender_addr    = {};
    socklen_t sender_addr_size = sizeof(sender_addr);

    while (data_sender->state.udp_listen_running) {
        // Wait for data
        int bytes_received = recvfrom(data_sender->udp_socket, buffer, sizeof(buffer) - 1, 0, reinterpret_cast<struct sockaddr*>(&sender_addr), &sender_addr_size);
        if (bytes_received == SOCKET_ERROR) {
            printf("[CRIT] Receiving UDP data failed with error code %d!\n", ERRNO);
            continue;
        }

        Packet packet(buffer, bytes_received);

        if (packet.is_valid()) {
            if (packet.is_register_device()) {
                std::scoped_lock lock(data_sender->udp_listen_mutex);

                // Check if destination is already registered. Add to vector if not.
                if (!std::any_of(data_sender->udp_destinations.begin(), data_sender->udp_destinations.end(), [&](const auto& dest) {
                        return dest.sin_addr.s_addr == sender_addr.sin_addr.s_addr;
                    })) {
                    char sender_ip[INET_ADDRSTRLEN] = {};
#if defined(_WIN32)
                    InetNtop(AF_INET, &sender_addr.sin_addr, sender_ip, INET_ADDRSTRLEN);
#else
                    inet_ntop(AF_INET, &sender_addr.sin_addr, sender_ip, INET_ADDRSTRLEN);
#endif
                    data_sender->udp_initialize_device(sender_ip);
                    printf("[++++] Registered sync device:\n\tIP: %s, Port: %d\n", sender_ip, sender_addr.sin_port);
                }
            }
        }
    }
}

void DataSender::udp_send_thread(DataSender* data_sender) {
    while (data_sender->state.udp_send_running) {
        data_sender->state.udp_send_queued.wait(false);

        {
            std::scoped_lock lock(data_sender->udp_send_mutex);

            for (; !data_sender->udp_send_queue.empty(); data_sender->udp_send_queue.pop()) {
                data_sender->udp_send(data_sender->udp_send_queue.front());
            }
        }

        data_sender->state.udp_send_queued = false;
    }
}

void DataSender::udp_discover_thread(DataSender* data_sender) {
    while (data_sender->state.udp_discover_running) {
        data_sender->udp_enqueue(Packet(Packet::destination_t::broadcast, Packet::type_t::discover_device, 0, 0));
        std::this_thread::sleep_for(std::chrono::seconds(5));
    }
}

bool DataSender::udp_send(const Packet& packet) {
    std::scoped_lock lock(this->udp_listen_mutex);

    // Check if zero data packets are sent repeatedly. If so, skip after RESEND_ZERO_PACKET_COUNT to free up network bandwidth.
    if (!packet.is_zero() || (this->udp_zero_packet_count++ < RESEND_ZERO_PACKET_COUNT)) {
        std::vector<uint8_t> raw = packet.to_raw();
        switch (packet.get_destination()) {
            case Packet::destination_t::broadcast: {
                if (!this->udp_send_raw(reinterpret_cast<const sockaddr*>(&this->udp_destinations[0]), raw)) { // First address is broadcast
                    return false;
                }
                break;
            }
            case Packet::destination_t::device: {
                for (size_t destination_index = 1; destination_index < this->udp_destinations.size(); ++destination_index) {
                    if (!this->udp_send_raw(reinterpret_cast<const sockaddr*>(&this->udp_destinations[destination_index]), raw)) { return false; }
                }
                break;
            }
        }

        // Only reset when packet is not zero
        if (!packet.is_zero()) { this->udp_zero_packet_count = 0; }
    }
    return true;
}

bool DataSender::udp_send_raw(const sockaddr* address, const std::vector<uint8_t>& packet) {
    if (sendto(this->udp_socket, reinterpret_cast<const char*>(packet.data()), packet.size(), 0, address, sizeof(*address)) == SOCKET_ERROR) {
        printf("[CRIT] Sending to UDP device failed with error code %d!\n", ERRNO);
        return false;
    }
    return true;
}

void DataSender::udp_enqueue(const Packet& packet) {
    std::scoped_lock lock(this->udp_send_mutex);

    if (this->udp_send_queue.size() < 10) {
        this->udp_send_queue.push(packet);
        this->state.udp_send_queued = true;
        this->state.udp_send_queued.notify_all();
    }
}

int DataSender::tcp_initialize(void) {
    int result = 0;

    // Create the socket
    if ((this->tcp_socket = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == INVALID_SOCKET) {
        printf("[CRIT] Opening the TCP socket failed with error code %ld!\n", ERRNO);
        return 1;
    }

    // Bind socket to allow listening on the network
    sockaddr_in local_addr     = {};
    local_addr.sin_family      = AF_INET;
    local_addr.sin_port        = htons(TCP_PORT);
    local_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    if (bind(this->tcp_socket, reinterpret_cast<struct sockaddr*>(&local_addr), sizeof(local_addr)) == SOCKET_ERROR) {
        printf("[CRIT] Binding the TCP socket failed with error code %d!\n", ERRNO);
        return 1;
    }

    // Start listening
    if (listen(this->tcp_socket, SOMAXCONN) == SOCKET_ERROR) {
        printf("[CRIT] TCP listen failed with error code %d!\n", ERRNO);
        return 1;
    }

    // Create and start the tcp thread
    this->state.tcp_send_running   = true;
    this->thread_instance.tcp_send = std::make_unique<std::thread>(std::thread(&tcp_send_thread, this));

    return 0;
}

void DataSender::tcp_send_thread(DataSender* data_sender) {
    while (data_sender->state.tcp_send_running) {
        // Wait for connection
        sockaddr_in client_addr    = {};
        socklen_t client_addr_size = sizeof(client_addr);
        SOCKET client              = accept(data_sender->tcp_socket, reinterpret_cast<struct sockaddr*>(&client_addr), &client_addr_size);

        // Check if connection successful
        if (client == INVALID_SOCKET) {
            int error = ERRNO;
#if defined(_WIN32)
            if (error == WSAEWOULDBLOCK) {
#else
            if (error == EAGAIN || error == EWOULDBLOCK) {
#endif
                std::this_thread::sleep_for(std::chrono::milliseconds(100)); // No connection available
                continue;
            }
            printf("[CRIT] TCP accept failed with error code %d!\n", error);
            continue;
        }

        // Connection successful
        char client_ip[INET_ADDRSTRLEN] = {};
#if defined(_WIN32)
        InetNtop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
#else
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
#endif
        printf("[++++] Registered TCP client:\n\tIP: %s, Port: %d\n", client_ip, ntohs(client_addr.sin_port));
        data_sender->tcp_zero_packet_count = 0; // Force resend when new client connects

        bool success = true;
        while (success) {
            data_sender->state.tcp_send_queued.wait(false);

            {
                std::scoped_lock lock(data_sender->tcp_send_mutex);

                for (; !data_sender->tcp_send_queue.empty(); data_sender->tcp_send_queue.pop()) {
                    success = data_sender->tcp_send(client, data_sender->tcp_send_queue.front());
                }
            }

            data_sender->state.tcp_send_queued = false;
        }

        printf("[----] Unregistered TCP client:\n\tIP: %s, Port: %d\n", client_ip, ntohs(client_addr.sin_port));
    }
}

bool DataSender::tcp_send(const SOCKET client, const Packet& packet) {
    // Check if zero data packets are sent repeatedly. If so, skip after RESEND_ZERO_PACKET_COUNT to free up network bandwidth.
    if (!packet.is_zero() || (this->tcp_zero_packet_count++ < RESEND_ZERO_PACKET_COUNT)) {
        std::vector<uint8_t> raw = packet.to_raw();
        if (send(client, reinterpret_cast<const char*>(raw.data()), raw.size(), 0) != raw.size()) { return false; }

        // Only reset when packet is not zero
        if (!packet.is_zero()) { this->tcp_zero_packet_count = 0; }
    }
    return true;
}

void DataSender::tcp_enqueue(const Packet& packet) {
    std::scoped_lock lock(this->tcp_send_mutex);

    if (this->tcp_send_queue.size() < 10) {
        this->tcp_send_queue.push(packet);
        this->state.tcp_send_queued = true;
        this->state.tcp_send_queued.notify_all();
    }
}
