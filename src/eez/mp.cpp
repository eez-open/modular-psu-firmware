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

#include <eez/mp.h>
#include <eez/system.h>
#include <eez/scpi/scpi.h>

#include <eez/libs/sd_fat/sd_fat.h>

#include <eez/modules/psu/psu.h>
#include <eez/modules/psu/scpi/psu.h>

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

/*
- Usporediti ovo https://github.com/numworks/epsilon sa onim Å¡to mi imamo i sa latest verzijom MP-a
- Pokrenuti one python make skripte i vidjeti Å¡to se dobije i kako se to razlikuje od onoga Å¡to je veÄ‡ buildano
- Dodati jedan modul
- Probati primiti str parameter i vratiti int ili string
- Delay funkcija
- Python thread
- Vidjeti kako zaglaviti i odglaviti
- scpi funkcija bi trebala poslati scpi komandu u scpi thread kao string i Ä�ekati rezultat
- python moÅ¾e zaglaviti u delay i scpi komandi

- dlog specificira X os (vrijeme, U) i Y osi (U, I, P). Za svaku os MIN, MAX, MIN_MEAS, MAX_MEAS.
  Za x os i STEP ili PERIOD. Ako step nije zadan onda se i vrijednosti i za X osi nalaze u podacima.

MAGIC1
MAGIC2
VERSION
TAG PARAMS
TAG PARAMS
0xFF
DATA

Tag 0x01 X-os unit min max min_meas max_meas
Tag 0x02 start time

Tag 0x10 Y1-os min max min_meas max_meas
Tag 0x11 Y2-os min max min_meas max_meas
Tag 0x12 Y3-os min max min_meas max_meas
Tag 0x1F Y16-os min max min_meas max_meas

Unit: 0x00 time, 0x01 VOLT, 0x02 AMPER, 0x03 WATT

- DLOG view dock legend


U1 DIV OFFSET |--------------------------------|
   CURSOR     |                                |
I1 DIV OFFSET |                                |
   CURSOR     |                                |
U2 DIV OFFSET |                                |
   CURSOR     |                                |
I2 DIV OFFSET |                                |
   CURSOR     |--------------------------------|

Width: 40 + 80 * 2 = 200px
Height: 8 x 30 = 240px

Ostaje za graf: 280px x 240px
7 x 6 divisiona

https://docs.micropython.org/en/latest/develop/cmodules.html

*/

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

static char *g_scriptPath = (char *)MP_BUFFER;
static char *g_scriptSource = g_scriptPath + MAX_PATH_LENGTH + 1;
static const size_t MAX_SCRIPT_SOURCE_AND_HEAP_SIZE = MP_BUFFER_SIZE - MAX_PATH_LENGTH + 1;
static size_t g_scriptSourceLength;

////////////////////////////////////////////////////////////////////////////////

using namespace eez::scpi;
using namespace eez::psu::scpi;

static char g_scpiData[SCPI_PARSER_INPUT_BUFFER_LENGTH];
static size_t g_scpiDataLen;

size_t SCPI_Write(scpi_t *context, const char *data, size_t len) {
    len = MIN(len, SCPI_PARSER_INPUT_BUFFER_LENGTH - g_scpiDataLen);
    if (len > 0) {
        strncpy(g_scpiData + g_scpiDataLen, data, len);
        g_scpiDataLen += len;
    }
    return len;
}

scpi_result_t SCPI_Flush(scpi_t *context) {
    return SCPI_RES_OK;
}

int SCPI_Error(scpi_t *context, int_fast16_t err) {
    //if (err != 0) {
    //    scpi::printError(err);

    //    if (err == SCPI_ERROR_INPUT_BUFFER_OVERRUN) {
    //        scpi::onBufferOverrun(*context);
    //    }
    //}

    return 0;
}

scpi_result_t SCPI_Control(scpi_t *context, scpi_ctrl_name_t ctrl, scpi_reg_val_t val) {
    //if (serial::g_testResult == TEST_OK) {
    //    char errorOutputBuffer[256];
    //    if (SCPI_CTRL_SRQ == ctrl) {
    //        sprintf(errorOutputBuffer, "**SRQ: 0x%X (%d)\r\n", val, val);
    //    } else {
    //        sprintf(errorOutputBuffer, "**CTRL %02x: 0x%X (%d)\r\n", ctrl, val, val);
    //    }
    //    Serial.println(errorOutputBuffer);
    //}

    return SCPI_RES_OK;
}

