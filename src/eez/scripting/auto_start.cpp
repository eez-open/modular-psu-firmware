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

#include <eez/modules/psu/gui/psu.h>

#include <eez/modules/mcu/encoder.h>

#include <eez/scripting/scripting.h>
#include <eez/scripting/flow.h>
#include <eez/scripting/internal.h>
#include <eez/scripting/scpi_context.h>
#include <eez/scripting/thread.h>
#include <eez/scripting/profile_parameters.h>

#include <eez/flow/flow.h>

namespace eez {
namespace scripting {

static bool g_autoStartConditionIsChecked;
static bool g_autoStartScriptIsRunning;

bool isAutoScriptRunning() {
    return g_autoStartScriptIsRunning;
}

void autoStartCleanup() {
	if (g_autoStartScriptIsRunning) {
		g_autoStartScriptIsRunning = false;
	}
}

bool isAutoStartEnabled() {
	if (!g_autoStartConditionIsChecked) {
	    sendMessageToLowPriorityThread(THREAD_MESSAGE_AUTO_START_SCRIPT, 0, 1);
		return true;
	}

	return g_autoStartScriptIsRunning;
}

void doAutoStart() {
	psu::gui::popPage();
    startScript(g_scriptingParameters.autoStartScript);
}

void skipAutoStart() {
	psu::gui::popPage();
	g_autoStartScriptIsRunning = false;
}

void getAutoStartConfirmationMessage(char *text, int count) {
	char fileName[MAX_PATH_LENGTH + 1];
	getFileName(g_scriptingParameters.autoStartScript, fileName, sizeof(fileName));
	snprintf(text, count, "Start \"%s\" script?", fileName);
}

void clearAutoStartScript() {
	g_scriptingParameters.autoStartScript[0] = 0;
	g_scriptParametersVersion++;
	int location = psu::persist_conf::getProfileAutoRecallLocation();
	if (location != 0) {
		psu::profile::saveToLocation(location);
	}
}

void autoStart() {
    if (!g_autoStartConditionIsChecked) {
		if (g_scriptingParameters.autoStartScript[0]) {
			if (mcu::encoder::isButtonPressed()) {
				psu::gui::yesNoDialog(PAGE_ID_YES_NO_AUTO_START_SKIPPED, Value(), clearAutoStartScript, nullptr, nullptr);
			} else {
				if (psu::sd_card::exists(g_scriptingParameters.autoStartScript, nullptr)) {
					g_autoStartScriptIsRunning = true;
					if (g_scriptingParameters.options.autoStartConfirmation) {
                        psu::gui::yesNoDialog(PAGE_ID_YES_NO_AUTO_START, Value(0, VALUE_TYPE_AUTO_START_SCRIPT_CONFIRMATION_MESSAGE), doAutoStart, skipAutoStart, skipAutoStart);
                    } else {
						startScript(g_scriptingParameters.autoStartScript);
                    }
				} else {
					ErrorTrace("Auto start script not found\n");
				}
			}
		}
	
		g_autoStartConditionIsChecked = true;
	}
}

} // scripting
} // eez
