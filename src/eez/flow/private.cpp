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
#include <eez/flow/debugger.h>

using namespace eez::gui;

namespace eez {
namespace flow {

uint32_t g_lastFlowStateIndex = 0;

void fixValue(Assets *assets, Value &value) {
	if (value.getType() == VALUE_TYPE_STRING) {
		value.strValue = ((AssetsPtr<const char> *)&value.uint32Value)->ptr(assets);
	} else if (value.getType() == VALUE_TYPE_ARRAY) {
		value.arrayValue = ((AssetsPtr<ArrayValue> *)&value.uint32Value)->ptr(assets);
		for (uint32_t i = 0; i < value.arrayValue->arraySize; i++) {
			fixValue(assets, value.arrayValue->values[i]);
		}
	}
}

void fixValues(Assets *assets, ListOfAssetsPtr<Value> &values) {
	for (uint32_t i = 0; i < values.count; i++) {
		auto value = values.item(assets, i);
		fixValue(assets, *value);
	}
}

void fixAssetValues(Assets *assets) {
	auto flowDefinition = assets->flowDefinition.ptr(assets);

	fixValues(assets, flowDefinition->constants);
	fixValues(assets, flowDefinition->globalVariables);

	for (uint32_t i = 0; i < flowDefinition->flows.count; i++) {
		auto flow = flowDefinition->flows.item(assets, i);
		fixValues(assets, flow->localVariables);
	}
}

bool isComponentReadyToRun(FlowState *flowState, unsigned componentIndex) {
	auto assets = flowState->assets;
	auto component = flowState->flow->components.item(assets, componentIndex);

	if (component->type < 1000) {
		return false;
	}

	// check if all inputs are defined
	for (unsigned inputIndex = 0; inputIndex < component->inputs.count; inputIndex++) {
		auto inputValueIndex = component->inputs.ptr(assets)[inputIndex];

		auto &value = flowState->values[inputValueIndex];
		if (value.type == VALUE_TYPE_UNDEFINED) {
			return false;
		}
	}

	return true;
}

bool pingComponent(FlowState *flowState, unsigned componentIndex) {
	if (isComponentReadyToRun(flowState, componentIndex)) {
		if (!addToQueue(flowState, componentIndex)) {
			auto component = flowState->flow->components.item(flowState->assets, componentIndex);
			throwError(flowState, component, "Execution queue is full\n");
			return false;
		}
		return true;
	}
	return false;
}


static FlowState *initFlowState(Assets *assets, int flowIndex, FlowState *parentFlowState) {
	auto flowDefinition = assets->flowDefinition.ptr(assets);
	auto flow = flowDefinition->flows.item(assets, flowIndex);

	auto nValues = flow->nInputValues + flow->localVariables.count;

	FlowState *flowState = (FlowState *)alloc(
		sizeof(FlowState) + nValues * sizeof(Value) + flow->components.count * sizeof(ComponenentExecutionState *),
		0x4c3b6ef5
	);

	flowState->flowStateIndex = ++g_lastFlowStateIndex;
	flowState->assets = assets;
	flowState->flowDefinition = assets->flowDefinition.ptr(assets);
	flowState->flow = flowDefinition->flows.item(assets, flowIndex);
	flowState->flowIndex = flowIndex;
	flowState->error = false;
	flowState->numActiveComponents = 0;
	flowState->parentFlowState = parentFlowState;
	flowState->parentComponent = nullptr;
	flowState->values = (Value *)(flowState + 1);
	flowState->componenentExecutionStates = (ComponenentExecutionState **)(flowState->values + nValues);

	for (unsigned i = 0; i < nValues; i++) {
		flowState->values[i].clear();
	}

	auto &undefinedValue = *flowDefinition->constants.item(assets, UNDEFINED_VALUE_INDEX);
	for (unsigned i = 0; i < flow->nInputValues; i++) {
		flowState->values[i] = undefinedValue;
	}

	for (unsigned i = 0; i < flow->localVariables.count; i++) {
		auto value = flow->localVariables.item(assets, i);
		flowState->values[flow->nInputValues + i] = *value;
	}

	for (unsigned i = 0; i < flow->components.count; i++) {
		flowState->componenentExecutionStates[i] = nullptr;
	}

	onFlowStateCreated(flowState);

	for (unsigned componentIndex = 0; componentIndex < flow->components.count; componentIndex++) {
		pingComponent(flowState, componentIndex);
	}

	return flowState;
}

FlowState *initActionFlowState(Assets *assets, int flowIndex, FlowState *parentFlowState) {
	auto flowState = initFlowState(assets, flowIndex, parentFlowState);
	if (flowState) {
		flowState->isAction = true;
	}
	return flowState;
}

FlowState *initPageFlowState(Assets *assets, int flowIndex, FlowState *parentFlowState) {
	auto flowState = initFlowState(assets, flowIndex, parentFlowState);
	if (flowState) {
		flowState->isAction = false;
	}
	return flowState;
}

void freeFlowState(FlowState *flowState) {
	auto flow = flowState->flow;

	auto valuesCount = flow->nInputValues + flow->localVariables.count;

	for (unsigned int i = 0; i < valuesCount; i++) {
		(flowState->values + i)->~Value();
	}

	for (unsigned i = 0; i < flow->components.count; i++) {
		auto componentExecutionState = flowState->componenentExecutionStates[i];
		if (componentExecutionState) {
			ObjectAllocator<ComponenentExecutionState>::deallocate(componentExecutionState);
			flowState->componenentExecutionStates[i] = nullptr;
		}
	}

	onFlowStateDestroyed(flowState);

	free(flowState);
}

void propagateValue(FlowState *flowState, ComponentOutput &componentOutput, const gui::Value &value) {
	auto assets = flowState->assets;
	for (unsigned connectionIndex = 0; connectionIndex < componentOutput.connections.count; connectionIndex++) {
		auto connection = componentOutput.connections.item(assets, connectionIndex);
		
		auto pValue = &flowState->values[connection->targetInputIndex];

		*pValue = value;
		
		onValueChanged(pValue);
		
		if (pingComponent(flowState, connection->targetComponentIndex) && connection->seqIn) {
			flowState->values[connection->targetInputIndex] = Value();
		}
	}
}

void propagateValue(FlowState *flowState, ComponentOutput &componentOutput) {
	auto &nullValue = *flowState->flowDefinition->constants.item(flowState->assets, NULL_VALUE_INDEX);
	propagateValue(flowState, componentOutput, nullValue);
}

void propagateValue(FlowState *flowState, unsigned componentIndex) {
	auto component = flowState->flow->components.item(flowState->assets, componentIndex);
	auto &componentOutput = *component->outputs.item(flowState->assets, 0);
	propagateValue(flowState, componentOutput);
}

////////////////////////////////////////////////////////////////////////////////

void getValue(uint16_t dataId, const WidgetCursor &widgetCursor, Value &value) {
	if (scripting::isFlowRunning()) {
		FlowState *flowState = widgetCursor.flowState;
		auto assets = flowState->assets;
		auto flow = flowState->flow;

		WidgetDataItem *widgetDataItem = flow->widgetDataItems.item(assets, dataId);
		if (widgetDataItem) {
			auto component = flow->components.item(assets, widgetDataItem->componentIndex);
			auto propertyValue = component->propertyValues.item(assets, widgetDataItem->propertyValueIndex);

			if (!evalExpression(flowState, component, propertyValue->evalInstructions, value, nullptr, widgetCursor.iterators)) {
				throwError(flowState, component, "doGetFlowValue failed\n");
			}
		}
	}
}

void setValue(uint16_t dataId, const WidgetCursor &widgetCursor, const Value& value) {
	if (scripting::isFlowRunning()) {
		FlowState *flowState = widgetCursor.flowState;
		auto assets = flowState->assets;
		auto flow = flowState->flow;

		WidgetDataItem *widgetDataItem = flow->widgetDataItems.item(assets, dataId);
		if (widgetDataItem) {
			auto component = flow->components.item(assets, widgetDataItem->componentIndex);
			auto propertyValue = component->propertyValues.item(assets, widgetDataItem->propertyValueIndex);

			Value dstValue;
			if (evalAssignableExpression(flowState, component, propertyValue->evalInstructions, dstValue, nullptr, widgetCursor.iterators)) {
				assignValue(flowState, component, dstValue, value);
			} else {
				throwError(flowState, component, "doSetFlowValue failed\n");
			}
		}
	}
}

////////////////////////////////////////////////////////////////////////////////

void assignValue(FlowState *flowState, Component *component, Value &dstValue, const Value &srcValue) {
	if (dstValue.getType() == VALUE_TYPE_FLOW_OUTPUT) {
		auto assets = flowState->assets;
		auto &componentOutput = *component->outputs.item(assets, dstValue.getUInt16());
		propagateValue(flowState, componentOutput, srcValue);
	} else {
		Value *pDstValue = dstValue.pValueValue;
		
		if (pDstValue->isInt32OrLess()) {
			pDstValue->int32Value = srcValue.toInt32();
		} else if (pDstValue->isFloat()) {
			pDstValue->floatValue = srcValue.toFloat();
		} else if (pDstValue->isDouble()) {
			pDstValue->doubleValue = srcValue.toDouble();
		}

		// TODO
		
		onValueChanged(pDstValue);
	}
}

////////////////////////////////////////////////////////////////////////////////

void throwError(FlowState *flowState, Component *component, const char *errorMessage) {
	if (component->logError) {
		ErrorTrace(errorMessage);
	}

	if (component->errorCatchOutput != -1) {
		propagateValue(
			flowState,
			*component->outputs.item(flowState->assets, component->errorCatchOutput),
			Value::makeStringRef(errorMessage, strlen(errorMessage), 0xef6f8414)
		);
	} else {
		flowState->error = true;
		scripting::stopScript();
	}
}

} // namespace flow
} // namespace eez
