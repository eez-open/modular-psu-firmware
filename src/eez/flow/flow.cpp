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

#include <eez/os.h>

#include <eez/gui/gui.h>
#include <eez/gui/widgets/input.h>

#include <eez/gui/widgets/containers/layout_view.h>

#include <eez/flow/flow.h>
#include <eez/flow/components.h>
#include <eez/flow/queue.h>
#include <eez/flow/flow_defs_v3.h>
#include <eez/flow/debugger.h>

using namespace eez::gui;

namespace eez {
namespace flow {

static const uint32_t FLOW_TICK_MAX_DURATION_MS = 20;

FlowState *g_mainPageFlowState;

static const uint32_t MAX_PAGES = 100;
FlowState *g_pagesFlowState[MAX_PAGES];

////////////////////////////////////////////////////////////////////////////////

unsigned start(Assets *assets) {
	auto flowDefinition = assets->flowDefinition.ptr(assets);
	if (flowDefinition->flows.count == 0) {
		return 0;
	}

	g_lastFlowStateIndex = 0;

	queueReset();

	scpiComponentInit();

	fixAssetValues(assets);

	for (uint32_t i = 0; i < MAX_PAGES; i++) {
		g_pagesFlowState[i] = nullptr;
	}

	onStarted(assets);

	return 1;
}

void tick() {
	if (!isFlowRunningHook()) {
		return;
	}

	uint32_t startTickCount = millis();

	while (true) {
		FlowState *flowState;
		unsigned componentIndex;
		if (!peekNextTaskFromQueue(flowState, componentIndex)) {
			break;
		}

		if (!canExecuteStep(flowState, componentIndex)) {
			break;
		}

		removeNextTaskFromQueue();

		executeComponent(flowState, componentIndex);

		auto component = flowState->flow->components.item(flowState->assets, componentIndex);

		auto assets = flowState->assets;

		for (uint32_t i = 0; i < component->inputs.count; i++) {
			auto inputIndex = component->inputs.ptr(flowState->assets)[i];
			if (flowState->flow->componentInputs.item(assets, inputIndex)->flags & COMPONENT_INPUT_FLAG_IS_SEQ_INPUT) {
				flowState->values[inputIndex] = Value();
			}
		}

		if (--flowState->numActiveComponents == 0 && flowState->isAction) {
			auto componentExecutionState = flowState->parentFlowState->componenentExecutionStates[flowState->parentComponentIndex];
			if (componentExecutionState) {
				flowState->parentFlowState->componenentExecutionStates[flowState->parentComponentIndex] = nullptr;
				ObjectAllocator<ComponenentExecutionState>::deallocate(componentExecutionState);
			} else {
				throwError(flowState, componentIndex, "Unexpected: no CallAction component state\n");
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
		for (unsigned i = 0; i < MAX_PAGES && g_pagesFlowState[i]; i++) {
			freeFlowState(g_pagesFlowState[i]);
		}
		g_mainPageFlowState = nullptr;
	}
	queueReset();
}

FlowState *getFlowState(int16_t pageId, const WidgetCursor &widgetCursor) {
	if (!isFlowRunningHook()) {
		return nullptr;
	}

	auto assets = widgetCursor.assets;

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

				auto layoutViewFlowState = initPageFlowState(flowState->assets, pageId, flowState, layoutViewWidgetComponentIndex);

				layoutViewWidgetExecutionState->flowState = layoutViewFlowState;
			}

			return layoutViewWidgetExecutionState->flowState;
		}
	} else {
		auto pageIndex = pageId;
		auto page = assets->pages.item(assets, pageIndex);
		if (!(page->flags & PAGE_IS_USED_AS_CUSTOM_WIDGET)) {
			if (!g_pagesFlowState[pageIndex]) {
				g_pagesFlowState[pageIndex] = initPageFlowState(assets, pageIndex, nullptr, 0);
				if (pageIndex == 0) {
					g_mainPageFlowState = g_pagesFlowState[0];
				}
			}
			return g_pagesFlowState[pageIndex];
		}
	}

