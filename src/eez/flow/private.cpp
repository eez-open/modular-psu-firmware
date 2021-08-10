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

#if defined(EEZ_PLATFORM_STM32)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wparentheses"
#endif
osMutexId(g_mutexId);
#if defined(EEZ_PLATFORM_STM32)
#pragma GCC diagnostic pop
#endif
osMutexDef(g_mutex);

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

bool pingComponent(FlowState *flowState, unsigned componentIndex) {
	if (isComponentReadyToRun(flowState, componentIndex)) {
		addToQueue(flowState, componentIndex);
		return true;
	}
	return false;
}


static FlowState *initFlowState(Assets *assets, int flowIndex) {
	if (!g_mutexId) {
		g_mutexId = osMutexCreate(osMutex(g_mutex));		
	}
	auto flowDefinition = assets->flowDefinition.ptr(assets);
	auto flow = flowDefinition->flows.item(assets, flowIndex);

	auto nValues = flow->nInputValues + flow->localVariables.count;

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

	for (unsigned componentIndex = 0; componentIndex < flow->components.count; componentIndex++) {
		pingComponent(flowState, componentIndex);
	}

	return flowState;
}

FlowState *initActionFlowState(Assets *assets, int flowIndex) {
	auto flowState = initFlowState(assets, flowIndex);
	if (flowState) {
		flowState->isAction = true;
	}
	return flowState;
}

FlowState *initPageFlowState(Assets *assets, int flowIndex) {
	auto flowState = initFlowState(assets, flowIndex);
	if (flowState) {
		flowState->isAction = false;
	}
	return flowState;
}

void freeFlowState(FlowState *flowState) {
	auto assets = flowState->assets;
	auto flowDefinition = assets->flowDefinition.ptr(assets);
	auto flow = flowDefinition->flows.item(assets, flowState->flowIndex);

	auto valuesCount = flow->nInputValues + flow->localVariables.count;

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

void propagateValue(FlowState *flowState, ComponentOutput &componentOutput, const gui::Value &value) {
	auto assets = flowState->assets;
	for (unsigned connectionIndex = 0; connectionIndex < componentOutput.connections.count; connectionIndex++) {
		auto connection = componentOutput.connections.item(assets, connectionIndex);
		flowState->values[connection->targetInputIndex] = value;
		if (pingComponent(flowState, connection->targetComponentIndex) && connection->seqIn) {
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
	int32_t iterators[MAX_ITERATORS];
	uint16_t dataId;
	Value value;
} g_getValueFromGuiThreadParams;

void getValueFromGuiThread(uint16_t dataId, const WidgetCursor &widgetCursor, Value &value) {
	while (g_getValueFromGuiThreadParams.isActive) {
		osDelay(0);
	}

	g_getValueFromGuiThreadParams.isActive = true;
	g_getValueFromGuiThreadParams.assets = widgetCursor.flowState->assets;
	g_getValueFromGuiThreadParams.flowState = widgetCursor.flowState;
	for (size_t i = 0; i < MAX_ITERATORS; i++) {
		g_getValueFromGuiThreadParams.iterators[i] = widgetCursor.iterators[i];
	}
	g_getValueFromGuiThreadParams.dataId = dataId;

	scripting::getFlowValueInScriptingThread();

	while (g_getValueFromGuiThreadParams.isActive) {
		osDelay(0);
	}

	value = g_getValueFromGuiThreadParams.value;
}

void doGetFlowValue() {
	if (scripting::isFlowRunning()) {
		Assets *assets = g_getValueFromGuiThreadParams.assets;
		FlowState *flowState = g_getValueFromGuiThreadParams.flowState;
		uint16_t dataId = g_getValueFromGuiThreadParams.dataId;
		
		auto flowDefinition = assets->flowDefinition.ptr(assets);
		auto flow = flowDefinition->flows.item(assets, flowState->flowIndex);

		WidgetDataItem *widgetDataItem = flow->widgetDataItems.item(assets, dataId);
		if (widgetDataItem) {
			auto component = flow->components.item(assets, widgetDataItem->componentIndex);
			auto propertyValue = component->propertyValues.item(assets, widgetDataItem->propertyValueIndex);

			if (!evalExpression(flowState, component, propertyValue->evalInstructions, g_getValueFromGuiThreadParams.value, nullptr, g_getValueFromGuiThreadParams.iterators)) {
				throwError(flowState, component, "doGetFlowValue failed\n");
			}
		}
	}
	g_getValueFromGuiThreadParams.isActive = false;
}

////////////////////////////////////////////////////////////////////////////////

struct {
	bool isActive;
	Assets *assets;
	FlowState *flowState;
	int32_t iterators[MAX_ITERATORS];
	uint16_t dataId;
	Value value;
} g_setValueFromGuiThreadParams;

void setValueFromGuiThread(uint16_t dataId, const WidgetCursor &widgetCursor, const Value& value) {
	while (g_setValueFromGuiThreadParams.isActive) {
		osDelay(0);
	}

	g_setValueFromGuiThreadParams.isActive = true;
	g_setValueFromGuiThreadParams.assets = widgetCursor.flowState->assets;
	g_setValueFromGuiThreadParams.flowState = widgetCursor.flowState;
	for (size_t i = 0; i < MAX_ITERATORS; i++) {
		g_getValueFromGuiThreadParams.iterators[i] = widgetCursor.iterators[i];
	}
	g_setValueFromGuiThreadParams.dataId = dataId;
	g_setValueFromGuiThreadParams.value = value;

	scripting::setFlowValueInScriptingThread();

	while (g_setValueFromGuiThreadParams.isActive) {
		osDelay(0);
	}
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
			if (evalAssignableExpression(flowState, component, propertyValue->evalInstructions, dstValue, nullptr, g_setValueFromGuiThreadParams.iterators)) {
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
