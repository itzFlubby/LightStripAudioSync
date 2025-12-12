#pragma once

#include <stdint.h>
#include <stdio.h>
#include <winsock2.h>
#include <Ws2tcpip.h>

// Link with ws2_32.lib
#pragma comment(lib, "Ws2_32.lib")

class DataSender {
    private:
        WSADATA wsaData                    = { 0 };
        SOCKET sock                        = { 0 };
        PCSTR destinationIp                = { 0 };
        unsigned short destinationPort     = 0;
        sockaddr_in destination            = { 0 };
        static const uint8_t message_size  = 2;
        uint8_t last_message[message_size] = { 0 };
        unsigned identical_message_count   = 0;

    public:
        DataSender(PCSTR destinationIp, unsigned short destinationPort);
        ~DataSender();

        int initialize();

        int sendMessage(uint8_t* message);
};
