#include <atlsafe.h>
#include <Windows.h>
// This include order is important
#include "AudioCapture.hpp"
#include "DataSender.hpp"
#include "FFTW/fftw3.h"
#include "RtAudio/RtAudio.h"

#include <stdio.h>
#include <stdlib.h>

#if !defined(BROADCAST_ADDRESS)
#define BROADCAST_ADDRESS "255.255.255.255" // Fallback broadcast address
#endif

double* magnitudes = (double*)calloc(2, sizeof(double));

DataSender* dataSender     = nullptr;
AudioCapture* audioCapture = nullptr;

int record(void* outputBuffer, void* inputBuffer, unsigned int nBufferFrames, double streamTime, RtAudioStreamStatus status, void* userData) {
    RtAudio::StreamParameters parameters = audioCapture->getParameters();
    unsigned int sampleRate              = audioCapture->getSampleRate();

    AudioCaptureOutputMode mode = audioCapture->getOutputMode();

    SYSTEMTIME systemTime;
    GetSystemTime(&systemTime);

    const int bandUpdateTimeDelta = 1;

    for (uint16_t frameIndex = 0; frameIndex < nBufferFrames; frameIndex++) {
        audioCapture->frequencies[frameIndex] = sampleRate * frameIndex / nBufferFrames;
    }

    for (uint16_t channelIndex = 0; channelIndex < parameters.nChannels; channelIndex++) {
        memset(audioCapture->magnitudesBin, 0x00, sizeof(double) * audioCapture->getMaxBands());

        for (uint16_t frameIndex = 0; frameIndex < nBufferFrames; frameIndex++) {
            audioCapture->fftw_in[frameIndex] = ((double*)inputBuffer)[frameIndex * parameters.nChannels + channelIndex];
        }

        fftw_plan plan = fftw_plan_dft_r2c_1d(nBufferFrames, audioCapture->fftw_in, audioCapture->fftw_out, FFTW_ESTIMATE);

        fftw_execute(plan);

        fftw_destroy_plan(plan);

        for (uint16_t frameIndex = 0; frameIndex < nBufferFrames; frameIndex++) {
            audioCapture->magnitudes[frameIndex] = std::sqrt(
                audioCapture->fftw_out[frameIndex][0] * audioCapture->fftw_out[frameIndex][0] + audioCapture->fftw_out[frameIndex][1] * audioCapture->fftw_out[frameIndex][1]
            );
            for (uint8_t bandIndex = 0; bandIndex < audioCapture->getMaxBands(); bandIndex++) {
                if (audioCapture->frequencies[frameIndex] >= audioCapture->getLowerBandBorderAt(bandIndex)
                    && audioCapture->frequencies[frameIndex] < audioCapture->getHigherBandBorderAt(bandIndex)) {
                    audioCapture->magnitudesBin[bandIndex] += audioCapture->magnitudes[frameIndex];
                    audioCapture->magnitudesBin[bandIndex] /= 2;
                }
            }
        }

        for (uint8_t bandIndex = 0; bandIndex < audioCapture->getMaxBands(); bandIndex++) {
            if (audioCapture->magnitudesBin[bandIndex] > audioCapture->getBandMaximumAt(bandIndex, channelIndex)) {
                audioCapture->setBandMaximumAt(bandIndex, channelIndex, audioCapture->magnitudesBin[bandIndex]);
            } else {
                if (abs(systemTime.wSecond - audioCapture->getLastUpdated().wSecond) > bandUpdateTimeDelta) {
                    audioCapture->setBandMaximumAt(bandIndex, channelIndex, audioCapture->getBandMaximumAt(bandIndex, channelIndex) * 0.99);
                }
            }
        }

        switch (mode) {
            case AUDIOCAPTURE_MODE_MONO_MAGNITUDE:
            case AUDIOCAPTURE_MODE_STEREO_MAGNITUDE: {
                magnitudes[channelIndex] = 0.2 * audioCapture->magnitudesBin[0] / audioCapture->getBandMaximumAt(0, channelIndex)
                    + 0.6 * audioCapture->magnitudesBin[1] / audioCapture->getBandMaximumAt(1, channelIndex)
                    + 0.2 * audioCapture->magnitudesBin[2] / audioCapture->getBandMaximumAt(2, channelIndex);
                break;
            }
        }
    }

    switch (mode) {
        case AUDIOCAPTURE_MODE_MONO_MAGNITUDE:
        case AUDIOCAPTURE_MODE_STEREO_MAGNITUDE: {
            uint8_t converted[2] = { static_cast<uint8_t>(magnitudes[0] * 255), static_cast<uint8_t>(magnitudes[1] * 255) };
            dataSender->sendMessage(converted);
            break;
        }
    }

    if (abs(systemTime.wSecond - audioCapture->getLastUpdated().wSecond) > bandUpdateTimeDelta) { audioCapture->setLastUpdated(&systemTime); }

    return 0;
}

int cleanup_and_exit(int code) {
    if (dataSender) { delete dataSender; }
    if (audioCapture) { delete audioCapture; }
    if (code) { printf("[CRIT] Setup failed!\n"); }
    return code;
}

int main() {
    printf("[LightStripAudioSync]\n\n");

    printf("[INFO] Starting audio capture...\n");
    audioCapture = new AudioCapture(1024);
    if (!audioCapture || audioCapture->getSampleRate() == 0) { return cleanup_and_exit(1); }
    audioCapture->setOutputMode(AUDIOCAPTURE_MODE_STEREO_MAGNITUDE);

    const char* broadcast_address = BROADCAST_ADDRESS;
    unsigned short broadcast_port = 3333;
    printf("[INFO] Opening broadcast to %s:%d...\n", broadcast_address, broadcast_port);
    dataSender = new DataSender(broadcast_address, broadcast_port);
    if (!dataSender || dataSender->initialize() != 0) { return cleanup_and_exit(1); }

    printf("[INFO] Streaming...\n");
    if (!audioCapture->openStream(&record)) { return cleanup_and_exit(1); }

    char input;
    while (true) {
        printf("> ");
        std::string input;
        std::getline(std::cin, input);

        if (input == "help" || input == "?") {
            printf("Available commands:\n");
            printf("  help, ?       Show this help message\n");
            printf("  exit, quit    Exit the program\n");
        } else if (input == "exit" || input == "quit" || input == "q") {
            break;
        } else {
            printf("Unknown command: %s\n", input.c_str());
        }
    }

    audioCapture->closeStream();

    return cleanup_and_exit(0);
}
