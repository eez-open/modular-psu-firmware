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

#include <eez/flow/flow.h>
#include <eez/flow/components.h>
#include <eez/flow/queue.h>

using namespace eez::gui;

namespace eez {
namespace flow {

static Assets *g_assets;
static FlowState *g_mainPageFlowState;

////////////////////////////////////////////////////////////////////////////////

unsigned start(Assets *assets) {
	auto flowDefinition = assets->flowDefinition.ptr(assets);
	if (flowDefinition->flows.count == 0) {
		return 0;
	}

	g_assets = assets;

	queueInit();

	scpiComponentInit();

	g_mainPageFlowState = initFlowState(assets, 0);

	auto flowState = g_mainPageFlowState;
	auto flow = flowDefinition->flows.item(assets, flowState->flowIndex);

	for (unsigned componentIndex = 0; componentIndex < flow->components.count; componentIndex++) {
		pingComponent(assets, flowState, componentIndex);
	}

	return 1;
}

void tick(unsigned flowHandle) {
	if (flowHandle != 1) {
		return;
	}

	Assets *assets = g_assets;

	FlowState *flowState;
	unsigned componentIndex;
	ComponenentExecutionState *componentExecutionState;
	if (removeFromQueue(flowState, componentIndex, componentExecutionState)) {
		executeComponent(assets, flowState, componentIndex, componentExecutionState);

		recalcFlowDataItems(assets, flowState);
	}
}

void stop() {
	FlowState *flowState;
	unsigned componentIndex;
	ComponenentExecutionState *componentExecutionState;
	while (removeFromQueue(flowState, componentIndex, componentExecutionState)) {
		if (componentExecutionState) {
			ObjectAllocator<ComponenentExecutionState>::deallocate(componentExecutionState);
		}
	}
}

void *getFlowState(int16_t pageId) {
	pageId = -pageId - 1;
	return g_mainPageFlowState;
}

void executeFlowAction(unsigned flowHandle, const gui::WidgetCursor &widgetCursor, int16_t actionId) {
	auto flowState = (FlowState *)widgetCursor.pageState;
	actionId = -actionId - 1;

	Assets *assets = g_assets;
	auto flowDefinition = assets->flowDefinition.ptr(assets);

	auto flow = flowDefinition->flows.item(assets, flowState->flowIndex);

	if (actionId >= 0 && actionId < (int16_t)flow->widgetActions.count) {
		auto componentOutput = flow->widgetActions.item(assets, actionId);
		if (componentOutput) {
			auto &nullValue = *flowDefinition->constants.item(assets, NULL_VALUE_INDEX);
			propagateValue(assets, flowState, *componentOutput, nullValue);
		}
	}
}

void dataOperation(unsigned flowHandle, int16_t dataId, DataOperationEnum operation, const gui::WidgetCursor &widgetCursor, Value &value) {
	getDataItemValue(g_assets, (FlowState *)widgetCursor.pageState, -dataId - 1, value);
}

} // namespace flow
} // namespace eez
