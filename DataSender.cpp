#include "DataSender.hpp"

#include <string>

#define _WINSOCK_DEPRECATED_NO_WARNINGS

DataSender::DataSender(void) {
    this->socket = INVALID_SOCKET;

    memset(this->last_magnitudes, 0x00, DataSender::magnitudes_size);
    this->identical_magnitudes_count = 0;
}

DataSender::~DataSender() {
    int result;

    this->discover_thread_running = false;
    this->discover_thread_instance->join();
    delete this->discover_thread_instance;

    this->listen_thread_running = false;
    this->listen_thread_instance->join();
    delete this->listen_thread_instance;

    if ((result = closesocket(this->socket)) == SOCKET_ERROR) { printf("[CRIT] Closing the socket failed with error code %d!\n", WSAGetLastError()); }

    WSACleanup();
}

int DataSender::initialize() {
    int result = 0;

    if ((result = WSAStartup(MAKEWORD(2, 2), &this->wsa_data)) != NO_ERROR) {
        printf("[CRIT] WSAStartup failed with error code %d!\n", result);
        return result;
    }

    if ((this->socket = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == INVALID_SOCKET) {
        printf("[CRIT] Opening the socket failed with error code %ld!\n", WSAGetLastError());
        WSACleanup();
        return 1;
    }

    uint8_t broadcast = 1;
    if (setsockopt(this->socket, SOL_SOCKET, SO_BROADCAST, reinterpret_cast<char*>(&broadcast), sizeof(broadcast)) == SOCKET_ERROR) {
        printf("[CRIT] Enabling broadcast failed with error code %ld!\n", WSAGetLastError());
        closesocket(this->socket);
        WSACleanup();
        return 1;
    }

    sockaddr_in local_addr     = {};
    local_addr.sin_family      = AF_INET;
    local_addr.sin_port        = htons(DataSender::port);
    local_addr.sin_addr.s_addr = INADDR_ANY;
    if (bind(this->socket, reinterpret_cast<SOCKADDR*>(&local_addr), sizeof(local_addr)) == SOCKET_ERROR) {
        printf("[CRIT] Binding the socket failed with error code %d!\n", WSAGetLastError());
        closesocket(this->socket);
        WSACleanup();
        return 1;
    }

    if (this->initialize_device("255.255.255.255") != 0) {
        printf("[CRIT] Starting broadcast failed!\n");
        return 1;
    }

    discover_thread_running        = true;
    this->discover_thread_instance = new std::thread(&DataSender::discover_thread, this, &this->discover_thread_running);

    listen_thread_running        = true;
    this->listen_thread_instance = new std::thread(&DataSender::listen_thread, this, &this->listen_thread_running);

    return result;
}

int DataSender::initialize_device(PCSTR destination_ip) {
    sockaddr_in destination = {};
    destination.sin_family  = AF_INET;
    destination.sin_port    = htons(DataSender::port);
    if (InetPton(AF_INET, destination_ip, &destination.sin_addr.s_addr) != 1) {
        printf("[CRIT] Invalid destination IP address %s!\n", destination_ip);
        closesocket(this->socket);
        WSACleanup();
        return 1;
    }
    this->destinations.push_back(destination);
    return 0;
}

void DataSender::discover_thread(DataSender* data_sender, std::atomic<bool>* running) {
    while (running) {
        data_sender->send_discover();
        std::this_thread::sleep_for(std::chrono::seconds(5));
    }
}

void DataSender::listen_thread(DataSender* data_sender, std::atomic<bool>* running) {
    char buffer[64]         = {};
    sockaddr_in sender_addr = {};
    int sender_addr_size    = sizeof(sender_addr);

    while (running) {
        int bytes_received = recvfrom(data_sender->socket, buffer, sizeof(buffer) - 1, 0, reinterpret_cast<SOCKADDR*>(&sender_addr), &sender_addr_size);
        if (bytes_received == SOCKET_ERROR) {
            printf("[CRIT] Receiving data failed with error code %d!\n", WSAGetLastError());
            continue;
        }

        if (memcmp(buffer, DataSender::register_message, strlen(DataSender::register_message)) == 0) {
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
                printf("[++++] Registered sync device:\n\tIP: %s\n", sender_ip); // Always print newline first to avoid overwriting user input
                data_sender->initialize_device(sender_ip);
            }
        }
    }
}

int DataSender::send_discover(void) {
    sockaddr_in broadcast_destination = this->destinations[0]; // First destination is the broadcast
    if (sendto(this->socket, DataSender::discover_message, strlen(DataSender::discover_message), 0, reinterpret_cast<SOCKADDR*>(&broadcast_destination), sizeof(broadcast_destination))
        == SOCKET_ERROR) {
        printf("[CRIT] Sending discover message failed with error code %d!\n", WSAGetLastError());
        closesocket(this->socket);
        WSACleanup();
        return 1;
    }
    return 0;
}

int DataSender::send_magnitudes(uint8_t left, uint8_t right) {
    uint8_t magnitudes[DataSender::magnitudes_size] = { left, right };

    if (memcmp(magnitudes, this->last_magnitudes, DataSender::magnitudes_size) == 0) {
        this->identical_magnitudes_count++;
    } else {
        memcpy(this->last_magnitudes, magnitudes, DataSender::magnitudes_size);
        this->identical_magnitudes_count = 0;
    }

    int result = 0;
    if (this->identical_magnitudes_count < 5) {
        for (size_t destination_index = 1; destination_index < this->destinations.size(); ++destination_index) {
            if ((result = sendto(
                     this->socket, reinterpret_cast<const char*>(magnitudes), DataSender::magnitudes_size, 0, reinterpret_cast<SOCKADDR*>(&this->destinations[destination_index]), sizeof(this->destinations[destination_index])
                 ))
                == SOCKET_ERROR) {
                printf("[CRIT] Sending to device failed with error code %d!\n", WSAGetLastError());
                closesocket(this->socket);
                WSACleanup();
                break;
            }
        }
    }
    return result;
}
