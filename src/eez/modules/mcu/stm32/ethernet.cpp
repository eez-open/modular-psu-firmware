/*
 * EEZ Middleware
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

#include <assert.h>
#include <stdint.h>
#include <stdlib.h>

#include <lwip.h>
#include <api.h>
#include <dhcp.h>
#include <ip_addr.h>
#include <netif.h>

#include <eez/system.h>

#include <eez/modules/mcu/ethernet.h>

extern struct netif gnetif;

namespace eez {
namespace mcu {
namespace ethernet {

static void mainLoop(const void *);

#if defined(EEZ_PLATFORM_STM32)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wwrite-strings"
#endif
osThreadDef(g_ethernetTask, mainLoop, osPriorityNormal, 0, 1024);

#if defined(EEZ_PLATFORM_STM32)
#pragma GCC diagnostic pop
#endif

static osThreadId g_ethernetTaskHandle;

bool onSystemStateChanged() {
    if (g_systemState == SystemState::BOOTING) {
        if (g_systemStatePhase == 0) {
            g_ethernetTaskHandle = osThreadCreate(osThread(g_ethernetTask), nullptr);
            assert(g_ethernetTaskHandle != NULL);
        }
    }

    return true;
}

#define MAX_DHCP_TRIES  4

static void mainLoop(const void *) {
	osDelay(2000);
    MX_LWIP_Init();
    netif_set_hostname(&gnetif, "EEZ-DIB");

    while(1) {
        if (dhcp_supplied_address(&gnetif)) {
            break;
        } else {
        	struct dhcp *dhcp;
        	dhcp = (struct dhcp *)netif_get_client_data(&gnetif, LWIP_NETIF_CLIENT_DATA_INDEX_DHCP);
        	if (dhcp->tries > MAX_DHCP_TRIES){
        		osDelay(250);
		    }
        }
        osDelay(250);
    }

    struct netconn *conn, *newconn;
    err_t err, accept_err;
    struct netbuf *buf;
    void *data;
    u16_t len;
    err_t recv_err;

    /* Create a new connection identifier. */
    conn = netconn_new(NETCONN_TCP);

    if (conn != NULL) {
        /* Bind connection to well known port number 7. */
        err = netconn_bind(conn, NULL, 7);

        if (err == ERR_OK) {
            /* Tell connection to go into listening mode. */
            netconn_listen(conn);

            while (1) {
                /* Grab new connection. */
                accept_err = netconn_accept(conn, &newconn);

                /* Process the new connection. */
                if (accept_err == ERR_OK) {
                    while (( recv_err = netconn_recv(newconn, &buf)) == ERR_OK) {
                        do {
                        netbuf_data(buf, &data, &len);
                        netconn_write(newconn, data, len, NETCONN_COPY);

                        } while (netbuf_next(buf) >= 0);

                        netbuf_delete(buf);
                    }

                    /* Close connection and discard connection identifier. */
                    netconn_close(newconn);
                    netconn_delete(newconn);
                }
            }
        } else {
            netconn_delete(newconn);
            printf(" can not bind TCP netconn");
        }
    } else {
        printf("can not create TCP netconn");
    }
}

////////////////////////////////////////////////////////////////////////////////

EthernetModule Ethernet;

bool EthernetModule::begin(uint8_t *mac, uint8_t *, uint8_t *, uint8_t *, uint8_t *) {
    return true;
}

uint8_t EthernetModule::maintain() {
    return 0;
}

IPAddress EthernetModule::localIP() {
    return IPAddress();
}

IPAddress EthernetModule::subnetMask() {
    return IPAddress();
}

IPAddress EthernetModule::gatewayIP() {
    return IPAddress();
}

IPAddress EthernetModule::dnsServerIP() {
    return IPAddress();
}

////////////////////////////////////////////////////////////////////////////////

void EthernetServer::init(int port_) {
    port = port_;
    client = true;
}

void EthernetServer::begin() {
}

EthernetClient EthernetServer::available() {
    return EthernetClient();
}

////////////////////////////////////////////////////////////////////////////////

EthernetClient::EthernetClient() : valid(false) {
}

EthernetClient::EthernetClient(bool valid_) : valid(valid_) {
}

bool EthernetClient::connected() {
    return false;
}

EthernetClient::operator bool() {
    return false;
}

size_t EthernetClient::available() {
    return 0;
}

size_t EthernetClient::read(uint8_t *buffer, size_t buffer_size) {
    return 0;
}

size_t EthernetClient::write(const char *buffer, size_t buffer_size) {
    return 0;
}

void EthernetClient::flush() {
}

void EthernetClient::stop() {
}

////////////////////////////////////////////////////////////////////////////////

uint8_t EthernetUDP::begin(uint16_t port) {
    return 0;
}

void EthernetUDP::stop() {
}

int EthernetUDP::beginPacket(const char *host, uint16_t port) {
    return 0;
}

size_t EthernetUDP::write(const uint8_t *buffer, size_t size) {
    return 0;
}

int EthernetUDP::endPacket() {
    return 0;
}

int EthernetUDP::read(unsigned char *buffer, size_t len) {
    return 0;
}

int EthernetUDP::parsePacket() {
    return 0;
}

} // namespace ethernet
} // namespace mcu
} // namespace eez

#endif
