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

#include <eez/core/assets.h>

#if EEZ_OPTION_GUI
#include <eez/gui/gui.h>
using namespace eez::gui;
#endif

namespace eez {
namespace flow {

#if defined(__EMSCRIPTEN__)
extern uint32_t g_wasmModuleId;
#endif

struct FlowState;

unsigned start(Assets *assets);
void tick();
void stop();

bool isFlowStopped();

#if EEZ_OPTION_GUI
FlowState *getPageFlowState(Assets *assets, int16_t pageIndex, const WidgetCursor &widgetCursor);
#else
FlowState *getPageFlowState(Assets *assets, int16_t pageIndex);
#endif
int getPageIndex(FlowState *flowState);

Value getGlobalVariable(uint32_t globalVariableIndex);
Value getGlobalVariable(Assets *assets, uint32_t globalVariableIndex);
void setGlobalVariable(uint32_t globalVariableIndex, const Value &value);
void setGlobalVariable(Assets *assets, uint32_t globalVariableIndex, const Value &value);

#if EEZ_OPTION_GUI
FlowState *getUserWidgetFlowState(FlowState *flowState, uint16_t userWidgetWidgetComponentIndex, int16_t pageId);
void executeFlowAction(const WidgetCursor &widgetCursor, int16_t actionId, void *param);
void dataOperation(int16_t dataId, DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value);
int16_t getNativeVariableId(const WidgetCursor &widgetCursor);
#endif

void setDebuggerMessageSubsciptionFilter(uint32_t filter);
void onDebuggerClientConnected();
void onDebuggerClientDisconnected();

void onArrayValueFree(ArrayValue *arrayValue);
void onFreeMQTTConnection(ArrayValue *mqttConnectionValue);

void executeScpi();
void flushToDebuggerMessage();

} // flow
} // eez
