#pragma once

#include "DataSender.hpp"
#include "FFTW/fftw3.h"
#include "RtAudio/RtAudio.h"

class AudioCapture {
    private:
        constexpr static unsigned AUTOSCALE_TIME_WINDOW_MS = 1000;
        constexpr static unsigned INPUT_BUFFER_SIZE        = 1024;
        constexpr static double MAX_FREQUENCY              = 2000.;
        constexpr static unsigned BINS_SIZE                = 20;

        struct Bin {
            public:
                double lower_frequency = 0.;
                double upper_frequency = 0.;
                double magnitude       = 0.;
                double max_magnitude   = 0.;

                // Normalize between 0.0 - 1.0
                double get_normalized_magnitude(void) {
                    if (this->max_magnitude == 0.) { return 0.; }
                    return this->magnitude / this->max_magnitude; // max_magnitude is guaranteed to be greater or equal to magnitude
                }

                double get_normalized_magnitude_log(void) {
                    if (this->max_magnitude == 0.) { return 0.; }
                    return std::log10(1.0 + 9.0 * this->get_normalized_magnitude()); // 0 … 1
                }
        };

    private:
        std::unique_ptr<RtAudio> rtaudio     = nullptr;
        RtAudio::StreamParameters parameters = { 0 };
        unsigned sample_rate                 = 0;
        unsigned input_buffer_size           = 0;
        unsigned output_buffer_size          = 0;

        std::vector<std::vector<Bin>> bins             = {}; // Channel dependent bins
        std::vector<unsigned> frame_index_to_bin_index = {}; // Cache frame index <> bin index for faster processing in record callback

        std::vector<double> fftw_in = {};
        fftw_complex* fftw_out      = nullptr;
        fftw_plan fftw              = nullptr;

        DataSender* data_sender         = nullptr;
        std::unique_ptr<uint8_t[]> data = nullptr;
        size_t data_size                = 0;

        double last_autoscale = 0.;

        unsigned open_stream(void);
        unsigned close_stream(void);
        void generate_bins();

        static int record(void* output_buffer, void* input_buffer, unsigned input_buffer_size, double stream_time, RtAudioStreamStatus status, void* user_data);

    public:
        AudioCapture(DataSender* data_sender, unsigned input_buffer_size = INPUT_BUFFER_SIZE, unsigned bins_size = BINS_SIZE);
        ~AudioCapture(void);

        unsigned initialize(void);
};
