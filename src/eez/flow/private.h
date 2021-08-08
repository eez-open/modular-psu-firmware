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
using eez::gui::Component;
using eez::gui::ComponentOutput;

struct ComponenentExecutionState {
	virtual ~ComponenentExecutionState() {}
};

struct FlowState {
	Assets *assets;
	uint16_t flowIndex;
	uint16_t error;
	uint32_t numActiveComponents;
	FlowState *parentFlowState;
	Component *parentComponent;
	Value *values;
	ComponenentExecutionState **componenentExecutionStates;
};

static const int UNDEFINED_VALUE_INDEX = 0;
static const int NULL_VALUE_INDEX = 1;

FlowState *initFlowState(Assets *assets, int flowIndex);
void freeFlowState(FlowState *flowState);

void recalcFlowDataItems(FlowState *flowState);
void recalcFlowDataItems(FlowState *flowState, unsigned componentIndex);

void propagateValue(FlowState *flowState, ComponentOutput &componentOutput, const gui::Value &value);
void propagateValue(FlowState *flowState, ComponentOutput &componentOutput); // propagates null value
void propagateValue(FlowState *flowState, unsigned componentIndex); // propagates null value through @seqout (0-th output)

void setValueFromGuiThread(FlowState *flowState, uint16_t dataId, const Value& value);
void assignValue(FlowState *flowState, Component *component, Value &dstValue, const Value &srcValue);

void throwError(FlowState *flowState, Component *component, const char *errorMessage);

} // flow
} // eez