#include "AudioCapture.hpp"
#include "Visualizer.hpp"

#define _USE_MATH_DEFINES
#include <math.h>

#if defined(_WIN32)
constexpr RtAudio::Api API = RtAudio::Api::WINDOWS_WASAPI;
#elif defined(__linux__)
constexpr RtAudio::Api API = RtAudio::Api::LINUX_ALSA;
#else
#error "Unsupported platform!"
#endif

Visualizer visualizer;

AudioCapture::AudioCapture(DataSender* data_sender, int device_id, unsigned input_buffer_size, unsigned bins_size) :
    data_sender(data_sender),
    input_buffer_size(input_buffer_size) {
    this->rtaudio = std::make_unique<RtAudio>(API);

    std::vector<RtAudio::Api> compiled_apis;
    RtAudio::getCompiledApi(compiled_apis);
    printf("[INFO] Compiled RtAudio APIs (* is in use):\n");
    for (RtAudio::Api api : compiled_apis) {
        printf("  - %s %c\n", RtAudio::getApiName(api).c_str(), (api == this->rtaudio->getCurrentApi()) ? '*' : ' ');
    }

    if (this->rtaudio->getDeviceCount() < 1) {
        printf("[CRIT] No audio devices found!\n");
        return;
    }

    std::vector<unsigned> device_ids = this->rtaudio->getDeviceIds();
    printf("[INFO] Available audio devices:\n");
    for (unsigned id : device_ids) {
        RtAudio::DeviceInfo info = this->rtaudio->getDeviceInfo(id);
        printf("  - %s (%d)\n", info.name.c_str(), info.ID);
    }

    // Set default device if none specified
    if (device_id == -1) { device_id = this->rtaudio->getDefaultOutputDevice(); }

    RtAudio::DeviceInfo device_info = this->rtaudio->getDeviceInfo(device_id);
    this->parameters.deviceId       = device_info.ID;
    this->parameters.nChannels      = std::max(device_info.outputChannels, device_info.inputChannels); // If default input device is used instead of output
    this->sample_rate               = device_info.preferredSampleRate;
    this->input_buffer_size         = input_buffer_size;
    this->output_buffer_size        = this->input_buffer_size / 2 + 1;

    if (this->parameters.nChannels == 0) {
        printf("[CRIT] Selected audio device has no input or output channels!\n");
        return;
    }

    printf(
        "[++++] Registered audio device:\n\tId: %d\n\tChannels: %d\n\tSample rate: %d\n\tFormats: %#x\n",
        this->parameters.deviceId,
        this->parameters.nChannels,
        this->sample_rate,
        device_info.nativeFormats
    );

    // Reserve memory for magnitude data
    this->data.resize(this->parameters.nChannels);

    // Initialize the bins
    this->bins.resize(this->parameters.nChannels, {});
    for (unsigned channel_index = 0; channel_index < this->bins.size(); ++channel_index) {
        this->bins[channel_index].resize(bins_size, {});
        for (unsigned bin_index = 0; bin_index < this->bins[channel_index].size(); ++bin_index) {
            this->bins[channel_index][bin_index].lower_frequency = (bin_index == 0) ? 0 : this->bins[channel_index][bin_index - 1].upper_frequency;
            this->bins[channel_index][bin_index].upper_frequency =
                this->bins[channel_index][bin_index].lower_frequency + (MAX_FREQUENCY / static_cast<double>(this->bins[channel_index].size()));
        }
    }

    // Calculate which frame index falls into what bin
    this->frame_index_to_bin_index.resize(this->output_buffer_size, this->bins[0].size() - 1); // All frame indices that don't belong to any bin will be placed in the last bin (init value)
    for (unsigned frame_index = 0; frame_index < this->output_buffer_size; ++frame_index) {
        double frequency = static_cast<double>(frame_index) * static_cast<double>(this->sample_rate) / static_cast<double>(this->input_buffer_size);
        for (unsigned bin_index = 0; bin_index < this->bins[0].size(); ++bin_index) { // Channel doesn't matter as bins have the same bandwith for all channels
            if (this->bins[0][bin_index].lower_frequency <= frequency && this->bins[0][bin_index].upper_frequency > frequency) {
                this->frame_index_to_bin_index[frame_index] = bin_index;
                break; // Continue with next frame_index when the correct bin was found
            }
        }
    }

    this->hann_window.resize(this->input_buffer_size);
    for (unsigned frame_index = 0; frame_index < this->input_buffer_size; ++frame_index) {
        this->hann_window[frame_index] = 0.5 * (1.0 - cos(2.0 * M_PI * frame_index / (this->input_buffer_size - 1)));
    }
    this->fftw_in.resize(this->input_buffer_size, 0.);
    this->fftw_out = reinterpret_cast<fftw_complex*>(fftw_malloc(sizeof(fftw_complex) * this->output_buffer_size));
}

