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

#include <eez/flow/private.h>

namespace eez {
namespace flow {

using eez::gui::MAX_ITERATORS;

struct EvalStack {
	Assets *assets;
	FlowState *flowState;
	const int32_t *iterators;

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

bool evalExpression(FlowState *flowState, Component *component, const uint8_t *instructions, EvalStack &stack, int *numInstructionBytes = nullptr);
bool evalExpression(FlowState *flowState, Component *component, const uint8_t *instructions, Value &result, int *numInstructionBytes = nullptr, const int32_t *iterators = nullptr);
bool evalAssignableExpression(FlowState *flowState, Component *component, const uint8_t *instructions, Value &result, int *numInstructionBytes = nullptr, const int32_t *iterators = nullptr);

} // flow
} // eez