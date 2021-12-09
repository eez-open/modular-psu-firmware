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
#include <eez/flow/operations.h>

#include <eez/gui/gui.h>

using namespace eez::gui;

namespace eez {
namespace flow {

bool evalExpression(FlowState *flowState, int componentIndex, const uint8_t *instructions, EvalStack &stack, int *numInstructionBytes) {
	auto assets = flowState->assets;
	auto flowDefinition = flowState->flowDefinition;
	auto flow = flowState->flow;

	int i = 0;
	while (true) {
		uint16_t instruction = instructions[i] + (instructions[i + 1] << 8);
		auto instructionType = instruction & EXPR_EVAL_INSTRUCTION_TYPE_MASK;
		auto instructionArg = instruction & EXPR_EVAL_INSTRUCTION_PARAM_MASK;
		if (instructionType == EXPR_EVAL_INSTRUCTION_TYPE_PUSH_CONSTANT) {
			if (!stack.push(*flowDefinition->constants.item(assets, instructionArg))) {
				return false;
			}
		} else if (instructionType == EXPR_EVAL_INSTRUCTION_TYPE_PUSH_INPUT) {
			if (!stack.push(flowState->values[instructionArg])) {
				return false;
			}
		} else if (instructionType == EXPR_EVAL_INSTRUCTION_TYPE_PUSH_LOCAL_VAR) {
			if (!stack.push(&flowState->values[flow->componentInputs.count + instructionArg])) {
				return false;
			}
		} else if (instructionType == EXPR_EVAL_INSTRUCTION_TYPE_PUSH_GLOBAL_VAR) {
			if ((uint32_t)instructionArg < flowDefinition->globalVariables.count) {
				if (!stack.push(flowDefinition->globalVariables.item(assets, instructionArg))) {
					return false;
				}
			} else {
				// native variable
				if (!stack.push(get(g_widgetCursor, instructionArg + 1))) {
					return false;
				}
			}
		} else if (instructionType == EXPR_EVAL_INSTRUCTION_TYPE_PUSH_OUTPUT) {
			if (!stack.push(Value((uint16_t)instructionArg, VALUE_TYPE_FLOW_OUTPUT))) {
				return false;
			}
		} else if (instructionType == EXPR_EVAL_INSTRUCTION_ARRAY_ELEMENT) {
			auto elementIndexValue = stack.pop();
			auto arrayValue = stack.pop();

			if (arrayValue.getType() == VALUE_TYPE_VALUE_PTR) {
				arrayValue = *arrayValue.pValueValue;
			}

			if (elementIndexValue.getType() == VALUE_TYPE_VALUE_PTR) {
				elementIndexValue = *elementIndexValue.pValueValue;
			}

			if (arrayValue.type != VALUE_TYPE_ARRAY) {
				throwError(flowState, componentIndex, "Array value expected\n");
				return false;
			}

			auto array = arrayValue.arrayValue;

			int err;
			auto elementIndex = elementIndexValue.toInt32(&err);
			if (err) {
				throwError(flowState, componentIndex, "Integer value expected for array element index\n");
				return false;
			}
		
			if (elementIndex < 0 || elementIndex >= (int)array->arraySize) {
				throwError(flowState, componentIndex, "Array element index out of bounds\n");
				return false;
			}

			if (!stack.push(&array->values[elementIndex])) {
				return false;
			}
		} else if (instructionType == EXPR_EVAL_INSTRUCTION_TYPE_OPERATION) {
			if (!g_evalOperations[instructionArg](stack)) {
				return false;
			}
		} else {
			i += 2;
			break;
		}

		i += 2;
	}

	if (numInstructionBytes) {
		*numInstructionBytes = i;
	}

	return true;
}

bool evalExpression(FlowState *flowState, int componentIndex, const uint8_t *instructions, Value &result, int *numInstructionBytes, const int32_t *iterators) {
	EvalStack stack;

	stack.flowState = flowState;
	stack.componentIndex = componentIndex;
	stack.iterators = iterators;

	if (evalExpression(flowState, componentIndex, instructions, stack, numInstructionBytes)) {
		if (stack.sp == 1) {
			auto finalResult = stack.pop();

			if (finalResult.getType() == VALUE_TYPE_VALUE_PTR) {
				result = *finalResult.pValueValue;
			} else {
				result = finalResult;
			}

			return true;
		}
	}

	result = Value();
	return false;
}

bool evalAssignableExpression(FlowState *flowState, int componentIndex, const uint8_t *instructions, Value &result, int *numInstructionBytes, const int32_t *iterators) {
	EvalStack stack;

	stack.flowState = flowState;
	stack.componentIndex = componentIndex;
	stack.iterators = iterators;

	if (evalExpression(flowState, componentIndex, instructions, stack, numInstructionBytes)) {
		if (stack.sp == 1) {
			auto finalResult = stack.pop();
			if (finalResult.getType() == VALUE_TYPE_VALUE_PTR || finalResult.getType() == VALUE_TYPE_FLOW_OUTPUT) {
				result = finalResult;
				return true;
			}
		}
	}

	return false;
}

} // flow
} // eez
