# LightStripAudioSync
 
LightStripAudioSync is a simple Windows tool that analyzes the audio stream of your default audio device.
It automatically discovers compatible devices on your network and sends them frequency-dependent magnitudes, i.e., for displaying a peakmeter.
The auto-discovery process is explained in the section `Device discovery`.

LightStripAudioSync is based on [FFTW3](https://fftw.org) and [RtAudio](https://github.com/thestk/rtaudio).
## Installation

1. Clone the repository

2. Copy the following files from the official [FFTW website](https://fftw.org/download.html) into the `FFTW` directory:
  - fftw3.h
  - libfftw3-3.dll
  - libfftw3-3.lib

3. Optional: configure for your use-case
  - Set your frequency weights in `main.cpp`

4. Run Cmake via `build.bat`

Hint: if `cmake ..` failes, check `cmake -G` any try building with a specific generator.

5. Run `LightStripAudioSync.exe`

## Device discovery

`LightStripAudioSync.exe` sends a UDP packet with the message `"DISCOVER_LIGHTSTRIP_AUDIOSYNC_DEVICE"` every five seconds on port `3333`.
Compatible devices on your network must reply to this message on the same port and with the message `"REGISTER_LIGHTSTRIP_AUDIOSYNC_DEVICE"`.
`LightStripAudioSync.exe` will then send them the magnitudes of the audio stream on your default audio device every ~30ms.

## Integration with esphome

Example config:

```yaml
globals:
  - id: magnitudes
    type: uint8_t[2]
    restore_value: no
    initial_value: "{ 0, 0 }"

udp:
  - id: lightstrip_audiosync_listener
    port : 3333
    on_receive:
      then:
        - if:
            condition:
              lambda: |-
                return (data.size() == 36) && (memcmp(data.data(), "DISCOVER_LIGHTSTRIP_AUDIOSYNC_DEVICE", data.size()) == 0);
            then:
              - udp.write:
                  id: lightstrip_audiosync_listener
                  data: "REGISTER_LIGHTSTRIP_AUDIOSYNC_DEVICE"
        - lambda: |-
            switch (data.size()) {
              case 2: {
                memcpy(id(magnitudes), data.data(), 2);
                break;
              }
            }
```