#pragma once

#include "AudioCapture.hpp"

#include <algorithm>
#include <stdio.h>
#include <Windows.h>

class Visualizer {
    private:
        unsigned console_width  = 0;
        unsigned console_height = 0;

    public:
        Visualizer() {
            CONSOLE_SCREEN_BUFFER_INFO csbi;
            GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi);
            this->console_width  = csbi.dwSize.X;
            this->console_height = csbi.dwSize.Y;
        }

        ~Visualizer() = default;

        void set_cursor_position(unsigned x, unsigned y) {
            HANDLE handle = GetStdHandle(STD_OUTPUT_HANDLE);
            CONSOLE_SCREEN_BUFFER_INFO info;
            COORD pos = { 0, 0 };
            DWORD dw;

            std::cout.flush();
            GetConsoleScreenBufferInfo(handle, &info);

            std::cout.flush();
            FillConsoleOutputAttribute(handle, info.wAttributes, info.dwSize.X * info.dwSize.Y, pos, &dw);
            std::cout.flush();
            FillConsoleOutputCharacter(handle, ' ', info.dwSize.X * info.dwSize.Y, pos, &dw);

            SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), pos);
        }

        void render(std::vector<std::vector<AudioCapture::Bin>>& bins) {
            this->set_cursor_position(0, 0);

            char* output = new char[this->console_width * (this->console_height + 2)]();

            for (unsigned bin_index = 0; bin_index < bins[0].size(); ++bin_index) {
                memset(output + bin_index * this->console_width, ' ', this->console_width);
                for (unsigned channel_index = 0; channel_index < 2; ++channel_index) {
                    int magnitude = std::max(static_cast<int>(bins[channel_index][bin_index].get_normalized_envelope() * this->console_width / 2), 1);
                    if (channel_index == 0) {
                        memset(output + bin_index * this->console_width + (this->console_width / 2 - magnitude), '#', magnitude);
                    } else {
                        memset(output + bin_index * this->console_width + (this->console_width / 2), '#', magnitude);
                    }
                }
            }

            memset(output + bins[0].size() * this->console_width, ' ', this->console_width * 2);
            for (unsigned channel_index = 0; channel_index < 2; ++channel_index) {
                int magnitude = (0.7 * bins[channel_index][0].get_normalized_envelope() + 0.2 * bins[channel_index][1].get_normalized_envelope()
                                 + 0.1 * bins[channel_index][2].get_normalized_envelope())
                    * this->console_width / 2;
                if (channel_index == 0) {
                    memset(output + (bins[0].size() + 1) * this->console_width + (this->console_width / 2 - magnitude), '#', magnitude);
                } else {
                    memset(output + (bins[0].size() + 1) * this->console_width + (this->console_width / 2), '#', magnitude);
                }
            }

            printf("%s", output);

            delete[] output;
        }
};
