#include "DataSender.hpp"
#include "logger.hpp"

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
#define OPEN_PIPE     _popen
#define CLOSE_PIPE    _pclose
#else
#include <errno.h>
#define ERRNO         errno
#define SOCKOPT_DTYPE int
#define OPEN_PIPE     popen
#define CLOSE_PIPE    pclose
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

    // Must happen before tcp_send thread exit to get exit message into python websocket
    if (this->thread_instance.ws_loop) {
        this->state.ws_running = false;
        this->tcp_enqueue(Packet(Packet::destination_t::device, Packet::type_t::exit, nullptr, 0));
        this->thread_instance.ws_loop->join();
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
        log("[CRIT] Closing the socket failed with error code %d!", ERRNO);

#if defined(_WIN32)
        WSACleanup();
#endif
    }
}

int DataSender::initialize(bool init_ws) {
    int result = 0;

#if defined(_WIN32)
    // Initialize WinSock
    if ((result = WSAStartup(MAKEWORD(2, 2), &this->wsa_data)) != NO_ERROR) {
        log("[CRIT] WSAStartup failed with error code %d!", result);
        return result;
    }
#endif

    if (this->udp_initialize() != 0) {
        log("[CRIT] UDP initialization failed!");
        return 1;
    }

    if (this->tcp_initialize() != 0) {
        log("[CRIT] TCP initialization failed!");
        return 1;
    }

    if (init_ws) {
        if (this->ws_initialize() != 0) {
            log("[CRIT] WebSocket initialization failed!");
            return 1;
        }
    }

    return result;
}

int DataSender::udp_initialize(void) {
    int result = 0;

    // Create the socket
    if ((this->udp_socket = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == INVALID_SOCKET) {
        log("[CRIT] Opening the UDP socket failed with error code %ld!", ERRNO);
        return 1;
    }

    // Allow broadcasts by the socket
    bool broadcast = true;
    if (setsockopt(this->udp_socket, SOL_SOCKET, SO_BROADCAST, reinterpret_cast<SOCKOPT_DTYPE*>(&broadcast), sizeof(SOCKOPT_DTYPE)) == SOCKET_ERROR) {
        log("[CRIT] Enabling UDP broadcast failed with error code %ld!", ERRNO);
        return 1;
    }

    // Bind socket to allow listening on the network
    sockaddr_in local_addr     = {};
    local_addr.sin_family      = AF_INET;
    local_addr.sin_port        = htons(UDP_PORT);
    local_addr.sin_addr.s_addr = INADDR_ANY;
    if (bind(this->udp_socket, reinterpret_cast<struct sockaddr*>(&local_addr), sizeof(local_addr)) == SOCKET_ERROR) {
        log("[CRIT] Binding the UDP socket failed with error code %d!", ERRNO);
        return 1;
    }

    // Initialize broadcast address
    if (this->udp_initialize_device("255.255.255.255") != 0) {
        log("[CRIT] Starting UDP broadcast failed!");
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
        log("[CRIT] Invalid destination IP address %s!", destination_ip);
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
            log("[CRIT] Receiving UDP data failed with error code %d!", ERRNO);
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
                    log("[++++] Registered sync device:\n\tIP: %s, Port: %d", sender_ip, sender_addr.sin_port);
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
        log("[CRIT] Sending to UDP device failed with error code %d!", ERRNO);
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
        log("[CRIT] Opening the TCP socket failed with error code %ld!", ERRNO);
        return 1;
    }

    // Bind socket to allow listening on the network
    sockaddr_in local_addr     = {};
    local_addr.sin_family      = AF_INET;
    local_addr.sin_port        = htons(TCP_PORT);
    local_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    if (bind(this->tcp_socket, reinterpret_cast<struct sockaddr*>(&local_addr), sizeof(local_addr)) == SOCKET_ERROR) {
        log("[CRIT] Binding the TCP socket failed with error code %d!", ERRNO);
        return 1;
    }

    // Start listening
    if (listen(this->tcp_socket, SOMAXCONN) == SOCKET_ERROR) {
        log("[CRIT] TCP listen failed with error code %d!", ERRNO);
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
            log("[CRIT] TCP accept failed with error code %d!", error);
            continue;
        }

        // Connection successful
        char client_ip[INET_ADDRSTRLEN] = {};
#if defined(_WIN32)
        InetNtop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
#else
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
#endif
        log("[++++] Registered TCP client:\n\tIP: %s, Port: %d", client_ip, ntohs(client_addr.sin_port));
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

        log("[----] Unregistered TCP client:\n\tIP: %s, Port: %d", client_ip, ntohs(client_addr.sin_port));
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

int DataSender::ws_initialize(void) {
    int result = 0;

    // Create and start the websocket thread
    this->state.ws_running        = true;
    this->thread_instance.ws_loop = std::make_unique<std::thread>(std::thread(&ws_thread, this));

    return 0;
}

void DataSender::ws_thread(DataSender* data_sender) {
#if defined(_WIN32)
    const char* command = "python -u \"websocket.py\" 2>&1";
#else
    const char* command = "python3 -u \"websocket.py\" 2>&1";
#endif
    FILE* pipe = OPEN_PIPE(command, "r");
    log("[++++] Registered WebSocket pipe!");
    if (pipe) {
        char buffer[64] = { 0 };
        while (data_sender->state.ws_running && fgets(buffer, sizeof(buffer), pipe)) {
            if (buffer[0]) { log_ws(buffer); }
            buffer[0] = 0;
        }
    } else {
        log("[CRIT] Failed to open WebSocket pipe!");
    }
    CLOSE_PIPE(pipe);
    log("[----] Unregistered WebSocket pipe!");
    data_sender->state.ws_running = false;
}