scpi_result_t SCPI_Reset(scpi_t *context) {
    //if (serial::g_testResult == TEST_OK) {
    //    char errorOutputBuffer[256];
    //    strcpy(errorOutputBuffer, "**Reset\r\n");
    //    Serial.println(errorOutputBuffer);
    //}

    return eez::psu::reset() ? SCPI_RES_OK : SCPI_RES_ERR;
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

bool onSystemStateChanged() {
    if (eez::g_systemState == eez::SystemState::BOOTING) {
        if (eez::g_systemStatePhase == 0) {
            eez::psu::scpi::init(g_scpiContext, g_scpiPsuContext, &g_scpiInterface, g_scpiInputBuffer, SCPI_PARSER_INPUT_BUFFER_LENGTH, g_errorQueueData, SCPI_PARSER_ERROR_QUEUE_SIZE + 1);

            g_mpMessageQueueId = osMessageCreate(osMessageQ(g_mpMessageQueue), NULL);
            g_mpTaskHandle = osThreadCreate(osThread(g_mpTask), nullptr);
        }
    }

    return true;
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
        switch (event.value.v) {
        case QUEUE_MESSAGE_START_SCRIPT:
#if 0
        	// this version reinitialise MP every time
			volatile char dummy;
			mp_stack_set_top((void *)&dummy);
			gc_init(g_scriptSource + g_scriptSourceLength, MP_BUFFER + MP_BUFFER_SIZE - 32768 - 1024);
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
				gc_init(g_scriptSource + g_scriptSourceLength, MP_BUFFER + MP_BUFFER_SIZE - 32768 - 1024);
				mp_init();
			}

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
			}

			mp_deinit();
#endif

            break;
        }
    }
}

enum {
    LOAD_SCRIPT,
    EXECUTE_SCPI
};

void startScript(const char *filePath) {
    strcpy(g_scriptPath, filePath);

    osMessagePut(scpi::g_scpiMessageQueueId, SCPI_QUEUE_MP_MESSAGE(LOAD_SCRIPT, 0), osWaitForever);
}

void loadScript() {
    uint32_t fileSize;
    uint32_t bytesRead;

    eez::File file;
    if (!file.open(g_scriptPath, FILE_OPEN_EXISTING | FILE_READ)) {
        goto ErrorNoClose;
    }

    fileSize = file.size();
    if (fileSize > MAX_SCRIPT_SOURCE_AND_HEAP_SIZE) {
        goto Error;
    }

    bytesRead = file.read(g_scriptSource, fileSize);

    file.close();

    if (bytesRead != fileSize) {
        goto Error;
    }

    g_scriptSourceLength = fileSize;

    osMessagePut(g_mpMessageQueueId, QUEUE_MESSAGE_START_SCRIPT, osWaitForever);

    return;

Error:
    file.close();

ErrorNoClose:
    // TODO error report
    return;
}

static const char *g_commandOrQueryText;

void onQueueMessage(uint32_t type, uint32_t param) {
    if (type == LOAD_SCRIPT) {
        loadScript();
    } else if (type == EXECUTE_SCPI) {
        input(g_scpiContext, (const char *)g_commandOrQueryText, strlen(g_commandOrQueryText));
        input(g_scpiContext, "\r\n", 2);

        osMessagePut(g_mpMessageQueueId, QUEUE_MESSAGE_SCPI_RESULT, osWaitForever);
    }
}

bool scpi(const char *commandOrQueryText, const char **resultText, size_t *resultTextLen) {
    g_scpiDataLen = 0;

    // g_commandOrQueryText = commandOrQueryText;
    // osMessagePut(scpi::g_scpiMessageQueueId, SCPI_QUEUE_MP_MESSAGE(EXECUTE_SCPI, 0), osWaitForever);

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

    input(g_scpiContext, (const char *)commandOrQueryText, strlen(commandOrQueryText));
    input(g_scpiContext, "\r\n", 2);

    if (g_scpiDataLen >= 2 && g_scpiData[g_scpiDataLen - 2] == '\r' && g_scpiData[g_scpiDataLen - 1] == '\n') {
        g_scpiDataLen -= 2;
    }

    *resultText = g_scpiData;
    *resultTextLen = g_scpiDataLen;
    return true;
}

} // mp
} // eez
