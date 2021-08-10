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

using namespace eez::gui;

namespace eez {
namespace flow {

bool evalExpression(FlowState *flowState, Component *component, const uint8_t *instructions, EvalStack &stack, int *numInstructionBytes) {
	auto assets = flowState->assets;
	auto flowDefinition = assets->flowDefinition.ptr(assets);
	auto flow = flowDefinition->flows.item(assets, flowState->flowIndex);

	int i = 0;
	while (true) {
		uint16_t instruction = instructions[i] + (instructions[i + 1] << 8);
		auto instructionType = instruction & EXPR_EVAL_INSTRUCTION_TYPE_MASK;
		auto instructionArg = instruction & EXPR_EVAL_INSTRUCTION_PARAM_MASK;
		if (instructionType == EXPR_EVAL_INSTRUCTION_TYPE_PUSH_CONSTANT) {
			stack.push(*flowDefinition->constants.item(assets, instructionArg));
		} else if (instructionType == EXPR_EVAL_INSTRUCTION_TYPE_PUSH_INPUT) {
			stack.push(flowState->values[instructionArg]);
		} else if (instructionType == EXPR_EVAL_INSTRUCTION_TYPE_PUSH_LOCAL_VAR) {
			stack.push(&flowState->values[flow->nInputValues + instructionArg]);
		} else if (instructionType == EXPR_EVAL_INSTRUCTION_TYPE_PUSH_GLOBAL_VAR) {
			stack.push(flowDefinition->globalVariables.item(assets, instructionArg));
		} else if (instructionType == EXPR_EVAL_INSTRUCTION_TYPE_PUSH_OUTPUT) {
			stack.push(Value((uint16_t)instructionArg, VALUE_TYPE_FLOW_OUTPUT));
		} else if (instructionType == EXPR_EVAL_INSTRUCTION_ARRAY_ELEMENT) {
			auto elementIndexValue = stack.pop();
			auto arrayValue = stack.pop();

			if (arrayValue.getType() == VALUE_TYPE_VALUE_PTR) {
				arrayValue = *arrayValue.pValueValue;
			}

			if (elementIndexValue.getType() == VALUE_TYPE_VALUE_PTR) {
				elementIndexValue = *elementIndexValue.pValueValue;
			}

			if (arrayValue.type != VALUE_TYPE_ASSETS_ARRAY) {
				throwError(flowState, component, "Array value expected\n");
				return false;
			}

			auto assetsArray = ((AssetsPtr<AssetsArray> *)&arrayValue.assetsOffsetValue)->ptr(assets);

			int err;
			auto elementIndex = elementIndexValue.toInt32(&err);
			if (err) {
				throwError(flowState, component, "Integer value expected for array element index\n");
				return false;
			}
		
			if (elementIndex < 0 || elementIndex >= (int)assetsArray->arraySize) {
				throwError(flowState, component, "Array element index out of bounds\n");
				return false;
			}

			stack.push(&assetsArray->values[elementIndex]);
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

bool evalExpression(FlowState *flowState, Component *component, const uint8_t *instructions, Value &result, int *numInstructionBytes, int32_t *iterators) {
	EvalStack stack;

	stack.assets = flowState->assets;
	stack.flowState = flowState;

	if (iterators) {
		for (size_t i = 0; i < MAX_ITERATORS; i++) {
			stack.iterators[i] = iterators[i];
		}
	} else {
		for (size_t i = 0; i < MAX_ITERATORS; i++) {
			stack.iterators[i] = -1;
		}
	}

	if (evalExpression(flowState, component, instructions, stack, numInstructionBytes)) {
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

bool evalAssignableExpression(FlowState *flowState, Component *component, const uint8_t *instructions, Value &result, int *numInstructionBytes, int32_t *iterators) {
	EvalStack stack;

	stack.assets = flowState->assets;
	stack.flowState = flowState;
	
	if (iterators) {
		for (size_t i = 0; i < MAX_ITERATORS; i++) {
			stack.iterators[i] = iterators[i];
		}
	} else {
		for (size_t i = 0; i < MAX_ITERATORS; i++) {
			stack.iterators[i] = -1;
		}
	}


	if (evalExpression(flowState, component, instructions, stack, numInstructionBytes)) {
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