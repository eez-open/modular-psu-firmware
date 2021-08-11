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

#include <eez/util.h>

#include <eez/gui/gui.h>
#include <eez/gui/widgets/input.h>

#include <eez/gui/widgets/layout_view.h>

#include <eez/scripting/scripting.h>

#include <eez/flow/flow.h>
#include <eez/flow/components.h>
#include <eez/flow/queue.h>
#include <eez/flow/flow_defs_v3.h>

using namespace eez::gui;

namespace eez {
namespace flow {

static const uint32_t FLOW_TICK_MAX_DURATION_MS = 500;

static FlowState *g_mainPageFlowState;

////////////////////////////////////////////////////////////////////////////////

unsigned start(Assets *assets) {
	auto flowDefinition = assets->flowDefinition.ptr(assets);
	if (flowDefinition->flows.count == 0) {
		return 0;
	}

	queueInit();

	scpiComponentInit();

	g_mainPageFlowState = initPageFlowState(assets, 0);

	return 1;
}

void tick() {
	if (!g_mainPageFlowState) {
		return;
	}

	uint32_t startTickCount = millis();

	while (true) {
		FlowState *flowState;
		unsigned componentIndex;
		if (!removeFromQueue(flowState, componentIndex)) {
			break;
		}

		executeComponent(flowState, componentIndex);

		auto component = flowState->flow->components.item(flowState->assets, componentIndex);

		if (--flowState->numActiveComponents == 0 && flowState->isAction) {
			auto componentExecutionState = flowState->parentFlowState->componenentExecutionStates[flowState->parentComponentIndex];
			if (componentExecutionState) {
				ObjectAllocator<ComponenentExecutionState>::deallocate(componentExecutionState);
				flowState->parentFlowState->componenentExecutionStates[flowState->parentComponentIndex] = nullptr;
			} else {
				throwError(flowState, component, "Unexpected: no CallAction component state\n");
				return;
			}
		}

		if (component->type == defs_v3::COMPONENT_TYPE_DELAY_ACTION) {
			break;
		}

		if (millis() - startTickCount >= FLOW_TICK_MAX_DURATION_MS) {
			break;
		}
	}
}

void stop() {
	if (g_mainPageFlowState) {
		freeFlowState(g_mainPageFlowState);
		g_mainPageFlowState = nullptr;
	}
}

FlowState *getFlowState(int16_t pageId, const WidgetCursor &widgetCursor) {
	pageId = -pageId - 1;

	if (widgetCursor.widget && widgetCursor.widget->type == WIDGET_TYPE_LAYOUT_VIEW) {
		if (widgetCursor.flowState) {
			auto layoutViewWidget = (LayoutViewWidget *)widgetCursor.widget;

			auto flowState = widgetCursor.flowState;

			struct LayoutViewWidgetExecutionState : public ComponenentExecutionState {
				FlowState *flowState;

				~LayoutViewWidgetExecutionState() {
					freeFlowState(flowState);
				}
			};

			auto layoutViewWidgetComponentIndex = layoutViewWidget->componentIndex;

			auto layoutViewWidgetExecutionState = (LayoutViewWidgetExecutionState *)flowState->componenentExecutionStates[layoutViewWidgetComponentIndex];
			if (!layoutViewWidgetExecutionState) {
				layoutViewWidgetExecutionState =  ObjectAllocator<LayoutViewWidgetExecutionState>::allocate(0xa570ccad);
				flowState->componenentExecutionStates[layoutViewWidgetComponentIndex] = layoutViewWidgetExecutionState;

				auto layoutViewFlowState = initPageFlowState(flowState->assets, pageId);

				layoutViewFlowState->parentFlowState = flowState;

				auto component = flowState->flow->components.item(flowState->assets, layoutViewWidgetComponentIndex);

				layoutViewFlowState->parentComponent = component;
				layoutViewFlowState->parentComponentIndex = layoutViewWidgetComponentIndex;

				layoutViewWidgetExecutionState->flowState = layoutViewFlowState;
			}

			return layoutViewWidgetExecutionState->flowState;
		}
	}
	
	return g_mainPageFlowState;
}

void executeFlowAction(const gui::WidgetCursor &widgetCursor, int16_t actionId) {
	auto flowState = widgetCursor.flowState;
	actionId = -actionId - 1;

	auto assets = flowState->assets;
	auto flow = flowState->flow;

	if (actionId >= 0 && actionId < (int16_t)flow->widgetActions.count) {
		auto componentOutput = flow->widgetActions.item(assets, actionId);
		if (componentOutput) {
			propagateValue(flowState, *componentOutput, widgetCursor.cursor);
		}
	}

	for (int i = 0; i < 3; i++) {
		tick();
	}
}

void dataOperation(int16_t dataId, DataOperationEnum operation, const gui::WidgetCursor &widgetCursor, Value &value) {
	auto flowState = widgetCursor.flowState;

	dataId = -dataId - 1;

	auto assets = flowState->assets;
	auto flow = flowState->flow;

	if (dataId >= 0 && dataId < (int16_t)flow->widgetDataItems.count) {
		if (operation == DATA_OPERATION_GET) {
			getValue(dataId, widgetCursor, value);
		} else if (operation == DATA_OPERATION_COUNT) {
			Value arrayValue;
			getValue(dataId, widgetCursor, arrayValue);
			if (arrayValue.getType() == VALUE_TYPE_ARRAY_REF) {
				value = ((ArrayRef *)arrayValue.refValue)->arraySize;
			} else if (arrayValue.getType() == VALUE_TYPE_ASSETS_ARRAY) {
				auto assetsArray = ((AssetsPtr<AssetsArray> *)&arrayValue.assetsOffsetValue)->ptr(assets);
				value = assetsArray->arraySize;
			} else {
				value = 0;
			}
		} if (operation == DATA_OPERATION_GET_MIN) {
			WidgetDataItem *widgetDataItem = flow->widgetDataItems.item(assets, dataId);
			auto component = flow->components.item(assets, widgetDataItem->componentIndex);
			if (component->type == WIDGET_TYPE_INPUT) {
				auto inputWidget = (InputWidget *)widgetCursor.widget;
				auto unitValue = get(widgetCursor, inputWidget->unit);
				Unit unit = getUnitFromName(unitValue.toString(assets, 0x5049bd52).getString());
				value = Value(get(widgetCursor, inputWidget->min).toFloat(), unit);
			}
		} else if (operation == DATA_OPERATION_GET_MAX) {
			WidgetDataItem *widgetDataItem = flow->widgetDataItems.item(assets, dataId);
			auto component = flow->components.item(assets, widgetDataItem->componentIndex);
			if (component->type == WIDGET_TYPE_INPUT) {
				auto inputWidget = (InputWidget *)widgetCursor.widget;
				auto unitValue = get(widgetCursor, inputWidget->unit);
				Unit unit = getUnitFromName(unitValue.toString(assets, 0x5049bd52).getString());
				value = Value(get(widgetCursor, inputWidget->max).toFloat(), unit);
			}
		} else if (operation == DATA_OPERATION_GET_PRECISION) {
			WidgetDataItem *widgetDataItem = flow->widgetDataItems.item(assets, dataId);
			auto component = flow->components.item(assets, widgetDataItem->componentIndex);
			if (component->type == WIDGET_TYPE_INPUT) {
				auto inputWidget = (InputWidget *)widgetCursor.widget;
				auto unitValue = get(widgetCursor, inputWidget->unit);
				Unit unit = getUnitFromName(unitValue.toString(assets, 0x5049bd52).getString());
				value = Value(get(widgetCursor, inputWidget->precision).toFloat(), unit);
			}
		} else if (operation == DATA_OPERATION_GET_UNIT) {
			WidgetDataItem *widgetDataItem = flow->widgetDataItems.item(assets, dataId);
			auto component = flow->components.item(assets, widgetDataItem->componentIndex);
			if (component->type == WIDGET_TYPE_INPUT) {
				auto inputWidget = (InputWidget *)widgetCursor.widget;
				auto unitValue = get(widgetCursor, inputWidget->unit);
				Unit unit = getUnitFromName(unitValue.toString(assets, 0x5049bd52).getString());
				value = unit;
			}
		} else if (operation == DATA_OPERATION_SET) {
			WidgetDataItem *widgetDataItem = flow->widgetDataItems.item(assets, dataId);
			auto component = flow->components.item(assets, widgetDataItem->componentIndex);
			if (component->type == WIDGET_TYPE_INPUT) {
				auto inputWidget = (InputWidget *)widgetCursor.widget;

				if (inputWidget->flags & INPUT_WIDGET_TYPE_NUMBER) {
					float precision = get(widgetCursor, inputWidget->precision).toFloat();
					float valueFloat = value.toFloat();
					setValue(dataId, widgetCursor, Value(roundPrec(valueFloat, precision), VALUE_TYPE_FLOAT));
				} else {
					setValue(dataId, widgetCursor, value);
				}

				scripting::executeFlowAction(widgetCursor, inputWidget->action);
			} else {
				setValue(dataId, widgetCursor, value);
			}
		}
	} else {
		// TODO this shouldn't happen
		value = Value();
	}
}

} // namespace flow
} // namespace eez
