#include "AudioCapture.hpp"

AudioCapture::AudioCapture(unsigned int bufferFrames, uint8_t maxBands) {
    this->rtaudio = new RtAudio(RtAudio::WINDOWS_WASAPI);

    printf("[INFO] RtAudio API: %s\n", this->rtaudio->getApiName(this->rtaudio->getCurrentApi()).c_str());

    if (this->rtaudio->getDeviceCount() < 1) {
        printf("[FAIL] No audio devices found!\n");
        return;
    }

    this->parameters.deviceId      = this->rtaudio->getDefaultOutputDevice();
    RtAudio::DeviceInfo deviceInfo = this->rtaudio->getDeviceInfo(this->parameters.deviceId);

    this->parameters.nChannels    = deviceInfo.outputChannels;
    this->parameters.firstChannel = 0;
    this->sampleRate              = deviceInfo.preferredSampleRate;
    this->bufferFrames            = bufferFrames;

    this->bandMaximums  = NULL;
    this->magnitudesBin = NULL;

    this->setMaxBands(maxBands);

    this->makeBandBorders();

    printf(
        "[++++] Registered audio device:\n\tId: %d\n\tChannels: %d\n\tSample rate: %d\n\tFormats: %#x\n",
        this->parameters.deviceId,
        this->parameters.nChannels,
        this->sampleRate,
        deviceInfo.nativeFormats
    );

    this->frequencies = (double*)calloc(this->bufferFrames, sizeof(double));
    this->fftw_in     = (double*)calloc(this->bufferFrames, sizeof(double));
    this->magnitudes  = (double*)calloc(this->bufferFrames, sizeof(double));
    this->fftw_out    = (fftw_complex*)fftw_malloc(sizeof(fftw_complex) * this->bufferFrames);
}

AudioCapture::~AudioCapture() {
    delete this->rtaudio;

    free(this->bandBorders);

    for (uint16_t i = 0; i < this->maxBands; i++) {
        free(this->bandMaximums[i]);
    }

    free(this->bandMaximums);

    free(this->frequencies);
    free(this->fftw_in);
    free(this->magnitudes);
    free(this->magnitudesBin);
    fftw_free(this->fftw_out);
}

bool AudioCapture::
    openStream(int record(void* outputBuffer, void* inputBuffer, unsigned int nBufferFrames, double streamTime, RtAudioStreamStatus status, void* userData)) {
    try {
        this->rtaudio->openStream(NULL, &(this->parameters), RTAUDIO_FLOAT64, this->sampleRate, &(this->bufferFrames), record);
        this->rtaudio->startStream();
    } catch (int e) {
        printf("[FAIL] Failed to open stream with error code %d!\n", e);
        return false;
    }
    return true;
}

bool AudioCapture::closeStream() {
    try {
        this->rtaudio->stopStream();
    } catch (int e) {
        printf("[FAIL] Failed to close stream with error code %d!\n", e);
        return false;
    }

    if (this->rtaudio->isStreamOpen()) { this->rtaudio->closeStream(); }
    return true;
}

void AudioCapture::makeBandBorders() {
    this->bandBorders = (borderBandStruct*)calloc(this->maxBands, sizeof(borderBandStruct));

    for (uint8_t i = 0; i < this->maxBands; i++) {
        this->bandBorders[i].lowerBand = (i > 0) ? this->bandBorders[i - 1].upperBand : 0;
        this->bandBorders[i].upperBand = (i > 0) ? this->bandBorders[i - 1].lowerBand + i * 90 : 30;
    }
}

RtAudio::StreamParameters AudioCapture::getParameters() {
    return this->parameters;
}

unsigned int AudioCapture::getSampleRate() {
    return this->sampleRate;
}

unsigned int AudioCapture::getBufferFrames() {
    return this->bufferFrames;
}

uint8_t AudioCapture::getMaxBands() {
    return this->maxBands;
}

SYSTEMTIME AudioCapture::getLastUpdated() {
    return this->lastUpdated;
}

double AudioCapture::getLowerBandBorderAt(uint8_t index) {
    return this->bandBorders[index].lowerBand;
}

double AudioCapture::getHigherBandBorderAt(uint8_t index) {
    return this->bandBorders[index].upperBand;
}

double AudioCapture::getBandMaximumAt(uint8_t index, uint8_t channel) {
    return this->bandMaximums[index][channel];
}

AudioCaptureOutputMode AudioCapture::getOutputMode() {
    return this->outputMode;
}

void AudioCapture::setBandMaximumAt(uint8_t index, uint8_t channel, double newMaximum) {
    this->bandMaximums[index][channel] = newMaximum;
}

void AudioCapture::setMaxBands(uint8_t maxBands) {
    this->maxBands = maxBands;

    if (this->bandMaximums) { free(this->bandMaximums); }

    if (this->magnitudesBin) { free(this->magnitudesBin); }

    this->bandMaximums = (double**)calloc(this->maxBands, sizeof(double*));

    for (uint16_t i = 0; i < this->maxBands; i++) {
        this->bandMaximums[i] = (double*)calloc(this->parameters.nChannels, sizeof(double));
    }

    this->magnitudesBin = (double*)calloc(this->getMaxBands(), sizeof(double));
}

void AudioCapture::setOutputMode(AudioCaptureOutputMode mode) {
    this->outputMode = mode;

    switch (this->outputMode) {
        case AUDIOCAPTURE_MODE_MONO_MAGNITUDE: this->setMaxBands(30); break;
        case AUDIOCAPTURE_MODE_STEREO_MAGNITUDE: this->setMaxBands(30); break;
        case AUDIOCAPTURE_MODE_STEREO_SPECTRUM: this->setMaxBands(80); break;
    }
}

void AudioCapture::setLastUpdated(SYSTEMTIME* updateTime) {
    memcpy((void*)&(this->lastUpdated), (void*)updateTime, sizeof(SYSTEMTIME));
}
