#pragma once

#include "AudioCapture.hpp"

#include <algorithm>
#include <cstring>
#include <iostream>
#include <stdio.h>

#if defined(_WIN32)
#include <Windows.h>
#else
#include <sys/ioctl.h>
#endif

#define __2D_flatten(x, y, dim_x) ((y) * (dim_x) + (x))

class Visualizer {
    private:
        unsigned console_width  = 0;
        unsigned console_height = 0;
        std::vector<char> screen_buffer;

    public:
        Visualizer() { }

        ~Visualizer() = default;

        void get_screen_size(void) {
#if defined(_WIN32)
            CONSOLE_SCREEN_BUFFER_INFO csbi;
            GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi);
            this->console_width  = csbi.dwSize.X;
            this->console_height = csbi.dwSize.Y;
#else
            struct winsize w;
            ioctl(fileno(stdout), TIOCGWINSZ, &w);
            this->console_width  = w.ws_col;
            this->console_height = w.ws_row;
#endif
        }

        void set_cursor_position(unsigned x, unsigned y) {
#if defined(_WIN32)
            printf("\u001b[%d;%dH", y + 1, x + 1);
#else
            printf("\033[%d;%dH", y + 1, x + 1);
#endif
            fflush(stdout);
        }

        void render(std::vector<std::vector<AudioCapture::Bin>>& bins) {
            this->set_cursor_position(0, 0);

            this->get_screen_size();

            if (this->console_height < 3) { return; } // Not enough space to render

            screen_buffer.resize(this->console_width * this->console_height + 1);
            std::fill(screen_buffer.begin(), screen_buffer.end(), ' ');
            screen_buffer[this->console_width * this->console_height] = '\0';

            unsigned offset_x    = 2; // Room for channel labels
            unsigned bar_width   = std::max(static_cast<unsigned>((this->console_width - offset_x) / bins[0].size()), static_cast<unsigned>(1));
            unsigned total_width = bar_width * bins[0].size();
            unsigned bar_spacing = 1;

            unsigned start = (this->console_width > total_width) ? ((this->console_width - total_width) / 2) : 0;

            unsigned channel_left_y                                             = (this->console_height - 3) / 2;
            screen_buffer[__2D_flatten(0, channel_left_y, this->console_width)] = 'L';

            unsigned right_channel_y                                             = (this->console_height - 3) / 2 + 1;
            screen_buffer[__2D_flatten(0, right_channel_y, this->console_width)] = 'R';

            for (unsigned bin_index = 0; bin_index < bins[0].size(); ++bin_index) {
                for (unsigned channel_index = 0; channel_index < 2; ++channel_index) {
                    int magnitude = std::max(static_cast<int>(bins[channel_index][bin_index].get_normalized_envelope() * (right_channel_y - 1)), 1);
                    if (channel_index == 0) {
                        for (unsigned i = 0; i < magnitude; ++i) {
                            std::vector<char>::iterator it =
                                screen_buffer.begin() + __2D_flatten(start + bin_index * bar_width + offset_x, channel_left_y - i, this->console_width);
                            std::fill(it, it + (bar_width - bar_spacing), '#');
                        }
                    } else {
                        for (unsigned i = 0; i < magnitude; ++i) {
                            std::vector<char>::iterator it =
                                screen_buffer.begin() + __2D_flatten(start + bin_index * bar_width + offset_x, right_channel_y + i, this->console_width);
                            std::fill(it, it + (bar_width - bar_spacing), '#');
                        }
                    }
                }
            }

            for (unsigned channel_index = 0; channel_index < 2; ++channel_index) {
                int magnitude = (0.7 * bins[channel_index][0].get_normalized_envelope() + 0.2 * bins[channel_index][1].get_normalized_envelope()
                                 + 0.1 * bins[channel_index][2].get_normalized_envelope())
                    * this->console_width / 2;
                if (channel_index == 0) {
                    std::vector<char>::iterator it = screen_buffer.begin() + (this->console_height - 1) * this->console_width + (this->console_width / 2) - magnitude;
                    std::fill(it, it + magnitude, '#');
                } else {
                    std::vector<char>::iterator it = screen_buffer.begin() + (this->console_height - 1) * this->console_width + (this->console_width / 2);
                    std::fill(it, it + magnitude, '#');
                }
            }

            printf("%s", screen_buffer.data());
        }
};