	return nullptr;
}

int getPageIndex(FlowState *flowState) {
	for (uint32_t pageIndex = 0; pageIndex < MAX_PAGES; pageIndex++) {
		if (g_pagesFlowState[pageIndex] == flowState) {
			return (int)pageIndex;
		}
	}
	return -1;
}

void executeFlowAction(const gui::WidgetCursor &widgetCursor, int16_t actionId) {
	if (!isFlowRunningHook()) {
		return;
	}

	auto flowState = widgetCursor.flowState;
	actionId = -actionId - 1;

	auto assets = flowState->assets;
	auto flow = flowState->flow;

	if (actionId >= 0 && actionId < (int16_t)flow->widgetActions.count) {
		auto componentOutput = flow->widgetActions.item(assets, actionId);
		if (componentOutput->componentIndex != -1 && componentOutput->componentOutputIndex != -1) {
			propagateValue(flowState, componentOutput->componentIndex, componentOutput->componentOutputIndex, widgetCursor.cursor);
		}
	}

	for (int i = 0; i < 3; i++) {
		tick();
	}
}

void dataOperation(int16_t dataId, DataOperationEnum operation, const gui::WidgetCursor &widgetCursor, Value &value) {
	if (!isFlowRunningHook()) {
		return;
	}

	auto flowState = widgetCursor.flowState;

	auto flowDataId = -dataId - 1;

	auto assets = flowState->assets;
	auto flow = flowState->flow;

	if (flowDataId >= 0 && flowDataId < (int16_t)flow->widgetDataItems.count) {
		WidgetDataItem *widgetDataItem = flow->widgetDataItems.item(assets, flowDataId);
		auto component = flow->components.item(assets, widgetDataItem->componentIndex);

		if (operation == DATA_OPERATION_GET) {
			getValue(flowDataId, operation, widgetCursor, value);
			if (component->type == WIDGET_TYPE_INPUT && dataId == widgetCursor.widget->data) {
				value = getInputWidgetData(widgetCursor, value);
			}
		} else if (operation == DATA_OPERATION_COUNT) {
			Value arrayValue;
			getValue(flowDataId, operation, widgetCursor, arrayValue);
			if (arrayValue.getType() == VALUE_TYPE_ARRAY) {
				value = arrayValue.arrayValue->arraySize;
			} else {
				value = 0;
			}
		}  else if (operation == DATA_OPERATION_GET_TEXT_CURSOR_POSITION) {
			getValue(flowDataId, operation, widgetCursor, value);
		} else if (operation == DATA_OPERATION_GET_MIN) {
			if (component->type == WIDGET_TYPE_INPUT) {
				value = getInputWidgetMin(widgetCursor);
			}
		} else if (operation == DATA_OPERATION_GET_MAX) {
			if (component->type == WIDGET_TYPE_INPUT) {
				value = getInputWidgetMax(widgetCursor);
			}
		} else if (operation == DATA_OPERATION_GET_PRECISION) {
			if (component->type == WIDGET_TYPE_INPUT) {
				value = getInputWidgetPrecision(widgetCursor);
			}
		} else if (operation == DATA_OPERATION_GET_UNIT) {
			if (component->type == WIDGET_TYPE_INPUT) {
				value = getBaseUnit(getInputWidgetUnit(widgetCursor));
			}
		} else if (operation == DATA_OPERATION_SET) {
			if (component->type == WIDGET_TYPE_INPUT) {
				auto inputWidget = (InputWidget *)widgetCursor.widget;
				if (inputWidget->flags & INPUT_WIDGET_TYPE_NUMBER) {
					if (value.isInt32()) {
						setValue(flowDataId, widgetCursor, value);
					} else {
						Value precisionValue = getInputWidgetPrecision(widgetCursor);
						float precision = precisionValue.toFloat();
						float valueFloat = value.toFloat();
						Unit unit = getInputWidgetUnit(widgetCursor);
						setValue(flowDataId, widgetCursor, Value(roundPrec(valueFloat, precision) / getUnitFactor(unit), VALUE_TYPE_FLOAT));
					}
				} else {
					setValue(flowDataId, widgetCursor, value);
				}

				executeFlowAction(widgetCursor, inputWidget->action);
			} else {
				setValue(flowDataId, widgetCursor, value);
			}
		}
	} else {
		// TODO this shouldn't happen
		value = Value();
	}
}

} // namespace flow
} // namespace eez
