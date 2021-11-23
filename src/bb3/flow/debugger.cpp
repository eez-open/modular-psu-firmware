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

#include <eez/flow/private.h>
#include <eez/flow/debugger.h>

#include <bb3/tasks.h>
#include <bb3/mcu/ethernet.h>

namespace eez {
namespace flow {

char *g_toDebuggerMessage = (char *)FLOW_TO_DEBUGGER_MESSAGE_BUFFER;
uint32_t g_toDebuggerMessagePosition = 0;

void startToDebuggerMessage() {
}

void flushToDebuggerMessageBuffer() {
	if (g_toDebuggerMessagePosition != 0) {
		sendMessageToLowPriorityThread(FLOW_FLUSH_TO_DEBUGGER_MESSAGE);

		while (g_toDebuggerMessagePosition != 0 && g_debuggerIsConnected) {
			osDelay(1);
			WATCHDOG_RESET(WATCHDOG_GUI_THREAD);
		}
	}
}

void flushToDebuggerMessage() {
	eez::mcu::ethernet::writeDebuggerBuffer(g_toDebuggerMessage, g_toDebuggerMessagePosition);
	g_toDebuggerMessagePosition = 0;
}

void writeDebuggerBuffer(const char *buffer, uint32_t length) {
	if (g_toDebuggerMessagePosition + length > FLOW_TO_DEBUGGER_MESSAGE_BUFFER_SIZE) {
		flushToDebuggerMessageBuffer();
	}

	memcpy(g_toDebuggerMessage + g_toDebuggerMessagePosition, buffer, length);
	g_toDebuggerMessagePosition += length;
}

void finishToDebuggerMessage() {
	flushToDebuggerMessageBuffer();
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
