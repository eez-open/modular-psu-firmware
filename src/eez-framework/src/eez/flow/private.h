/*
* EEZ Generic Firmware
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

#pragma once

#include <eez/core/os.h>
#include <eez/core/assets.h>
#include <eez/core/value.h>

#if EEZ_OPTION_GUI
#include <eez/gui/gui.h>
using namespace eez::gui;
#endif

namespace eez {
namespace flow {

static const int UNDEFINED_VALUE_INDEX = 0;
static const int NULL_VALUE_INDEX = 1;

struct ComponenentExecutionState {
    uint32_t lastExecutedTime;
    ComponenentExecutionState() : lastExecutedTime(millis()) {}
	virtual ~ComponenentExecutionState() {}
};

struct CatchErrorComponenentExecutionState : public ComponenentExecutionState {
	Value message;
};

struct FlowState {
	uint32_t flowStateIndex;
	Assets *assets;
	FlowDefinition *flowDefinition;
	Flow *flow;
	uint16_t flowIndex;
	bool isAction;
	bool error;
	uint32_t numAsyncComponents;
	FlowState *parentFlowState;
	Component *parentComponent;
	int parentComponentIndex;
	Value *values;
	ComponenentExecutionState **componenentExecutionStates;
    bool *componenentAsyncStates;
    unsigned executingComponentIndex;
    float timelinePosition;
#if defined(EEZ_FOR_LVGL)
    int32_t lvglWidgetStartIndex;
#endif

    FlowState *firstChild;
    FlowState *lastChild;
    FlowState *previousSibling;
    FlowState *nextSibling;
};

extern int g_selectedLanguage;
extern FlowState *g_firstFlowState;
extern FlowState *g_lastFlowState;

FlowState *initActionFlowState(int flowIndex, FlowState *parentFlowState, int parentComponentIndex);
FlowState *initPageFlowState(Assets *assets, int flowIndex, FlowState *parentFlowState, int parentComponentIndex);

bool canFreeFlowState(FlowState *flowState, bool includingWatchVariable = true);
void freeFlowState(FlowState *flowState);

void deallocateComponentExecutionState(FlowState *flowState, unsigned componentIndex);

extern void onComponentExecutionStateChanged(FlowState *flowState, int componentIndex);
template<class T>
T *allocateComponentExecutionState(FlowState *flowState, unsigned componentIndex) {
    if (flowState->componenentExecutionStates[componentIndex]) {
        deallocateComponentExecutionState(flowState, componentIndex);
    }
    auto executionState = ObjectAllocator<T>::allocate(0x72dc3bf4);
    flowState->componenentExecutionStates[componentIndex] = executionState;
    onComponentExecutionStateChanged(flowState, componentIndex);
    return executionState;
}

void resetSequenceInputs(FlowState *flowState);

void propagateValue(FlowState *flowState, unsigned componentIndex, unsigned outputIndex, const Value &value);
void propagateValue(FlowState *flowState, unsigned componentIndex, unsigned outputIndex); // propagates null value
void propagateValueThroughSeqout(FlowState *flowState, unsigned componentIndex); // propagates null value through @seqout (0-th output)

#if EEZ_OPTION_GUI
void getValue(uint16_t dataId, DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value);
void setValue(uint16_t dataId, const WidgetCursor &widgetCursor, const Value& value);
#endif

void assignValue(FlowState *flowState, int componentIndex, Value &dstValue, const Value &srcValue);

void clearInputValue(FlowState *flowState, int inputIndex);

void startAsyncExecution(FlowState *flowState, int componentIndex);
void endAsyncExecution(FlowState *flowState, int componentIndex);

void executeCallAction(FlowState *flowState, unsigned componentIndex, int flowIndex);

enum FlowEvent {
    FLOW_EVENT_OPEN_PAGE,
    FLOW_EVENT_CLOSE_PAGE
};

void onEvent(FlowState *flowState, FlowEvent flowEvent);

void throwError(FlowState *flowState, int componentIndex, const char *errorMessage);
void throwError(FlowState *flowState, int componentIndex, const char *errorMessage, const char *errorMessageDescription);

void enableThrowError(bool enable);

} // flow
} // eez
