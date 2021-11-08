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

#include <eez/gui/assets.h>

namespace eez {
namespace flow {

using eez::gui::Value;
using eez::gui::Assets;
using eez::gui::FlowDefinition;
using eez::gui::Flow;
using eez::gui::Component;
using eez::gui::ComponentOutput;
using eez::gui::WidgetCursor;

static const int UNDEFINED_VALUE_INDEX = 0;
static const int NULL_VALUE_INDEX = 1;

struct ComponenentExecutionState {
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
	uint16_t error;
	uint32_t numActiveComponents;
	FlowState *parentFlowState;
	Component *parentComponent;
	int parentComponentIndex;
	Value *values;
	ComponenentExecutionState **componenentExecutionStates;
};

extern FlowState *g_mainPageFlowState;

extern uint32_t g_lastFlowStateIndex;

void fixAssetValues(Assets *assets);

FlowState *initActionFlowState(Assets *assets, int flowIndex, FlowState *parentFlowState, int parentComponentIndex);
FlowState *initPageFlowState(Assets *assets, int flowIndex, FlowState *parentFlowState, int parentComponentIndex);

void freeFlowState(FlowState *flowState);

void propagateValue(FlowState *flowState, unsigned componentIndex, unsigned outputIndex, const gui::Value &value);
void propagateValue(FlowState *flowState, unsigned componentIndex, unsigned outputIndex); // propagates null value
void propagateValueThroughSeqout(FlowState *flowState, unsigned componentIndex); // propagates null value through @seqout (0-th output)

void getValue(uint16_t dataId, const WidgetCursor &widgetCursor, Value &value);
void setValue(uint16_t dataId, const WidgetCursor &widgetCursor, const Value& value);

void assignValue(FlowState *flowState, int componentIndex, Value &dstValue, const Value &srcValue);

void throwError(FlowState *flowState, int componentIndex, const char *errorMessage);

} // flow
} // eez