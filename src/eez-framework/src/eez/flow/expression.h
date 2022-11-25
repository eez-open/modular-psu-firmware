/*
* EEZ Generic Firmware
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

#pragma once

#include <eez/core/util.h>
#include <eez/flow/private.h>

namespace eez {
namespace flow {

static const size_t STACK_SIZE = 20;

struct EvalStack {
	FlowState *flowState;
	int componentIndex;
	const int32_t *iterators;

	Value stack[STACK_SIZE];
	size_t sp = 0;

    char errorMessage[512];

	bool push(const Value &value) {
		if (sp >= STACK_SIZE) {
			throwError(flowState, componentIndex, "Evaluation stack is full\n");
			return false;
		}
		stack[sp++] = value;
		return true;
	}

	bool push(Value *pValue) {
		if (sp >= STACK_SIZE) {
			return false;
		}
		stack[sp++] = Value(pValue, VALUE_TYPE_VALUE_PTR);
		return true;
	}

	Value pop() {
		return stack[--sp];
	}

    void setErrorMessage(const char *str) {
        stringCopy(errorMessage, sizeof(errorMessage), str);
    }
};

#if EEZ_OPTION_GUI
bool evalExpression(FlowState *flowState, int componentIndex, const uint8_t *instructions, Value &result, const char *errorMessage, int *numInstructionBytes = nullptr, const int32_t *iterators = nullptr, eez::gui::DataOperationEnum operation = eez::gui::DATA_OPERATION_GET);
#else
bool evalExpression(FlowState *flowState, int componentIndex, const uint8_t *instructions, Value &result, const char *errorMessage, int *numInstructionBytes = nullptr, const int32_t *iterators = nullptr);
#endif
bool evalAssignableExpression(FlowState *flowState, int componentIndex, const uint8_t *instructions, Value &result, const char *errorMessage, int *numInstructionBytes = nullptr, const int32_t *iterators = nullptr);

#if EEZ_OPTION_GUI
bool evalProperty(FlowState *flowState, int componentIndex, int propertyIndex, Value &result, const char *errorMessage, int *numInstructionBytes = nullptr, const int32_t *iterators = nullptr, eez::gui::DataOperationEnum operation = eez::gui::DATA_OPERATION_GET);
#else
bool evalProperty(FlowState *flowState, int componentIndex, int propertyIndex, Value &result, const char *errorMessage, int *numInstructionBytes = nullptr, const int32_t *iterators = nullptr);
#endif
bool evalAssignableProperty(FlowState *flowState, int componentIndex, int propertyIndex, Value &result, const char *errorMessage, int *numInstructionBytes = nullptr, const int32_t *iterators = nullptr);

} // flow
} // eez
