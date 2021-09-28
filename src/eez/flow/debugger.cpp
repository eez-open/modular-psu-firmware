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
#include <inttypes.h>

#include <eez/flow/flow.h>
#include <eez/flow/private.h>

#include <eez/modules/mcu/ethernet.h>

namespace eez {
namespace flow {

enum MessagesToDebugger {
	MESSAGE_TO_DEBUGGER_STATE_CHANGED,
    MESSAGE_TO_DEBUGGER_ADD_TO_QUEUE,
    MESSAGE_TO_DEBUGGER_REMOVE_FROM_QUEUE,
    MESSAGE_TO_DEBUGGER_INPUT_CHANGED,
	MESSAGE_TO_DEBUGGER_FLOW_STATE_CREATED,
	MESSAGE_TO_DEBUGGER_FLOW_STATE_DESTROYED,
    MESSAGE_TO_DEBUGGER_LOG,
};

enum MessagesFromDebugger {
    MESSAGE_FROM_DEBUGGER_RESUME,
    MESSAGE_FROM_DEBUGGER_PAUSE,
	MESSAGE_FROM_DEBUGGER_SINGLE_STEP,
	MESSAGE_FREM_DEBUGGER_RESTART
};

enum LogItemType {
	LOG_ITEM_TYPE_FATAL,
	LOG_ITEM_TYPE_ERROR,
    LOG_ITEM_TYPE_WARNING ,
    LOG_ITEM_TYPE_SCPI,
    LOG_ITEM_TYPE_INFO,
    LOG_ITEM_TYPE_DEBUG
};

enum DebuggerState {
    DEBUGGER_STATE_RESUMED,
    DEBUGGER_STATE_PAUSED,
    DEBUGGER_STATE_SINGLE_STEP,
};

static bool g_debuggerIsConnected;
static DebuggerState g_debuggerState;

static void processDebuggerInput(char *buffer, uint32_t length);

static void setDebuggerState(DebuggerState newState) {
	if (newState != g_debuggerState) {
		g_debuggerState = newState;

		char buffer[100];
		snprintf(buffer, sizeof(buffer), "%d\t%d\n",
			MESSAGE_TO_DEBUGGER_STATE_CHANGED,
			g_debuggerState
		);
		eez::mcu::ethernet::writeDebuggerBuffer(buffer, strlen(buffer));
	}
}

void onDebuggerClientConnected() {
    g_debuggerIsConnected = true;
    setDebuggerState(DEBUGGER_STATE_PAUSED);
}

void onDebuggerClientDisconnected() {
    g_debuggerIsConnected = false;
    setDebuggerState(DEBUGGER_STATE_RESUMED);
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
    int messageFromDebugger = buffer[0] - '0';
    if (messageFromDebugger == MESSAGE_FROM_DEBUGGER_RESUME) {
        setDebuggerState(DEBUGGER_STATE_RESUMED);
    } else if (messageFromDebugger == MESSAGE_FROM_DEBUGGER_PAUSE) {
		setDebuggerState(DEBUGGER_STATE_PAUSED);
    } else if (messageFromDebugger == MESSAGE_FROM_DEBUGGER_SINGLE_STEP) {
		setDebuggerState(DEBUGGER_STATE_SINGLE_STEP);
    }
}

bool canExecuteStep() {
    if (!g_debuggerIsConnected) {
        return true;
    }

    if (g_debuggerState == DEBUGGER_STATE_RESUMED) {
        return true;
    }

    if (g_debuggerState == DEBUGGER_STATE_PAUSED) {
        return false;
    }

	setDebuggerState(DEBUGGER_STATE_PAUSED);

    return true;
}

void writeString(const char *str) {
	char tempStr[64];
	int i = 0;

#define WRITE_CH(CH) \
	tempStr[i++] = CH; \
	if (i == sizeof(tempStr)) { \
		eez::mcu::ethernet::writeDebuggerBuffer(tempStr, i); \
		i = 0; \
	}

	WRITE_CH('"');

	for (const char *p = str; *p; p++) {
		if (*p == '"') {
			WRITE_CH('\\');
			WRITE_CH('"');
		} else if (*p == '\t') {
			WRITE_CH('\\');
			WRITE_CH('t');
		} else if (*p == '\n') {
			WRITE_CH('\\');
			WRITE_CH('n');
		} else {
			WRITE_CH(*p);
		}
	}

	WRITE_CH('"');

	WRITE_CH('\n');

	if (i > 0) {
		eez::mcu::ethernet::writeDebuggerBuffer(tempStr, i);
	}
}

void writeValue(const Value &value) {
    char tempStr[64];

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4474)
#endif

