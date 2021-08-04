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
#include <math.h>

#include <eez/alloc.h>
#include <eez/system.h>
#include <eez/scripting/scripting.h>

#include <eez/flow/components.h>
#include <eez/flow/flow_defs_v3.h>
#include <eez/flow/queue.h>

using namespace eez::gui;

namespace eez {
namespace flow {

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
		} else {
			g_waitingForScpiResult = this;
			g_scpiResultIsReady = false;

			stringAppendString(commandOrQueryText, sizeof(commandOrQueryText), "\n");
			scripting::executeScpiFromFlow(commandOrQueryText);
		}
		return false;
	}
};

ScpiComponentExecutionState *ScpiComponentExecutionState::g_waitingForScpiResult;
bool ScpiComponentExecutionState::g_scpiResultIsReady;

void scpiComponentInit() {
	ScpiComponentExecutionState::g_waitingForScpiResult = nullptr;
}

void scpiResultIsReady() {
	ScpiComponentExecutionState::g_scpiResultIsReady = true;
}

bool executeScpiComponent(Assets *assets, FlowState *flowState, Component *component, ComponenentExecutionState *&componentExecutionState) {
	struct ScpiActionComponent : public Component {
		uint8_t instructions[1];
	};

	auto specific = (ScpiActionComponent *)component;
	auto instructions = specific->instructions;

	static const int SCPI_PART_STRING = 1;
	static const int SCPI_PART_EXPR = 2;
	static const int SCPI_PART_QUERY_WITH_ASSIGNMENT = 3;
	static const int SCPI_PART_QUERY = 4;
	static const int SCPI_PART_COMMAND = 5;
	static const int SCPI_PART_END = 6;

	ScpiComponentExecutionState *scpiComponentExecutionState;
	if (componentExecutionState) {
		scpiComponentExecutionState = (ScpiComponentExecutionState *)componentExecutionState;
	} else {
		scpiComponentExecutionState = ObjectAllocator<ScpiComponentExecutionState>::allocate();
		scpiComponentExecutionState->op = instructions[scpiComponentExecutionState->instructionIndex++];

		componentExecutionState = scpiComponentExecutionState;
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
			if (!evalExpression(assets, flowState, instructions + scpiComponentExecutionState->instructionIndex, value, &numInstructionBytes)) {
				throwError(assets, flowState, component, "scpi component eval assignable expression\n");
				return false;
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
			if (!scpiComponentExecutionState->scpi()) {
				break;
			}

			const char *resultText;
			size_t resultTextLen;
			int err;
			if (!scripting::getLatestScpiResult(&resultText, &resultTextLen, &err)) {
				char errorMessage[300];
				snprintf(errorMessage, sizeof(errorMessage), "SCPI '%s': %s",
					SCPI_ErrorTranslate(err),
					scpiComponentExecutionState->commandOrQueryText
				);
				throwError(assets, flowState, component, errorMessage);
				return false;
			}

			Value dstValue;
			int numInstructionBytes;
			if (!evalAssignableExpression(assets, flowState, instructions + scpiComponentExecutionState->instructionIndex, dstValue, &numInstructionBytes)) {
				throwError(assets, flowState, component, "scpi component eval assignable expression\n");
				return false;
			}
			scpiComponentExecutionState->instructionIndex += numInstructionBytes;

			scpiComponentExecutionState->commandOrQueryText[0] = 0;

			Value srcValue = Value::makeStringRef(resultText, resultTextLen);

			assignValue(assets, flowState, component, dstValue, srcValue);
		} else if (scpiComponentExecutionState->op == SCPI_PART_QUERY) {
			if (!scpiComponentExecutionState->scpi()) {
				break;
			}
			scpiComponentExecutionState->commandOrQueryText[0] = 0;
		} else if (scpiComponentExecutionState->op == SCPI_PART_COMMAND) {
			if (!scpiComponentExecutionState->scpi()) {
				break;
			}
			scpiComponentExecutionState->commandOrQueryText[0] = 0;

		} else if (scpiComponentExecutionState->op == SCPI_PART_END) {
			ObjectAllocator<ScpiComponentExecutionState>::deallocate(scpiComponentExecutionState);
			componentExecutionState = nullptr;

			break;
		}

		scpiComponentExecutionState->op = instructions[scpiComponentExecutionState->instructionIndex++];
	}

	return true;
}

} // namespace flow
} // namespace eez
