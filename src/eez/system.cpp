/*
* EEZ Generic Firmware
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

#include <stdio.h>

#include <eez/system.h>
#include <eez/index.h>

#include <eez/apps/psu/psu.h>
#include <eez/apps/psu/serial_psu.h>
#include <eez/scpi/scpi.h>
using namespace eez::psu::serial;
using namespace eez::scpi;

namespace eez {

SystemState g_systemState;
int g_systemStatePhase;

static bool g_shutdown = false;

void mainTask(const void *);

#if defined(EEZ_PLATFORM_STM32)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wwrite-strings"
#endif

osThreadDef(g_mainTask, mainTask, osPriorityNormal, 0, 1024);

#if defined(EEZ_PLATFORM_STM32)
#pragma GCC diagnostic pop
#endif

osThreadId g_mainTaskHandle;

#if defined(EEZ_PLATFORM_SIMULATOR) && !defined(__EMSCRIPTEN__)
void consoleInputTask(const void *);
osThreadDef(g_consoleInputTask, consoleInputTask, osPriorityNormal, 0, 1024);
osThreadId g_consoleInputTaskHandle;
#endif

void setSystemState(SystemState systemState) {
    g_systemState = systemState;

    g_systemStatePhase = 0;

    while (true) {
        bool finalPhase = true;

        for (int i = 0; i < g_numOnSystemStateChangedCallbacks; ++i) {
            if (!g_onSystemStateChangedCallbacks[i]()) {
                finalPhase = false;
            }
        }

        if (finalPhase) {
            break;
        }

        g_systemStatePhase++;
    }

#if defined(EEZ_PLATFORM_SIMULATOR) && !defined(__EMSCRIPTEN__)
    g_consoleInputTaskHandle = osThreadCreate(osThread(g_consoleInputTask), nullptr);
#endif
}

void mainTask(const void *) {
#if defined(__EMSCRIPTEN__)
    if (!g_systemState) {
        setSystemState(BOOTING);
    }
#else
    setSystemState(BOOTING);
    while (1) {
        osDelay(1000);
    }
#endif
}

#if defined(EEZ_PLATFORM_SIMULATOR) && !defined(__EMSCRIPTEN__)
void consoleInputTask(const void *) {
    osMessagePut(g_scpiMessageQueueId, SCPI_QUEUE_SERIAL_MESSAGE(SERIAL_LINE_STATE_CHANGED, 1), osWaitForever);

    while (1) {
        int ch = getchar();
        if (ch == EOF) {
            break;
        }
        Serial.put(ch);
    }
}
#endif

void boot() {
    g_mainTaskHandle = osThreadCreate(osThread(g_mainTask), nullptr);

    // mainTask(nullptr);
    osKernelStart();

#if defined(EEZ_PLATFORM_SIMULATOR) && !defined(__EMSCRIPTEN__)
    while (!g_shutdown) {
        osDelay(100);
    }
    setSystemState(SHUTING_DOWN);
#endif
}

void shutdown() {
    g_shutdown = true;
}

} // namespace eez
