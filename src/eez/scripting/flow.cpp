/*
* EEZ Generic Firmware
* Copyright (C) 2018-present, Envox d.o.o.
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

#include <eez/modules/psu/gui/psu.h>

#include <eez/scripting/scripting.h>
#include <eez/scripting/flow.h>
#include <eez/scripting/internal.h>
#include <eez/scripting/scpi_context.h>
#include <eez/scripting/thread.h>

#include <eez/flow/flow.h>

namespace eez {
namespace scripting {

static unsigned g_flowHandle;

WidgetCursor g_actionWidgetCursor;
int16_t g_actionId;

bool loadFlowScript(int *err) {
	int localErr = SCPI_RES_OK;
	if (!err) {
		err = &localErr;
	}

	if (!eez::gui::loadExternalAssets(g_scriptPath, err) || !psu::gui::g_psuAppContext.dialogOpen(err)) {
		char scriptName[64];
		getFileName(g_scriptPath, scriptName, sizeof(scriptName));
		ErrorTrace("App error: %s\n", scriptName);

		afterScriptCleanup(true);
		setStateIdle();

		if (err == &localErr) {
			generateError(*err);
		}

		return false;
	}

	startFlowScriptInScriptingThread();
	return true;

}

void startFlowScript() {
	char scriptName[64];
	getFileName(g_scriptPath, scriptName, sizeof(scriptName));
	InfoTrace("App started: %s\n", scriptName);

    afterScriptCleanup(false);

	g_flowHandle = flow::start(g_externalAssets);
}

void stopFlowScript() {
	g_flowHandle = 0;
	stopFlowInScriptingThread();
}

void freeFlowMemory() {
	flow::stop();
}

void flowTick() {
	if (g_flowHandle != 0 && g_state != STATE_STOPPING) {
		flow::tick(g_flowHandle);
	}
}

void executeScpiFromFlow(const char *commandOrQueryText) {
	// DebugTrace("> %s\n", commandOrQueryText);
	executeScpi(commandOrQueryText, true);
}

void executeFlowAction(const WidgetCursor &widgetCursor, int16_t actionId) {
	// TODO check overwrite
	g_actionWidgetCursor = widgetCursor;
	g_actionId = actionId;

	executeFlowActionInScriptingThread();
}

void doExecuteFlowAction() {
	if (isFlowRunning()) {
		flow::executeFlowAction(g_flowHandle, g_actionWidgetCursor, g_actionId);
	}
}

bool isFlowRunning() {
	return g_flowHandle != 0;
}

void dataOperation(int16_t dataId, DataOperationEnum operation, const gui::WidgetCursor &widgetCursor, Value &value) {
	if (isFlowRunning()) {
		flow::dataOperation(g_flowHandle, dataId, operation, widgetCursor, value);
	}
}

} // scripting
} // eez
