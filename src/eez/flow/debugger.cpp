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

#include <string.h>
#include <stdio.h>

#include <eez/flow/flow.h>
#include <eez/flow/private.h>

#include <eez/modules/mcu/ethernet.h>

namespace eez {
namespace flow {

enum MessagesToDebugger {
	MESSAGE_TO_DEBUGGER_FLOW_STATE_CREATED,
	MESSAGE_TO_DEBUGGER_FLOW_STATE_DESTROYED,
    MESSAGE_TO_DEBUGGER_LOG,
};

enum LogItemType {
	LOG_ITEM_TYPE_FATAL,
	LOG_ITEM_TYPE_ERROR,
    LOG_ITEM_TYPE_WARNING ,
    LOG_ITEM_TYPE_SCPI,
    LOG_ITEM_TYPE_INFO,
    LOG_ITEM_TYPE_DEBUG
};

static bool g_debuggerIsConnected;

static void processDebuggerInput(char *buffer, uint32_t length);

void onDebuggerClientConnected() {
    g_debuggerIsConnected = true;
}

void onDebuggerClientDisconnected() {
    g_debuggerIsConnected = false;
}

void onDebuggerInputAvailable() {
    char *buffer;
    uint32_t length;
    eez::mcu::ethernet::getDebuggerInputBuffer(0, &buffer, &length);
    if (buffer && length) {
        processDebuggerInput(buffer, length);
        eez::mcu::ethernet::releaseDebuggerInputBuffer();
    }    
}

static void processDebuggerInput(char *buffer, uint32_t length) {
	
}

void onFlowStateCreated(FlowState *flowState) {
    if (g_debuggerIsConnected) {
        char buffer[100];
		snprintf(buffer, sizeof(buffer), "%d\t%d\t%d\t%d\n",
			MESSAGE_TO_DEBUGGER_FLOW_STATE_CREATED,
			flowState->flowStateIndex,
			flowState->flowIndex,
			flowState->parentFlowState ? flowState->parentFlowState->flowStateIndex : 0
		);
        eez::mcu::ethernet::writeDebuggerBuffer(buffer, strlen(buffer));
    }
}

void onFlowStateDestroyed(FlowState *flowState) {
	if (g_debuggerIsConnected) {
		char buffer[100];
		snprintf(buffer, sizeof(buffer), "%d\t%d\n",
			MESSAGE_TO_DEBUGGER_FLOW_STATE_DESTROYED,
			flowState->flowStateIndex
		);
		eez::mcu::ethernet::writeDebuggerBuffer(buffer, strlen(buffer));
	}
}

void logScpiCommand(FlowState *flowState, unsigned componentIndex, const char *cmd) {
	if (g_debuggerIsConnected) {
		char buffer[256];
		snprintf(buffer, sizeof(buffer), "%d\t%d\t%d\t%d\tSCPI COMMAND: %s\n",
			MESSAGE_TO_DEBUGGER_LOG,
            LOG_ITEM_TYPE_SCPI,
            flowState->flowStateIndex,
			componentIndex,
            cmd
		);
		eez::mcu::ethernet::writeDebuggerBuffer(buffer, strlen(buffer));
    }
}

void logScpiQuery(FlowState *flowState, unsigned componentIndex, const char *query) {
	if (g_debuggerIsConnected) {
		char buffer[256];
		snprintf(buffer, sizeof(buffer), "%d\t%d\t%d\t%d\tSCPI QUERY: %s\n",
			MESSAGE_TO_DEBUGGER_LOG,
            LOG_ITEM_TYPE_SCPI,
            (int)flowState->flowStateIndex,
			componentIndex,
            query
		);
		eez::mcu::ethernet::writeDebuggerBuffer(buffer, strlen(buffer));
    }
}

void logScpiQueryResult(FlowState *flowState, unsigned componentIndex, const char *resultText, size_t resultTextLen) {
	if (g_debuggerIsConnected) {
		char buffer[256];
		snprintf(buffer, sizeof(buffer), "%d\t%d\t%d\t%d\tSCPI QUERY RESULT: ",
			MESSAGE_TO_DEBUGGER_LOG,
            LOG_ITEM_TYPE_SCPI,
            (int)flowState->flowStateIndex,
			componentIndex
		);
        stringAppendStringLength(buffer, sizeof(buffer), resultText, resultTextLen);
        stringAppendString(buffer, sizeof(buffer), "\n");
		eez::mcu::ethernet::writeDebuggerBuffer(buffer, strlen(buffer));
    }
}

} // namespace flow
} // namespace eez
