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

#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <inttypes.h>

#include <eez/core/debug.h>
#include <eez/core/os.h>

#include <eez/flow/flow.h>
#include <eez/flow/private.h>
#include <eez/flow/debugger.h>
#include <eez/flow/hooks.h>

namespace eez {
namespace flow {

enum MessagesToDebugger {
    MESSAGE_TO_DEBUGGER_STATE_CHANGED, // STATE

    MESSAGE_TO_DEBUGGER_ADD_TO_QUEUE, // FLOW_STATE_INDEX, SOURCE_COMPONENT_INDEX, SOURCE_OUTPUT_INDEX, TARGET_COMPONENT_INDEX, TARGET_INPUT_INDEX
    MESSAGE_TO_DEBUGGER_REMOVE_FROM_QUEUE, // no params

    MESSAGE_TO_DEBUGGER_GLOBAL_VARIABLE_INIT, // GLOBAL_VARIABLE_INDEX, VALUE_ADDR, VALUE
    MESSAGE_TO_DEBUGGER_LOCAL_VARIABLE_INIT, // FLOW_STATE_INDEX, LOCAL_VARIABLE_INDEX, VALUE_ADDR, VALUE
    MESSAGE_TO_DEBUGGER_COMPONENT_INPUT_INIT, // FLOW_STATE_INDEX, COMPONENT_INPUT_INDEX, VALUE_ADDR, VALUE

    MESSAGE_TO_DEBUGGER_VALUE_CHANGED, // VALUE_ADDR, VALUE

    MESSAGE_TO_DEBUGGER_FLOW_STATE_CREATED, // FLOW_STATE_INDEX, FLOW_INDEX, PARENT_FLOW_STATE_INDEX (-1 - NO PARENT), PARENT_COMPONENT_INDEX (-1 - NO PARENT COMPONENT)
    MESSAGE_TO_DEBUGGER_FLOW_STATE_DESTROYED, // FLOW_STATE_INDEX

	MESSAGE_TO_DEBUGGER_FLOW_STATE_ERROR, // FLOW_STATE_INDEX, COMPONENT_INDEX, ERROR_MESSAGE

    MESSAGE_TO_DEBUGGER_LOG, // LOG_ITEM_TYPE, FLOW_STATE_INDEX, COMPONENT_INDEX, MESSAGE

	MESSAGE_TO_DEBUGGER_PAGE_CHANGED // PAGE_ID
};

enum MessagesFromDebugger {
    MESSAGE_FROM_DEBUGGER_RESUME, // no params
    MESSAGE_FROM_DEBUGGER_PAUSE, // no params
    MESSAGE_FROM_DEBUGGER_SINGLE_STEP, // no params

