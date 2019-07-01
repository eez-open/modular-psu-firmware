/*
* EEZ PSU Firmware
* Copyright (C) 2018-present, Envox d.o.o.
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
* along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <eez/apps/psu/init.h>

#include <eez/system.h>

#include <eez/apps/psu/psu.h>

namespace eez {
namespace psu {

void mainLoop(const void *);

#if defined(EEZ_PLATFORM_STM32)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wwrite-strings"
#endif

osThreadDef(g_psuTask, mainLoop, osPriorityNormal, 0, 1024);

#if defined(EEZ_PLATFORM_STM32)
#pragma GCC diagnostic pop
#endif

osThreadId g_psuTaskHandle;

bool onSystemStateChanged() {
    if (g_systemState == eez::SystemState::BOOTING) {
        if (g_systemStatePhase == 0) {
#ifdef EEZ_PLATFORM_SIMULATOR
            eez::psu::simulator::init();
#endif
            boot();

            g_psuTaskHandle = osThreadCreate(osThread(g_psuTask), nullptr);
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
        osDelay(1);
        oneIter();
    }
#endif
}

void oneIter() {
    tick();
}

} // namespace psu
} // namespace eez
