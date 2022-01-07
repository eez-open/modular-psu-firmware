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

#include <stdio.h>

#include <bb3/psu/psu.h>
#include <bb3/psu/gui/psu.h>
#include <bb3/psu/gui/file_manager.h>

#include <bb3/scripting/auto_start.h>
#include <bb3/scripting/flow.h>
#include <bb3/scripting/mp.h>
#include <bb3/scripting/scripting.h>
#include <bb3/scripting/scpi_context.h>
#include <bb3/scripting/thread.h>
#include <bb3/scripting/profile_parameters.h>

namespace eez {
namespace scripting {

State g_state;
char *g_scriptPath;

////////////////////////////////////////////////////////////////////////////////

void setStateIdle() {
	g_state = STATE_IDLE;

	autoStartCleanup();

	if (g_scriptPath) {
		free(g_scriptPath);
		g_scriptPath = nullptr;
	}

	freeMpMemory();

	freeFlowMemory();

	sendMessageToGuiThread(GUI_QUEUE_MESSAGE_UNLOAD_EXTERNAL_ASSETS);
}

bool loadScript(int *err = nullptr) {
	if (endsWithNoCase(g_scriptPath, ".py")) {
		return loadMpScript(err);
	} {
		return loadFlowScript(err);
	}
}

bool startScript(const char *filePath, int *err) {
	if (g_state == STATE_IDLE) {
		g_state = STATE_EXECUTING;

		int localErr = SCPI_RES_OK;
		if (!err) {
			err = &localErr;
		}

		auto scriptPathSize = strlen(filePath) + 1;
		g_scriptPath = (char *)alloc(scriptPathSize, 0xc4b6e523);
		if (g_scriptPath) {
			stringCopy(g_scriptPath, scriptPathSize, filePath);

			*err = SCPI_RES_OK;

			if (isLowPriorityThread()) {
				loadScript(err);
			} else {
				sendMessageToLowPriorityThread(MP_LOAD_SCRIPT);
				*err = SCPI_RES_OK;
			}

			if (*err == SCPI_RES_OK && !isAutoScriptRunning()) {
				psu::gui::showAsyncOperationInProgress();
			}
		} else {
			*err = SCPI_ERROR_OUT_OF_DEVICE_MEMORY;
		}

		return *err == SCPI_RES_OK;
	}

	if (err) {
		*err = SCPI_ERROR_SCRIPT_ENGINE_ALREADY_RUNNING;
	}

	return false;
}

void doStopScript() {
	g_state = STATE_STOPPING;

	if (isFlowRunning()) {
		stopFlowScript();
	} else {
		terminateThread();
		startThread();
	}

	psu::gui::hideAsyncOperationInProgress();
	psu::gui::clearTextMessage();

	char scriptName[64];
	getFileName(g_scriptPath, scriptName, sizeof(scriptName));
	InfoTrace("Script stopped: %s\n", scriptName);

	setStateIdle();

	refreshScreen();
}

bool stopScript(int *err) {
	if (g_state == STATE_EXECUTING) {
		if (!isLowPriorityThread()) {
			sendMessageToLowPriorityThread(MP_STOP_SCRIPT);
			return true;
		}

		doStopScript();

		return true;
	}
	
	if (err) {
		*err = SCPI_ERROR_EXECUTION_ERROR;
	}
	return false;
}

void onLowPriorityQueueMessage(uint32_t type, uint32_t param) {
    if (type == MP_LOAD_SCRIPT) {
        loadScript();
    } else if (type == MP_STOP_SCRIPT) {
        stopScript();
    } else if (type == MP_EXECUTE_SCPI) {
		doExecuteScpi();
	}
}

} // scripting

namespace gui {

using namespace ::eez::scripting;
using namespace ::eez::psu::gui;

void data_sys_settings_scripting_auto_start_script(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
	if (operation == DATA_OPERATION_GET) {
        if (g_scriptingParameters.autoStartScript[0]) {
            static char g_fileName[MAX_PATH_LENGTH + 1];
            getFileName(g_scriptingParameters.autoStartScript, g_fileName, sizeof(g_fileName));
            value = Value(g_scriptParametersVersion, g_fileName);
        } else {
            value = "<not selected>";
        }
	}
}

void data_sys_settings_scripting_auto_start_script_enabled(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
	if (operation == DATA_OPERATION_GET) {
		value = g_scriptingParameters.autoStartScript[0] ? 1 : 0;
	}
}

void data_sys_settings_scripting_auto_start_confirmation(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
	if (operation == DATA_OPERATION_GET) {
		value = g_scriptingParameters.options.autoStartConfirmation ? 1 : 0;
	}
}

void action_show_sys_settings_scripting() {
	pushPage(PAGE_ID_SYS_SETTINGS_SCRIPTING);
}

void onAutoStartScriptFileSelected(const char *filePath) {
	stringCopy(g_scriptingParameters.autoStartScript, sizeof(g_scriptingParameters.autoStartScript), filePath);
	g_scriptParametersVersion++;
}

void action_sys_settings_scripting_select_auto_start_script() {
	browseForFile(
		"Select auto start script",
		"/Scripts", 
		FILE_TYPE_MICROPYTHON,
		file_manager::DIALOG_TYPE_OPEN,
		onAutoStartScriptFileSelected,
		nullptr,
		[] (FileType type) { 
			return type == FILE_TYPE_MICROPYTHON || type == FILE_TYPE_APP; 
		}
	);
}

void action_sys_settings_scripting_clear_auto_start_script() {
	g_scriptingParameters.autoStartScript[0] = 0;
	g_scriptParametersVersion++;
}

void action_sys_settings_scripting_toggle_auto_start_confirmation() {
	g_scriptingParameters.options.autoStartConfirmation = !g_scriptingParameters.options.autoStartConfirmation;
	g_scriptParametersVersion++;
}

} // gui
} // eez