    MESSAGE_FROM_DEBUGGER_ADD_BREAKPOINT, // FLOW_INDEX, COMPONENT_INDEX
    MESSAGE_FROM_DEBUGGER_REMOVE_BREAKPOINT, // FLOW_INDEX, COMPONENT_INDEX
    MESSAGE_FROM_DEBUGGER_ENABLE_BREAKPOINT, // FLOW_INDEX, COMPONENT_INDEX
    MESSAGE_FROM_DEBUGGER_DISABLE_BREAKPOINT, // FLOW_INDEX, COMPONENT_INDEX
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

bool g_debuggerIsConnected;
static DebuggerState g_debuggerState;
static bool g_skipNextBreakpoint;

static char g_inputFromDebugger[64];
static unsigned g_inputFromDebuggerPosition;

////////////////////////////////////////////////////////////////////////////////

static void setDebuggerState(DebuggerState newState) {
	if (newState != g_debuggerState) {
		g_debuggerState = newState;

		if (g_debuggerIsConnected) {
			startToDebuggerMessageHook();

			char buffer[100];
			snprintf(buffer, sizeof(buffer), "%d\t%d\n",
				MESSAGE_TO_DEBUGGER_STATE_CHANGED,
				g_debuggerState
			);
			writeDebuggerBufferHook(buffer, strlen(buffer));
		}
	}
}

////////////////////////////////////////////////////////////////////////////////

void onDebuggerClientConnected() {
    g_debuggerIsConnected = true;

	g_skipNextBreakpoint = false;
	g_inputFromDebuggerPosition = 0;

    setDebuggerState(DEBUGGER_STATE_PAUSED);
}

void onDebuggerClientDisconnected() {
    g_debuggerIsConnected = false;
    setDebuggerState(DEBUGGER_STATE_RESUMED);
}

////////////////////////////////////////////////////////////////////////////////

void processDebuggerInput(char *buffer, uint32_t length) {
	for (uint32_t i = 0; i < length; i++) {
		if (buffer[i] == '\n') {
			int messageFromDebugger = g_inputFromDebugger[0] - '0';
			
			if (messageFromDebugger == MESSAGE_FROM_DEBUGGER_RESUME) {
				setDebuggerState(DEBUGGER_STATE_RESUMED);
			} else if (messageFromDebugger == MESSAGE_FROM_DEBUGGER_PAUSE) {
				setDebuggerState(DEBUGGER_STATE_PAUSED);
			} else if (messageFromDebugger == MESSAGE_FROM_DEBUGGER_SINGLE_STEP) {
				setDebuggerState(DEBUGGER_STATE_SINGLE_STEP);
			} else if (
				messageFromDebugger >= MESSAGE_FROM_DEBUGGER_ADD_BREAKPOINT &&
				messageFromDebugger <= MESSAGE_FROM_DEBUGGER_DISABLE_BREAKPOINT
			) {
				char *p;
				auto flowIndex = (uint32_t)strtol(g_inputFromDebugger + 2, &p, 10);
				auto componentIndex = (uint32_t)strtol(p + 1, nullptr, 10);

				auto assets = g_mainPageFlowState->assets;
				auto flowDefinition = static_cast<FlowDefinition *>(assets->flowDefinition);
				if (flowIndex >= 0 && flowIndex < flowDefinition->flows.count) {
					auto flow = flowDefinition->flows[flowIndex];
					if (componentIndex >= 0 && componentIndex < flow->components.count) {
						auto component = flow->components[componentIndex];
						
						component->breakpoint = messageFromDebugger == MESSAGE_FROM_DEBUGGER_ADD_BREAKPOINT ||
							messageFromDebugger == MESSAGE_FROM_DEBUGGER_ENABLE_BREAKPOINT ? 1 : 0;
					} else {
						ErrorTrace("Invalid breakpoint component index\n");
					}
				} else {
					ErrorTrace("Invalid breakpoint flow index\n");
				}
			}

			g_inputFromDebuggerPosition = 0;
		} else {
			if (g_inputFromDebuggerPosition < sizeof(g_inputFromDebugger)) {
				g_inputFromDebugger[g_inputFromDebuggerPosition++] = buffer[i];
			} else if (g_inputFromDebuggerPosition == sizeof(g_inputFromDebugger)) {
				ErrorTrace("Input from debugger buffer overflow\n");
			}
		}
	}
}

////////////////////////////////////////////////////////////////////////////////

bool canExecuteStep(FlowState *&flowState, unsigned &componentIndex) {
    if (!g_debuggerIsConnected) {
        return true;
    }

    if (g_debuggerState == DEBUGGER_STATE_PAUSED) {
        return false;
    }

    if (g_debuggerState == DEBUGGER_STATE_SINGLE_STEP) {
        g_skipNextBreakpoint = false;
	    setDebuggerState(DEBUGGER_STATE_PAUSED);
        return true;
    }

    // g_debuggerState == DEBUGGER_STATE_RESUMED

    if (g_skipNextBreakpoint) {
        g_skipNextBreakpoint = false;
    } else {
        auto component = flowState->flow->components[componentIndex];
        if (component->breakpoint) {
            g_skipNextBreakpoint = true;
			setDebuggerState(DEBUGGER_STATE_PAUSED);
            return false;
        }
    }

    return true;
}

////////////////////////////////////////////////////////////////////////////////

char outputBuffer[64];
int outputBufferPosition = 0;

#define WRITE_TO_OUTPUT_BUFFER(ch) \
	outputBuffer[outputBufferPosition++] = ch; \
	if (outputBufferPosition == sizeof(outputBuffer)) { \
		writeDebuggerBufferHook(outputBuffer, outputBufferPosition); \
		outputBufferPosition = 0; \
	}

#define FLUSH_OUTPUT_BUFFER() \
	if (outputBufferPosition > 0) { \
		writeDebuggerBufferHook(outputBuffer, outputBufferPosition); \
		outputBufferPosition = 0; \
	}

void writeValueAddr(const void *pValue) {
	char tmpStr[32];
	snprintf(tmpStr, sizeof(tmpStr), "%p", pValue);
	auto len = strlen(tmpStr);
	for (size_t i = 0; i < len; i++) {
		WRITE_TO_OUTPUT_BUFFER(tmpStr[i]);
	}
}

void writeString(const char *str) {
	WRITE_TO_OUTPUT_BUFFER('"');

	for (const char *p = str; *p; p++) {
		if (*p == '"') {
			WRITE_TO_OUTPUT_BUFFER('\\');
			WRITE_TO_OUTPUT_BUFFER('"');
		} else if (*p == '\t') {
			WRITE_TO_OUTPUT_BUFFER('\\');
			WRITE_TO_OUTPUT_BUFFER('t');
		} else if (*p == '\n') {
			WRITE_TO_OUTPUT_BUFFER('\\');
			WRITE_TO_OUTPUT_BUFFER('n');
		} else {
			WRITE_TO_OUTPUT_BUFFER(*p);
		}
	}

	WRITE_TO_OUTPUT_BUFFER('"');
	WRITE_TO_OUTPUT_BUFFER('\n');
	FLUSH_OUTPUT_BUFFER();
}

void writeArray(ArrayValue *arrayValue) {
	WRITE_TO_OUTPUT_BUFFER('{');

	writeValueAddr(arrayValue);

	for (uint32_t i = 0; i < arrayValue->arraySize; i++) {
		WRITE_TO_OUTPUT_BUFFER(',');
		writeValueAddr(&arrayValue->values[i]);
	}

	WRITE_TO_OUTPUT_BUFFER('}');
	WRITE_TO_OUTPUT_BUFFER('\n');
	FLUSH_OUTPUT_BUFFER();

	for (uint32_t i = 0; i < arrayValue->arraySize; i++) {
		onValueChanged(&arrayValue->values[i]);
	}
}

void writeValue(const Value &value) {
	char tempStr[64];

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4474)
#endif

