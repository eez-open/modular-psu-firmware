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

#include <eez/gui/gui.h>
#include <eez/gui/widgets/input.h>

#include <eez/scripting/scripting.h>

#include <eez/flow/flow.h>
#include <eez/flow/components.h>
#include <eez/flow/queue.h>

using namespace eez::gui;

namespace eez {
namespace flow {

static FlowState *g_mainPageFlowState;

////////////////////////////////////////////////////////////////////////////////

unsigned start(Assets *assets) {
	auto flowDefinition = assets->flowDefinition.ptr(assets);
	if (flowDefinition->flows.count == 0) {
		return 0;
	}

	queueInit();

	scpiComponentInit();

	g_mainPageFlowState = initFlowState(assets, 0);

	return 1;
}

void tick(unsigned flowHandle) {
	if (flowHandle != 1) {
		return;
	}

	FlowState *flowState;
	unsigned componentIndex;
	if (removeFromQueue(flowState, componentIndex)) {
		executeComponent(flowState, componentIndex);

		if (--flowState->numActiveComponents == 0) {
			freeFlowState(flowState);
		}

		recalcFlowDataItems(flowState);
	}
}

void stop() {
	FlowState *flowState;
	unsigned componentIndex;
	while (removeFromQueue(flowState, componentIndex)) {
		if (--flowState->numActiveComponents == 0) {
			freeFlowState(flowState);
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

	Assets *assets = flowState->assets;
	auto flowDefinition = assets->flowDefinition.ptr(assets);
	auto flow = flowDefinition->flows.item(assets, flowState->flowIndex);

	if (actionId >= 0 && actionId < (int16_t)flow->widgetActions.count) {
		auto componentOutput = flow->widgetActions.item(assets, actionId);
		if (componentOutput) {
			propagateValue(flowState, *componentOutput);
		}
	}
}

void dataOperation(unsigned flowHandle, int16_t dataId, DataOperationEnum operation, const gui::WidgetCursor &widgetCursor, Value &value) {
	auto flowState = (FlowState *)widgetCursor.pageState;

	dataId = -dataId - 1;

	auto assets = flowState->assets;
	auto flowDefinition = assets->flowDefinition.ptr(assets);
	auto flow = flowDefinition->flows.item(assets, flowState->flowIndex);
	auto dataItemsOffset = flow->nInputValues + flow->localVariables.count;

	if (dataId >= 0 && dataId < (int16_t)flow->widgetDataItems.count) {
		if (operation == DATA_OPERATION_GET) {
			value = flowState->values[dataItemsOffset + dataId];
		} else {
			WidgetDataItem *widgetDataItem = flow->widgetDataItems.item(assets, dataId);
			if (widgetDataItem) {
				auto component = flow->components.item(assets, widgetDataItem->componentIndex);
				if (component->type == WIDGET_TYPE_INPUT) {
					auto inputWidget = (InputWidget *)widgetCursor.widget;
					
					auto unitValue = get(widgetCursor, inputWidget->unit);
					Unit unit = getUnitFromName(unitValue.toString(assets, 0x5049bd52).getString()); 
					
					if (operation == DATA_OPERATION_GET_MIN) {
						value = Value(get(widgetCursor, inputWidget->min).toFloat(), unit);
					} else if (operation == DATA_OPERATION_GET_MAX) {
						value = Value(get(widgetCursor, inputWidget->max).toFloat(), unit);
					} else if (operation == DATA_OPERATION_GET_PRECISION) {
						value = Value(get(widgetCursor, inputWidget->precision).toFloat(), unit);
					} else if (operation == DATA_OPERATION_GET_UNIT) {
						value = unit;
					} else if (operation == DATA_OPERATION_SET) {
						setValueFromGuiThread(flowState, dataId, value);
						scripting::executeFlowAction(widgetCursor, inputWidget->action);
					}
				}
			}
		}
	} else {
		// TODO this shouldn't happen
		value = Value();
	}
}

} // namespace flow
} // namespace eez
