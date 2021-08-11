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

#include <eez/sound.h>

#include <eez/modules/psu/psu.h>
#include <eez/modules/psu/datetime.h>
#include <eez/modules/psu/scpi/psu.h>

#include <eez/scripting/scripting.h>
#include <eez/scripting/thread.h>

namespace eez {
namespace scripting {

////////////////////////////////////////////////////////////////////////////////

using namespace eez::scpi;
using namespace eez::psu::scpi;

static char g_scpiData[SCPI_PARSER_INPUT_BUFFER_LENGTH + 1];
static size_t g_scpiDataLen;
static int_fast16_t g_lastError;

size_t SCPI_Write(scpi_t *context, const char *data, size_t len) {
    len = MIN(len, SCPI_PARSER_INPUT_BUFFER_LENGTH - g_scpiDataLen);
    if (len > 0) {
        memcpy(g_scpiData + g_scpiDataLen, data, len);
        g_scpiDataLen += len;
        g_scpiData[g_scpiDataLen] = 0;
    }
    return len;
}

scpi_result_t SCPI_Flush(scpi_t *context) {
    return SCPI_RES_OK;
}

int SCPI_Error(scpi_t *context, int_fast16_t err) {
    g_lastError = err;

    if (err != 0) {
		if (!isFlowRunning()) {
        	sound::playBeep();
		}
        
        DebugTrace("**ERROR");

        char datetime_buffer[32] = { 0 };
        if (psu::datetime::getDateTimeAsString(datetime_buffer)) {
            DebugTrace(" [%s]", datetime_buffer);
        }

        DebugTrace(": %d,\"%s\"\r\n", (int16_t)err, SCPI_ErrorTranslate(err));

        if (err == SCPI_ERROR_INPUT_BUFFER_OVERRUN) {
            psu::scpi::onBufferOverrun(*context);
        }
    }

    return 0;
}

scpi_result_t SCPI_Control(scpi_t *context, scpi_ctrl_name_t ctrl, scpi_reg_val_t val) {
    return SCPI_RES_OK;
}

scpi_result_t SCPI_Reset(scpi_t *context) {
    return eez::reset() ? SCPI_RES_OK : SCPI_RES_ERR;
}

static scpi_reg_val_t g_scpiPsuRegs[SCPI_PSU_REG_COUNT];
static scpi_psu_t g_scpiPsuContext = { g_scpiPsuRegs };

static scpi_interface_t g_scpiInterface = {
    SCPI_Error, SCPI_Write, SCPI_Control, SCPI_Flush, SCPI_Reset,
};

static char g_scpiInputBuffer[SCPI_PARSER_INPUT_BUFFER_LENGTH];
static scpi_error_t g_errorQueueData[SCPI_PARSER_ERROR_QUEUE_SIZE + 1];

scpi_t g_scpiContext; 

static const char *g_commandOrQueryText;

////////////////////////////////////////////////////////////////////////////////

void initScpiContext() {
	eez::psu::scpi::init(g_scpiContext, g_scpiPsuContext, &g_scpiInterface, g_scpiInputBuffer, SCPI_PARSER_INPUT_BUFFER_LENGTH, g_errorQueueData, SCPI_PARSER_ERROR_QUEUE_SIZE + 1);
}

void executeScpi(const char *commandOrQueryText, bool executeInLowPriorityThread) {
    if (executeInLowPriorityThread) {
        g_commandOrQueryText = commandOrQueryText;
        sendMessageToLowPriorityThread(MP_EXECUTE_SCPI);
        return;
    }

	g_scpiDataLen = 0;
	g_lastError = 0;

	input(g_scpiContext, (const char *)commandOrQueryText, strlen(commandOrQueryText));
	input(g_scpiContext, "\r\n", 2);
}

void doExecuteScpi() {
    executeScpi(g_commandOrQueryText, false);
	scpiResultIsReady();
}

bool getLatestScpiResult(const char **resultText, size_t *resultTextLen, int *err) {
	if (g_lastError != 0) {
		if (err) {
			*err = g_lastError;
		}
		return false;
	}

	if (g_scpiDataLen >= 2 && g_scpiData[g_scpiDataLen - 2] == '\r' && g_scpiData[g_scpiDataLen - 1] == '\n') {
		g_scpiDataLen -= 2;
		g_scpiData[g_scpiDataLen] = 0;
	}

	if (g_scpiDataLen >= 3 && g_scpiData[0] == '"' && g_scpiData[g_scpiDataLen - 1] == '"') {
		// replace "" with "
		size_t j = 1;
		size_t i;
		for (i = 1; i < g_scpiDataLen - 2; i++, j++) {
			g_scpiData[j] = g_scpiData[i];
			if (g_scpiData[i] == '"' && g_scpiData[i + 1] == '"') {
				i++;
			}
		}
		g_scpiData[j] = g_scpiData[i];
		g_scpiData[j + 1] = '"';
		g_scpiDataLen -= i - j;
	}

	// if (g_scpiDataLen > 0) {
	//     DebugTrace("< %s\n", g_scpiData);
	// }

	*resultText = g_scpiData;
	*resultTextLen = g_scpiDataLen;
	return true;
}

} // scripting
} // eez