	switch (value.getType()) {
	case VALUE_TYPE_UNDEFINED:
		stringCopy(tempStr, sizeof(tempStr) - 1, "undefined\n");
		break;

	case VALUE_TYPE_NULL:
		stringCopy(tempStr, sizeof(tempStr) - 1, "null\n");
		break;

	case VALUE_TYPE_BOOLEAN:
		stringCopy(tempStr, sizeof(tempStr) - 1, value.getBoolean() ? "true" : "false");
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

	case VALUE_TYPE_DOUBLE:
		snprintf(tempStr, sizeof(tempStr) - 1, "%g", value.doubleValue);
		break;

	case VALUE_TYPE_FLOAT:
		snprintf(tempStr, sizeof(tempStr) - 1, "%g", value.floatValue);
		break;

	case VALUE_TYPE_STRING:
	case VALUE_TYPE_STRING_REF:
		writeString(value.getString());
		return;

	case VALUE_TYPE_ARRAY:
		writeArray(value.arrayValue);
		return;

	default:
		tempStr[0] = 0;
		break;
	}

#ifdef _MSC_VER
#pragma warning(pop)
#endif

	stringAppendString(tempStr, sizeof(tempStr), "\n");

	writeDebuggerBufferHook(tempStr, strlen(tempStr));
}

////////////////////////////////////////////////////////////////////////////////

void onStarted(Assets *assets) {
    if (g_debuggerIsConnected) {
		startToDebuggerMessageHook();

		auto flowDefinition = static_cast<FlowDefinition *>(assets->flowDefinition);

		for (uint32_t i = 0; i < flowDefinition->globalVariables.count; i++) {
			auto pValue = flowDefinition->globalVariables[i];

            char buffer[100];
            snprintf(buffer, sizeof(buffer), "%d\t%d\t%p\t",
                MESSAGE_TO_DEBUGGER_GLOBAL_VARIABLE_INIT,
				(int)i,
                (const void *)pValue
            );
            writeDebuggerBufferHook(buffer, strlen(buffer));

			writeValue(*pValue);
        }
    }
}

void onAddToQueue(FlowState *flowState, int sourceComponentIndex, int sourceOutputIndex, unsigned targetComponentIndex, int targetInputIndex) {
    if (g_debuggerIsConnected) {
		startToDebuggerMessageHook();

        char buffer[100];
		snprintf(buffer, sizeof(buffer), "%d\t%d\t%d\t%d\t%d\t%d\n",
			MESSAGE_TO_DEBUGGER_ADD_TO_QUEUE,
			(int)flowState->flowStateIndex,
			sourceComponentIndex,
			sourceOutputIndex,
			targetComponentIndex,
			targetInputIndex
		);
        writeDebuggerBufferHook(buffer, strlen(buffer));
    }
}

void onRemoveFromQueue() {
    if (g_debuggerIsConnected) {
		startToDebuggerMessageHook();

        char buffer[100];
		snprintf(buffer, sizeof(buffer), "%d\n",
			MESSAGE_TO_DEBUGGER_REMOVE_FROM_QUEUE
		);
        writeDebuggerBufferHook(buffer, strlen(buffer));
    }
}

void onValueChanged(const Value *pValue) {
    if (g_debuggerIsConnected) {
		startToDebuggerMessageHook();

        char buffer[100];
		snprintf(buffer, sizeof(buffer), "%d\t%p\t",
			MESSAGE_TO_DEBUGGER_VALUE_CHANGED,
            (const void *)pValue
		);
        writeDebuggerBufferHook(buffer, strlen(buffer));
        
		writeValue(*pValue);
    }
}

void onFlowStateCreated(FlowState *flowState) {
    if (g_debuggerIsConnected) {
		startToDebuggerMessageHook();

        char buffer[100];
		snprintf(buffer, sizeof(buffer), "%d\t%d\t%d\t%d\t%d\n",
			MESSAGE_TO_DEBUGGER_FLOW_STATE_CREATED,
			(int)flowState->flowStateIndex,
			flowState->flowIndex,
			(int)(flowState->parentFlowState ? flowState->parentFlowState->flowStateIndex : -1),
			flowState->parentComponentIndex
		);
        writeDebuggerBufferHook(buffer, strlen(buffer));
		
		auto flow = flowState->flow;

		for (uint32_t i = 0; i < flow->localVariables.count; i++) {
			auto pValue = &flowState->values[flow->componentInputs.count + i];

            char buffer[100];
            snprintf(buffer, sizeof(buffer), "%d\t%d\t%d\t%p\t",
                MESSAGE_TO_DEBUGGER_LOCAL_VARIABLE_INIT,
				(int)flowState->flowStateIndex,
				(int)i,
                (const void *)pValue
            );
            writeDebuggerBufferHook(buffer, strlen(buffer));

			writeValue(*pValue);
        }

		for (uint32_t i = 0; i < flow->componentInputs.count; i++) {
			auto &input = flow->componentInputs[i];
			if (!(input & COMPONENT_INPUT_FLAG_IS_SEQ_INPUT)) {
				auto pValue = &flowState->values[i];

				char buffer[100];
				snprintf(buffer, sizeof(buffer), "%d\t%d\t%d\t%p\t",
					MESSAGE_TO_DEBUGGER_COMPONENT_INPUT_INIT,
					(int)flowState->flowStateIndex,
					(int)i,
					(const void *)pValue
				);
				writeDebuggerBufferHook(buffer, strlen(buffer));

				writeValue(*pValue);
			}
        }
	}
}

void onFlowStateDestroyed(FlowState *flowState) {
	if (g_debuggerIsConnected) {
		startToDebuggerMessageHook();

		char buffer[100];
		snprintf(buffer, sizeof(buffer), "%d\t%d\n",
			MESSAGE_TO_DEBUGGER_FLOW_STATE_DESTROYED,
			(int)flowState->flowStateIndex
		);
		writeDebuggerBufferHook(buffer, strlen(buffer));
	}
}

void onFlowError(FlowState *flowState, int componentIndex, const char *errorMessage) {
	if (g_debuggerIsConnected) {
		startToDebuggerMessageHook();
		
		char buffer[100];
		snprintf(buffer, sizeof(buffer), "%d\t%d\t%d\t",
			MESSAGE_TO_DEBUGGER_FLOW_STATE_ERROR,
			(int)flowState->flowStateIndex,
			componentIndex
		);
		writeDebuggerBufferHook(buffer, strlen(buffer));
		writeString(errorMessage);
	}
}

void writeLogMessage(const char *str) {
	for (const char *p = str; *p; p++) {
		if (*p == '\t') {
			WRITE_TO_OUTPUT_BUFFER('\\');
			WRITE_TO_OUTPUT_BUFFER('t');
		} if (*p == '\n') {
			WRITE_TO_OUTPUT_BUFFER('\\');
			WRITE_TO_OUTPUT_BUFFER('n');
		} else {
			WRITE_TO_OUTPUT_BUFFER(*p);
		}
	}

	WRITE_TO_OUTPUT_BUFFER('\n');
	FLUSH_OUTPUT_BUFFER();
}

void writeLogMessage(const char *str, size_t len) {
	for (size_t i = 0; i < len; i++) {
		if (str[i] == '\t') {
			WRITE_TO_OUTPUT_BUFFER('\\');
			WRITE_TO_OUTPUT_BUFFER('t');
		} if (str[i] == '\n') {
			WRITE_TO_OUTPUT_BUFFER('\\');
			WRITE_TO_OUTPUT_BUFFER('n');
		} else {
			WRITE_TO_OUTPUT_BUFFER(str[i]);
		}
	}

	WRITE_TO_OUTPUT_BUFFER('\n');
	FLUSH_OUTPUT_BUFFER();
}

void logInfo(FlowState *flowState, unsigned componentIndex, const char *message) {
	if (g_debuggerIsConnected) {
		startToDebuggerMessageHook();
				
		char buffer[256];
		snprintf(buffer, sizeof(buffer), "%d\t%d\t%d\t%d\t",
			MESSAGE_TO_DEBUGGER_LOG,
            LOG_ITEM_TYPE_INFO,
            (int)flowState->flowStateIndex,
			componentIndex
		);
		writeDebuggerBufferHook(buffer, strlen(buffer));
		writeLogMessage(message);
    }
}

void logScpiCommand(FlowState *flowState, unsigned componentIndex, const char *cmd) {
	if (g_debuggerIsConnected) {
		startToDebuggerMessageHook();

		char buffer[256];
		snprintf(buffer, sizeof(buffer), "%d\t%d\t%d\t%d\tSCPI COMMAND: ",
			MESSAGE_TO_DEBUGGER_LOG,
            LOG_ITEM_TYPE_SCPI,
            (int)flowState->flowStateIndex,
			componentIndex
		);
		writeDebuggerBufferHook(buffer, strlen(buffer));
		writeLogMessage(cmd);
    }
}

void logScpiQuery(FlowState *flowState, unsigned componentIndex, const char *query) {
	if (g_debuggerIsConnected) {
		startToDebuggerMessageHook();

		char buffer[256];
		snprintf(buffer, sizeof(buffer), "%d\t%d\t%d\t%d\tSCPI QUERY: ",
			MESSAGE_TO_DEBUGGER_LOG,
            LOG_ITEM_TYPE_SCPI,
            (int)flowState->flowStateIndex,
			componentIndex
		);
		writeDebuggerBufferHook(buffer, strlen(buffer));
		writeLogMessage(query);
    }
}

void logScpiQueryResult(FlowState *flowState, unsigned componentIndex, const char *resultText, size_t resultTextLen) {
	if (g_debuggerIsConnected) {
		startToDebuggerMessageHook();

		char buffer[256];
		snprintf(buffer, sizeof(buffer) - 1, "%d\t%d\t%d\t%d\tSCPI QUERY RESULT: ",
			MESSAGE_TO_DEBUGGER_LOG,
            LOG_ITEM_TYPE_SCPI,
            (int)flowState->flowStateIndex,
			componentIndex
		);
		writeDebuggerBufferHook(buffer, strlen(buffer));
		writeLogMessage(resultText, resultTextLen);
    }
}

void onPageChanged(int pageId) {
	if (g_debuggerIsConnected) {
		startToDebuggerMessageHook();

		char buffer[100];
		snprintf(buffer, sizeof(buffer), "%d\t%d\n",
			MESSAGE_TO_DEBUGGER_PAGE_CHANGED,
			-pageId - 1
		);
		writeDebuggerBufferHook(buffer, strlen(buffer));
	}
}

} // namespace flow
} // namespace eez