AudioCapture::~AudioCapture() {
    this->close_stream();
    fftw_free(this->fftw_out);
}

int AudioCapture::record(void* output_buffer, void* input_buffer, unsigned input_buffer_size, double stream_time, RtAudioStreamStatus status, void* user_data) {
    if ((status == RTAUDIO_INPUT_OVERFLOW) || (status == RTAUDIO_OUTPUT_UNDERFLOW)) {
        // These aren't fatal errors, therefore, ignore and continue
    }

    AudioCapture* audio_capture = reinterpret_cast<AudioCapture*>(user_data);

    bool autoscale = ((stream_time - audio_capture->last_autoscale) > (AUTOSCALE_TIME_WINDOW_MS / 1000.));
    if (autoscale) { audio_capture->last_autoscale = stream_time; }

    for (unsigned channel_index = 0; channel_index < audio_capture->parameters.nChannels; ++channel_index) {
        for (unsigned frame_index = 0; frame_index < input_buffer_size; ++frame_index) {
            audio_capture->fftw_in[frame_index] = (reinterpret_cast<double*>(input_buffer))[frame_index * audio_capture->parameters.nChannels + channel_index]
                * audio_capture->hann_window[frame_index];
        }

        fftw_execute(audio_capture->fftw);

        // Reset all magnitudes
        for (Bin& bin : audio_capture->bins[channel_index]) {
            bin.magnitude = 0.;
        }

        // Bin the magnitudes
        for (unsigned frame_index = 0; frame_index < audio_capture->output_buffer_size; ++frame_index) {
            double frame_magnitude = std::sqrt(
                audio_capture->fftw_out[frame_index][0] * audio_capture->fftw_out[frame_index][0]
                + audio_capture->fftw_out[frame_index][1] * audio_capture->fftw_out[frame_index][1]
            );
            audio_capture->bins[channel_index][audio_capture->frame_index_to_bin_index[frame_index]].magnitude += frame_magnitude;
        }

        // Follow the magnitudes' envelopes
        for (Bin& bin : audio_capture->bins[channel_index]) {
            bin.follow_envelope();
        }

        // Autoscale the envelopes
        for (Bin& bin : audio_capture->bins[channel_index]) {
            if (bin.envelope > bin.max_envelope) { bin.max_envelope = bin.envelope; }
            if (autoscale && ((bin.max_envelope * AudioCapture::AUTOSCALE_VALUE) > bin.envelope)) { bin.max_envelope *= AudioCapture::AUTOSCALE_VALUE; }
        }

        // Weights MUST be combined no more than 1.
        audio_capture->data[channel_index] = static_cast<unsigned char>(std::min(
            255.
                * (0.7 * audio_capture->bins[channel_index][0].get_normalized_envelope() + 0.2 * audio_capture->bins[channel_index][1].get_normalized_envelope()
                   + 0.1 * audio_capture->bins[channel_index][2].get_normalized_envelope()),
            255.
        ));
    }

    audio_capture->data_sender->enqueue(Packet(Packet::destination_t::device, Packet::type_t::data, audio_capture->data.data(), audio_capture->data.size()));

    // visualizer.render(audio_capture->bins); // Uncomment to enable console visualizer

    return 0;
}

unsigned AudioCapture::open_stream(void) {
    RtAudioErrorType result = RTAUDIO_NO_ERROR;
    if ((result = this->rtaudio->openStream(
             NULL, &(this->parameters), RTAUDIO_FLOAT64, this->sample_rate, &(this->input_buffer_size), AudioCapture::record, reinterpret_cast<void*>(this)
         ))
        != RTAUDIO_NO_ERROR) {
        printf("[CRIT] Failed to open stream with error code %d!\n", result);
        return result;
    }

    if ((this->fftw = fftw_plan_dft_r2c_1d(this->input_buffer_size, this->fftw_in.data(), this->fftw_out, FFTW_ESTIMATE)) == NULL) {
        printf("[CRIT] Failed to create FFTW plan!\n");
        return 1;
    }

    if ((result = this->rtaudio->startStream()) != RTAUDIO_NO_ERROR) {
        printf("[CRIT] Failed to start stream with error code %d!\n", result);
        return result;
    }

    return 0;
}

unsigned AudioCapture::close_stream(void) {
    RtAudioErrorType result = RTAUDIO_NO_ERROR;
    if (this->rtaudio->isStreamRunning()) {
        if ((result = this->rtaudio->stopStream()) != RTAUDIO_NO_ERROR) {
            printf("[CRIT] Failed to stop stream with error code %d!\n", result);
            return result;
        }
    }

    if (this->fftw) {
        fftw_destroy_plan(this->fftw);
        this->fftw = nullptr;
    }

    if (this->rtaudio->isStreamOpen()) { this->rtaudio->closeStream(); }
    return 0;
}

unsigned AudioCapture::initialize(void) {
    return this->open_stream();
}
