# LightStripAudioSync
 
LightStripAudioSync is a simple Windows tool that analyzes the audio stream of your default audio device and broadcasts frequency-dependent magnitudes over UDP for use with other devices, such as audio-synced LED strips.

LightStripAudioSync is based on [FFTW3](https://fftw.org) and [RtAudio](https://github.com/thestk/rtaudio).

## Installation

1. Clone the repository

2. Create a folder called `FFTW`

3. Copy the following files from the official [FFTW website](https://fftw.org/download.html) into the `FFTW` directory:
  - fftw3.h
  - libfftw3-3.dll
  - libfftw3-3.lib

4. Configure for your use-case
  - Set `BROADCAST_ADDRESS` in CMakeLists.txt 
  - Set your frequency weights in `main.cpp` (optional)

5. Run Cmake
```
mkdir build
cd build
cmake ..
cmake --build . --config Release
```

Hint: if `cmake ..` failes, check `cmake -G` any try building with a specific generator.

6. Run `LightStripAudioSync.exe`