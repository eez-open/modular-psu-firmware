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

#include <eez/apps/psu/psu.h>

#include <eez/apps/psu/serial_psu.h>

#define CONF_CHUNK_SIZE CONF_SERIAL_BUFFER_SIZE

namespace eez {

using namespace scpi;
    
namespace psu {

using namespace scpi;

namespace serial {

TestResult g_testResult = TEST_FAILED;

long g_bauds[] = { 4800, 9600, 19200, 38400, 57600, 115200 };
size_t g_baudsSize = sizeof(g_bauds) / sizeof(long);

size_t SCPI_Write(scpi_t *context, const char *data, size_t len) {
    Serial.write(data, len);
    return len;
}

scpi_result_t SCPI_Flush(scpi_t *context) {
    return SCPI_RES_OK;
}

int SCPI_Error(scpi_t *context, int_fast16_t err) {
    if (err != 0) {
        scpi::printError(err);

        if (err == SCPI_ERROR_INPUT_BUFFER_OVERRUN) {
            scpi::onBufferOverrun(*context);
        }
    }

    return 0;
}

scpi_result_t SCPI_Control(scpi_t *context, scpi_ctrl_name_t ctrl, scpi_reg_val_t val) {
    if (serial::g_testResult == TEST_OK) {
        char errorOutputBuffer[256];
        if (SCPI_CTRL_SRQ == ctrl) {
            sprintf(errorOutputBuffer, "**SRQ: 0x%X (%d)\r\n", val, val);
        } else {
            sprintf(errorOutputBuffer, "**CTRL %02x: 0x%X (%d)\r\n", ctrl, val, val);
        }
        Serial.println(errorOutputBuffer);
    }

    return SCPI_RES_OK;
}

scpi_result_t SCPI_Reset(scpi_t *context) {
    if (serial::g_testResult == TEST_OK) {
        char errorOutputBuffer[256];
        strcpy(errorOutputBuffer, "**Reset\r\n");
        Serial.println(errorOutputBuffer);
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

UARTClass::UARTModes getConfig() {
    switch (persist_conf::getSerialParity()) {
    case PARITY_NONE:
        return UARTClass::Mode_8N1;
    case PARITY_EVEN:
        return UARTClass::Mode_8E1;
    case PARITY_ODD:
        return UARTClass::Mode_8O1;
    case PARITY_MARK:
        return UARTClass::Mode_8M1;
    case PARITY_SPACE:
        return UARTClass::Mode_8S1;
    }

    return UARTClass::Mode_8N1;
}

void init() {
    scpi::init(g_scpiContext, g_scpiPsuContext, &g_scpiInterface, g_scpiInputBuffer,
        SCPI_PARSER_INPUT_BUFFER_LENGTH, g_errorQueueData, SCPI_PARSER_ERROR_QUEUE_SIZE + 1);

    if (g_testResult == TEST_OK) {
        Serial.end();
    }

    if (!persist_conf::isSerialEnabled()) {
        g_testResult = TEST_SKIPPED;
        return;
    }

    Serial.begin(persist_conf::getBaudFromIndex(persist_conf::getSerialBaudIndex()),
                      getConfig());

#ifdef EEZ_PLATFORM_SIMULATOR
    Serial.print("EEZ PSU software simulator ver. ");
    Serial.println(FIRMWARE);
#else
    Serial.println("EEZ PSU serial com ready");
#endif

    g_testResult = TEST_OK;
}

void onQueueMessage(uint32_t type, uint32_t param) {
    if (type == SERIAL_LINE_STATE_CHANGED) {
        bool isConnected = param ? true : false;
        if (isConnected != g_isConnected) {
            g_isConnected = isConnected;
            if (g_isConnected) {
                scpi::emptyBuffer(g_scpiContext);
            }
        }
    } else if (type == SERIAL_INPUT_AVAILABLE) {
        uint8_t *buffer;
        uint32_t length;
        Serial.getInputBuffer(param, &buffer, &length);
        input(g_scpiContext, (const char *)buffer, length);
        Serial.releaseInputBuffer();
    }
}

bool isConnected() {
    return g_testResult == TEST_OK && g_isConnected;
}

void update() {
    init();
}

} // namespace serial
} // namespace psu
} // namespace eez
