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

bool isComponentReadyToRun(FlowState *flowState, unsigned componentIndex) {
	auto assets = flowState->assets;
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
		if (value.type == VALUE_TYPE_UNDEFINED) {
			return false;
		}
	}

	return true;
}

void pingComponent(FlowState *flowState, unsigned componentIndex) {
	if (isComponentReadyToRun(flowState, componentIndex)) {
		addToQueue(flowState, componentIndex);
	}
}


FlowState *initFlowState(Assets *assets, int flowIndex) {
	auto flowDefinition = assets->flowDefinition.ptr(assets);
	auto flow = flowDefinition->flows.item(assets, flowIndex);

	auto nValues = flow->nInputValues + flow->localVariables.count + flow->widgetDataItems.count;

	FlowState *flowState = (FlowState *)alloc(
		sizeof(FlowState) + nValues * sizeof(Value) + flow->components.count * sizeof(ComponenentExecutionState),
		0x4c3b6ef5
	);

	flowState->assets = assets;
	flowState->flowIndex = flowIndex;
	flowState->error = false;
	flowState->numActiveComponents = 0;
	flowState->parentFlowState = nullptr;
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

	recalcFlowDataItems(flowState);

	for (unsigned componentIndex = 0; componentIndex < flow->components.count; componentIndex++) {
		pingComponent(flowState, componentIndex);
	}

	return flowState;
}

void freeFlowState(FlowState *flowState) {
	auto assets = flowState->assets;
	auto flowDefinition = assets->flowDefinition.ptr(assets);
	auto flow = flowDefinition->flows.item(assets, flowState->flowIndex);

	auto valuesCount = flow->nInputValues + flow->localVariables.count + flow->widgetDataItems.count;

	for (unsigned int i = 0; i < valuesCount; i++) {
		(flowState->values + i)->~Value();
	}

	for (unsigned i = 0; i < flow->components.count; i++) {
		auto componentExecutionState = flowState->componenentExecutionStates[i];
		if (componentExecutionState) {
			ObjectAllocator<ComponenentExecutionState>::deallocate(componentExecutionState);
		}
	}


	free(flowState);
}

void recalcFlowDataItems(FlowState *flowState) {
	auto assets = flowState->assets;
	auto flowDefinition = assets->flowDefinition.ptr(assets);
	auto flow = flowDefinition->flows.item(assets, flowState->flowIndex);
	auto dataItemsOffset = flow->nInputValues + flow->localVariables.count;

	for (unsigned i = 0; i < flow->widgetDataItems.count; i++) {
		auto &value = flowState->values[dataItemsOffset + i];

		WidgetDataItem *widgetDataItem = flow->widgetDataItems.item(assets, i);
		if (widgetDataItem) {
			auto component = flow->components.item(assets, widgetDataItem->componentIndex);
			auto propertyValue = component->propertyValues.item(assets, widgetDataItem->propertyValueIndex);

			evalExpression(flowState, component, propertyValue->evalInstructions, value);
			if (flowState->error) {
				break;
			}
		} else {
			value = Value();
		}
	}
}

void propagateValue(FlowState *flowState, ComponentOutput &componentOutput, const gui::Value &value) {
	auto assets = flowState->assets;
	for (unsigned connectionIndex = 0; connectionIndex < componentOutput.connections.count; connectionIndex++) {
		auto connection = componentOutput.connections.item(assets, connectionIndex);
		flowState->values[connection->targetInputIndex] = value;
		pingComponent(flowState, connection->targetComponentIndex);
		if (connection->seqIn) {
			flowState->values[connection->targetInputIndex] = Value();
		}
	}
}

void propagateValue(FlowState *flowState, ComponentOutput &componentOutput) {
	auto assets = flowState->assets;
	auto flowDefinition = assets->flowDefinition.ptr(assets);
	auto &nullValue = *flowDefinition->constants.item(assets, NULL_VALUE_INDEX);
	propagateValue(flowState, componentOutput, nullValue);
}

void propagateValue(FlowState *flowState, unsigned componentIndex) {
	auto assets = flowState->assets;
	auto flowDefinition = assets->flowDefinition.ptr(assets);
	auto flow = flowDefinition->flows.item(assets, flowState->flowIndex);
	auto component = flow->components.item(assets, componentIndex);
	auto &componentOutput = *component->outputs.item(assets, 0);
	propagateValue(flowState, componentOutput);
}

////////////////////////////////////////////////////////////////////////////////

struct {
	bool isActive;
	Assets *assets;
	FlowState *flowState;
	uint16_t dataId;
	Value value;
} g_setValueFromGuiThreadParams;

void setValueFromGuiThread(FlowState *flowState, uint16_t dataId, const Value& value) {
	while (g_setValueFromGuiThreadParams.isActive) {
		osDelay(1);
	}

	g_setValueFromGuiThreadParams.isActive = true;
	g_setValueFromGuiThreadParams.assets = flowState->assets;
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
			if (evalAssignableExpression(flowState, component, propertyValue->evalInstructions, dstValue)) {
				assignValue(flowState, component, dstValue, g_setValueFromGuiThreadParams.value);
			} else {
				throwError(flowState, component, "doSetFlowValue failed\n");
			}
		}
	}
	g_setValueFromGuiThreadParams.isActive = false;
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
	}
}

////////////////////////////////////////////////////////////////////////////////

void throwError(FlowState *flowState, Component *component, const char *errorMessage) {
	if (component->logError) {
		ErrorTrace(errorMessage);
	}

	if (component->errorCatchOutput != -1) {
		propagateValue(flowState, *component->outputs.item(flowState->assets, component->errorCatchOutput));
	} else {
		flowState->error = true;
		scripting::stopScript();
	}
}

} // namespace flow
} // namespace eez
