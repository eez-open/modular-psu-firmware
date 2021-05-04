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

#if OPTION_ETHERNET

#include <stdio.h>

#include <eez/firmware.h>

#include <eez/modules/psu/psu.h>
#include <eez/modules/psu/event_queue.h>
#include <eez/modules/psu/persist_conf.h>
#include <eez/modules/psu/serial_psu.h>
#include <eez/modules/psu/ethernet.h>

#include <eez/modules/mcu/ethernet.h>

#define CONF_CHECK_DHCP_LEASE_SEC 60

namespace eez {

using namespace scpi;

namespace psu {

using namespace scpi;

namespace ethernet {

TestResult g_testResult = TEST_FAILED;

static bool g_isConnected = false;

////////////////////////////////////////////////////////////////////////////////

static const size_t OUTPUT_BUFFER_MAX_SIZE = 1024;
static char g_outputBuffer[OUTPUT_BUFFER_MAX_SIZE];

size_t ethernetClientWrite(const char *data, size_t len) {
    g_messageAvailable = true;
    return eez::mcu::ethernet::writeBuffer(data, len);
}

static OutputBufferWriter g_outputBufferWriter(&g_outputBuffer[0], OUTPUT_BUFFER_MAX_SIZE, ethernetClientWrite);

////////////////////////////////////////////////////////////////////////////////

size_t SCPI_Write(scpi_t *context, const char *data, size_t len) {
    return g_outputBufferWriter.write(data, len);
}

scpi_result_t SCPI_Flush(scpi_t *context) {
    g_outputBufferWriter.flush();
    return SCPI_RES_OK;
}

int SCPI_Error(scpi_t *context, int_fast16_t err) {
    return printError(context, err, g_outputBufferWriter);
}

scpi_result_t SCPI_Control(scpi_t *context, scpi_ctrl_name_t ctrl, scpi_reg_val_t val) {
    char outputBuffer[256];
    if (SCPI_CTRL_SRQ == ctrl) {
        snprintf(outputBuffer, sizeof(outputBuffer), "**SRQ: 0x%X (%d)\r\n", val, val);
    } else {
        snprintf(outputBuffer, sizeof(outputBuffer), "**CTRL %02x: 0x%X (%d)\r\n", ctrl, val, val);
    }

    g_outputBufferWriter.write(outputBuffer, strlen(outputBuffer));

    return SCPI_RES_OK;
}

scpi_result_t SCPI_Reset(scpi_t *context) {
    //char errorOutputBuffer[256];
    //stringCopy(errorOutputBuffer, sizeof(errorOutputBuffer), "**Reset\r\n");
    //g_outputBufferWriter.write(errorOutputBuffer, strlen(errorOutputBuffer));

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

////////////////////////////////////////////////////////////////////////////////

void init() {
    initScpi();

    if (!persist_conf::isEthernetEnabled()) {
        g_testResult = TEST_SKIPPED;
        return;
    }

    eez::mcu::ethernet::begin();

    g_testResult = TEST_CONNECTING;
}

void initScpi() {
    scpi::init(g_scpiContext, g_scpiPsuContext, &g_scpiInterface, g_scpiInputBuffer, SCPI_PARSER_INPUT_BUFFER_LENGTH, g_errorQueueData, SCPI_PARSER_ERROR_QUEUE_SIZE + 1);
}

bool test() {
    if (g_testResult == TEST_FAILED) {
        generateError(SCPI_ERROR_ETHERNET_TEST_FAILED);
    }

    return g_testResult != TEST_FAILED;
}

void onQueueMessage(uint32_t type, uint32_t param) {
    if (type == ETHERNET_CONNECTED) {
        bool isConnected = param ? true : false;
        if (!isConnected) {
            g_testResult = TEST_WARNING;
            event_queue::pushEvent(event_queue::EVENT_WARNING_ETHERNET_NOT_CONNECTED);
            return;
        } else {
            if (g_testResult == TEST_WARNING) {
                event_queue::pushEvent(event_queue::EVENT_INFO_ETHERNET_CONNECTED);
            }
        }

        g_testResult = TEST_OK;

        eez::mcu::ethernet::beginServer(persist_conf::devConf.ethernetScpiPort);
        //DebugTrace("Listening on port %d", (int)persist_conf::devConf.ethernetScpiPort);
    } else if (type == ETHERNET_CLIENT_CONNECTED) {
        g_isConnected = true;
        initScpi();
    } else if (type == ETHERNET_CLIENT_DISCONNECTED) {
        g_isConnected = false;
    } else if (type == ETHERNET_INPUT_AVAILABLE) {
        char *buffer;
        uint32_t length;
        eez::mcu::ethernet::getInputBuffer(param, &buffer, &length);
        if (buffer && length) {
            input(g_scpiContext, (const char *)buffer, length);
            eez::mcu::ethernet::releaseInputBuffer();
        }
    }
}

uint32_t getIpAddress() {
    return eez::mcu::ethernet::localIP();
}

bool isConnected() {
    return g_isConnected;
}

void update() {
    static TestResult g_testResultAtBoot;

    if (persist_conf::isEthernetEnabled() && !g_shutdownInProgress) {
        auto callBeginServer = g_testResult == TEST_SKIPPED && g_testResultAtBoot == TEST_OK;

        g_testResult = g_testResultAtBoot;

        if (callBeginServer) {
            eez::mcu::ethernet::beginServer(persist_conf::devConf.ethernetScpiPort);
        }
    } else {
        if (g_isConnected) {
            eez::mcu::ethernet::disconnectClient();
            g_isConnected = false;
        }

        eez::mcu::ethernet::endServer();

        if (g_testResult != TEST_SKIPPED) {
            g_testResultAtBoot = g_testResult;
        }
        g_testResult = TEST_SKIPPED;
    }
}

} // namespace ethernet
} // namespace psu
} // namespace eez

#endif
