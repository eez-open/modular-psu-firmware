/*
 * EEZ Modular Firmware
 * Copyright (C) 2015-present, Envox d.o.o.
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

#include <eez/firmware.h>
#include <eez/usb.h>

#include <eez/modules/psu/psu.h>
#include <eez/modules/psu/serial_psu.h>

namespace eez {

using namespace scpi;
    
namespace psu {

using namespace scpi;

namespace serial {

TestResult g_testResult = TEST_FAILED;

static const size_t OUTPUT_BUFFER_MAX_SIZE = 1024;
static char g_outputBuffer[OUTPUT_BUFFER_MAX_SIZE];

size_t serialWrite(const char *data, size_t len) {
    g_messageAvailable = true;
    Serial.write(data, len);
    return len;
}

static OutputBufferWriter g_outputBufferWriter(&g_outputBuffer[0], OUTPUT_BUFFER_MAX_SIZE, serialWrite);

////////////////////////////////////////////////////////////////////////////////

size_t SCPI_Write(scpi_t *context, const char *data, size_t len) {
    g_outputBufferWriter.write(data, len);
    return len;
}

scpi_result_t SCPI_Flush(scpi_t *context) {
    g_outputBufferWriter.flush();
    return SCPI_RES_OK;
}

int SCPI_Error(scpi_t *context, int_fast16_t err) {
    return printError(context, err, g_outputBufferWriter);
}

scpi_result_t SCPI_Control(scpi_t *context, scpi_ctrl_name_t ctrl, scpi_reg_val_t val) {
    if (serial::g_testResult == TEST_OK) {
        char errorOutputBuffer[256];
        if (SCPI_CTRL_SRQ == ctrl) {
            snprintf(errorOutputBuffer, sizeof(errorOutputBuffer), "**SRQ: 0x%X (%d)\r\n", val, val);
        } else {
            snprintf(errorOutputBuffer, sizeof(errorOutputBuffer), "**CTRL %02x: 0x%X (%d)\r\n", ctrl, val, val);
        }
        g_outputBufferWriter.write(errorOutputBuffer, strlen(errorOutputBuffer));
    }

    return SCPI_RES_OK;
}

scpi_result_t SCPI_Reset(scpi_t *context) {
    if (serial::g_testResult == TEST_OK) {
        char errorOutputBuffer[256];
        strcpy(errorOutputBuffer, "**Reset\r\n");
        g_outputBufferWriter.write(errorOutputBuffer, strlen(errorOutputBuffer));
    }

    return reset() ? SCPI_RES_OK : SCPI_RES_ERR;
}

////////////////////////////////////////////////////////////////////////////////

static scpi_reg_val_t g_scpiPsuRegs[SCPI_PSU_REG_COUNT];
static scpi_psu_t g_scpiPsuContext = { g_scpiPsuRegs };

static scpi_interface_t g_scpiInterface = {
    SCPI_Error, SCPI_Write, SCPI_Control, SCPI_Flush, SCPI_Reset,
};

static char g_scpiInputBuffer[SCPI_PARSER_INPUT_BUFFER_LENGTH];
static scpi_error_t g_errorQueueData[SCPI_PARSER_ERROR_QUEUE_SIZE + 1];

scpi_t g_scpiContext;

static bool g_isConnected;

////////////////////////////////////////////////////////////////////////////////

void init() {
    initScpi();
    
#ifdef EEZ_PLATFORM_SIMULATOR
    if (usb::isVirtualComPortActive()) {
        Serial.print("EEZ BB3 software simulator ver. ");
        Serial.println(MCU_FIRMWARE);
    }
#endif
}

void onQueueMessage(uint32_t type, uint32_t param) {
    if (type == SERIAL_LINE_STATE_CHANGED) {
        bool wasConnected = isConnected();
        g_isConnected = param ? true : false;
        if (isConnected() && !wasConnected) {
            initScpi();
        }
    } else if (type == SERIAL_INPUT_AVAILABLE) {
        uint8_t *buffer;
        uint32_t length;
        Serial.getInputBuffer(param, &buffer, &length);
        if (g_testResult == TEST_OK) {
            input(g_scpiContext, (const char *)buffer, length);
        }
        Serial.releaseInputBuffer();
    }
}

bool isConnected() {
    return g_testResult == TEST_OK && g_isConnected;
}

void initScpi() {
    scpi::init(g_scpiContext, g_scpiPsuContext, &g_scpiInterface, g_scpiInputBuffer, SCPI_PARSER_INPUT_BUFFER_LENGTH, g_errorQueueData, SCPI_PARSER_ERROR_QUEUE_SIZE + 1);
}

} // namespace serial
} // namespace psu
} // namespace eez
