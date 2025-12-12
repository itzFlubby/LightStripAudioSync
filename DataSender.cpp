#include "DataSender.hpp"

#define _WINSOCK_DEPRECATED_NO_WARNINGS

DataSender::DataSender(PCSTR destinationIp, unsigned short destinationPort) {
    this->sock            = INVALID_SOCKET;
    this->destinationIp   = destinationIp;
    this->destinationPort = destinationPort;

    memset(this->last_message, 0x00, DataSender::message_size);
    this->identical_message_count = 0;
}

DataSender::~DataSender() {
    int result;

    if ((result = closesocket(this->sock)) == SOCKET_ERROR) { printf("[FAIL] Closing the socket failed with error code %d!\n", WSAGetLastError()); }

    WSACleanup();
}

int DataSender::initialize() {
    int result = 0;

    if ((result = WSAStartup(MAKEWORD(2, 2), &wsaData)) != NO_ERROR) {
        printf("[FAIL] WSAStartup failed with error code %d!\n", result);
        return result;
    }

    if ((this->sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == INVALID_SOCKET) {
        printf("[FAIL] Opening the socket failed with error code %ld!\n", WSAGetLastError());
        WSACleanup();
        return 1;
    }

    destination.sin_family = AF_INET;
    destination.sin_port   = htons(this->destinationPort);
    if (InetPton(AF_INET, this->destinationIp, &destination.sin_addr.s_addr) != 1) {
        printf("[FAIL] Invalid destination IP address!\n");
        closesocket(this->sock);
        WSACleanup();
        return 1;
    }

    return result;
}

int DataSender::sendMessage(uint8_t* message) {
    if (memcmp(message, this->last_message, DataSender::message_size) == 0) {
        this->identical_message_count++;
    } else {
        memcpy(this->last_message, message, DataSender::message_size);
        this->identical_message_count = 0;
    }

    int result = 0;
    if (this->identical_message_count < 5) {
        if ((result = sendto(this->sock, reinterpret_cast<const char*>(message), DataSender::message_size, 0, reinterpret_cast<SOCKADDR*>(&this->destination), sizeof(this->destination)))
            == SOCKET_ERROR) {
            printf("[FAIL] Sending to device failed with error code %d!\n", WSAGetLastError());
            closesocket(this->sock);
            WSACleanup();
            return 1;
        }
    }
    return result;
}
