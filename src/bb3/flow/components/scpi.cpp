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

#include <stdio.h>

#include <eez/core/alloc.h>

#include <eez/flow/flow.h>
#include <eez/flow/components.h>
#include <eez/flow/flow_defs_v3.h>
#include <eez/flow/expression.h>
#include <eez/flow/queue.h>
#include <eez/flow/debugger.h>
#include <eez/flow/hooks.h>

#include <bb3/psu/psu.h>
#include <bb3/psu/datetime.h>
#include <bb3/psu/scpi/psu.h>

using namespace eez::gui;

namespace eez {
namespace flow {

////////////////////////////////////////////////////////////////////////////////

using namespace eez::scpi;
using namespace eez::psu::scpi;

static char g_scpiData[SCPI_PARSER_INPUT_BUFFER_LENGTH + 1];
static size_t g_scpiDataLen;
static int_fast16_t g_lastError;

size_t SCPI_Write(scpi_t *context, const char *data, size_t len) {
    len = MIN(len, SCPI_PARSER_INPUT_BUFFER_LENGTH - g_scpiDataLen);
    if (len > 0) {
        memcpy(g_scpiData + g_scpiDataLen, data, len);
        g_scpiDataLen += len;
        g_scpiData[g_scpiDataLen] = 0;
    }
    return len;
}

scpi_result_t SCPI_Flush(scpi_t *context) {
    return SCPI_RES_OK;
}

int SCPI_Error(scpi_t *context, int_fast16_t err) {
    g_lastError = err;

    if (err != 0) {
        DebugTrace("**ERROR");

        char datetime_buffer[32] = { 0 };
        if (psu::datetime::getDateTimeAsString(datetime_buffer)) {
            DebugTrace(" [%s]", datetime_buffer);
        }

        DebugTrace(": %d,\"%s\"\r\n", (int16_t)err, SCPI_ErrorTranslate(err));

        if (err == SCPI_ERROR_INPUT_BUFFER_OVERRUN) {
            psu::scpi::onBufferOverrun(*context);
        }
    }

    return 0;
}

scpi_result_t SCPI_Control(scpi_t *context, scpi_ctrl_name_t ctrl, scpi_reg_val_t val) {
    return SCPI_RES_OK;
}

scpi_result_t SCPI_Reset(scpi_t *context) {
    return eez::reset() ? SCPI_RES_OK : SCPI_RES_ERR;
}

static scpi_reg_val_t g_scpiPsuRegs[SCPI_PSU_REG_COUNT];
static scpi_psu_t g_scpiPsuContext = { g_scpiPsuRegs };

static scpi_interface_t g_scpiInterface = {
    SCPI_Error, SCPI_Write, SCPI_Control, SCPI_Flush, SCPI_Reset,
};

static char g_scpiInputBuffer[SCPI_PARSER_INPUT_BUFFER_LENGTH];
static scpi_error_t g_errorQueueData[SCPI_PARSER_ERROR_QUEUE_SIZE + 1];

scpi_t g_scpiContext;

////////////////////////////////////////////////////////////////////////////////

bool getLatestScpiResult(const char **resultText, size_t *resultTextLen, int *err) {
	if (g_lastError != 0) {
		if (err) {
			*err = g_lastError;
		}
		return false;
	}

	if (g_scpiDataLen >= 2 && g_scpiData[g_scpiDataLen - 2] == '\r' && g_scpiData[g_scpiDataLen - 1] == '\n') {
		g_scpiDataLen -= 2;
		g_scpiData[g_scpiDataLen] = 0;
	}

	if (g_scpiDataLen >= 3 && g_scpiData[0] == '"' && g_scpiData[g_scpiDataLen - 1] == '"') {
		// replace "" with "
		size_t j = 1;
		size_t i;
		for (i = 1; i < g_scpiDataLen - 2; i++, j++) {
			g_scpiData[j] = g_scpiData[i];
			if (g_scpiData[i] == '"' && g_scpiData[i + 1] == '"') {
				i++;
			}
		}
		g_scpiData[j] = g_scpiData[i];
		g_scpiData[j + 1] = '"';
		g_scpiDataLen -= i - j;
	}

	// if (g_scpiDataLen > 0) {
	//     DebugTrace("< %s\n", g_scpiData);
	// }

	*resultText = g_scpiData;
	*resultTextLen = g_scpiDataLen;
	return true;
}

////////////////////////////////////////////////////////////////////////////////

// When passed quoted string as '"str"' it will return unquoted string as 'str'.
// Returns false if passed value is not a valid string.
bool parseScpiString(const char *&textArg, size_t &textLenArg) {
	const char *text = textArg;
	size_t textLen = textLenArg;

	if (textLen < 2 || text[0] != '"' || text[textLen - 1] != '"') {
		return false;
	}

	text++;
	textLen -= 2;

	// make sure there is no single quote (") inside
	// scpi can return list of strings as "str1","str2","str3",...
	// and we don't want to recognize this as string
	for (size_t i = 0; i < textLen; i++) {
		if (text[i] == '"') {
			if (i == textLen - 1 || text[i + 1] != '"') {
				return false;
			}
			i++;
		}
	}

	textArg = text;
	textLenArg = textLen;
	return true;
}

////////////////////////////////////////////////////////////////////////////////

struct ScpiComponentExecutionState : public ComponenentExecutionState {
	uint8_t op;
	int instructionIndex;
	char commandOrQueryText[256];

