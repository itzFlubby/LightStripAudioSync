#include "DataSender.hpp"

#include <algorithm>
#include <stdio.h>
#include <string>
#include <Ws2tcpip.h>

#define _WINSOCK_DEPRECATED_NO_WARNINGS

DataSender::~DataSender(void) {
    int result;

    if (this->listen_thread_instance) {
        this->listen_thread_is_running = false;
        this->listen_thread_instance->join();
    }

    if (this->send_thread_instance) {
        this->send_thread_is_running = false;
        this->send_thread_instance->join();
    }

    if (this->discover_thread_instance) {
        this->discover_thread_is_running = false;
        this->discover_thread_instance->join();
    }

    if ((result = closesocket(this->socket)) == SOCKET_ERROR) { printf("[CRIT] Closing the socket failed with error code %d!\n", WSAGetLastError()); }

    WSACleanup();
}

int DataSender::initialize(void) {
    int result = 0;

    // Initialize WinSock
    if ((result = WSAStartup(MAKEWORD(2, 2), &this->wsa_data)) != NO_ERROR) {
        printf("[CRIT] WSAStartup failed with error code %d!\n", result);
        return result;
    }

    // Create the socket
    if ((this->socket = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == INVALID_SOCKET) {
        printf("[CRIT] Opening the socket failed with error code %ld!\n", WSAGetLastError());
        return 1;
    }

    // Allow broadcasts by the socket
    uint8_t broadcast = 1;
    if (setsockopt(this->socket, SOL_SOCKET, SO_BROADCAST, reinterpret_cast<char*>(&broadcast), sizeof(broadcast)) == SOCKET_ERROR) {
        printf("[CRIT] Enabling broadcast failed with error code %ld!\n", WSAGetLastError());
        return 1;
    }

    // Bind socket to allow listening on the network
    sockaddr_in local_addr     = {};
    local_addr.sin_family      = AF_INET;
    local_addr.sin_port        = htons(PORT);
    local_addr.sin_addr.s_addr = INADDR_ANY;
    if (bind(this->socket, reinterpret_cast<SOCKADDR*>(&local_addr), sizeof(local_addr)) == SOCKET_ERROR) {
        printf("[CRIT] Binding the socket failed with error code %d!\n", WSAGetLastError());
        return 1;
    }

    // Initialize broadcast address
    if (this->initialize_device("255.255.255.255") != 0) {
        printf("[CRIT] Starting broadcast failed!\n");
        return 1;
    }
    // Create and start the listen thread
    this->listen_thread_is_running = true;
    this->listen_thread_instance   = std::make_unique<std::thread>(std::thread(&listen_thread, this));

    // Create and start the send thread
    this->send_thread_is_running = true;
    this->send_thread_instance   = std::make_unique<std::thread>(std::thread(&send_thread, this));

    // Create and start the discover thread
    this->discover_thread_is_running = true;
    this->discover_thread_instance   = std::make_unique<std::thread>(std::thread(&discover_thread, this));

    return result;
}

int DataSender::initialize_device(const char* destination_ip) {
    sockaddr_in destination = {};
    destination.sin_family  = AF_INET;
    destination.sin_port    = htons(PORT);

    // Check if destination_ip is valid
    if (InetPton(AF_INET, destination_ip, &destination.sin_addr.s_addr) == 1) {
        this->destinations.push_back(destination);
    } else {
        printf("[CRIT] Invalid destination IP address %s!\n", destination_ip);
        return 1;
    }
    return 0;
}

void DataSender::listen_thread(DataSender* data_sender) {
    char buffer[64]         = {};
    sockaddr_in sender_addr = {};
    int sender_addr_size    = sizeof(sender_addr);

    while (data_sender->listen_thread_is_running) {
        // Wait for data
        int bytes_received = recvfrom(data_sender->socket, buffer, sizeof(buffer) - 1, 0, reinterpret_cast<SOCKADDR*>(&sender_addr), &sender_addr_size);
        if (bytes_received == SOCKET_ERROR) {
            printf("[CRIT] Receiving data failed with error code %d!\n", WSAGetLastError());
            continue;
        }

        Packet packet(buffer, bytes_received);

        if (packet.is_valid()) {
            if (packet.is_register()) {
                std::scoped_lock lock(data_sender->destination_mutex);

                // Check if destination is already registered. Add to vector if not.
                if (!std::any_of(data_sender->destinations.begin(), data_sender->destinations.end(), [&](const auto& dest) {
                        return dest.sin_addr.s_addr == sender_addr.sin_addr.s_addr;
                    })) {
                    char sender_ip[INET_ADDRSTRLEN] = {};
                    InetNtop(AF_INET, &sender_addr.sin_addr, sender_ip, INET_ADDRSTRLEN);
                    data_sender->initialize_device(sender_ip);
                    printf("[++++] Registered sync device:\n\tIP: %s\n", sender_ip);
                }
            }
        }
    }
}

void DataSender::send_thread(DataSender* data_sender) {
    while (data_sender->send_thread_is_running) {
        data_sender->send_thread_has_queued_data.wait(false);

        {
            std::scoped_lock lock(data_sender->send_queue_mutex);

            for (; !data_sender->send_queue.empty(); data_sender->send_queue.pop()) {
                data_sender->send(data_sender->send_queue.front());
            }
        }

        data_sender->send_thread_has_queued_data = false;
    }
}

void DataSender::discover_thread(DataSender* data_sender) {
    while (data_sender->discover_thread_is_running) {
        data_sender->enqueue(Packet(Packet::destination_t::broadcast, Packet::type_t::discover, 0, 0));
        std::this_thread::sleep_for(std::chrono::seconds(5));
    }
}

void DataSender::enqueue(const Packet& packet) {
    std::scoped_lock lock(this->send_queue_mutex);

    if (this->send_queue.size() < 10) {
        this->send_queue.push(packet);
        this->send_thread_has_queued_data = true;
        this->send_thread_has_queued_data.notify_all();
    }
}

bool DataSender::send(const Packet& packet) {
    std::scoped_lock lock(this->destination_mutex);

    // Check if zero data packets are sent repeatedly. If so, skip after RESEND_ZERO_PACKET_COUNT to free up network bandwidth.
    if (!packet.is_zero() || (this->zero_packet_count++ < RESEND_ZERO_PACKET_COUNT)) {
        std::vector<uint8_t> raw = packet.to_raw();
        switch (packet.get_destination()) {
            case Packet::destination_t::broadcast: {
                if (!this->send_raw(reinterpret_cast<const sockaddr*>(&this->destinations[0]), raw)) { // First address is broadcast
                    return false;
                }
                break;
            }
            case Packet::destination_t::device: {
                for (size_t destination_index = 1; destination_index < this->destinations.size(); ++destination_index) {
                    if (!this->send_raw(reinterpret_cast<const sockaddr*>(&this->destinations[destination_index]), raw)) { return false; }
                }
                break;
            }
        }

        // Only reset when packet is not zero
        if (!packet.is_zero()) { this->zero_packet_count = 0; }
    }
    return true;
}

bool DataSender::send_raw(const sockaddr* address, const std::vector<uint8_t>& packet) {
    if (sendto(this->socket, reinterpret_cast<const char*>(packet.data()), packet.size(), 0, address, sizeof(*address)) == SOCKET_ERROR) {
        printf("[CRIT] Sending to device failed with error code %d!\n", WSAGetLastError());
        return false;
    }
    return true;
}
