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

#include <string.h>
#include <stdio.h>

#include <eez/firmware.h>
#include <eez/mp.h>
#include <eez/system.h>
#include <eez/sound.h>

#include <eez/libs/sd_fat/sd_fat.h>

#include <eez/modules/psu/psu.h>
#include <eez/modules/psu/datetime.h>
#include <eez/modules/psu/event_queue.h>
#include <eez/modules/psu/profile.h>
#include <eez/modules/psu/persist_conf.h>
#include <eez/modules/psu/sd_card.h>
#include <eez/modules/psu/scpi/psu.h>
#include <eez/modules/psu/gui/psu.h>
#include <eez/modules/psu/gui/file_manager.h>

#include <eez/modules/mcu/encoder.h>

#include <eez/memory.h>

#ifdef _MSC_VER
#pragma warning( push )
#pragma warning( disable : 4200)
#endif

extern "C" {
#include "py/compile.h"
#include "py/runtime.h"
#include "py/gc.h"
#include "py/stackctrl.h"
}

#ifdef _MSC_VER
#pragma warning( pop )
#endif

namespace eez {
namespace mp {

void mainLoop(const void *);

osThreadId g_mpTaskHandle;

#if defined(EEZ_PLATFORM_STM32)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wwrite-strings"
#endif

osThreadDef(g_mpTask, mainLoop, osPriorityNormal, 0, 4096);

#if defined(EEZ_PLATFORM_STM32)
#pragma GCC diagnostic pop
#endif

#define MP_QUEUE_SIZE 5

osMessageQDef(g_mpMessageQueue, MP_QUEUE_SIZE, uint32_t);
osMessageQId g_mpMessageQueueId;

State g_state;

char *g_scriptPath = (char *)MP_BUFFER;
static char *g_scriptSource = g_scriptPath + MAX_PATH_LENGTH + 1;
static const size_t MAX_SCRIPT_LENGTH = 32 * 1024;
static size_t g_scriptSourceLength;

static struct eez::psu::profile::ScriptingParameters g_scriptingParameters = { "", 1 };
static uint32_t g_scriptParametersVersion;

static bool g_autoStartConditionIsChecked;
static bool g_autoStartScriptIsRunning;

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
        sound::playBeep();
        
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

////////////////////////////////////////////////////////////////////////////////

void initMessageQueue() {
    eez::psu::scpi::init(g_scpiContext, g_scpiPsuContext, &g_scpiInterface, g_scpiInputBuffer, SCPI_PARSER_INPUT_BUFFER_LENGTH, g_errorQueueData, SCPI_PARSER_ERROR_QUEUE_SIZE + 1);
    g_mpMessageQueueId = osMessageCreate(osMessageQ(g_mpMessageQueue), 0);
}

void startThread() {
    g_mpTaskHandle = osThreadCreate(osThread(g_mpTask), nullptr);
}

void terminateThread() {
	osThreadTerminate(g_mpTaskHandle);
}

void oneIter();

void mainLoop(const void *) {
#ifdef __EMSCRIPTEN__
    oneIter();
#else
    while (1) {
        oneIter();
    }
#endif
}

enum {
    QUEUE_MESSAGE_START_SCRIPT,
    QUEUE_MESSAGE_SCPI_RESULT
};

void oneIter() {
    osEvent event = osMessageGet(g_mpMessageQueueId, osWaitForever);
    if (event.status == osEventMessage) {
        if (event.value.v == QUEUE_MESSAGE_START_SCRIPT) {
            char scriptName[64];
            getFileName(g_scriptPath, scriptName, sizeof(scriptName));
            InfoTrace("Script started: %s\n", scriptName);

#if 1
        	// this version reinitialise MP every time

			volatile char dummy;
			mp_stack_set_top((void *)&dummy);
			gc_init(g_scriptSource + MAX_SCRIPT_LENGTH, MP_BUFFER + MP_BUFFER_SIZE - MAX_SCRIPT_LENGTH);
			mp_init();

            bool wasException = false;

            nlr_buf_t nlr;
            if (nlr_push(&nlr) == 0) {
                mp_lexer_t *lex = mp_lexer_new_from_str_len(MP_QSTR__lt_stdin_gt_, g_scriptSource, g_scriptSourceLength, 0);
                qstr source_name = lex->source_name;
                mp_parse_tree_t parse_tree = mp_parse(lex, MP_PARSE_FILE_INPUT);
                mp_obj_t module_fun = mp_compile(&parse_tree, source_name/*, MP_EMIT_OPT_NONE*/, true);
                mp_call_function_0(module_fun);
                nlr_pop();
            } else {
                // uncaught exception
                mp_obj_print_exception(&mp_plat_print, (mp_obj_t)nlr.ret_val);
                onUncaughtScriptExceptionHook();
                wasException = true;
            }

            gc_sweep_all();
            mp_deinit();
#endif

#if 0
        	// this version doesn't reinitialise MP every time

			static bool g_initialized = false;
			if (!g_initialized) {
				volatile char dummy;
				g_initialized = true;
				mp_stack_set_top((void *)&dummy);
				gc_init(g_scriptSource + MAX_SCRIPT_LENGTH, MP_BUFFER + MP_BUFFER_SIZE - MAX_SCRIPT_LENGTH);
				mp_init();
			}

			nlr_buf_t nlr;
			if (nlr_push(&nlr) == 0) {
				mp_lexer_t *lex = mp_lexer_new_from_str_len(MP_QSTR__lt_stdin_gt_, g_scriptSource, g_scriptSourceLength, 0);
				qstr source_name = lex->source_name;
				mp_parse_tree_t parse_tree = mp_parse(lex, MP_PARSE_FILE_INPUT);
				mp_obj_t module_fun = mp_compile(&parse_tree, source_name/*, MP_EMIT_OPT_NONE*/, true);
                //DebugTrace("T3 %d\n", millis());
				mp_call_function_0(module_fun);
				nlr_pop();
			} else {
				// uncaught exception
				mp_obj_print_exception(&mp_plat_print, (mp_obj_t)nlr.ret_val);
                onUncaughtScriptExceptionHook();
			}
#endif

            psu::gui::hideAsyncOperationInProgress();
            psu::gui::clearTextMessage();

            g_state = STATE_IDLE;

            InfoTrace("Script ended: %s\n", scriptName);

            if (g_autoStartScriptIsRunning) {
                g_autoStartScriptIsRunning = false;
                if (wasException) {
                    psu::gui::showMainPage();
                }
            }
        }
    }
}

void doStopScript() {
	terminateThread();

	psu::gui::hideAsyncOperationInProgress();
	psu::gui::clearTextMessage();

	g_state = STATE_IDLE;

	char scriptName[64];
	getFileName(g_scriptPath, scriptName, sizeof(scriptName));
	InfoTrace("Script stopped: %s\n", scriptName);

	if (g_autoStartScriptIsRunning) {
		g_autoStartScriptIsRunning = false;
	}

	psu::gui::showMainPage();

	startThread();
}

bool loadScript(int *err = nullptr) {
    uint32_t fileSize;
    uint32_t bytesRead;

    eez::File file;
    if (!file.open(g_scriptPath, FILE_OPEN_EXISTING | FILE_READ)) {
		if (err) {
			*err = SCPI_ERROR_FILE_NOT_FOUND;
		} else {
			generateError(SCPI_ERROR_FILE_NOT_FOUND);
		}
        goto ErrorNoClose;
    }

    fileSize = file.size();
    if (fileSize > MAX_SCRIPT_LENGTH) {
		if (err) {
			*err = SCPI_ERROR_OUT_OF_DEVICE_MEMORY;
		} else {
			generateError(SCPI_ERROR_OUT_OF_DEVICE_MEMORY);
		}
        goto Error;
    }

    bytesRead = file.read(g_scriptSource, fileSize);

    file.close();

    if (bytesRead != fileSize) {
		if (err) {
			*err = SCPI_ERROR_MASS_STORAGE_ERROR;
		} else {
			generateError(SCPI_ERROR_MASS_STORAGE_ERROR);
		}
        goto Error;
    }

    g_scriptSourceLength = fileSize;

    osMessagePut(g_mpMessageQueueId, QUEUE_MESSAGE_START_SCRIPT, osWaitForever);

    return true;

Error:
    file.close();

ErrorNoClose:
    psu::gui::hideAsyncOperationInProgress();

    g_state = STATE_IDLE;

	if (g_autoStartScriptIsRunning) {
		g_autoStartScriptIsRunning = false;
		psu::gui::showMainPage();
	}

	gui::refreshScreen();

    return false;
}

bool startScript(const char *filePath, int *err) {
	if (g_state == STATE_IDLE) {
		g_state = STATE_EXECUTING;
		stringCopy(g_scriptPath, MAX_PATH_LENGTH, filePath);

		int localErr = SCPI_RES_OK;
		if (!err) {
			err = &localErr;
		}

		*err = SCPI_RES_OK;

		if (isLowPriorityThread()) {
			loadScript(err);
		} else {
			sendMessageToLowPriorityThread(MP_LOAD_SCRIPT);
			*err = SCPI_RES_OK;
		}

		if (*err == SCPI_RES_OK && !g_autoStartScriptIsRunning) {
			psu::gui::showAsyncOperationInProgress();
		}

		return *err == SCPI_RES_OK;
	}

	if (err) {
		*err = SCPI_ERROR_SCRIPT_ENGINE_ALREADY_RUNNING;
	}

	return false;
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

static const char *g_commandOrQueryText;

void onQueueMessage(uint32_t type, uint32_t param) {
    if (type == MP_LOAD_SCRIPT) {
        loadScript();
    } else if (type == MP_STOP_SCRIPT) {
        stopScript();
    } else if (type == MP_EXECUTE_SCPI) {
        input(g_scpiContext, (const char *)g_commandOrQueryText, strlen(g_commandOrQueryText));
        input(g_scpiContext, "\r\n", 2);

        osMessagePut(g_mpMessageQueueId, QUEUE_MESSAGE_SCPI_RESULT, osWaitForever);
   }
}

bool scpi(const char *commandOrQueryText, const char **resultText, size_t *resultTextLen) {
    // DebugTrace("> %s\n", commandOrQueryText);

    g_scpiDataLen = 0;
    g_lastError = 0;

    bool executeInLowPriorityThread = true;
    if (startsWithNoCase(commandOrQueryText, "DISP:INPUT?") || startsWithNoCase(commandOrQueryText, "DISP:DIALOG:ACTION?")) {
        executeInLowPriorityThread = false;
    }

    if (executeInLowPriorityThread) {
        g_commandOrQueryText = commandOrQueryText;
        sendMessageToLowPriorityThread(MP_EXECUTE_SCPI);

        static const uint32_t SCPI_TIMEOUT = 60 * 60 * 1000;

        while (true) {
            osEvent event = osMessageGet(g_mpMessageQueueId, SCPI_TIMEOUT);
            if (event.status == osEventMessage && event.value.v == QUEUE_MESSAGE_SCPI_RESULT) {
                break;
            } else {
                static char g_scpiError[48];
                snprintf(g_scpiError, sizeof(g_scpiError), "SCPI timeout");
                mp_raise_ValueError(g_scpiError);
            }
        }
    } else {
        input(g_scpiContext, (const char *)commandOrQueryText, strlen(commandOrQueryText));
        input(g_scpiContext, "\r\n", 2);
    }

    if (g_lastError != 0) {
        static char g_scpiError[48];
        snprintf(g_scpiError, sizeof(g_scpiError), "SCPI error %d, \"%s\"", (int)g_lastError, SCPI_ErrorTranslate(g_lastError));
        mp_raise_ValueError(g_scpiError);
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

void resetSettings() {
	g_scriptingParameters.autoStartScript[0] = 0;
    g_scriptingParameters.options.autoStartConfirmation = 1;
	g_scriptParametersVersion++;
}

bool isAutoStartEnabled() {
	if (!g_autoStartConditionIsChecked) {
	    sendMessageToLowPriorityThread(THREAD_MESSAGE_AUTO_START_SCRIPT);
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

} // mp

namespace gui {

using namespace ::eez::mp;
using namespace ::eez::psu::gui;

void data_sys_settings_scripting_auto_start_script(DataOperationEnum operation, Cursor cursor, Value &value) {
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

void data_sys_settings_scripting_auto_start_script_enabled(DataOperationEnum operation, Cursor cursor, Value &value) {
	if (operation == DATA_OPERATION_GET) {
		value = g_scriptingParameters.autoStartScript[0] ? 1 : 0;
	}
}

void data_sys_settings_scripting_auto_start_confirmation(DataOperationEnum operation, Cursor cursor, Value &value) {
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
	browseForFile("Select auto start script", "/Scripts", FILE_TYPE_MICROPYTHON, file_manager::DIALOG_TYPE_OPEN, onAutoStartScriptFileSelected);
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

}
}

} // eez
