[![GitHub release](https://img.shields.io/github/release/eez-open/modular-psu-firmware.svg)](https://github.com/eez-open/modular-psu-firmware/releases)
[![license](https://img.shields.io/github/license/eez-open/modular-psu-firmware.svg)](https://github.com/eez-open/modular-psu-firmware/blob/master/LICENSE.TXT)

### Ownership and License

The contributors are listed in CONTRIB.TXT. This project uses the GPL v3 license, see LICENSE.TXT.
EEZ psu-firmware uses the [C4.1 (Collective Code Construction Contract)](http://rfc.zeromq.org/spec:22) process for contributions.
To report an issue, use the [EEZ modular-psu-firmware issue](https://github.com/eez-open/modular-psu-firmware/issues) tracker.

## Introduction

Work in progress ... 

_For existing Programmable Power supply (EEZ H24005) firmware visit [psu-firmware](https://github.com/eez-open/psu-firmware) repository._

## Build

### Linux

```
sudo apt-get update
sudo apt-get install -y git libsdl2-dev libsdl2-image-dev libsdl2-ttf-dev cmake build-essentials
git clone https://github.com/eez-open/modular-psu-firmware
mkdir -p modular-psu-firmware/build/linux
cd modular-psu-firmware/build/linux
cmake ../..
make
```

### Emscripten

[Download and install Emscripten](https://emscripten.org/docs/getting_started/downloads.html)

```
source /path/to/emsdk/emsdk_env.sh
export EMSCRIPTEN=/path/to/emsdk/fastcomp/emscripten
mkdir -p /path/to/modular-psu-firmware/build/emscripten
cd /path/to/modular-psu-firmware/build/emscripten
cmake -DCMAKE_TOOLCHAIN_FILE=../../cmake/Emscripten.cmake -DCMAKE_BUILD_TYPE=Debug -G "Unix Makefiles" ../..
make
```

### Windows

Install [Visual Studio Community 2017](https://visualstudio.microsoft.com/downloads/) and [CMake](https://cmake.org/install/).

Use git to clone https://github.com/eez-open/modular-psu-firmware.

Create folder `\path\to\modular-psu-firmware\build\windows'.

From command line:

```
cd \path\to\modular-psu-firmware\build\windows
cmake ..\..
```

Visual Studio solution is created in `\path\to\modular-psu-firmware\build\windows'.

### STM32 firmware

Import project from `/path/to/modular-psu-firmware/src/third_party/stm32` into [TrueStudio](https://atollic.com/truestudio/) and build it.