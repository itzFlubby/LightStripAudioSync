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

enum class ColorCode : unsigned short {
    black = 0,
    red,
    green,
    yellow,
    magenta,
    cyan
};

inline const char* color_code_to_ansi(ColorCode color_code) {
    switch (color_code) {
        case ColorCode::black: {
            return "\x1b[0m";
        }
        case ColorCode::red: {
            return "\x1b[31m";
        }
        case ColorCode::green: {
            return "\x1b[32m";
        }
        case ColorCode::yellow: {
            return "\x1b[33m";
        }
        case ColorCode::magenta: {
            return "\x1b[35m";
        }
        case ColorCode::cyan: {
            return "\x1b[36m";
        }
    }
    return "";
}

struct color_index_t {
    public:
        unsigned index       = 0;
        ColorCode color_code = ColorCode::black;
};

class Visualizer {
    private:
        unsigned console_width  = 0;
        unsigned console_height = 0;
        std::vector<char> screen_buffer;
        std::vector<color_index_t> color_indices;
        std::vector<char> output;

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

        void set_cursor_position(unsigned x, unsigned y) { printf("\x1b[%d;%dH", y + 1, x + 1); }

        void render(AudioCapture* const audio_capture) {
            this->set_cursor_position(0, 0);

            this->get_screen_size();

            if (this->console_height < 3) { return; } // Not enough space to render

            screen_buffer.resize(this->console_width * this->console_height);
            std::fill(screen_buffer.begin(), screen_buffer.end(), ' ');

            const unsigned bins_size   = audio_capture->bins[0].size();
            const unsigned offset_x    = 2; // Room for channel labels
            const unsigned bar_width   = std::max(static_cast<unsigned>((this->console_width - offset_x) / bins_size), static_cast<unsigned>(1));
            const unsigned total_width = bar_width * bins_size;
            const unsigned bar_spacing = 1;

            color_indices.clear();
            color_indices.reserve(bins_size * 2 * 2); // bins * channels * pre- & post color

            const unsigned start          = (this->console_width > total_width) ? ((this->console_width - total_width) / 2) : 0;
            const unsigned channel_left_y = (this->console_height - 3) / 2;
            color_indices.push_back({ .index = __2D_flatten(0, channel_left_y - 1, this->console_width), .color_code = ColorCode::magenta });
            screen_buffer[__2D_flatten(0, channel_left_y - 1, this->console_width)] = 'L';

            const unsigned right_channel_y = (this->console_height - 3) / 2 + 1;
            color_indices.push_back({ .index = __2D_flatten(0, right_channel_y + 1, this->console_width), .color_code = ColorCode::magenta });
            screen_buffer[__2D_flatten(0, right_channel_y + 1, this->console_width)] = 'R';

            const unsigned max_bar_height = channel_left_y;

            output.clear();
            output.reserve(screen_buffer.size() + color_indices.capacity());

            unsigned color_index = 0;
            for (unsigned bin_index = 0; bin_index < bins_size; ++bin_index) {
                for (unsigned channel_index = 0; channel_index < 2; ++channel_index) {
                    int magnitude   = std::max(static_cast<int>(audio_capture->bins[channel_index][bin_index].get_normalized_envelope() * max_bar_height), 1);
                    int sign        = (channel_index == 0) ? -1 : +1;
                    unsigned offset = (channel_index == 0) ? 0 : 1;
                    for (unsigned i = 0; i < magnitude; ++i) {
                        std::vector<char>::iterator it = screen_buffer.begin()
                            + __2D_flatten(start + bin_index * bar_width + offset_x, channel_left_y + (i * sign) + offset, this->console_width);
                        std::fill(it, it + (bar_width - bar_spacing), '#');
                        color_index_t color_index { .index = static_cast<unsigned>(it - screen_buffer.begin()) };
                        if (i == 0) {
                            color_index.color_code = ColorCode::cyan;
                        } else if (1 <= i && i < (7 * max_bar_height / 10)) {
                            color_index.color_code = ColorCode::green;
                        } else if ((7 * max_bar_height / 10) <= i && i < (9 * max_bar_height / 10)) {
                            color_index.color_code = ColorCode::yellow;
                        } else {
                            color_index.color_code = ColorCode::red;
                        }
                        color_indices.push_back(color_index);
                    }
                }
            }

            color_indices.push_back({ .index = (this->console_height - 1) * this->console_width, .color_code = ColorCode::black });
            for (unsigned channel_index = 0; channel_index < 2; ++channel_index) {
                int magnitude =
                    (0.7 * audio_capture->bins[channel_index][0].get_normalized_envelope() + 0.2 * audio_capture->bins[channel_index][1].get_normalized_envelope()
                     + 0.1 * audio_capture->bins[channel_index][2].get_normalized_envelope())
                    * this->console_width / 2;
                int offset                     = (channel_index == 0) ? -magnitude : 0;
                std::vector<char>::iterator it = screen_buffer.begin() + (this->console_height - 1) * this->console_width + (this->console_width / 2) + offset;
                std::fill(it, it + magnitude, '#');
            }

            std::sort(color_indices.begin(), color_indices.end(), [](auto& a, auto& b) { return a.index < b.index; });

            unsigned pre = 0;
            for (const color_index_t& color_index : color_indices) {
                std::string color_code_str = color_code_to_ansi(color_index.color_code);
                std::copy(screen_buffer.data() + pre, screen_buffer.data() + color_index.index, std::back_inserter(output));
                std::copy(color_code_str.begin(), color_code_str.end(), std::back_inserter(output));
                pre = color_index.index;
            }

            std::copy(screen_buffer.begin() + pre, screen_buffer.end(), std::back_inserter(output));
            output.push_back('\0');

            printf("%s", output.data());
        }
};
