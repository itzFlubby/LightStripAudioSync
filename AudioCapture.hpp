#pragma once

#include "FFTW/fftw3.h"
#include "RtAudio/RtAudio.h"

typedef struct borderBandStruct_type {
    public:
        double lowerBand = 0.;
        double upperBand = 0.;
} borderBandStruct;

enum AudioCaptureOutputMode {
    AUDIOCAPTURE_MODE_MONO_MAGNITUDE = 0,
    AUDIOCAPTURE_MODE_STEREO_MAGNITUDE,
    AUDIOCAPTURE_MODE_STEREO_SPECTRUM
};

class AudioCapture {
    private:
        RtAudio* rtaudio                     = nullptr;
        RtAudio::StreamParameters parameters = { 0 };
        unsigned int sampleRate              = 0;
        unsigned int bufferFrames            = 0;
        uint8_t maxBands                     = 0;
        borderBandStruct* bandBorders        = nullptr;
        double** bandMaximums                = nullptr;
        SYSTEMTIME lastUpdated               = { 0 };
        AudioCaptureOutputMode outputMode    = AUDIOCAPTURE_MODE_STEREO_MAGNITUDE;

    public:
        double* frequencies    = nullptr;
        double* fftw_in        = nullptr;
        double* magnitudes     = nullptr;
        double* magnitudesBin  = nullptr;
        fftw_complex* fftw_out = nullptr;

        AudioCapture(unsigned int bufferFrames, uint8_t maxBands = 30);
        ~AudioCapture();
        bool openStream(int record(void* outputBuffer, void* inputBuffer, unsigned int nBufferFrames, double streamTime, RtAudioStreamStatus status, void* userData));
        bool closeStream();
        void makeBandBorders();
        RtAudio::StreamParameters getParameters();
        unsigned int getSampleRate();
        unsigned int getBufferFrames();
        uint8_t getMaxBands();
        AudioCaptureOutputMode getOutputMode();
        SYSTEMTIME getLastUpdated();
        double getLowerBandBorderAt(uint8_t index);
        double getHigherBandBorderAt(uint8_t index);
        double getBandMaximumAt(uint8_t index, uint8_t channel);
        void setBandMaximumAt(uint8_t index, uint8_t channel, double newMaximum);
        void setMaxBands(uint8_t maxBands);
        void setOutputMode(AudioCaptureOutputMode mode);
        void setLastUpdated(SYSTEMTIME* updateTime);
};
