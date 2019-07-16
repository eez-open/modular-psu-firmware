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
#include <memory.h>

#include <lwip.h>
#include <api.h>
#include <dhcp.h>
#include <ip_addr.h>
#include <netif.h>

#include <eez/system.h>

#include <eez/modules/mcu/ethernet.h>

#include <eez/apps/psu/psu.h>
#include <eez/apps/psu/ethernet.h>
using namespace eez::psu::ethernet;

#include <eez/scpi/scpi.h>
using namespace eez::scpi;

extern struct netif gnetif;

namespace eez {
namespace mcu {
namespace ethernet {

static void mainLoop(const void *);

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wwrite-strings"

osThreadDef(g_ethernetTask, mainLoop, osPriorityNormal, 0, 1024);

#pragma GCC diagnostic pop

osMessageQDef(g_ethernetMessageQueue, 5, uint32_t);
osMessageQId(g_ethernetMessageQueueId);

static osThreadId g_ethernetTaskHandle;

bool onSystemStateChanged() {
    if (g_systemState == SystemState::BOOTING) {
        if (eez::g_systemStatePhase == 0) {
        	g_ethernetMessageQueueId = osMessageCreate(osMessageQ(g_ethernetMessageQueue), NULL);
            return false;
        } else if (eez::g_systemStatePhase == 1) {
            g_ethernetTaskHandle = osThreadCreate(osThread(g_ethernetTask), nullptr);
        }
    }

    return true;
}

enum {
	QUEUE_MESSAGE_CONNECT,
	QUEUE_MESSAGE_CREATE_TCP_SERVER,
	QUEUE_MESSAGE_ACCEPT_CLIENT,
	QUEUE_MESSAGE_CLIENT_MESSAGE
};

enum ConnectionState {
    CONNECTION_STATE_INITIALIZED = 0,
    CONNECTION_STATE_CONNECTED = 1,
    CONNECTION_STATE_CONNECTING = 2,
    CONNECTION_STATE_BEGIN_SERVER = 3,
    CONNECTION_STATE_CLIENT_AVAILABLE = 4,
    CONNECTION_STATE_DATA_AVAILABLE = 5
};

static ConnectionState g_connectionState = CONNECTION_STATE_INITIALIZED;
static uint16_t g_port;
struct netconn *g_tcpListenConnection;
struct netconn *g_tcpClientConnection;
static struct netbuf *g_inbuf;

static void netconnCallback(struct netconn *conn, enum netconn_evt evt, u16_t len) {
	switch (evt) {
	case NETCONN_EVT_RCVPLUS:
		if (conn == g_tcpListenConnection) {
			osMessagePut(g_ethernetMessageQueueId, QUEUE_MESSAGE_ACCEPT_CLIENT, osWaitForever);
		} else if (conn == g_tcpClientConnection) {
			osMessagePut(g_scpiMessageQueueId, SCPI_QUEUE_ETHERNET_MESSAGE(ETHERNET_INPUT_AVAILABLE, 0), osWaitForever);
		}
		break;

	case NETCONN_EVT_RCVMINUS:
		osDelay(0);
		break;

	case NETCONN_EVT_SENDPLUS:
		osDelay(0);
		break;

	case NETCONN_EVT_SENDMINUS:
		osDelay(0);
		break;

	case NETCONN_EVT_ERROR:
		osDelay(0);
		break;
	}
}

static void mainLoop(const void *) {
	while (1) {
		osEvent event = osMessageGet(g_ethernetMessageQueueId, osWaitForever);
		if (event.status == osEventMessage) {
			switch (event.value.v) {
			case QUEUE_MESSAGE_CONNECT:
				{
					MX_LWIP_Init();
					netif_set_hostname(&gnetif, "EEZ-DIB");
					while(!dhcp_supplied_address(&gnetif)) {
						osDelay(10);
					}
					g_connectionState = CONNECTION_STATE_CONNECTED;
					osMessagePut(g_scpiMessageQueueId, SCPI_QUEUE_ETHERNET_MESSAGE(ETHERNET_CONNECTED, 1), osWaitForever);
				}
			    break;

			case QUEUE_MESSAGE_CREATE_TCP_SERVER:
				g_tcpListenConnection = netconn_new_with_callback(NETCONN_TCP, netconnCallback);
				if (g_tcpListenConnection == nullptr) {
					break;
				}

				// Is this required?
				// netconn_set_nonblocking(conn, 1);

				if (netconn_bind(g_tcpListenConnection, nullptr, g_port) != ERR_OK) {
					netconn_delete(g_tcpListenConnection);
					break;
				}

				netconn_listen(g_tcpListenConnection);
				break;

			case QUEUE_MESSAGE_ACCEPT_CLIENT:
				{
					struct netconn *newConnection;
					if (netconn_accept(g_tcpListenConnection, &newConnection) == ERR_OK) {
						if (g_tcpClientConnection) {
							// there is a client already connected, close this connection
							netconn_close(newConnection);
							netconn_delete(newConnection);
						} else {
							// connection with the client established
							g_tcpClientConnection = newConnection;
							osMessagePut(g_scpiMessageQueueId, SCPI_QUEUE_ETHERNET_MESSAGE(ETHERNET_CLIENT_CONNECTED, 0), osWaitForever);
						}
					}
				}
				break;
			}
		}
	}
}

////////////////////////////////////////////////////////////////////////////////

void begin(uint8_t *mac, uint8_t *, uint8_t *, uint8_t *, uint8_t *) {
	g_connectionState = CONNECTION_STATE_CONNECTING;
	osMessagePut(g_ethernetMessageQueueId, QUEUE_MESSAGE_CONNECT, osWaitForever);
}

IPAddress localIP() {
    return IPAddress(gnetif.ip_addr.addr);
}

IPAddress subnetMask() {
    return IPAddress(gnetif.netmask.addr);
}

IPAddress gatewayIP() {
    return IPAddress(gnetif.gw.addr);
}

IPAddress dnsServerIP() {
    return IPAddress();
}

void beginServer(uint16_t port) {
    g_port = port;
    osMessagePut(g_ethernetMessageQueueId, QUEUE_MESSAGE_CREATE_TCP_SERVER, osWaitForever);
}

void getInputBuffer(int bufferPosition, char **buffer, uint32_t *length) {
	if (netconn_recv(g_tcpClientConnection, &g_inbuf) != ERR_OK) {
		goto fail1;
	}

	if (netconn_err(g_tcpClientConnection) != ERR_OK) {
		goto fail2;
	}

	uint8_t* data;
	u16_t dataLength;
	netbuf_data(g_inbuf, (void**)&data, &dataLength);

    if (dataLength > 0) {
    	*buffer = (char *)data;
    	*length = dataLength;
    } else {
        netbuf_delete(g_inbuf);
        g_inbuf = nullptr;
    	*buffer = nullptr;
    	*length = 0;
    }

    return;

fail2:
	netbuf_delete(g_inbuf);
	g_inbuf = nullptr;

fail1:
	netconn_close(g_tcpClientConnection);
	netconn_delete(g_tcpClientConnection);
	g_tcpClientConnection = nullptr;
	osMessagePut(g_scpiMessageQueueId, SCPI_QUEUE_ETHERNET_MESSAGE(ETHERNET_CLIENT_DISCONNECTED, 0), osWaitForever);

	*buffer = nullptr;
	length = 0;
}

void releaseInputBuffer() {
	netbuf_delete(g_inbuf);
	g_inbuf = nullptr;
}

int writeBuffer(const char *buffer, uint32_t length) {
	netconn_write(g_tcpClientConnection, (void *)buffer, (uint16_t)length, NETCONN_COPY);
    return length;
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
