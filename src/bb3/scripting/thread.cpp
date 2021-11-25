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

#include <eez/os.h>

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
	
void mainLoop(void *);

EEZ_THREAD_DECLARE(MP, BelowNormal, 4096);

#define MP_QUEUE_SIZE 5

EEZ_MESSAGE_QUEUE_DECLARE(MP, {
	uint8_t type;
	uint32_t param;
});

////////////////////////////////////////////////////////////////////////////////

void initMessageQueue() {
    initScpiContext();
	EEZ_MESSAGE_QUEUE_CREATE(MP, MP_QUEUE_SIZE);
}

void startThread() {
	EEZ_THREAD_CREATE(MP, mainLoop);
}

void terminateThread() {
	EEZ_THREAD_TERMINATE(MP);
}

void startMpScriptInScriptingThread() {
    MPMessageQueueObject obj;
    obj.type = QUEUE_MESSAGE_START_MP_SCRIPT;
	EEZ_MESSAGE_QUEUE_PUT(MP, obj, osWaitForever);
}

void scpiResultIsReady() {
    MPMessageQueueObject obj;
    obj.type = QUEUE_MESSAGE_SCPI_RESULT;
	EEZ_MESSAGE_QUEUE_PUT(MP, obj, osWaitForever);
}

void oneIter();

void mainLoop(void *) {
#ifdef __EMSCRIPTEN__
    oneIter();
#else
    while (1) {
        oneIter();
    }
#endif
}

void oneIter() {
    MPMessageQueueObject obj;
	if (EEZ_MESSAGE_QUEUE_GET(MP, obj, 1)) {
		if (obj.type == QUEUE_MESSAGE_START_MP_SCRIPT) {
			startMpScript();
		}
	}
}

bool waitScpiResult() {
	static const uint32_t SCPI_TIMEOUT = 60 * 60 * 1000;

	while (true) {
		MPMessageQueueObject obj;
		if (EEZ_MESSAGE_QUEUE_GET(MP, obj, SCPI_TIMEOUT)) {
			if (obj.type == QUEUE_MESSAGE_SCPI_RESULT) {
				return true;
			}
		} else {
			return false;
		}
	}
}

} // scripting
} // eez
