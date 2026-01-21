#pragma once

#include "DataSender.hpp"

#if defined(_WIN32)
#include "FFTW/fftw3.h"
#include "RtAudio/RtAudio.h"
#else
#include <fftw3.h>
#include <RtAudio.h>
#endif

#include <cmath>

class AudioCapture {
    private:
        constexpr static unsigned AUTOSCALE_TIME_WINDOW_MS = 1000;
        constexpr static double AUTOSCALE_VALUE            = 0.95;
        constexpr static unsigned INPUT_BUFFER_SIZE        = 1024;
        constexpr static double MAX_FREQUENCY              = 2000.;
        constexpr static unsigned BINS_SIZE                = 20;

        constexpr static double ENVELOPE_FOLLOWER_ATTACK  = 0.99; // Perceived as delay when peak is rising (higher is faster)
        constexpr static double ENVELOPE_FOLLOWER_RELEASE = 0.40; // Perceived as delay when peak is falling (higher is faster)

        struct Bin {
            public:
                double lower_frequency = 0.;
                double upper_frequency = 0.;
                double magnitude       = 0.;
                double envelope        = 0.;
                double max_envelope    = 0.;

                void follow_envelope(void) {
                    if (this->magnitude > this->envelope) {
                        this->envelope = ENVELOPE_FOLLOWER_ATTACK * this->magnitude + (1. - ENVELOPE_FOLLOWER_ATTACK) * this->envelope;
                    } else {
                        this->envelope = ENVELOPE_FOLLOWER_RELEASE * this->magnitude + (1. - ENVELOPE_FOLLOWER_RELEASE) * this->envelope;
                    }
                }

                // Normalize between 0.0 - 1.0
                double get_normalized_envelope(void) {
                    if (this->max_envelope == 0.) { return 0.; }
                    return this->envelope / this->max_envelope; // max_envelope is guaranteed to be greater or equal to magnitude
                }

                double get_normalized_envelope_log(void) {
                    if (this->max_envelope == 0.) { return 0.; }
                    return std::log10(1.0 + 9.0 * this->get_normalized_envelope()); // 0 … 1
                }
        };

    private:
        std::unique_ptr<RtAudio> rtaudio = nullptr;

        RtAudio::StreamParameters parameters = { 0 };
        unsigned sample_rate                 = 0;
        unsigned input_buffer_size           = 0;
        unsigned output_buffer_size          = 0;

        std::vector<std::vector<Bin>> bins             = {}; // Channel dependent bins
        std::vector<unsigned> frame_index_to_bin_index = {}; // Cache frame index <> bin index for faster processing in record callback

        std::vector<double> hann_window = {};
        std::vector<double> fftw_in     = {};
        fftw_complex* fftw_out          = nullptr;
        fftw_plan fftw                  = nullptr;

        DataSender* data_sender   = nullptr;
        std::vector<uint8_t> data = {};

        double last_autoscale = 0.;

        unsigned open_stream(void);
        unsigned close_stream(void);
        void generate_bins();

        static int record(void* output_buffer, void* input_buffer, unsigned input_buffer_size, double stream_time, RtAudioStreamStatus status, void* user_data);

    public:
        AudioCapture(DataSender* data_sender, std::string device_name, bool use_input_device, unsigned input_buffer_size = INPUT_BUFFER_SIZE, unsigned bins_size = BINS_SIZE);
        ~AudioCapture(void);

        unsigned initialize(void);

        friend class Visualizer;
};
