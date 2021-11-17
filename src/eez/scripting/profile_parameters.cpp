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

#include <bb3/psu/gui/psu.h>

#include <eez/scripting/scripting.h>
#include <eez/scripting/flow.h>
#include <eez/scripting/internal.h>
#include <eez/scripting/scpi_context.h>
#include <eez/scripting/thread.h>

#include <eez/flow/flow.h>

namespace eez {
namespace scripting {

eez::psu::profile::ScriptingParameters g_scriptingParameters = { "", 1 };
uint32_t g_scriptParametersVersion;

void resetSettings() {
	g_scriptingParameters.autoStartScript[0] = 0;
    g_scriptingParameters.options.autoStartConfirmation = 1;
	g_scriptParametersVersion++;
}

void resetProfileParameters(psu::profile::Parameters &profileParams) {
    profileParams.scriptingParameters.autoStartScript[0] = 0;
    profileParams.scriptingParameters.options.autoStartConfirmation = 1;
}

void getProfileParameters(psu::profile::Parameters &profileParams) {
    memcpy(&profileParams.scriptingParameters, &g_scriptingParameters, sizeof(profileParams.scriptingParameters));
}

void setProfileParameters(const psu::profile::Parameters &profileParams) {
    memcpy(&g_scriptingParameters, &profileParams.scriptingParameters, sizeof(g_scriptingParameters));
}

bool writeProfileProperties(psu::profile::WriteContext &ctx, const psu::profile::Parameters &profileParams) {
	ctx.group("scripting");
	WRITE_PROPERTY("autoStartConfirmation", profileParams.scriptingParameters.options.autoStartConfirmation);
	WRITE_PROPERTY("autoStartScript", profileParams.scriptingParameters.autoStartScript);
	return true;
}

bool readProfileProperties(psu::profile::ReadContext &ctx, psu::profile::Parameters &profileParams) {
	if (ctx.matchGroup("scripting")) {
		READ_FLAG("autoStartConfirmation", profileParams.scriptingParameters.options.autoStartConfirmation);
		READ_STRING_PROPERTY("autoStartScript", profileParams.scriptingParameters.autoStartScript, sizeof(profileParams.scriptingParameters.autoStartScript));
	}
	return false;
}


} // scripting
} // eez
