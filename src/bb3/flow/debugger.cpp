/*
 * EEZ Modular Firmware
 * Copyright (C) 2021-present, Envox d.o.o.
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

#include <eez/gui/thread.h>

#include <eez/flow/private.h>
#include <eez/flow/debugger.h>

#include <bb3/tasks.h>
#include <bb3/mcu/ethernet.h>

namespace eez {
namespace flow {

EEZ_MUTEX_DECLARE(flowDebugger);
static bool g_mutexInitialized = false;

char *g_toDebuggerMessage = (char *)FLOW_TO_DEBUGGER_MESSAGE_BUFFER;
uint32_t g_toDebuggerMessagePosition = 0;

void startToDebuggerMessage() {
	if (!g_mutexInitialized) {
		EEZ_MUTEX_CREATE(flowDebugger);
		g_mutexInitialized = true;
	}
}

void flushToDebuggerMessage() {
	auto toDebuggerMessagePosition = g_toDebuggerMessagePosition;
	
	eez::mcu::ethernet::writeDebuggerBuffer(g_toDebuggerMessage, toDebuggerMessagePosition);

	if (EEZ_MUTEX_WAIT(flowDebugger, 0)) {
		auto n = g_toDebuggerMessagePosition - toDebuggerMessagePosition;
		if (n > 0) {
			memmove(g_toDebuggerMessage, g_toDebuggerMessage + toDebuggerMessagePosition, n);
		}
		g_toDebuggerMessagePosition = n;
		EEZ_MUTEX_RELEASE(flowDebugger);
	}
}

void dealWithCongestedBufferSituation(const char *buffer, uint32_t length) {
	sendMessageToLowPriorityThread(FLOW_FLUSH_TO_DEBUGGER_MESSAGE);
	
	do {
		osDelay(1);
		eez::gui::processGuiQueue();

		if (!g_debuggerIsConnected) {
			return;
		}
	} while (g_toDebuggerMessagePosition + length > FLOW_TO_DEBUGGER_MESSAGE_BUFFER_SIZE);

	if (EEZ_MUTEX_WAIT(flowDebugger, 0)) {
		memcpy(g_toDebuggerMessage + g_toDebuggerMessagePosition, buffer, length);
		g_toDebuggerMessagePosition += length;
		EEZ_MUTEX_RELEASE(flowDebugger);
	}

	return;
}

void writeDebuggerBuffer(const char *buffer, uint32_t length) {
	if (!g_debuggerIsConnected) {
		return;
	}

	if (EEZ_MUTEX_WAIT(flowDebugger, 0)) {
		if (g_toDebuggerMessagePosition + length > FLOW_TO_DEBUGGER_MESSAGE_BUFFER_SIZE) {
			EEZ_MUTEX_RELEASE(flowDebugger);
			dealWithCongestedBufferSituation(buffer, length);
			return;
		}

		memcpy(g_toDebuggerMessage + g_toDebuggerMessagePosition, buffer, length);
		g_toDebuggerMessagePosition += length;
		EEZ_MUTEX_RELEASE(flowDebugger);
	}
}

void finishToDebuggerMessage() {
	if (g_debuggerIsConnected && g_toDebuggerMessagePosition > 0) {
		sendMessageToLowPriorityThread(FLOW_FLUSH_TO_DEBUGGER_MESSAGE);
	}
}

void onDebuggerInputAvailable() {
	char *buffer;
	uint32_t length;
	eez::mcu::ethernet::getDebuggerInputBuffer(&buffer, &length);
	if (buffer && length) {
		processDebuggerInput(buffer, length);
		eez::mcu::ethernet::releaseDebuggerInputBuffer();
	}
}

} // namespace flow
} // namespace eez
