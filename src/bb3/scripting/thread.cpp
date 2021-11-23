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

#include <bb3/system.h>

#include <eez/flow/flow.h>

#include <bb3/scripting/scripting.h>
#include <bb3/scripting/scpi_context.h>
#include <bb3/scripting/mp.h>
#include <bb3/scripting/flow.h>

namespace eez {
namespace scripting {

enum {
	QUEUE_MESSAGE_START_MP_SCRIPT,
	QUEUE_MESSAGE_SCPI_RESULT
};
	
void mainLoop(const void *);

osThreadId g_mpTaskHandle;

#if defined(EEZ_PLATFORM_STM32)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wwrite-strings"
#endif

osThreadDef(g_mpTask, mainLoop, osPriorityBelowNormal, 0, 4096);

#if defined(EEZ_PLATFORM_STM32)
#pragma GCC diagnostic pop
#endif

#define MP_QUEUE_SIZE 5

osMessageQDef(g_mpMessageQueue, MP_QUEUE_SIZE, uint32_t);
osMessageQId g_mpMessageQueueId;

////////////////////////////////////////////////////////////////////////////////

void initMessageQueue() {
    initScpiContext();
    g_mpMessageQueueId = osMessageCreate(osMessageQ(g_mpMessageQueue), 0);
}

void startThread() {
    g_mpTaskHandle = osThreadCreate(osThread(g_mpTask), nullptr);
}

void terminateThread() {
	osThreadTerminate(g_mpTaskHandle);
}

void startMpScriptInScriptingThread() {
	osMessagePut(g_mpMessageQueueId, QUEUE_MESSAGE_START_MP_SCRIPT, osWaitForever);
}

void scpiResultIsReady() {
	osMessagePut(g_mpMessageQueueId, QUEUE_MESSAGE_SCPI_RESULT, osWaitForever);
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
    osEvent event = osMessageGet(g_mpMessageQueueId, 1);
    if (event.status == osEventMessage) {
		if (event.value.v == QUEUE_MESSAGE_START_MP_SCRIPT) {
			startMpScript();
		}
	}
}

bool waitScpiResult() {
	static const uint32_t SCPI_TIMEOUT = 60 * 60 * 1000;

	while (true) {
		osEvent event = osMessageGet(g_mpMessageQueueId, SCPI_TIMEOUT);
		if (event.status == osEventMessage) {
			if (event.value.v == QUEUE_MESSAGE_SCPI_RESULT) {
				return true;
			}
		} else {
			return false;
		}
	}
}

} // scripting
} // eez
