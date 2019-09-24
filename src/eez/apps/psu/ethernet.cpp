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

#include <eez/apps/psu/psu.h>

#include <eez/apps/psu/event_queue.h>
#include <eez/apps/psu/persist_conf.h>
#include <eez/apps/psu/serial_psu.h>
#include <eez/apps/psu/watchdog.h>

#include <eez/apps/psu/ethernet.h>

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

size_t ethernet_client_write(const char *data, size_t len) {
    size_t size = eez::mcu::ethernet::writeBuffer(data, len);
    return size;
}

size_t ethernet_client_write_str(const char *str) {
    return ethernet_client_write(str, strlen(str));
}

////////////////////////////////////////////////////////////////////////////////

size_t SCPI_Write(scpi_t *context, const char *data, size_t len) {
    return ethernet_client_write(data, len);
}

scpi_result_t SCPI_Flush(scpi_t *context) {
    return SCPI_RES_OK;
}

int SCPI_Error(scpi_t *context, int_fast16_t err) {
    if (err != 0) {
        char errorOutputBuffer[256];
        sprintf(errorOutputBuffer, "**ERROR: %d,\"%s\"\r\n", (int16_t)err,
                SCPI_ErrorTranslate(err));
        ethernet_client_write(errorOutputBuffer, strlen(errorOutputBuffer));

        if (err == SCPI_ERROR_INPUT_BUFFER_OVERRUN) {
            scpi::onBufferOverrun(*context);
        }
    }

    return 0;
}

scpi_result_t SCPI_Control(scpi_t *context, scpi_ctrl_name_t ctrl, scpi_reg_val_t val) {
    char outputBuffer[256];
    if (SCPI_CTRL_SRQ == ctrl) {
        sprintf(outputBuffer, "**SRQ: 0x%X (%d)\r\n", val, val);
    } else {
        sprintf(outputBuffer, "**CTRL %02x: 0x%X (%d)\r\n", ctrl, val, val);
    }

    ethernet_client_write(outputBuffer, strlen(outputBuffer));

    return SCPI_RES_OK;
}

scpi_result_t SCPI_Reset(scpi_t *context) {
    char errorOutputBuffer[256];
    strcpy(errorOutputBuffer, "**Reset\r\n");
    ethernet_client_write(errorOutputBuffer, strlen(errorOutputBuffer));

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
    scpi::init(g_scpiContext, g_scpiPsuContext, &g_scpiInterface, g_scpiInputBuffer,
        SCPI_PARSER_INPUT_BUFFER_LENGTH, g_errorQueueData, SCPI_PARSER_ERROR_QUEUE_SIZE + 1);

    if (!persist_conf::isEthernetEnabled()) {
        g_testResult = TEST_SKIPPED;
        return;
    }

    if (persist_conf::isEthernetDhcpEnabled()) {
        eez::mcu::ethernet::begin(persist_conf::devConf2.ethernetMacAddress);
    } else {
        uint8_t ipAddress[4];
        ipAddressToArray(persist_conf::devConf2.ethernetIpAddress, ipAddress);

        uint8_t dns[4];
        ipAddressToArray(persist_conf::devConf2.ethernetIpAddress, ipAddress);

        uint8_t gateway[4];
        ipAddressToArray(persist_conf::devConf2.ethernetIpAddress, ipAddress);

        uint8_t subnetMask[4];
        ipAddressToArray(persist_conf::devConf2.ethernetIpAddress, ipAddress);

        eez::mcu::ethernet::begin(persist_conf::devConf2.ethernetMacAddress, ipAddress, dns, gateway, subnetMask);
    }

    g_testResult = TEST_CONNECTING;
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
            DebugTrace("Ethernet not connected!");
            event_queue::pushEvent(event_queue::EVENT_WARNING_ETHERNET_NOT_CONNECTED);
            return;
        }

        g_testResult = TEST_OK;

        eez::mcu::ethernet::beginServer(persist_conf::devConf2.ethernetScpiPort);
        //DebugTrace("Listening on port %d", (int)persist_conf::devConf2.ethernetScpiPort);
    } else if (type == ETHERNET_CLIENT_CONNECTED) {
        g_isConnected = true;
        scpi::emptyBuffer(g_scpiContext);
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
    // TODO
}

} // namespace ethernet
} // namespace psu
} // namespace eez

#endif
