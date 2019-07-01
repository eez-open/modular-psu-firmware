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

### Emscripten

```
source ~/emsdk/emsdk_env.sh

export EMSCRIPTEN=/path/to/emsdk/fastcomp/emscripten

mkdir /path/to/modular-psu-firmware/build/emscripten

cd /path/to/modular-psu-firmware/build/emscripten

cmake -DCMAKE_TOOLCHAIN_FILE=../../cmake/Emscripten.cmake -DCMAKE_BUILD_TYPE=Debug -G "Unix Makefiles" ../..
```