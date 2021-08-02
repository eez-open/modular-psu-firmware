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

#include <eez/gui/assets.h>

namespace eez {
namespace flow {

using eez::gui::Value;
using eez::gui::Assets;
using eez::gui::Component;
using eez::gui::ComponentOutput;

struct FlowState {
	unsigned flowIndex;
	eez::gui::Value values[1];
};

struct ComponenentExecutionState {
	virtual ~ComponenentExecutionState() {}
};

struct EvalStack {
	Value stack[100];
	int sp = 0;

	void push(const Value &value) {
		stack[sp++] = value;
	}

	void push(Value *pValue) {
		stack[sp++] = Value(pValue, VALUE_TYPE_VALUE_PTR);
	}

	Value pop() {
		return stack[--sp];
	}

};

static const int UNDEFINED_VALUE_INDEX = 0;
static const int NULL_VALUE_INDEX = 1;

FlowState *initFlowState(Assets *assets, int flowIndex);

void recalcFlowDataItems(Assets *assets, FlowState *flowState);

void pingComponent(Assets *assets, FlowState *flowState, unsigned componentIndex);
void propagateValue(Assets *assets, FlowState *flowState, ComponentOutput &componentOutput, const gui::Value &value);

void setValueFromGuiThread(Assets *assets, FlowState *flowState, uint16_t dataId, const Value& value);
void assignValue(Assets *assets, FlowState *flowState, Component *component, Value &dstValue, const Value &srcValue);

bool evalExpression(Assets *assets, FlowState *flowState, const uint8_t *instructions, EvalStack &stack, int *numInstructionBytes = nullptr);
bool evalExpression(Assets *assets, FlowState *flowState, const uint8_t *instructions, Value &result, int *numInstructionBytes = nullptr);
bool evalAssignableExpression(Assets *assets, FlowState *flowState, const uint8_t *instructions, Value &result, int *numInstructionBytes = nullptr);

void throwError(const char *errorMessage);

} // flow
} // eez