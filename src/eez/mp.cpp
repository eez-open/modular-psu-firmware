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
#include <eez/modules/psu/scpi/psu.h>
#include <eez/modules/psu/gui/psu.h>

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

////////////////////////////////////////////////////////////////////////////////

using namespace eez::scpi;
using namespace eez::psu::scpi;

static char g_scpiData[SCPI_PARSER_INPUT_BUFFER_LENGTH + 1];
static size_t g_scpiDataLen;
static int_fast16_t g_lastError;

size_t SCPI_Write(scpi_t *context, const char *data, size_t len) {
    len = MIN(len, SCPI_PARSER_INPUT_BUFFER_LENGTH - g_scpiDataLen);
    if (len > 0) {
        strncpy(g_scpiData + g_scpiDataLen, data, len);
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
    g_mpMessageQueueId = osMessageCreate(osMessageQ(g_mpMessageQueue), NULL);
}

void startThread() {
    g_mpTaskHandle = osThreadCreate(osThread(g_mpTask), nullptr);
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
            getBaseFileName(g_scriptPath, scriptName, sizeof(scriptName));
            InfoTrace("Script started: %s\n", scriptName);

#if 0
        	// this version reinitialise MP every time
			volatile char dummy;
			mp_stack_set_top((void *)&dummy);
			gc_init(g_scriptSource + MAX_SCRIPT_LENGTH, MP_BUFFER + MP_BUFFER_SIZE - MAX_SCRIPT_LENGTH);
			mp_init();

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
            }

            gc_sweep_all();
            mp_deinit();
#endif

#if 1
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

            g_state = STATE_IDLE;

            InfoTrace("Script ended: %s\n", scriptName);
        }
    }
}

void startScript(const char *filePath) {
    if (g_state == STATE_IDLE) {
        g_state = STATE_EXECUTING;
        strcpy(g_scriptPath, filePath);
        //DebugTrace("T1 %d\n", millis());
        sendMessageToLowPriorityThread(MP_LOAD_SCRIPT);

        psu::gui::showAsyncOperationInProgress();
    }
}

void loadScript() {
    uint32_t fileSize;
    uint32_t bytesRead;

    eez::File file;
    if (!file.open(g_scriptPath, FILE_OPEN_EXISTING | FILE_READ)) {
        generateError(SCPI_ERROR_FILE_NOT_FOUND);
        goto ErrorNoClose;
    }

    fileSize = file.size();
    if (fileSize > MAX_SCRIPT_LENGTH) {
        generateError(SCPI_ERROR_OUT_OF_DEVICE_MEMORY);
        goto Error;
    }

    bytesRead = file.read(g_scriptSource, fileSize);

    file.close();

    if (bytesRead != fileSize) {
        generateError(SCPI_ERROR_MASS_STORAGE_ERROR);
        goto Error;
    }

    g_scriptSourceLength = fileSize;

    //DebugTrace("T2 %d\n", millis());
    osMessagePut(g_mpMessageQueueId, QUEUE_MESSAGE_START_SCRIPT, osWaitForever);

    return;

Error:
    file.close();

ErrorNoClose:
    psu::gui::hideAsyncOperationInProgress();

    g_state = STATE_IDLE;

    return;
}

static const char *g_commandOrQueryText;

void onQueueMessage(uint32_t type, uint32_t param) {
    if (type == MP_LOAD_SCRIPT) {
        loadScript();
    } else if (type == MP_EXECUTE_SCPI) {
        input(g_scpiContext, (const char *)g_commandOrQueryText, strlen(g_commandOrQueryText));
        input(g_scpiContext, "\r\n", 2);

        osMessagePut(g_mpMessageQueueId, QUEUE_MESSAGE_SCPI_RESULT, osWaitForever);
    }
}

bool scpi(const char *commandOrQueryText, const char **resultText, size_t *resultTextLen) {
    //DebugTrace("T4 %d\n", millis());

    g_scpiDataLen = 0;

    // g_commandOrQueryText = commandOrQueryText;
    // sendMessageToLowPriorityThread(MP_EXECUTE_SCPI);

    // while (true) {
    //    osEvent event = osMessageGet(g_mpMessageQueueId, osWaitForever);
    //    if (event.status == osEventMessage) {
    //        switch (event.value.v) {
    //        case QUEUE_MESSAGE_SCPI_RESULT:
    //            *resultText = g_scpiData;
    //            *resultTextLen = g_scpiDataLen;
    //            return true;
    //        }
    //    }
    // }

    g_lastError = 0;

    input(g_scpiContext, (const char *)commandOrQueryText, strlen(commandOrQueryText));
    input(g_scpiContext, "\r\n", 2);

    if (g_lastError != 0) {
        static char g_scpiError[48];
        snprintf(g_scpiError, 48, "SCPI error %d, \"%s\"", g_lastError, SCPI_ErrorTranslate(g_lastError));
        mp_raise_ValueError(g_scpiError);
    }

    if (g_scpiDataLen >= 2 && g_scpiData[g_scpiDataLen - 2] == '\r' && g_scpiData[g_scpiDataLen - 1] == '\n') {
        g_scpiDataLen -= 2;
        g_scpiData[g_scpiDataLen] = 0;
    }

    *resultText = g_scpiData;
    *resultTextLen = g_scpiDataLen;
    return true;
}

} // mp
} // eez
