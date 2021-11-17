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

#include <eez/gui/gui.h>

#include <bb3/psu/psu.h>
#include <bb3/psu/scpi/psu.h>

#include <eez/scripting/scripting.h>
#include <eez/scripting/profile_parameters.h>

using namespace eez::scripting;

namespace eez {
namespace psu {
namespace scpi {

scpi_result_t scpi_cmd_scriptRun(scpi_t *context) {
	char scriptFilePath[MAX_PATH_LENGTH + 1];
	if (!getFilePath(context, scriptFilePath, false)) {
		return SCPI_RES_ERR;
	}

	int err;
	if (!startScript(scriptFilePath, &err)) {
		SCPI_ErrorPush(context, err);
		return SCPI_RES_ERR;
	}
	
	return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_scriptRunQ(scpi_t *context) {
	if (!isIdle()) {
		SCPI_ResultText(context, g_scriptPath);
	} else {
		SCPI_ResultText(context, "");
	}
	return SCPI_RES_OK;
}


scpi_result_t scpi_cmd_scriptStop(scpi_t *context) {
	int err;

	if (!stopScript(&err)) {
		SCPI_ErrorPush(context, err);
		return SCPI_RES_ERR;
	}

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_scriptRecall(scpi_t *context) {
	char scriptFilePath[MAX_PATH_LENGTH + 1];
	if (!getFilePath(context, scriptFilePath, false)) {
		return SCPI_RES_ERR;
	}

    int err;
    if (!sd_card::exists(scriptFilePath, &err)) {
        if (err != 0) {
            SCPI_ErrorPush(context, err);
        }
        return SCPI_RES_ERR;
    }

	stringCopy(g_scriptingParameters.autoStartScript, sizeof(g_scriptingParameters.autoStartScript), scriptFilePath);
	g_scriptParametersVersion++;
	
	return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_scriptRecallQ(scpi_t *context) {
	SCPI_ResultText(context, g_scriptingParameters.autoStartScript);
    
	return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_scriptRecallClear(scpi_t *context) {
	g_scriptingParameters.autoStartScript[0] = 0;
	g_scriptParametersVersion++;
	
	return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_scriptRecallConfirmation(scpi_t *context) {
	bool enable;
	if (!SCPI_ParamBool(context, &enable, TRUE)) {
		return SCPI_RES_ERR;
	}

	g_scriptingParameters.options.autoStartConfirmation = enable;
	g_scriptParametersVersion++;

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_scriptRecallConfirmationQ(scpi_t *context) {
	SCPI_ResultBool(context, g_scriptingParameters.options.autoStartConfirmation);

	return SCPI_RES_OK;
}

} // scpi
} // psu
} // eez
