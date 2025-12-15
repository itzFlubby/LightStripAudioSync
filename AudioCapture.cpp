#include "AudioCapture.hpp"

#define _USE_MATH_DEFINES
#include <math.h>

AudioCapture::AudioCapture(DataSender* data_sender, unsigned input_buffer_size, unsigned bins_size) :
    data_sender(data_sender),
    input_buffer_size(input_buffer_size) {
    this->rtaudio = std::make_unique<RtAudio>(RtAudio(RtAudio::WINDOWS_WASAPI));

    printf("[INFO] RtAudio API: %s\n", this->rtaudio->getApiName(this->rtaudio->getCurrentApi()).c_str());

    if (this->rtaudio->getDeviceCount() < 1) {
        printf("[CRIT] No audio devices found!\n");
        return;
    }

    RtAudio::DeviceInfo device_info = this->rtaudio->getDeviceInfo(this->rtaudio->getDefaultOutputDevice());
    this->parameters.deviceId       = device_info.ID;
    this->parameters.nChannels      = device_info.outputChannels;
    this->sample_rate               = device_info.preferredSampleRate;
    this->input_buffer_size         = input_buffer_size;
    this->output_buffer_size        = this->input_buffer_size / 2 + 1;

    printf(
        "[++++] Registered audio device:\n\tId: %d\n\tChannels: %d\n\tSample rate: %d\n\tFormats: %#x\n",
        this->parameters.deviceId,
        this->parameters.nChannels,
        this->sample_rate,
        device_info.nativeFormats
    );

    // Reserve memory for magnitude data
    this->data_size                 = this->parameters.nChannels + 3; // STX, LEN, <DATA>, ETX
    this->data                      = std::make_unique<uint8_t[]>(this->data_size);
    this->data[0]                   = 0x02;                       // STX
    this->data[1]                   = this->parameters.nChannels; // LEN
    this->data[this->data_size - 1] = 0x03;                       // ETX

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

    this->fftw_in.resize(this->input_buffer_size, 0.);
    this->fftw_out = reinterpret_cast<fftw_complex*>(fftw_malloc(sizeof(fftw_complex) * this->output_buffer_size));
}

AudioCapture::~AudioCapture() {
    this->close_stream();
    fftw_free(this->fftw_out);
}

int AudioCapture::record(void* output_buffer, void* input_buffer, unsigned input_buffer_size, double stream_time, RtAudioStreamStatus status, void* user_data) {
    if ((status == RTAUDIO_INPUT_OVERFLOW) || (status == RTAUDIO_OUTPUT_UNDERFLOW)) { return status; }

    AudioCapture* audio_capture = reinterpret_cast<AudioCapture*>(user_data);

    bool autoscale = ((stream_time - audio_capture->last_autoscale) > (AUTOSCALE_TIME_WINDOW_MS / 1000.));
    if (autoscale) { audio_capture->last_autoscale = stream_time; }

    for (unsigned channel_index = 0; channel_index < audio_capture->parameters.nChannels; ++channel_index) {
        for (unsigned frame_index = 0; frame_index < input_buffer_size; ++frame_index) {
            audio_capture->fftw_in[frame_index] = (reinterpret_cast<double*>(input_buffer))[frame_index * audio_capture->parameters.nChannels + channel_index];
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

        // Autoscale the magnitudes
        for (Bin& bin : audio_capture->bins[channel_index]) {
            if (bin.magnitude > bin.max_magnitude) { bin.max_magnitude = bin.magnitude; }
            if (autoscale && ((bin.max_magnitude * 0.95) > bin.magnitude)) { bin.max_magnitude *= 0.95; }
        }

        // Weights MUST be combined no more than 1.
        audio_capture->data[2 + channel_index] = // Offset of 2 for ETX and LEN
            static_cast<unsigned char>(std::min(
                255.
                    * (0.8 * audio_capture->bins[channel_index][0].get_normalized_magnitude() + 0.1 * audio_capture->bins[channel_index][1].get_normalized_magnitude()
                       + 0.1 * audio_capture->bins[channel_index][2].get_normalized_magnitude()),
                255.
            ));
    }

    audio_capture->data_sender->enqueue(audio_capture->data.get(), audio_capture->data_size, DataSender::QueuedData::destination_t::device);

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