	static ScpiComponentExecutionState *g_waitingForScpiResult;
	static bool g_scpiResultIsReady;

	ScpiComponentExecutionState() {
		instructionIndex = 0;
		commandOrQueryText[0] = 0;
	}

	bool scpi() {
		if (g_waitingForScpiResult) {
			if (g_waitingForScpiResult == this && g_scpiResultIsReady) {
				g_waitingForScpiResult = nullptr;
				return true;
			}
	
			return false;
		} 

		g_waitingForScpiResult = this;
		g_scpiResultIsReady = false;

		sendMessageToLowPriorityThread(FLOW_EXECUTE_SCPI);
		return false;
	}
};

ScpiComponentExecutionState *ScpiComponentExecutionState::g_waitingForScpiResult;
bool ScpiComponentExecutionState::g_scpiResultIsReady;

////////////////////////////////////////////////////////////////////////////////

void scpiComponentInit() {
	eez::psu::scpi::init(g_scpiContext, g_scpiPsuContext, &g_scpiInterface, g_scpiInputBuffer, SCPI_PARSER_INPUT_BUFFER_LENGTH, g_errorQueueData, SCPI_PARSER_ERROR_QUEUE_SIZE + 1);

	ScpiComponentExecutionState::g_waitingForScpiResult = nullptr;
}

void executeScpi() {
	if (!isFlowRunningHook()) {
		return;
	}

	g_scpiDataLen = 0;
	g_lastError = 0;

	input(g_scpiContext, 
		(const char *)ScpiComponentExecutionState::g_waitingForScpiResult->commandOrQueryText,
		strlen(ScpiComponentExecutionState::g_waitingForScpiResult->commandOrQueryText)
	);
	input(g_scpiContext, "\r\n", 2);
	
	ScpiComponentExecutionState::g_scpiResultIsReady = true;
}

////////////////////////////////////////////////////////////////////////////////

struct ScpiActionComponent : public Component {
	uint8_t instructions[1];
};

void executeScpiComponent(FlowState *flowState, unsigned componentIndex) {
    auto assets = flowState->assets;
    auto component = (ScpiActionComponent *)flowState->flow->components.item(assets, componentIndex);

	auto instructions = component->instructions;

	static const int SCPI_PART_STRING = 1;
	static const int SCPI_PART_EXPR = 2;
	static const int SCPI_PART_QUERY_WITH_ASSIGNMENT = 3;
	static const int SCPI_PART_QUERY = 4;
	static const int SCPI_PART_COMMAND = 5;
	static const int SCPI_PART_END = 6;

	auto scpiComponentExecutionState = (ScpiComponentExecutionState *)flowState->componenentExecutionStates[componentIndex];

	if (!scpiComponentExecutionState) {
		scpiComponentExecutionState = ObjectAllocator<ScpiComponentExecutionState>::allocate(0x38e134d2);
		scpiComponentExecutionState->op = instructions[scpiComponentExecutionState->instructionIndex++];

		flowState->componenentExecutionStates[componentIndex] = scpiComponentExecutionState;
	}

	while (true) {
		if (scpiComponentExecutionState->op == SCPI_PART_STRING) {
			uint16_t sizeLowByte = instructions[scpiComponentExecutionState->instructionIndex++];
			uint16_t sizeHighByte = instructions[scpiComponentExecutionState->instructionIndex++];
			uint16_t stringLength = sizeLowByte | (sizeHighByte << 8);
			stringAppendStringLength(
				scpiComponentExecutionState->commandOrQueryText,
				sizeof(scpiComponentExecutionState->commandOrQueryText),
				(const char *)instructions + scpiComponentExecutionState->instructionIndex,
				(size_t)stringLength
			);
			scpiComponentExecutionState->instructionIndex += stringLength;
		} else if (scpiComponentExecutionState->op == SCPI_PART_EXPR) {
			Value value;
			int numInstructionBytes;
			if (!evalExpression(flowState, componentIndex, instructions + scpiComponentExecutionState->instructionIndex, value, &numInstructionBytes)) {
				throwError(flowState, componentIndex, "Failed to evaluate assignable expression in SCPI\n");

				ObjectAllocator<ScpiComponentExecutionState>::deallocate(scpiComponentExecutionState);
				flowState->componenentExecutionStates[componentIndex] = nullptr;

				return;
			}
			scpiComponentExecutionState->instructionIndex += numInstructionBytes;

			char valueStr[256];
			value.toText(valueStr, sizeof(valueStr));

			stringAppendString(
				scpiComponentExecutionState->commandOrQueryText,
				sizeof(scpiComponentExecutionState->commandOrQueryText),
				valueStr
			);
		} else if (scpiComponentExecutionState->op == SCPI_PART_QUERY_WITH_ASSIGNMENT) {
			if (!scpiComponentExecutionState->g_waitingForScpiResult) {
				logScpiQuery(flowState, componentIndex, scpiComponentExecutionState->commandOrQueryText);
			}

			if (!scpiComponentExecutionState->scpi()) {
				if (!addToQueue(flowState, componentIndex)) {
					throwError(flowState, componentIndex, "Execution queue is full\n");
				}
				return;
			}

			const char *resultText;
			size_t resultTextLen;
			int err = 0;
			if (!getLatestScpiResult(&resultText, &resultTextLen, &err)) {
				char errorMessage[300];
				snprintf(errorMessage, sizeof(errorMessage), "%s\n", SCPI_ErrorTranslate(err));

				throwError(flowState, componentIndex, errorMessage);

				ObjectAllocator<ScpiComponentExecutionState>::deallocate(scpiComponentExecutionState);
				flowState->componenentExecutionStates[componentIndex] = nullptr;

				return;
			}

			logScpiQueryResult(flowState, componentIndex, resultText, resultTextLen);

			Value dstValue;
			int numInstructionBytes;
			if (!evalAssignableExpression(flowState, componentIndex, instructions + scpiComponentExecutionState->instructionIndex, dstValue, &numInstructionBytes)) {
				throwError(flowState, componentIndex, "Failed to evaluate assignable expression in SCPI\n");

				ObjectAllocator<ScpiComponentExecutionState>::deallocate(scpiComponentExecutionState);
				flowState->componenentExecutionStates[componentIndex] = nullptr;

				return;
			}
			scpiComponentExecutionState->instructionIndex += numInstructionBytes;

			scpiComponentExecutionState->commandOrQueryText[0] = 0;

			Value srcValue;
			if (parseScpiString(resultText, resultTextLen)) {
				srcValue = Value::makeStringRef(resultText, resultTextLen, 0x09143fa4);
			} else {
				char *strEnd;
				long num = strtol(resultText, &strEnd, 10);
				if (*strEnd == 0) {
					srcValue = Value((int)num, VALUE_TYPE_INT32);
				} else {
					float fnum = strtof(resultText, &strEnd);
					if (*strEnd == 0) {
						srcValue = Value(fnum, VALUE_TYPE_FLOAT);
					} else {
						srcValue = Value::makeStringRef(resultText, resultTextLen, 0x09143fa4);
					}
				}
			}

			assignValue(flowState, componentIndex, dstValue, srcValue);
		} else if (scpiComponentExecutionState->op == SCPI_PART_QUERY) {
			if (!scpiComponentExecutionState->g_waitingForScpiResult) {
				logScpiQuery(flowState, componentIndex, scpiComponentExecutionState->commandOrQueryText);
			}

			if (!scpiComponentExecutionState->scpi()) {
				if (!addToQueue(flowState, componentIndex)) {
					throwError(flowState, componentIndex, "Execution queue is full\n");
				}
				return;
			}

			const char *resultText;
			size_t resultTextLen;
			int err = 0;
			if (!getLatestScpiResult(&resultText, &resultTextLen, &err)) {
				char errorMessage[300];
				snprintf(errorMessage, sizeof(errorMessage), "%s\n", SCPI_ErrorTranslate(err));

				throwError(flowState, componentIndex, errorMessage);

				ObjectAllocator<ScpiComponentExecutionState>::deallocate(scpiComponentExecutionState);
				flowState->componenentExecutionStates[componentIndex] = nullptr;

				return;
			}

			scpiComponentExecutionState->commandOrQueryText[0] = 0;
		} else if (scpiComponentExecutionState->op == SCPI_PART_COMMAND) {
			if (!scpiComponentExecutionState->g_waitingForScpiResult) {
				logScpiCommand(flowState, componentIndex, scpiComponentExecutionState->commandOrQueryText);
			}

			if (!scpiComponentExecutionState->scpi()) {
				if (!addToQueue(flowState, componentIndex)) {
					throwError(flowState, componentIndex, "Execution queue is full\n");
				}
				return;
			}

			const char *resultText;
			size_t resultTextLen;
			int err = 0;
			if (!getLatestScpiResult(&resultText, &resultTextLen, &err)) {
				char errorMessage[300];
				snprintf(errorMessage, sizeof(errorMessage), "%s\n", SCPI_ErrorTranslate(err));

				throwError(flowState, componentIndex, errorMessage);

				ObjectAllocator<ScpiComponentExecutionState>::deallocate(scpiComponentExecutionState);
				flowState->componenentExecutionStates[componentIndex] = nullptr;

				return;
			}

			scpiComponentExecutionState->commandOrQueryText[0] = 0;
		} else if (scpiComponentExecutionState->op == SCPI_PART_END) {
			ObjectAllocator<ScpiComponentExecutionState>::deallocate(scpiComponentExecutionState);
			flowState->componenentExecutionStates[componentIndex] = nullptr;

			propagateValueThroughSeqout(flowState, componentIndex);
			return;
		}

		scpiComponentExecutionState->op = instructions[scpiComponentExecutionState->instructionIndex++];
	}
}

} // namespace flow
} // namespace eez
