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

#if defined(EEZ_PLATFORM_STM32)
#include <lwip.h>
#include <api.h>
#include <dhcp.h>
#include <ip_addr.h>
#include <netif.h>
extern struct netif gnetif;
#endif

#if defined(EEZ_PLATFORM_SIMULATOR)
#ifdef EEZ_PLATFORM_SIMULATOR_WIN32
#undef INPUT
#undef OUTPUT
#define WIN32_LEAN_AND_MEAN
#include <stdio.h>
#include <stdlib.h>
#include <windows.h>
#include <winsock2.h>
#undef _WIN32_WINNT
#define _WIN32_WINNT 0x0501
#include <ws2tcpip.h>
#else
#include <errno.h>
#include <fcntl.h>
#include <memory.h>
#include <netinet/in.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#endif
#endif

#include <eez/system.h>
#include <eez/scpi/scpi.h>
#include <eez/modules/mcu/ethernet.h>
#include <eez/modules/psu/psu.h>
#include <eez/modules/psu/ethernet.h>
#include <eez/modules/psu/persist_conf.h>

#include <eez/mqtt.h>

using namespace eez::psu::ethernet;
using namespace eez::scpi;

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

osMessageQDef(g_ethernetMessageQueue, 10, uint32_t);
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
	QUEUE_MESSAGE_CLIENT_MESSAGE,
    QUEUE_MESSAGE_PUSH_EVENT
};

#if defined(EEZ_PLATFORM_STM32)
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
static netbuf *g_inbuf;

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

