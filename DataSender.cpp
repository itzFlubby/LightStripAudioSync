#include "DataSender.hpp"

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
    local_addr.sin_port        = htons(DataSender::port);
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

    this->listen_thread_is_running = true;
    this->listen_thread_instance   = std::make_unique<std::thread>(std::thread(&DataSender::listen_thread, this));

    this->send_thread_is_running = true;
    this->send_thread_instance   = std::make_unique<std::thread>(std::thread(&DataSender::send_thread, this));

    this->discover_thread_is_running = true;
    this->discover_thread_instance   = std::make_unique<std::thread>(std::thread(&DataSender::discover_thread, this));

    return result;
}

int DataSender::initialize_device(const char* destination_ip) {
    sockaddr_in destination = {};
    destination.sin_family  = AF_INET;
    destination.sin_port    = htons(DataSender::port);
    if (InetPton(AF_INET, destination_ip, &destination.sin_addr.s_addr) != 1) {
        printf("[CRIT] Invalid destination IP address %s!\n", destination_ip);
        return 1;
    }

    {
        std::scoped_lock lock(this->thread_mutex);
        this->destinations.push_back(destination);
    }
    return 0;
}

void DataSender::listen_thread(DataSender* data_sender) {
    char buffer[64]         = {};
    sockaddr_in sender_addr = {};
    int sender_addr_size    = sizeof(sender_addr);

    while (data_sender->listen_thread_is_running) {
        int bytes_received = recvfrom(data_sender->socket, buffer, sizeof(buffer) - 1, 0, reinterpret_cast<SOCKADDR*>(&sender_addr), &sender_addr_size);
        if (bytes_received == SOCKET_ERROR) {
            printf("[CRIT] Receiving data failed with error code %d!\n", WSAGetLastError());
            continue;
        }

        if (memcmp(buffer, DataSender::data_register, strlen(DataSender::data_register)) == 0) {
            bool registered = false;
            for (const auto& dest : data_sender->destinations) {
                if (dest.sin_addr.s_addr == sender_addr.sin_addr.s_addr) {
                    registered = true;
                    break;
                }
            }
            if (!registered) {
                char sender_ip[INET_ADDRSTRLEN] = {};
                InetNtop(AF_INET, &sender_addr.sin_addr, sender_ip, INET_ADDRSTRLEN);
                printf("[++++] Registered sync device:\n\tIP: %s\n", sender_ip);
                data_sender->initialize_device(sender_ip);
            }
        }
    }
}

void DataSender::send_thread(DataSender* data_sender) {
    while (data_sender->send_thread_is_running) {
        data_sender->send_thread_has_queued_data.wait(false);

        {
            std::scoped_lock lock(data_sender->thread_mutex);
            for (; !data_sender->send_queue.empty(); data_sender->send_queue.pop()) {
                QueuedData& queued_data = data_sender->send_queue.front();
                switch (queued_data.destination) {
                    case QueuedData::destination_t::broadcast: {
                        if (!data_sender->send(queued_data.data.get(), queued_data.data_size, data_sender->destinations[0])) { // First address is broadcast
                            continue;
                        }
                        break;
                    }
                    case QueuedData::destination_t::device: {
                        for (size_t destination_index = 1; destination_index < data_sender->destinations.size(); ++destination_index) {
                            if (!data_sender->send(queued_data.data.get(), queued_data.data_size, data_sender->destinations[destination_index])) { continue; };
                        }
                        break;
                    }
                }
            }
        }

        data_sender->send_thread_has_queued_data = false;
    }
}

void DataSender::discover_thread(DataSender* data_sender) {
    while (data_sender->discover_thread_is_running) {
        data_sender->enqueue(reinterpret_cast<const uint8_t*>(DataSender::data_discover), strlen(DataSender::data_discover), QueuedData::destination_t::broadcast);
        std::this_thread::sleep_for(std::chrono::seconds(5));
    }
}

bool DataSender::send(const uint8_t* data, size_t data_size, sockaddr_in& address) {
    if (sendto(this->socket, reinterpret_cast<const char*>(data), data_size, 0, reinterpret_cast<SOCKADDR*>(&address), sizeof(address)) == SOCKET_ERROR) {
        printf("[CRIT] Sending to device failed with error code %d!\n", WSAGetLastError());
        return false;
    }
    return true;
}

void DataSender::enqueue(const uint8_t* data, size_t data_size, const QueuedData::destination_t destination) {
    {
        std::scoped_lock lock(this->thread_mutex);
        if (this->send_queue.size() < 10) {
            QueuedData queued_data = { .data = std::make_shared<uint8_t[]>(data_size), .data_size = data_size, .destination = destination };
            memcpy(queued_data.data.get(), data, data_size);
            this->send_queue.push(queued_data);
        }
    }
    this->send_thread_has_queued_data = true;
    this->send_thread_has_queued_data.notify_all();
}
