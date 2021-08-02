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

#include <eez/debug.h>

#include <eez/scripting/scripting.h>
#include <eez/scripting/thread.h>

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

		WidgetDataItem *widgetDataItem = flow->widgetDataItems.item(assets, i);
		if (widgetDataItem) {
			auto component = flow->components.item(assets, widgetDataItem->componentIndex);
			auto propertyValue = component->propertyValues.item(assets, widgetDataItem->propertyValueIndex);

			evalExpression(assets, flowState, propertyValue->evalInstructions, value);
		} else {
			value = Value();
		}
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

struct {
	bool isActive;
	Assets *assets;
	FlowState *flowState;
	uint16_t dataId;
	Value value;
} g_setValueFromGuiThreadParams;

void setValueFromGuiThread(Assets *assets, FlowState *flowState, uint16_t dataId, const Value& value) {
	while (g_setValueFromGuiThreadParams.isActive) {
		osDelay(1);
	}

	g_setValueFromGuiThreadParams.isActive = true;
	g_setValueFromGuiThreadParams.assets = assets;
	g_setValueFromGuiThreadParams.flowState = flowState;
	g_setValueFromGuiThreadParams.dataId = dataId;
	g_setValueFromGuiThreadParams.value = value;

	scripting::setFlowValueInScriptingThread();
}

void doSetFlowValue() {
	if (scripting::isFlowRunning()) {
		Assets *assets = g_setValueFromGuiThreadParams.assets;
		FlowState *flowState = g_setValueFromGuiThreadParams.flowState;
		uint16_t dataId = g_setValueFromGuiThreadParams.dataId;
		
		auto flowDefinition = assets->flowDefinition.ptr(assets);
		auto flow = flowDefinition->flows.item(assets, flowState->flowIndex);

		WidgetDataItem *widgetDataItem = flow->widgetDataItems.item(assets, dataId);
		if (widgetDataItem) {
			auto component = flow->components.item(assets, widgetDataItem->componentIndex);
			auto propertyValue = component->propertyValues.item(assets, widgetDataItem->propertyValueIndex);

			Value dstValue;
			if (evalAssignableExpression(assets, flowState, propertyValue->evalInstructions, dstValue)) {
				assignValue(assets, flowState, component, dstValue, g_setValueFromGuiThreadParams.value);
			} else {
				throwError("doSetFlowValue failed");
			}
		}
	}
	g_setValueFromGuiThreadParams.isActive = false;
}

////////////////////////////////////////////////////////////////////////////////

void assignValue(Assets *assets, FlowState *flowState, Component *component, Value &dstValue, const Value &srcValue) {
	if (dstValue.getType() == VALUE_TYPE_FLOW_OUTPUT) {
		auto &componentOutput = *component->outputs.item(assets, dstValue.getUInt16());
		propagateValue(assets, flowState, componentOutput, srcValue);
	} else {
		Value *pDstValue = dstValue.pValue_;
		if (pDstValue->isInt32OrLess()) {
			pDstValue->int32_ = srcValue.toInt32();
		} else if (pDstValue->isFloat()) {
			pDstValue->float_ = srcValue.toFloat();
		} else if (pDstValue->isDouble()) {
			pDstValue->float_ = srcValue.toDouble();
		}
		// TODO
	}
}

////////////////////////////////////////////////////////////////////////////////

bool evalExpression(Assets *assets, FlowState *flowState, const uint8_t *instructions, EvalStack &stack, int *numInstructionBytes) {
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

bool evalExpression(Assets *assets, FlowState *flowState, const uint8_t *instructions, Value &result, int *numInstructionBytes) {
	EvalStack stack;

	if (evalExpression(assets, flowState, instructions, stack, numInstructionBytes)) {
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

bool evalAssignableExpression(Assets *assets, FlowState *flowState, const uint8_t *instructions, Value &result, int *numInstructionBytes) {
	EvalStack stack;

	if (evalExpression(assets, flowState, instructions, stack, numInstructionBytes)) {
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

////////////////////////////////////////////////////////////////////////////////

void throwError(const char *errorMessage) {
	ErrorTrace(errorMessage);
	scripting::stopScript();
}

} // namespace flow
} // namespace eez