static void onEvent(uint8_t eventType) {
	switch (eventType) {
	case QUEUE_MESSAGE_CONNECT:
		{
			MX_LWIP_Init();
			netif_set_hostname(&gnetif, psu::persist_conf::devConf.ethernetHostName);
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

void onIdle() {
}
#endif

#if defined(EEZ_PLATFORM_SIMULATOR)
#define INPUT_BUFFER_SIZE 1024

static uint16_t g_port;
static char g_inputBuffer[INPUT_BUFFER_SIZE];
static uint32_t g_inputBufferLength;

////////////////////////////////////////////////////////////////////////////////

bool bind(int port);
bool client_available();
bool connected();
int available();
int read(char *buffer, int buffer_size);
int write(const char *buffer, int buffer_size);
void stop();

#ifdef EEZ_PLATFORM_SIMULATOR_WIN32
static SOCKET listen_socket = INVALID_SOCKET;
static SOCKET client_socket = INVALID_SOCKET;
#else
static int listen_socket = -1;
static int client_socket = -1;

bool enable_non_blocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0) {
        return false;
    }
    flags = flags | O_NONBLOCK;
    if (fcntl(fd, F_SETFL, flags) < 0) {
        return false;
    }
    return true;
}

#endif

bool bind(int port) {
#ifdef EEZ_PLATFORM_SIMULATOR_WIN32
    WSADATA wsaData;
    int iResult;

    struct addrinfo *result = NULL;
    struct addrinfo hints;

    // Initialize Winsock
    iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (iResult != 0) {
        DebugTrace("EHTERNET: WSAStartup failed with error %d\n", iResult);
        return false;
    }

    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_flags = AI_PASSIVE;

    // Resolve the server address and port
    char port_str[16];
#ifdef _MSC_VER
    _itoa(port, port_str, 10);
#else
    itoa(port, port_str, 10);
#endif
    iResult = getaddrinfo(NULL, port_str, &hints, &result);
    if (iResult != 0) {
        DebugTrace("EHTERNET: getaddrinfo failed with error %d\n", iResult);
        return false;
    }

    // Create a SOCKET for connecting to server
    listen_socket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
    if (listen_socket == INVALID_SOCKET) {
        DebugTrace("EHTERNET: socket failed with error %d\n", WSAGetLastError());
        freeaddrinfo(result);
        return false;
    }

    u_long iMode = 1;
    iResult = ioctlsocket(listen_socket, FIONBIO, &iMode);
    if (iResult != NO_ERROR) {
        DebugTrace("EHTERNET: ioctlsocket failed with error %d\n", iResult);
        freeaddrinfo(result);
        closesocket(listen_socket);
        listen_socket = INVALID_SOCKET;
        return false;
    }

    // Setup the TCP listening socket
    iResult = ::bind(listen_socket, result->ai_addr, (int)result->ai_addrlen);
    if (iResult == SOCKET_ERROR) {
        DebugTrace("EHTERNET: bind failed with error %d\n", WSAGetLastError());
        freeaddrinfo(result);
        closesocket(listen_socket);
        listen_socket = INVALID_SOCKET;
        return false;
    }

    freeaddrinfo(result);

    iResult = listen(listen_socket, SOMAXCONN);
    if (iResult == SOCKET_ERROR) {
        DebugTrace("EHTERNET listen failed with error %d\n", WSAGetLastError());
        closesocket(listen_socket);
        listen_socket = INVALID_SOCKET;
        return false;
    }

    return true;
#else
    sockaddr_in serv_addr;
    listen_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_socket < 0) {
        DebugTrace("EHTERNET: socket failed with error %d", errno);
        return false;
    }

    if (!enable_non_blocking(listen_socket)) {
        DebugTrace("EHTERNET: ioctl on listen socket failed with error %d", errno);
        close(listen_socket);
        listen_socket = -1;
        return false;
    }

    bzero((char *)&serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(port);
    if (::bind(listen_socket, (sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        DebugTrace("EHTERNET: bind failed with error %d", errno);
        close(listen_socket);
        listen_socket = -1;
        return false;
    }

    if (listen(listen_socket, 5) < 0) {
        DebugTrace("EHTERNET: listen failed with error %d", errno);
        close(listen_socket);
        listen_socket = -1;
        return false;
    }

    return true;
#endif    
}

bool client_available() {
#ifdef EEZ_PLATFORM_SIMULATOR_WIN32
    if (connected())
        return true;

    if (listen_socket == INVALID_SOCKET) {
        return false;
    }

    // Accept a client socket
    client_socket = accept(listen_socket, NULL, NULL);
    if (client_socket == INVALID_SOCKET) {
        if (WSAGetLastError() == WSAEWOULDBLOCK) {
            return false;
        }

        DebugTrace("EHTERNET accept failed with error %d\n", WSAGetLastError());
        closesocket(listen_socket);
        listen_socket = INVALID_SOCKET;
        return false;
    }

    return true;
#else
    if (connected())
        return true;

    if (listen_socket == -1) {
        return 0;
    }

    sockaddr_in cli_addr;
    socklen_t clilen = sizeof(cli_addr);
    client_socket = accept(listen_socket, (sockaddr *)&cli_addr, &clilen);
    if (client_socket < 0) {
        if (errno == EWOULDBLOCK) {
            return false;
        }

        DebugTrace("EHTERNET: accept failed with error %d", errno);
        close(listen_socket);
        listen_socket = -1;
        return false;
    }

    if (!enable_non_blocking(client_socket)) {
        DebugTrace("EHTERNET: ioctl on client socket failed with error %d", errno);
        close(client_socket);
        listen_socket = -1;
        return false;
    }

    return true;
#endif    
}

bool connected() {
#ifdef EEZ_PLATFORM_SIMULATOR_WIN32
    return client_socket != INVALID_SOCKET;
#else
    return client_socket != -1;
#endif    
}

int available() {
#ifdef EEZ_PLATFORM_SIMULATOR_WIN32
    if (client_socket == INVALID_SOCKET)
        return 0;

    int iResult = ::recv(client_socket, g_inputBuffer, INPUT_BUFFER_SIZE, MSG_PEEK);
    if (iResult > 0) {
        return iResult;
    }

    if (iResult < 0 && WSAGetLastError() == WSAEWOULDBLOCK) {
        return 0;
    }

    stop();

    return 0;
#else
    if (client_socket == -1)
        return 0;

    char buffer[1000];
    int iResult = ::recv(client_socket, buffer, 1000, MSG_PEEK);
    if (iResult > 0) {
        return iResult;
    }

    if (iResult < 0 && errno == EWOULDBLOCK) {
        return 0;
    }

    stop();

    return 0;
#endif        
}

int read(char *buffer, int buffer_size) {
#ifdef EEZ_PLATFORM_SIMULATOR_WIN32
    int iResult = ::recv(client_socket, buffer, buffer_size, 0);
    if (iResult > 0) {
        return iResult;
    }

    if (iResult < 0 && WSAGetLastError() == WSAEWOULDBLOCK) {
        return 0;
    }

    stop();

    return 0;
#else
    int n = ::read(client_socket, buffer, buffer_size);
    if (n > 0) {
        return n;
    }

    if (n < 0 && errno == EWOULDBLOCK) {
        return 0;
    }

    stop();

    return 0;
#endif    
}

int write(const char *buffer, int buffer_size) {
#ifdef EEZ_PLATFORM_SIMULATOR_WIN32
    int iSendResult;

    if (client_socket != INVALID_SOCKET) {
        // Echo the buffer back to the sender
        iSendResult = ::send(client_socket, buffer, buffer_size, 0);
        if (iSendResult == SOCKET_ERROR) {
            DebugTrace("send failed with error: %d\n", WSAGetLastError());
            closesocket(client_socket);
            client_socket = INVALID_SOCKET;
            return 0;
        }
        return iSendResult;
    }

    return 0;
#else
    if (client_socket != -1) {
        int n = ::write(client_socket, buffer, buffer_size);
        if (n < 0) {
            close(client_socket);
            client_socket = -1;
            return 0;
        }
        return n;
    }

    return 0;
#endif    
}

void stop() {
#ifdef EEZ_PLATFORM_SIMULATOR_WIN32
    if (client_socket != INVALID_SOCKET) {
        int iResult = ::shutdown(client_socket, SD_SEND);
        if (iResult == SOCKET_ERROR) {
            DebugTrace("EHTERNET shutdown failed with error %d\n", WSAGetLastError());
        }
        closesocket(client_socket);
        client_socket = INVALID_SOCKET;
    }
#else
    int result = ::shutdown(client_socket, SHUT_WR);
    if (result < 0) {
        DebugTrace("ETHERNET shutdown failed with error %d\n", errno);
    }
    close(client_socket);
    client_socket = -1;
#endif    
}

void onEvent(uint8_t eventType) {
    switch (eventType) {
    case QUEUE_MESSAGE_CONNECT:
        osMessagePut(g_scpiMessageQueueId, SCPI_QUEUE_ETHERNET_MESSAGE(ETHERNET_CONNECTED, 1), osWaitForever);
        break;

    case QUEUE_MESSAGE_CREATE_TCP_SERVER:
        bind(g_port);
        break;
    }
}

void onIdle() {
    static bool wasConnected = false;

    if (wasConnected) {
        if (connected()) {
            if (!g_inputBufferLength && available()) {
                g_inputBufferLength = read(g_inputBuffer, INPUT_BUFFER_SIZE);
                osMessagePut(g_scpiMessageQueueId, SCPI_QUEUE_ETHERNET_MESSAGE(ETHERNET_INPUT_AVAILABLE, 0), osWaitForever);
            }
        } else {
            osMessagePut(g_scpiMessageQueueId, SCPI_QUEUE_ETHERNET_MESSAGE(ETHERNET_CLIENT_DISCONNECTED, 0), osWaitForever);
            wasConnected = false;
        }
    } else {
        if (client_available()) {
            wasConnected = true;
            osMessagePut(g_scpiMessageQueueId, SCPI_QUEUE_ETHERNET_MESSAGE(ETHERNET_CLIENT_CONNECTED, 0), osWaitForever);
        }
    }
}
#endif

void mainLoop(const void *) {
    while (1) {
        osEvent event = osMessageGet(g_ethernetMessageQueueId, 0);
        if (event.status == osEventMessage) {
            uint8_t eventType = event.value.v & 0xFF;
            if (eventType == QUEUE_MESSAGE_PUSH_EVENT) {
                mqtt::pushEvent((int16_t)(event.value.v >> 8));
            } else {
                onEvent(eventType);
            }
        } else {
            onIdle();
            mqtt::tick(micros());
        }
    }
}

////////////////////////////////////////////////////////////////////////////////

void begin(const uint8_t *mac, const uint8_t *, const uint8_t *, const uint8_t *, const uint8_t *) {
#if defined(EEZ_PLATFORM_STM32)
	g_connectionState = CONNECTION_STATE_CONNECTING;
#endif
    osMessagePut(g_ethernetMessageQueueId, QUEUE_MESSAGE_CONNECT, osWaitForever);
}

IPAddress localIP() {
#if defined(EEZ_PLATFORM_STM32)
    return IPAddress(gnetif.ip_addr.addr);
#endif

#if defined(EEZ_PLATFORM_SIMULATOR)
    return IPAddress();
#endif
}

IPAddress subnetMask() {
#if defined(EEZ_PLATFORM_STM32)
    return IPAddress(gnetif.netmask.addr);
#endif

#if defined(EEZ_PLATFORM_SIMULATOR)
    return IPAddress();
#endif
}

IPAddress gatewayIP() {
#if defined(EEZ_PLATFORM_STM32)
    return IPAddress(gnetif.gw.addr);
#endif

#if defined(EEZ_PLATFORM_SIMULATOR)
    return IPAddress();
#endif
}

IPAddress dnsServerIP() {
#if defined(EEZ_PLATFORM_STM32)
    return IPAddress();
#endif

#if defined(EEZ_PLATFORM_SIMULATOR)
    return IPAddress();
#endif
}

void beginServer(uint16_t port) {
    g_port = port;
    osMessagePut(g_ethernetMessageQueueId, QUEUE_MESSAGE_CREATE_TCP_SERVER, osWaitForever);
}

void getInputBuffer(int bufferPosition, char **buffer, uint32_t *length) {
#if defined(EEZ_PLATFORM_STM32)
	if (!g_tcpClientConnection) {
		return;
	}

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
#endif

#if defined(EEZ_PLATFORM_SIMULATOR)
    *buffer = g_inputBuffer;
    *length = g_inputBufferLength;
#endif
}

void releaseInputBuffer() {
#if defined(EEZ_PLATFORM_STM32)
	netbuf_delete(g_inbuf);
	g_inbuf = nullptr;
#endif

#if defined(EEZ_PLATFORM_SIMULATOR)
    g_inputBufferLength = 0;
#endif
}

int writeBuffer(const char *buffer, uint32_t length) {
#if defined(EEZ_PLATFORM_STM32)
	netconn_write(g_tcpClientConnection, (void *)buffer, (uint16_t)length, NETCONN_COPY);
    return length;
#endif

#if defined(EEZ_PLATFORM_SIMULATOR)
    int numWritten = write(buffer, length);
    osDelay(1);
    return numWritten;
#endif
}

void pushEvent(int16_t eventId) {
    osMessagePut(g_ethernetMessageQueueId, ((uint32_t)(uint16_t)eventId << 8) | QUEUE_MESSAGE_PUSH_EVENT, 0);
}

} // namespace ethernet
} // namespace mcu
} // namespace eez

#endif
