# LightStripAudioSync
 
LightStripAudioSync is a cross-platform tool that analyzes the audio stream of your default audio device.
It automatically discovers compatible devices on your network and sends them audio-channel-dependent magnitudes, i.e., for displaying a peakmeter. 
An example config for making [esphome](https://esphome.io/) devices compatible is shown in section `Integrating with esphome`.
The auto-discovery process is explained in the section `Device discovery`.

LightStripAudioSync is based on [FFTW3](https://fftw.org) and [RtAudio](https://github.com/thestk/rtaudio).
## Installation

### Windows

1. Clone the repository via `git clone https://github.com/itzFlubby/LightStripAudioSync.git`

2. Clone the submodules via `git submodule update --init --recursive`

3. Download the precompiled FFTW DLLs from the [FFTW website](https://www.fftw.org/install/windows.html):
    - Copy `fftw3.h`, `libfftw3-3.dll`, and, `libfftw3-3.def` into the `FFTW` directory.
    - Generate the `libfftw3-3.lib` with MSVC `lib.exe /def:libfftw3-3.def` in the `FFTW` directory.
      Hint: The usual install path is `C:\Program Files\Microsoft Visual Studio\XX\Community\VC\Tools\MSVC\XX.XX.XXXXX\bin\Hostx64\x64\lib.exe`

4. Optional: configure for your use-case
    - Set frequency weights in `AudioCapture.cpp`
    - Set `MAX_FREQUENCY` and `BINS_SIZE` in `AudioCapture.hpp`

5. Run Cmake via `build.bat`

Hint: if `cmake ..` fails, check `cmake -G` and try building with a specific generator.

6. Run `LightStripAudioSync.exe`

### Linux

1. Clone the repository via `git clone https://github.com/itzFlubby/LightStripAudioSync.git`

2. Install dependencies: `sudo apt update && sudo apt install libfftw3-dev librtaudio-dev build-essential cmake`

3. Optional: configure for your use-case
    - Set frequency weights in `AudioCapture.cpp`
    - Set `MAX_FREQUENCY` and `BINS_SIZE` in `AudioCapture.hpp`

4. Run Cmake via `build.sh`

Hint: if `cmake ..` fails, check `cmake -G` and try building with a specific generator.

5. Run `./build/LightStripAudioSync`

## Device discovery

The general protocol is defined as follows:
```
<STX><TYPE><LEN><DATA><ETX>
```
i.e.
```
Discover packet: 0x02 0x00 0x00 0x03
Register packet: 0x02 0x01 0x00 0x03
Data packet:     0x02 0x02 0x02 0x31 0x2f 0x03
```

For discovering devices, `LightStripAudioSync.exe` sends a discovery packet as a broadcast UDP packet every five seconds on port `3333`.
Compatible devices on your network must respond to the broadcast with a registration packet on the same port.
`LightStripAudioSync.exe` will then start sending the device the magnitudes of the audio stream on your default audio device every ~30ms.
`<LEN>` will be the number of channels of your default audio device, and `<DATA>` the respective magnitudes for each channel.

## Integrating with esphome

Example config:

```yaml
esphome:
  name: lightstripaudiosync
  friendly_name: "LightStripAudioSync"
  
  includes:
    - LightStripAudioSync/Packet.hpp

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
                Packet packet(reinterpret_cast<const char*>(data.data()), data.size());
                return packet.is_valid() && packet.is_discover();
            then:
              - udp.write:
                  id: lightstrip_audiosync_listener
                  data: !lambda |-
                    Packet packet(Packet::destination_t::broadcast, Packet::type_t::register_, 0, 0); 
                    return packet.to_raw();
                    
        - lambda: |-
            Packet packet(reinterpret_cast<const char*>(data.data()), data.size());
            if (packet.is_valid() && packet.is_data()) {
              if (packet.get_payload_size() == 2) {
                memcpy(id(magnitudes), packet.get_payload(), packet.get_payload_size());
              }
            }
```