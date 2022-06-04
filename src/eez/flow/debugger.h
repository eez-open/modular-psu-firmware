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

namespace eez {
namespace flow {

extern bool g_debuggerIsConnected;

enum {
    DEBUGGER_MODE_RUN,
    DEBUGGER_MODE_DEBUG,
};
extern int g_debuggerMode;

bool canExecuteStep(FlowState *&flowState, unsigned &componentIndex);

void onStarted(gui::Assets *assets);
void onStopped();

void onAddToQueue(FlowState *flowState, int sourceComponentIndex, int sourceOutputIndex, unsigned targetComponentIndex, int targetInputIndex);
void onRemoveFromQueue();

void onValueChanged(const gui::Value *pValue);

void onFlowStateCreated(FlowState *flowState);
void onFlowStateDestroyed(FlowState *flowState);
void onFlowStateTimelineChanged(FlowState *flowState);

void onFlowError(FlowState *flowState, int componentIndex, const char *errorMessage);

void onComponentExecutionStateChanged(FlowState *flowState, int componentIndex);
void onComponentAsyncStateChanged(FlowState *flowState, int componentIndex);

void logInfo(FlowState *flowState, unsigned componentIndex, const char *message);

void logScpiCommand(FlowState *flowState, unsigned componentIndex, const char *cmd);
void logScpiQuery(FlowState *flowState, unsigned componentIndex, const char *query);
void logScpiQueryResult(FlowState *flowState, unsigned componentIndex, const char *resultText, size_t resultTextLen);

void onPageChanged(int previousPageId, int activePageId);

void processDebuggerInput(char *buffer, uint32_t length);

} // flow
} // eez
