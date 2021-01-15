[![GitHub release](https://img.shields.io/github/release/eez-open/modular-psu-firmware.svg)](https://github.com/eez-open/modular-psu-firmware/releases)  [![license](https://img.shields.io/github/license/eez-open/modular-psu-firmware.svg)](https://github.com/eez-open/modular-psu-firmware/blob/master/LICENSE.TXT) [![liberapay](https://img.shields.io/liberapay/receives/eez-open.svg?logo=liberapay)](https://liberapay.com/eez-open/donate)

### Ownership and License

The contributors are listed in CONTRIB.TXT. This project uses the GPL v3 license, see LICENSE.TXT.
EEZ psu-firmware uses the [C4.1 (Collective Code Construction Contract)](http://rfc.zeromq.org/spec:22) process for contributions.
To report an issue, use the [EEZ modular-psu-firmware issue](https://github.com/eez-open/modular-psu-firmware/issues) tracker.

## Introduction

Firmare for STM32F7 MCU used in [EEZ BB3](https://github.com/eez-open/modular-psu) Test & Measurement chassis. 
Currently supported modules:

* [DCP405](https://github.com/eez-open/modular-psu/tree/master/dcp405) 0 - 40 V / 5 A programmable power source
* [DCM220](https://github.com/eez-open/modular-psu/tree/master/dcm220) dual 1 - 20 V / 4 A auxiliary power source

Under development:

* [MIO168](https://github.com/eez-open/dib-mio168) mixed I/O module
* [PREL6](https://github.com/eez-open/dib-prel6) 6 power relays module
* [SMX46](https://github.com/eez-open/dib-smx46) 4 x 6 programmable switch matrix 

---

_For EEZ H24005 firmware visit [psu-firmware](https://github.com/eez-open/psu-firmware) repository._

## Build

### Firmware Simulator

#### Linux

```
sudo apt-get update
sudo apt-get install -y git libsdl2-dev libsdl2-image-dev libsdl2-ttf-dev cmake build-essential libbsd-dev
git clone https://github.com/eez-open/modular-psu-firmware
mkdir -p modular-psu-firmware/build/linux
cd modular-psu-firmware/build/linux
cmake ../..
make
```

#### Emscripten

[Download and install Emscripten](https://emscripten.org/docs/getting_started/downloads.html)

```
source /path/to/emsdk/emsdk_env.sh
export EMSCRIPTEN=/path/to/emsdk/upstream/emscripten
mkdir -p /path/to/modular-psu-firmware/build/emscripten
cd /path/to/modular-psu-firmware/build/emscripten
cmake -DCMAKE_TOOLCHAIN_FILE=../../cmake/Emscripten.cmake -DCMAKE_BUILD_TYPE=Debug -G "Unix Makefiles" ../..
make
```

#### Windows

Install [Visual Studio Community 2017](https://visualstudio.microsoft.com/downloads/) and [CMake](https://cmake.org/install/).

Use git to clone https://github.com/eez-open/modular-psu-firmware.

Execute `cmake.bat`.

Visual Studio solution is created in `\path\to\modular-psu-firmware\build\win32`.

### STM32 firmware

Import project from `/path/to/modular-psu-firmware/src/third_party/stm32_cubeide` into [STM32CubeIDE](https://www.st.com/en/development-tools/stm32cubeide.html) and build it.