    switch (value.getType()) {
        case VALUE_TYPE_STRING:
        case VALUE_TYPE_STRING_REF:
            writeString(value.getString());
            return;

        case VALUE_TYPE_BOOLEAN:
            stringCopy(tempStr, sizeof(tempStr) - 1, value.getBoolean() ? "true" : "false");
            break;

        case VALUE_TYPE_DOUBLE:
            snprintf(tempStr, sizeof(tempStr) - 1, "%g", value.doubleValue);
            break;

        case VALUE_TYPE_FLOAT:
            snprintf(tempStr, sizeof(tempStr) - 1, "%g", value.floatValue);
            break;

        case VALUE_TYPE_INT8:
            snprintf(tempStr, sizeof(tempStr) - 1, "%" PRId8 "", value.int8Value);
            break;

        case VALUE_TYPE_UINT8:
            snprintf(tempStr, sizeof(tempStr) - 1, "%" PRIu8 "", value.uint8Value);
            break;

        case VALUE_TYPE_INT16:
            snprintf(tempStr, sizeof(tempStr) - 1, "%" PRId16 "", value.int16Value);
            break;

        case VALUE_TYPE_UINT16:
            snprintf(tempStr, sizeof(tempStr) - 1, "%" PRIu16 "", value.uint16Value);
            break;

        case VALUE_TYPE_INT32:
            snprintf(tempStr, sizeof(tempStr) - 1, "%" PRId32 "", value.int32Value);
            break;

        case VALUE_TYPE_UINT32:
            snprintf(tempStr, sizeof(tempStr) - 1, "%" PRIu32 "", value.uint32Value);
            break;

        case VALUE_TYPE_INT64:
            snprintf(tempStr, sizeof(tempStr) - 1, "%" PRId64 "", value.int64Value);
            break;

        case VALUE_TYPE_UINT64:
            snprintf(tempStr, sizeof(tempStr) - 1, "%" PRIu64 "", value.uint64Value);
            break;

        default:
            stringCopy(tempStr, sizeof(tempStr) - 1, "\"TODO!\"\n");
            break;
    }

#ifdef _MSC_VER
#pragma warning(pop)
#endif

    stringAppendString(tempStr, sizeof(tempStr), "\n");

    eez::mcu::ethernet::writeDebuggerBuffer(tempStr, strlen(tempStr));
}

void onAddToQueue(FlowState *flowState, unsigned componentIndex) {
    if (g_debuggerIsConnected) {
        char buffer[100];
		snprintf(buffer, sizeof(buffer), "%d\t%d\t%d\n",
			MESSAGE_TO_DEBUGGER_ADD_TO_QUEUE,
			(int)flowState->flowStateIndex,
			componentIndex
		);
        eez::mcu::ethernet::writeDebuggerBuffer(buffer, strlen(buffer));
    }
}

void onRemoveFromQueue() {
    if (g_debuggerIsConnected) {
        char buffer[100];
		snprintf(buffer, sizeof(buffer), "%d\n",
			MESSAGE_TO_DEBUGGER_REMOVE_FROM_QUEUE
		);
        eez::mcu::ethernet::writeDebuggerBuffer(buffer, strlen(buffer));
    }
}

void onInputChanged(FlowState *flowState, uint16_t inputIndex, const Value& value) {
    if (g_debuggerIsConnected) {
        char buffer[100];
		snprintf(buffer, sizeof(buffer), "%d\t%d\t%d\t",
			MESSAGE_TO_DEBUGGER_INPUT_CHANGED,
            (int)flowState->flowStateIndex,
            inputIndex
		);
        eez::mcu::ethernet::writeDebuggerBuffer(buffer, strlen(buffer));
        writeValue(value);
    }
}

void onFlowStateCreated(FlowState *flowState) {
    if (g_debuggerIsConnected) {
        char buffer[100];
		snprintf(buffer, sizeof(buffer), "%d\t%d\t%d\t%d\n",
			MESSAGE_TO_DEBUGGER_FLOW_STATE_CREATED,
			(int)flowState->flowStateIndex,
			flowState->flowIndex,
			(int)(flowState->parentFlowState ? flowState->parentFlowState->flowStateIndex : 0)
		);
        eez::mcu::ethernet::writeDebuggerBuffer(buffer, strlen(buffer));
    }
}

void onFlowStateDestroyed(FlowState *flowState) {
	if (g_debuggerIsConnected) {
		char buffer[100];
		snprintf(buffer, sizeof(buffer), "%d\t%d\n",
			MESSAGE_TO_DEBUGGER_FLOW_STATE_DESTROYED,
			(int)flowState->flowStateIndex
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
            (int)flowState->flowStateIndex,
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
		snprintf(buffer, sizeof(buffer) - 1, "%d\t%d\t%d\t%d\tSCPI QUERY RESULT: ",
			MESSAGE_TO_DEBUGGER_LOG,
            LOG_ITEM_TYPE_SCPI,
            (int)flowState->flowStateIndex,
			componentIndex
		);
        stringAppendStringLength(buffer, sizeof(buffer) - 1, resultText, resultTextLen);
        stringAppendString(buffer, sizeof(buffer), "\n");
		eez::mcu::ethernet::writeDebuggerBuffer(buffer, strlen(buffer));
    }
}

} // namespace flow
} // namespace eez
