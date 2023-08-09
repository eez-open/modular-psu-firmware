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

#include <eez/conf-internal.h>

#include <stdio.h>

#include <eez/core/util.h>

#include <eez/core/os.h>

#include <eez/flow/flow.h>
#include <eez/flow/components.h>
#include <eez/flow/queue.h>
#include <eez/flow/flow_defs_v3.h>
#include <eez/flow/debugger.h>
#include <eez/flow/hooks.h>
#include <eez/flow/components/lvgl_user_widget.h>

#if EEZ_OPTION_GUI
#include <eez/gui/gui.h>
#include <eez/gui/keypad.h>
#include <eez/gui/widgets/input.h>
#include <eez/gui/widgets/containers/user_widget.h>
using namespace eez::gui;
#endif

#if defined(EEZ_DASHBOARD_API)
#include <eez/flow/dashboard_api.h>
#endif

namespace eez {
namespace flow {

#if defined(__EMSCRIPTEN__)
uint32_t g_wasmModuleId = 0;
#endif

static const uint32_t FLOW_TICK_MAX_DURATION_MS = 5;

int g_selectedLanguage = 0;
FlowState *g_firstFlowState;
FlowState *g_lastFlowState;

static bool g_isStopping = false;
static bool g_isStopped = true;

static void doStop();

////////////////////////////////////////////////////////////////////////////////

unsigned start(Assets *assets) {
	auto flowDefinition = static_cast<FlowDefinition *>(assets->flowDefinition);
	if (flowDefinition->flows.count == 0) {
		return 0;
	}

    g_isStopped = false;

	queueReset();

	scpiComponentInitHook();

	onStarted(assets);

	return 1;
}

void tick() {
	if (isFlowStopped()) {
		return;
	}

    if (g_isStopping) {
        doStop();
        return;
    }

	uint32_t startTickCount = millis();

    auto n = getQueueSize();

    for (size_t i = 0; i < n || g_numContinuousTaskInQueue > 0; i++) {
		FlowState *flowState;
		unsigned componentIndex;
        bool continuousTask;
		if (!peekNextTaskFromQueue(flowState, componentIndex, continuousTask)) {
			break;
		}

		if (!continuousTask && !canExecuteStep(flowState, componentIndex)) {
			break;
		}

		removeNextTaskFromQueue();

        flowState->executingComponentIndex = componentIndex;

        if (flowState->error) {
            deallocateComponentExecutionState(flowState, componentIndex);
        } else {
            if (continuousTask) {
                auto componentExecutionState = (ComponenentExecutionState *)flowState->componenentExecutionStates[componentIndex];
                if (!componentExecutionState) {
                    executeComponent(flowState, componentIndex);
                } else if (componentExecutionState->lastExecutedTime + FLOW_TICK_MAX_DURATION_MS <= startTickCount) {
                    componentExecutionState->lastExecutedTime = startTickCount;
                    executeComponent(flowState, componentIndex);
                } else {
                    addToQueue(flowState, componentIndex, -1, -1, -1, true);
                }
            } else {
                executeComponent(flowState, componentIndex);
            }
        }

        if (isFlowStopped() || g_isStopping) {
            break;
        }

        resetSequenceInputs(flowState);

        if (canFreeFlowState(flowState)) {
            freeFlowState(flowState);
        }

        if ((i + 1) % 5 == 0) {
            if (millis() - startTickCount >= FLOW_TICK_MAX_DURATION_MS) {
                break;
            }
        }
	}

	finishToDebuggerMessageHook();
}

void freeAllChildrenFlowStates(FlowState *firstChildFlowState) {
    auto flowState = firstChildFlowState;
    while (flowState != nullptr) {
        auto nextFlowState = flowState->nextSibling;
        freeAllChildrenFlowStates(flowState->firstChild);
        freeFlowState(flowState);
        flowState = nextFlowState;
    }
}

void stop() {
    g_isStopping = true;
}

void doStop() {
    onStopped();
    finishToDebuggerMessageHook();
    g_debuggerIsConnected = false;

    freeAllChildrenFlowStates(g_firstFlowState);
    g_firstFlowState = nullptr;
    g_lastFlowState = nullptr;

    g_isStopped = true;

	queueReset();
}

bool isFlowStopped() {
    return g_isStopped;
}

#if EEZ_OPTION_GUI

FlowState *getPageFlowState(Assets *assets, int16_t pageIndex, const WidgetCursor &widgetCursor) {
	if (!assets->flowDefinition) {
		return nullptr;
	}

	if (isFlowStopped()) {
		return nullptr;
	}

	if (widgetCursor.widget && widgetCursor.widget->type == WIDGET_TYPE_USER_WIDGET) {
		if (widgetCursor.flowState) {
			auto userWidgetWidget = (UserWidgetWidget *)widgetCursor.widget;
			auto flowState = widgetCursor.flowState;
			auto userWidgetWidgetComponentIndex = userWidgetWidget->componentIndex;

			return getUserWidgetFlowState(flowState, userWidgetWidgetComponentIndex, pageIndex);
		}
	} else {
		auto page = assets->pages[pageIndex];
		if (!(page->flags & PAGE_IS_USED_AS_USER_WIDGET)) {
            FlowState *flowState;
            for (flowState = g_firstFlowState; flowState; flowState = flowState->nextSibling) {
                if (flowState->flowIndex == pageIndex) {
                    break;
                }
            }

            if (!flowState) {
				flowState = initPageFlowState(assets, pageIndex, nullptr, 0);
			}

			return flowState;
		}
	}

	return nullptr;
}

#else

FlowState *getPageFlowState(Assets *assets, int16_t pageIndex) {
	if (!assets->flowDefinition) {
		return nullptr;
	}

	if (isFlowStopped()) {
		return nullptr;
	}

    FlowState *flowState;
    for (flowState = g_firstFlowState; flowState; flowState = flowState->nextSibling) {
        if (flowState->flowIndex == pageIndex) {
            break;
        }
    }

    if (!flowState) {
        flowState = initPageFlowState(assets, pageIndex, nullptr, 0);
    }

    return flowState;
}

#endif // EEZ_OPTION_GUI

int getPageIndex(FlowState *flowState) {
	return flowState->flowIndex;
}

Value getGlobalVariable(uint32_t globalVariableIndex) {
    return getGlobalVariable(g_mainAssets, globalVariableIndex);
}

Value getGlobalVariable(Assets *assets, uint32_t globalVariableIndex) {
    if (globalVariableIndex >= 0 && globalVariableIndex < assets->flowDefinition->globalVariables.count) {
        return *assets->flowDefinition->globalVariables[globalVariableIndex];
    }
    return Value();
}

void setGlobalVariable(uint32_t globalVariableIndex, const Value &value) {
    setGlobalVariable(g_mainAssets, globalVariableIndex, value);
}

void setGlobalVariable(Assets *assets, uint32_t globalVariableIndex, const Value &value) {
    if (globalVariableIndex >= 0 && globalVariableIndex < assets->flowDefinition->globalVariables.count) {
        *assets->flowDefinition->globalVariables[globalVariableIndex] = value;
    }
}

#if EEZ_OPTION_GUI
void executeFlowAction(const WidgetCursor &widgetCursor, int16_t actionId, void *param) {
	if (isFlowStopped()) {
		return;
	}

	auto flowState = widgetCursor.flowState;
	actionId = -actionId - 1;

	auto flow = flowState->flow;

	if (actionId >= 0 && actionId < (int16_t)flow->widgetActions.count) {
		auto componentOutput = flow->widgetActions[actionId];
		if (componentOutput->componentIndex != -1 && componentOutput->componentOutputIndex != -1) {
            if (widgetCursor.widget->type == WIDGET_TYPE_DROP_DOWN_LIST) {
                auto params = Value::makeArrayRef(defs_v3::SYSTEM_STRUCTURE_DROP_DOWN_LIST_CHANGE_EVENT_NUM_FIELDS, defs_v3::SYSTEM_STRUCTURE_DROP_DOWN_LIST_CHANGE_EVENT, 0x53e3b30b);

                // index
                ((ArrayValueRef *)params.refValue)->arrayValue.values[defs_v3::SYSTEM_STRUCTURE_DROP_DOWN_LIST_CHANGE_EVENT_FIELD_INDEX] = widgetCursor.iterators[0];

                // indexes
                auto indexes = Value::makeArrayRef(MAX_ITERATORS, defs_v3::ARRAY_TYPE_INTEGER, 0xb1f68ef8);
                for (size_t i = 0; i < MAX_ITERATORS; i++) {
                    ((ArrayValueRef *)indexes.refValue)->arrayValue.values[i] = (int)widgetCursor.iterators[i];
                }
                ((ArrayValueRef *)params.refValue)->arrayValue.values[defs_v3::SYSTEM_STRUCTURE_DROP_DOWN_LIST_CHANGE_EVENT_FIELD_INDEXES] = indexes;

                // selectedIndex
                ((ArrayValueRef *)params.refValue)->arrayValue.values[defs_v3::SYSTEM_STRUCTURE_DROP_DOWN_LIST_CHANGE_EVENT_FIELD_SELECTED_INDEX] = *((int *)param);

                propagateValue(flowState, componentOutput->componentIndex, componentOutput->componentOutputIndex, params);
            } else {
                auto params = Value::makeArrayRef(defs_v3::SYSTEM_STRUCTURE_CLICK_EVENT_NUM_FIELDS, defs_v3::SYSTEM_STRUCTURE_CLICK_EVENT, 0x285940bb);

                // index
                ((ArrayValueRef *)params.refValue)->arrayValue.values[defs_v3::SYSTEM_STRUCTURE_CLICK_EVENT_FIELD_INDEX] = widgetCursor.iterators[0];

                // indexes
                auto indexes = Value::makeArrayRef(MAX_ITERATORS, defs_v3::ARRAY_TYPE_INTEGER, 0xb1f68ef8);
                for (size_t i = 0; i < MAX_ITERATORS; i++) {
                    ((ArrayValueRef *)indexes.refValue)->arrayValue.values[i] = (int)widgetCursor.iterators[i];
                }
                ((ArrayValueRef *)params.refValue)->arrayValue.values[defs_v3::SYSTEM_STRUCTURE_CLICK_EVENT_FIELD_INDEXES] = indexes;

                propagateValue(flowState, componentOutput->componentIndex, componentOutput->componentOutputIndex, params);
            }
		} else if (componentOutput->componentOutputIndex != -1) {
            propagateValue(flowState, componentOutput->componentIndex, componentOutput->componentOutputIndex);
        }
	}

	for (int i = 0; i < 3; i++) {
		tick();
	}
}

void dataOperation(int16_t dataId, DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
	if (isFlowStopped()) {
		return;
	}

	auto flowState = widgetCursor.flowState;

	auto flowDataId = -dataId - 1;

	auto flow = flowState->flow;

	if (flowDataId >= 0 && flowDataId < (int16_t)flow->widgetDataItems.count) {
		WidgetDataItem *widgetDataItem = flow->widgetDataItems[flowDataId];
		auto component = flow->components[widgetDataItem->componentIndex];

		if (operation == DATA_OPERATION_GET) {
			getValue(flowDataId, operation, widgetCursor, value);
			if (component->type == WIDGET_TYPE_INPUT && dataId == widgetCursor.widget->data) {
				value = getInputWidgetData(widgetCursor, value);
			}
		} else if (operation == DATA_OPERATION_COUNT) {
			Value arrayValue;
			getValue(flowDataId, operation, widgetCursor, arrayValue);
			if (arrayValue.isArray()) {
                auto array = arrayValue.getArray();
                if (array->arrayType == defs_v3::SYSTEM_STRUCTURE_SCROLLBAR_STATE) {
                    value = array->values[defs_v3::SYSTEM_STRUCTURE_SCROLLBAR_STATE_FIELD_NUM_ITEMS];
                } else {
				    value = array->arraySize;
                }
			} else {
				value = 0;
			}
		}
		else if (operation == DATA_OPERATION_GET_MIN) {
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

				executeFlowAction(widgetCursor, inputWidget->action, nullptr);
			} else {
				setValue(flowDataId, widgetCursor, value);
			}
		} else if (operation == DATA_OPERATION_YT_DATA_GET_SIZE) {
            Value arrayValue;
            getValue(flowDataId, operation, widgetCursor, arrayValue);
            if (arrayValue.isArray()) {
                auto array = arrayValue.getArray();
                if (array->arrayType == defs_v3::SYSTEM_STRUCTURE_SCROLLBAR_STATE) {
                    value = array->values[defs_v3::SYSTEM_STRUCTURE_SCROLLBAR_STATE_FIELD_NUM_ITEMS].toInt32();
                } else {
                    value = 0;
                }
            } else {
                value = 0;
            }
        } else if (operation == DATA_OPERATION_YT_DATA_GET_PAGE_SIZE) {
            Value arrayValue;
            getValue(flowDataId, operation, widgetCursor, arrayValue);
            if (arrayValue.isArray()) {
                auto array = arrayValue.getArray();
                if (array->arrayType == defs_v3::SYSTEM_STRUCTURE_SCROLLBAR_STATE) {
                    value = array->values[defs_v3::SYSTEM_STRUCTURE_SCROLLBAR_STATE_FIELD_ITEMS_PER_PAGE].toInt32();
                } else {
                    value = 0;
                }
            } else {
                value = 0;
            }
        } else if (operation == DATA_OPERATION_YT_DATA_GET_POSITION_INCREMENT) {
            Value arrayValue;
            getValue(flowDataId, operation, widgetCursor, arrayValue);
            if (arrayValue.isArray()) {
                auto array = arrayValue.getArray();
                if (array->arrayType == defs_v3::SYSTEM_STRUCTURE_SCROLLBAR_STATE) {
                    value = array->values[defs_v3::SYSTEM_STRUCTURE_SCROLLBAR_STATE_FIELD_POSITION_INCREMENT].toInt32();
                } else {
                    value = 0;
                }
            } else {
                value = 0;
            }
        } else if (operation == DATA_OPERATION_YT_DATA_GET_POSITION) {
            Value arrayValue;
            getValue(flowDataId, operation, widgetCursor, arrayValue);
            if (arrayValue.isArray()) {
                auto array = arrayValue.getArray();
                if (array->arrayType == defs_v3::SYSTEM_STRUCTURE_SCROLLBAR_STATE) {
                    value = array->values[defs_v3::SYSTEM_STRUCTURE_SCROLLBAR_STATE_FIELD_POSITION].toInt32();
                } else {
                    value = 0;
                }
            } else {
                value = 0;
            }
        } else if (operation == DATA_OPERATION_YT_DATA_SET_POSITION) {
            Value arrayValue;
            getValue(flowDataId, operation, widgetCursor, arrayValue);
            if (arrayValue.isArray()) {
                auto array = arrayValue.getArray();
                if (array->arrayType == defs_v3::SYSTEM_STRUCTURE_SCROLLBAR_STATE) {
                    auto newPosition = value.getInt();
                    auto numItems = array->values[defs_v3::SYSTEM_STRUCTURE_SCROLLBAR_STATE_FIELD_NUM_ITEMS].getInt();
                    auto itemsPerPage = array->values[defs_v3::SYSTEM_STRUCTURE_SCROLLBAR_STATE_FIELD_ITEMS_PER_PAGE].getInt();
                    if (newPosition < 0) {
                        newPosition = 0;
                    } else if (newPosition > numItems - itemsPerPage) {
                        newPosition = numItems - itemsPerPage;
                    }
                    array->values[defs_v3::SYSTEM_STRUCTURE_SCROLLBAR_STATE_FIELD_POSITION] = newPosition;
                    onValueChanged(&array->values[defs_v3::SYSTEM_STRUCTURE_SCROLLBAR_STATE_FIELD_POSITION]);
                } else {
                    value = 0;
                }
            } else {
                value = 0;
            }
        } else if (operation == DATA_OPERATION_GET_TEXT_REFRESH_RATE) {
            getValue(flowDataId, operation, widgetCursor, value);
        }
#if OPTION_KEYPAD
		else if (operation == DATA_OPERATION_GET_TEXT_CURSOR_POSITION) {
            getValue(flowDataId, operation, widgetCursor, value);
		}
#endif

	} else {
		// TODO this shouldn't happen
		value = Value();
	}
}

#endif // EEZ_OPTION_GUI

void onArrayValueFree(ArrayValue *arrayValue) {
#if defined(EEZ_DASHBOARD_API)
    if (g_dashboardValueFree) {
        return;
    }
#endif

    if (arrayValue->arrayType == defs_v3::OBJECT_TYPE_MQTT_CONNECTION) {
        onFreeMQTTConnection(arrayValue);
    }

    if (eez::flow::onArrayValueFreeHook) {
        eez::flow::onArrayValueFreeHook(arrayValue);
    }
}

} // namespace flow
} // namespace eez
