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

#include <eez/flow/flow.h>
#include <eez/flow/operations.h>
#include <eez/flow/queue.h>

using namespace eez::gui;

namespace eez {
namespace flow {

FlowState *initFlowState(Assets *assets, int flowIndex) {
	auto flowDefinition = assets->flowDefinition.ptr(assets);
	auto flow = flowDefinition->flows.item(assets, flowIndex);

	FlowState *flowState = (FlowState *)alloc(sizeof(FlowState) + (flow->nInputValues + flow->localVariables.count + flow->widgetDataItems.count - 1) * sizeof(Value));

	flowState->flowIndex = flowIndex;

	auto &undefinedValue = *flowDefinition->constants.item(assets, UNDEFINED_VALUE_INDEX);

	for (unsigned i = 0; i < flow->nInputValues; i++) {
		flowState->values[i] = undefinedValue;
	}

	for (unsigned i = 0; i < flow->localVariables.count; i++) {
		auto value = flow->localVariables.item(assets, i);
		flowState->values[flow->nInputValues + i] = *value;
	}

	recalcFlowDataItems(assets, flowState);

	return flowState;
}

void recalcFlowDataItems(Assets *assets, FlowState *flowState) {
	auto flowDefinition = assets->flowDefinition.ptr(assets);
	auto flow = flowDefinition->flows.item(assets, flowState->flowIndex);
	auto dataItemsOffset = flow->nInputValues + flow->localVariables.count;

	for (unsigned i = 0; i < flow->widgetDataItems.count; i++) {
		auto &value = flowState->values[dataItemsOffset + i];

		auto propertyValue = flow->widgetDataItems.item(assets, i);
		if (propertyValue) {
			evalExpression(assets, flowState, propertyValue->evalInstructions, value);
		} else {
			value = Value();
		}
	}
}

void getDataItemValue(Assets *assets, FlowState *flowState, int16_t dataId, Value& value) {
	auto flowDefinition = assets->flowDefinition.ptr(assets);
	auto flow = flowDefinition->flows.item(assets, flowState->flowIndex);
	auto dataItemsOffset = flow->nInputValues + flow->localVariables.count;

	if (dataId >= 0 && dataId < (int16_t)flow->widgetDataItems.count) {
		value = flowState->values[dataItemsOffset + dataId];
	} else {
		// TODO this shouldn't happen
		value = Value();
	}
}

////////////////////////////////////////////////////////////////////////////////

bool isComponentReadyToRun(Assets *assets, FlowState *flowState, unsigned componentIndex) {
	auto flowDefinition = assets->flowDefinition.ptr(assets);
	auto flow = flowDefinition->flows.item(assets, flowState->flowIndex);
	auto component = flow->components.item(assets, componentIndex);

	if (component->type < 1000) {
		return false;
	}

	// check if all inputs are defined
	for (unsigned inputIndex = 0; inputIndex < component->inputs.count; inputIndex++) {
		auto inputValueIndex = component->inputs.ptr(assets)[inputIndex];

		auto &value = flowState->values[inputValueIndex];
		if (value.type_ == VALUE_TYPE_UNDEFINED) {
			return false;
		}
	}

	return true;
}

void pingComponent(Assets *assets, FlowState *flowState, unsigned componentIndex) {
	if (isComponentReadyToRun(assets, flowState, componentIndex)) {
		addToQueue(flowState, componentIndex, nullptr);
	}
}

void propagateValue(Assets *assets, FlowState *flowState, ComponentOutput &componentOutput, const gui::Value &value) {
	for (unsigned connectionIndex = 0; connectionIndex < componentOutput.connections.count; connectionIndex++) {
		auto connection = componentOutput.connections.item(assets, connectionIndex);
		flowState->values[connection->targetInputIndex] = value;
		pingComponent(assets, flowState, connection->targetComponentIndex);
	}
}

////////////////////////////////////////////////////////////////////////////////

void assignValue(Value &dstValue, const Value &srcValue) {
	if (dstValue.isInt32OrLess()) {
		dstValue.int32_ = srcValue.toInt32();
	} else if (dstValue.isFloat()) {
		dstValue.float_ = srcValue.toFloat();
	} else if (dstValue.isDouble()) {
		dstValue.float_ = srcValue.toDouble();
	}
	// TODO
}

////////////////////////////////////////////////////////////////////////////////

bool evalExpression(Assets *assets, FlowState *flowState, uint8_t *instructions, EvalStack &stack, int *numInstructionBytes) {
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

bool evalExpression(Assets *assets, FlowState *flowState, uint8_t *instructions, Value &result) {
	EvalStack stack;

	if (evalExpression(assets, flowState, instructions, stack)) {
		if (stack.sp == 1) {
			auto finalResult = stack.pop();

			if (finalResult.getType() == VALUE_TYPE_VALUE_PTR) {
				result = *finalResult.pValue_;
			} else {
				result = finalResult;
			}

			return true;
		}
	}

	result = Value();
	return false;
}

bool evalAssignableExpression(Assets *assets, FlowState *flowState, uint8_t *instructions, Value **result, int *numInstructionBytes) {
	EvalStack stack;

	if (evalExpression(assets, flowState, instructions, stack, numInstructionBytes)) {
		if (stack.sp == 1) {
			auto finalResult = stack.pop();
			if (finalResult.getType() == VALUE_TYPE_VALUE_PTR) {
				*result = finalResult.pValue_;
				return true;
			}
		}
	}

	*result = nullptr;
	return false;
}

////////////////////////////////////////////////////////////////////////////////

void throwError() {
	// TODO
}

} // namespace flow
} // namespace eez
