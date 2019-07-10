/*
* EEZ Generic Firmware
* Copyright (C) 2019-present, Envox d.o.o.
*
* This program is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.

* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.

* You should have received a copy of the GNU General Public License
* along with this program.  If not, see http://www.gnu.org/licenses.
*/

#include <eez/scpi/scpi.h>

#include <eez/system.h>

#include <eez/apps/psu/psu.h>
#include <eez/apps/psu/serial_psu.h>
#if OPTION_ETHERNET
#include <eez/apps/psu/ethernet.h>
#endif

namespace eez {
namespace scpi {

void mainLoop(const void *);

osThreadId g_scpiTaskHandle;

#if defined(EEZ_PLATFORM_STM32)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wwrite-strings"
#endif

osThreadDef(g_scpiTask, mainLoop, osPriorityNormal, 0, 1024);

#if defined(EEZ_PLATFORM_STM32)
#pragma GCC diagnostic pop
#endif

bool onSystemStateChanged() {
    if (eez::g_systemState == eez::SystemState::BOOTING) {
        if (eez::g_systemStatePhase == 0) {
            g_scpiTaskHandle = osThreadCreate(osThread(g_scpiTask), nullptr);
        }
    }

    return true;
}

void oneIter();

void mainLoop(const void *) {
#ifdef __EMSCRIPTEN__
    oneIter();
#else
    while (1) {
        oneIter();
    }
#endif
}

void oneIter() {
    uint32_t tick_usec = micros();

    psu::serial::tick(tick_usec);

#if OPTION_ETHERNET
    psu::ethernet::tick(tick_usec);
#endif

    delay(1);
}


}
}