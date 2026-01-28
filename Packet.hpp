#pragma once

#include <cstring>
#include <stdint.h>
#include <vector>

/* THE PACKET CLASS MUST CONSIST OF A SINGLE HEADER FILE FOR COMPATIBILITY WITH ESPHOME! */

class Packet {
    private:
        constexpr static unsigned PACKET_LENGTH_OVERHEAD = 4;
        constexpr static uint8_t STX                     = 0x02;
        constexpr static uint8_t ETX                     = 0x03;

    public:
        enum class type_t : uint8_t {
            discover_device = 0x00,
            register_device,
            data,
            undefined
        };

        enum class destination_t : uint8_t {
            broadcast = 0,
            device,
            undefined
        };

    private:
        destination_t destination    = destination_t::undefined;
        type_t type                  = type_t::undefined;
        std::vector<uint8_t> payload = {};

    public:
        Packet(const destination_t destination, const type_t type, const uint8_t* payload, const size_t payload_size) : type(type), destination(destination) {
            this->payload.resize(payload_size);
            memcpy(this->payload.data(), payload, payload_size);
        }

        Packet(const char* packet, const size_t packet_size) {
            // If packet is smaller than the overhead it can't be any valid data
            if (packet_size >= PACKET_LENGTH_OVERHEAD) {
                // Check for STX and ETX
                if ((packet[0] == STX) && (packet[packet_size - 1] == ETX)) {
                    // Check if packet type is valid
                    if (packet[1] < static_cast<uint8_t>(type_t::undefined)) {
                        // Check if payload size is valid
                        if (packet[2] == (packet_size - PACKET_LENGTH_OVERHEAD)) {
                            // Packet is valid, copy into object
                            this->type = static_cast<type_t>(packet[1]);
                            this->payload.resize(packet[2]);
                            memcpy(this->payload.data(), packet + PACKET_LENGTH_OVERHEAD - 1, packet[2]);
                        }
                    }
                }
            }
        }

        ~Packet(void) = default;

        destination_t get_destination(void) const { return this->destination; }

        const uint8_t* get_payload(void) const { return this->payload.data(); }

        size_t get_payload_size(void) const { return this->payload.size(); }

        bool is_valid(void) const { return (this->type != type_t::undefined); }

        bool is_discover_device(void) const {
            if ((this->type == type_t::discover_device) && (this->payload.size() == 0)) { return true; }
            return false;
        }

        bool is_register_device(void) const {
            if ((this->type == type_t::register_device) && (this->payload.size() == 0)) { return true; }
            return false;
        }

        bool is_data(void) const {
            if (this->type == type_t::data) { return true; }
            return false;
        }

        bool is_zero(void) const {
            if (!this->is_data()) { return false; }
            for (size_t index = 0; index < this->payload.size(); ++index) {
                if (this->payload[index] != 0x00) { return false; }
            }
            return true;
        }

        std::vector<uint8_t> to_raw(void) const {
            std::vector<uint8_t> packet(this->payload.size() + PACKET_LENGTH_OVERHEAD);
            packet[0] = STX;
            packet[1] = static_cast<uint8_t>(this->type);
            packet[2] = this->payload.size();
            memcpy(packet.data() + 3, this->payload.data(), this->payload.size());
            packet.back() = ETX;
            return packet;
        }

        const char* c_str(void) const {
            std::vector<uint8_t> packet = this->to_raw();
            packet.push_back(0x00);
            return reinterpret_cast<const char*>(packet.data());
        }
};
