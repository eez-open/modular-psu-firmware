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
#include <ethernetif.h>
#include <dns.h>
extern struct netif gnetif;
extern ip4_addr_t ipaddr;
ip4_addr_t dns;
extern ip4_addr_t netmask;
extern ip4_addr_t gw;
#endif // EEZ_PLATFORM_STM32

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
#endif // EEZ_PLATFORM_SIMULATOR

#include <eez/core/os.h>

#include <bb3/firmware.h>
#include <bb3/system.h>
#include <bb3/mcu/ethernet.h>
#include <bb3/psu/psu.h>
#include <bb3/psu/ethernet_scpi.h>
#include <bb3/psu/persist_conf.h>

#include <bb3/mqtt.h>
#include <bb3/psu/ntp.h>

using namespace eez::psu::ethernet;
using namespace eez::scpi;

#include <eez/gui/thread.h>
using namespace eez::gui;

#define CONF_CONNECT_TIMEOUT 30000
#define ACCEPT_CLIENT_TIMEOUT 1000

namespace eez {
namespace mcu {
namespace ethernet {

static void mainLoop(void *);

EEZ_THREAD_DECLARE(ethernet, Normal, 2048);

#if defined(EEZ_PLATFORM_STM32)
#define ETHERNET_MESSAGE_QUEUE_SIZE 20
#endif
#if defined(EEZ_PLATFORM_SIMULATOR)
#define ETHERNET_MESSAGE_QUEUE_SIZE 100
#endif

EEZ_MESSAGE_QUEUE_DECLARE(ethernet, {
	uint8_t type;
	union {
		struct {
			int16_t eventId;
			int8_t channelIndex;
		} pushEvent;
		int transition;
	};
});

void initMessageQueue() {
	EEZ_MESSAGE_QUEUE_CREATE(ethernet, ETHERNET_MESSAGE_QUEUE_SIZE);
}

void startThread() {
	EEZ_THREAD_CREATE(ethernet, mainLoop);
}

enum {
	QUEUE_MESSAGE_CONNECT,
	QUEUE_MESSAGE_CREATE_TCP_SERVER,
    QUEUE_MESSAGE_DESTROY_TCP_SERVER,
	QUEUE_MESSAGE_ACCEPT_SCPI_CLIENT,
    QUEUE_MESSAGE_ACCEPT_DEBUGGER_CLIENT,
    QUEUE_MESSAGE_PUSH_EVENT,
    QUEUE_MESSAGE_NTP_STATE_TRANSITION
};

#if defined(EEZ_PLATFORM_STM32)

enum ConnectionState {
    CONNECTION_STATE_INITIALIZED,
    CONNECTION_STATE_CONNECTED,
    CONNECTION_STATE_CONNECTING,
    CONNECTION_STATE_CONNECT_ERROR,
    CONNECTION_STATE_BEGIN_SERVER,
    CONNECTION_STATE_CLIENT_AVAILABLE,
    CONNECTION_STATE_DATA_AVAILABLE
};

static ConnectionState g_connectionState = CONNECTION_STATE_INITIALIZED;
static uint16_t g_scpiPort;

struct netconn *g_scpiListenConnection;
struct netconn *g_scpiClientConnection;
static netbuf *g_scpiInbuf;
static bool g_acceptScpiClientIsDone;

struct netconn *g_debuggerListenConnection;
struct netconn *g_debuggerClientConnection;
static netbuf *g_debuggerInbuf;
static bool g_acceptDebuggerClientIsDone;

static bool g_checkLinkWhileIdle = false;

static void netconnCallback(struct netconn *conn, enum netconn_evt evt, u16_t len) {
	switch (evt) {
	case NETCONN_EVT_RCVPLUS:
		if (conn == g_scpiListenConnection) {
            g_acceptScpiClientIsDone = false;

            ethernetMessageQueueObject obj;
            obj.type = QUEUE_MESSAGE_ACCEPT_SCPI_CLIENT;
            EEZ_MESSAGE_QUEUE_PUT(ethernet, obj, osWaitForever);

			for (int i = 0; i < ACCEPT_CLIENT_TIMEOUT; i++) {
                if (g_acceptScpiClientIsDone) {
                    break;
                }
                osDelay(1);
            }
		} else if (conn == g_scpiClientConnection) {
			sendMessageToLowPriorityThread(ETHERNET_INPUT_AVAILABLE);
		} else if (conn == g_debuggerListenConnection) {
            g_acceptDebuggerClientIsDone = false;

            ethernetMessageQueueObject obj;
            obj.type = QUEUE_MESSAGE_ACCEPT_DEBUGGER_CLIENT;
            EEZ_MESSAGE_QUEUE_PUT(ethernet, obj, osWaitForever);

            for (int i = 0; i < ACCEPT_CLIENT_TIMEOUT; i++) {
                if (g_acceptDebuggerClientIsDone) {
                    break;
                }
                osDelay(1);
            }
		} else if (conn == g_debuggerClientConnection) {
			sendMessageToGuiThread(GUI_QUEUE_MESSAGE_DEBUGGER_INPUT_AVAILABLE);
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
        // DebugTrace("NETCONN_EVT_ERROR\n");
		osDelay(0);
		break;
	}
}

static void dhcpStart() {
    uint32_t connectStart = millis();

    if (psu::persist_conf::isEthernetDhcpEnabled()) {
        // Start DHCP negotiation for a network interface (IPv4)
        dhcp_start(&gnetif);
        while(!dhcp_supplied_address(&gnetif)) {
            if ((millis() - connectStart) >= CONF_CONNECT_TIMEOUT) {
                g_connectionState = CONNECTION_STATE_CONNECT_ERROR;
                sendMessageToLowPriorityThread(ETHERNET_CONNECTED);
                return;
            }
            osDelay(10);
        }
    } else {
        dns_setserver(0, &dns);
    }

    g_connectionState = CONNECTION_STATE_CONNECTED;
    sendMessageToLowPriorityThread(ETHERNET_CONNECTED, 1);
    return;
}

void destroyScpiTcpServer() {
    if (g_scpiListenConnection) {
        auto tmp = g_scpiListenConnection;
        g_scpiListenConnection = nullptr;
        netconn_close(tmp);
        //netconn_shutdown(tmp, true, true);
        netconn_delete(tmp);
    }
}

void destroyDebuggerTcpServer() {
    if (g_debuggerListenConnection) {
        auto tmp = g_debuggerListenConnection;
        g_debuggerListenConnection = nullptr;
        netconn_close(tmp);
        //netconn_shutdown(tmp, true, true);
        netconn_delete(tmp);
    }
}

void destroyTcpServer() {
    destroyScpiTcpServer();
    destroyDebuggerTcpServer();
}

static void onEvent(uint8_t eventType) {
	switch (eventType) {
	case QUEUE_MESSAGE_CONNECT:
		{
            g_checkLinkWhileIdle = true;

            // MX_LWIP_Init();

            // Initilialize the LwIP stack with RTOS
            tcpip_init(NULL, NULL);

            if (psu::persist_conf::isEthernetDhcpEnabled()) {            
                ipaddr.addr = 0;
                dns.addr = 0;
                netmask.addr = 0;
                gw.addr = 0;
            } else {
            	ipaddr.addr = psu::persist_conf::devConf.ethernetIpAddress;
                dns.addr = psu::persist_conf::devConf.ethernetDns;
            	netmask.addr = psu::persist_conf::devConf.ethernetSubnetMask;
            	gw.addr = psu::persist_conf::devConf.ethernetGateway;
            }

            // add the network interface (IPv4/IPv6) with RTOS
            netif_add(&gnetif, &ipaddr, &netmask, &gw, NULL, &ethernetif_init, &tcpip_input);

			netif_set_hostname(&gnetif, psu::persist_conf::devConf.ethernetHostName);

            // Registers the default network interface
            netif_set_default(&gnetif);

            if (netif_is_link_up(&gnetif)) {
                // When the netif is fully configured this function must be called
                netif_set_up(&gnetif);
                dhcpStart();
            } else {
                // When the netif link is down this function must be called
                netif_set_down(&gnetif);
                g_connectionState = CONNECTION_STATE_CONNECT_ERROR;
                sendMessageToLowPriorityThread(ETHERNET_CONNECTED);
            }

		}
		break;

	case QUEUE_MESSAGE_CREATE_TCP_SERVER:
	    if (g_scpiClientConnection) {
	        auto tmp = g_scpiClientConnection;
		    g_scpiClientConnection = nullptr;
		    netconn_delete(tmp);
		    sendMessageToLowPriorityThread(ETHERNET_CLIENT_DISCONNECTED);
	    }

	    if (g_debuggerClientConnection) {
	        auto tmp = g_debuggerClientConnection;
		    g_debuggerClientConnection = nullptr;
		    netconn_delete(tmp);
            sendMessageToGuiThread(GUI_QUEUE_MESSAGE_DEBUGGER_CLIENT_DISCONNECTED);
	    }

	    if (g_scpiListenConnection) {
			break;
		}

        // create SCPI server
		g_scpiListenConnection = netconn_new_with_callback(NETCONN_TCP, netconnCallback);
		if (g_scpiListenConnection == nullptr) {
			break;
		}

		if (netconn_bind(g_scpiListenConnection, nullptr, g_scpiPort) != ERR_OK) {
		    destroyTcpServer();
			break;
		}

		netconn_listen(g_scpiListenConnection);

        // create debugger server
		g_debuggerListenConnection = netconn_new_with_callback(NETCONN_TCP, netconnCallback);
		if (g_debuggerListenConnection != nullptr) {
            if (netconn_bind(g_debuggerListenConnection, nullptr, DEBUGGER_TCP_PORT) == ERR_OK) {
		        netconn_listen(g_debuggerListenConnection);
            } else {
                destroyDebuggerTcpServer();
            }
        }

		break;

    case QUEUE_MESSAGE_DESTROY_TCP_SERVER:
        destroyTcpServer();
        break;

	case QUEUE_MESSAGE_ACCEPT_SCPI_CLIENT:
		{
			struct netconn *newConnection;
			if (netconn_accept(g_scpiListenConnection, &newConnection) == ERR_OK) {
				if (g_scpiClientConnection) {
					// there is a client already connected, close this connection
                    g_acceptScpiClientIsDone = true;
                    osDelay(10);
					netconn_delete(newConnection);
				} else {
					// connection with the client established
					g_scpiClientConnection = newConnection;
					sendMessageToLowPriorityThread(ETHERNET_CLIENT_CONNECTED);
                    g_acceptScpiClientIsDone = true;
				}
			}  else {
                g_acceptScpiClientIsDone = true;
            }
		}
        break;

    case QUEUE_MESSAGE_ACCEPT_DEBUGGER_CLIENT:
		{
			struct netconn *newConnection;
			if (netconn_accept(g_debuggerListenConnection, &newConnection) == ERR_OK) {
				if (g_debuggerClientConnection) {
					// there is a client already connected, close this connection
                    g_acceptDebuggerClientIsDone = true;
                    osDelay(10);
					netconn_delete(newConnection);
				} else {
					// connection with the client established
					g_debuggerClientConnection = newConnection;
					sendMessageToGuiThread(GUI_QUEUE_MESSAGE_DEBUGGER_CLIENT_CONNECTED);
                    g_acceptDebuggerClientIsDone = true;
				}
			}  else {
                g_acceptDebuggerClientIsDone = true;
            }
		}
		break;
	}
}

void onIdle() {
	if (g_checkLinkWhileIdle) {
		uint32_t regvalue = 0;

		/* Read PHY_BSR*/
		HAL_ETH_ReadPHYRegister(&heth, PHY_BSR, &regvalue);

		regvalue &= PHY_LINKED_STATUS;

		/* Check whether the netif link down and the PHY link is up */
		if(!netif_is_link_up(&gnetif) && (regvalue)) {
			/* network cable is connected */
			netif_set_up(&gnetif);
			netif_set_link_up(&gnetif);
            dhcpStart();
		} else if(netif_is_link_up(&gnetif) && (!regvalue)) {
			/* network cable is dis-connected */
			netif_set_link_down(&gnetif);
            g_connectionState = CONNECTION_STATE_CONNECT_ERROR;
            sendMessageToLowPriorityThread(ETHERNET_CONNECTED);
		}
	}
}
#endif // EEZ_PLATFORM_STM32

#if defined(EEZ_PLATFORM_SIMULATOR)
#define INPUT_BUFFER_SIZE 1024

static uint16_t g_scpiPort;
static char g_scpiInputBuffer[INPUT_BUFFER_SIZE];
static uint32_t g_scpiInputBufferLength;

static char g_debuggerInputBuffer[INPUT_BUFFER_SIZE];
static uint32_t g_debuggerInputBufferLength;

////////////////////////////////////////////////////////////////////////////////

#ifdef EEZ_PLATFORM_SIMULATOR_WIN32
static SOCKET g_scpiListenSocket = INVALID_SOCKET;
static SOCKET g_scpiClientSocket = INVALID_SOCKET;

static SOCKET g_debuggerListenSocket = INVALID_SOCKET;
static SOCKET g_debuggerClientSocket = INVALID_SOCKET;

typedef SOCKET SocketType;
#else
static int g_scpiListenSocket = -1;
static int g_scpiClientSocket = -1;

static int g_debuggerListenSocket = -1;
static int g_debuggerClientSocket = -1;

typedef int SocketType;

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

bool bind(int port, SocketType &listenSocket);
bool client_available(SocketType &listenSocket, SocketType &clientSocket);
bool connected(SocketType &listenSocket);
int available(SocketType &listenSocket);
int read(SocketType &listenSocket, char *buffer, int buffer_size);
int write(SocketType &listenSocket, const char *buffer, int buffer_size);
void stop(SocketType &listenSocket);

bool bind(int port, SocketType &listenSocket) {
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
    listenSocket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
    if (listenSocket == INVALID_SOCKET) {
        DebugTrace("EHTERNET: socket failed with error %d\n", WSAGetLastError());
        freeaddrinfo(result);
        return false;
    }

    u_long iMode = 1;
    iResult = ioctlsocket(listenSocket, FIONBIO, &iMode);
    if (iResult != NO_ERROR) {
        DebugTrace("EHTERNET: ioctlsocket failed with error %d\n", iResult);
        freeaddrinfo(result);
        closesocket(listenSocket);
        listenSocket = INVALID_SOCKET;
        return false;
    }

    // Setup the TCP listening socket
    iResult = ::bind(listenSocket, result->ai_addr, (int)result->ai_addrlen);
    if (iResult == SOCKET_ERROR) {
        DebugTrace("EHTERNET: bind failed with error %d\n", WSAGetLastError());
        freeaddrinfo(result);
        closesocket(listenSocket);
        listenSocket = INVALID_SOCKET;
        return false;
    }

    freeaddrinfo(result);

    iResult = listen(listenSocket, SOMAXCONN);
    if (iResult == SOCKET_ERROR) {
        DebugTrace("EHTERNET listen failed with error %d\n", WSAGetLastError());
        closesocket(listenSocket);
        listenSocket = INVALID_SOCKET;
        return false;
    }

    return true;
#else
    sockaddr_in serv_addr;
    listenSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (listenSocket < 0) {
        DebugTrace("EHTERNET: socket failed with error %d", errno);
        return false;
    }

    if (!enable_non_blocking(listenSocket)) {
        DebugTrace("EHTERNET: ioctl on listen socket failed with error %d", errno);
        close(listenSocket);
        listenSocket = -1;
        return false;
    }

    bzero((char *)&serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(port);
    if (::bind(listenSocket, (sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        DebugTrace("EHTERNET: bind failed with error %d", errno);
        close(listenSocket);
        listenSocket = -1;
        return false;
    }

    if (listen(listenSocket, 5) < 0) {
        DebugTrace("EHTERNET: listen failed with error %d", errno);
        close(listenSocket);
        listenSocket = -1;
        return false;
    }

    return true;
#endif    
}

bool client_available(SocketType &listenSocket, SocketType &clientSocket) {
#ifdef EEZ_PLATFORM_SIMULATOR_WIN32
    if (connected(clientSocket))
        return true;

    if (listenSocket == INVALID_SOCKET) {
        return false;
    }

    // Accept a client socket
    clientSocket = accept(listenSocket, NULL, NULL);
    if (clientSocket == INVALID_SOCKET) {
        if (WSAGetLastError() == WSAEWOULDBLOCK) {
            return false;
        }

        DebugTrace("EHTERNET accept failed with error %d\n", WSAGetLastError());
        closesocket(listenSocket);
        listenSocket = INVALID_SOCKET;
        return false;
    }

    return true;
#else
    if (connected(clientSocket))
        return true;

    if (listenSocket == -1) {
        return 0;
    }

    sockaddr_in cli_addr;
    socklen_t clilen = sizeof(cli_addr);
    clientSocket = accept(listenSocket, (sockaddr *)&cli_addr, &clilen);
    if (clientSocket < 0) {
        if (errno == EWOULDBLOCK) {
            return false;
        }

        DebugTrace("EHTERNET: accept failed with error %d", errno);
        close(listenSocket);
        listenSocket = -1;
        return false;
    }

    if (!enable_non_blocking(clientSocket)) {
        DebugTrace("EHTERNET: ioctl on client socket failed with error %d", errno);
        close(clientSocket);
        listenSocket = -1;
        return false;
    }

    return true;
#endif    
}

bool connected(SocketType &clientSocket) {
#ifdef EEZ_PLATFORM_SIMULATOR_WIN32
    return clientSocket != INVALID_SOCKET;
#else
    return clientSocket != -1;
#endif    
}

int available(SocketType &clientSocket, char *inputBuffer) {
#ifdef EEZ_PLATFORM_SIMULATOR_WIN32
    if (clientSocket == INVALID_SOCKET)
        return 0;

    int iResult = ::recv(clientSocket, inputBuffer, INPUT_BUFFER_SIZE, MSG_PEEK);
    if (iResult > 0) {
        return iResult;
    }

    if (iResult < 0 && WSAGetLastError() == WSAEWOULDBLOCK) {
        return 0;
    }

    stop(clientSocket);

    return 0;
#else
    if (clientSocket == -1)
        return 0;

    char buffer[1000];
    int iResult = ::recv(clientSocket, buffer, 1000, MSG_PEEK);
    if (iResult > 0) {
        return iResult;
    }

    if (iResult < 0 && errno == EWOULDBLOCK) {
        return 0;
    }

    stop(clientSocket);

    return 0;
#endif        
}

int read(SocketType &clientSocket, char *buffer, int buffer_size) {
#ifdef EEZ_PLATFORM_SIMULATOR_WIN32
    int iResult = ::recv(clientSocket, buffer, buffer_size, 0);
    if (iResult > 0) {
        return iResult;
    }

    if (iResult < 0 && WSAGetLastError() == WSAEWOULDBLOCK) {
        return 0;
    }

    stop(clientSocket);

    return 0;
#else
    int n = ::read(clientSocket, buffer, buffer_size);
    if (n > 0) {
        return n;
    }

    if (n < 0 && errno == EWOULDBLOCK) {
        return 0;
    }

    stop(clientSocket);

    return 0;
#endif    
}

int write(SocketType &clientSocket, const char *buffer, int buffer_size) {
#ifdef EEZ_PLATFORM_SIMULATOR_WIN32
    int iSendResult;

    if (clientSocket != INVALID_SOCKET) {
        // Echo the buffer back to the sender
        iSendResult = ::send(clientSocket, buffer, buffer_size, 0);
        if (iSendResult == SOCKET_ERROR) {
            DebugTrace("send failed with error: %d\n", WSAGetLastError());
            closesocket(clientSocket);
            clientSocket = INVALID_SOCKET;
            return 0;
        }
        return iSendResult;
    }

    return 0;
#else
    if (clientSocket != -1) {
        int n = ::write(clientSocket, buffer, buffer_size);
        if (n < 0) {
            close(clientSocket);
            clientSocket = -1;
            return 0;
        }
        return n;
    }

    return 0;
#endif    
}

void stop(SocketType &clientSocket) {
#ifdef EEZ_PLATFORM_SIMULATOR_WIN32
    if (clientSocket != INVALID_SOCKET) {
        int iResult = ::shutdown(clientSocket, SD_SEND);
        if (iResult == SOCKET_ERROR) {
            DebugTrace("EHTERNET shutdown failed with error %d\n", WSAGetLastError());
        }
        closesocket(clientSocket);
        clientSocket = INVALID_SOCKET;
    }
#else
    int result = ::shutdown(clientSocket, SHUT_WR);
    if (result < 0) {
        DebugTrace("ETHERNET shutdown failed with error %d\n", errno);
    }
    close(clientSocket);
    clientSocket = -1;
#endif    
}

void onEvent(uint8_t eventType) {
    switch (eventType) {
    case QUEUE_MESSAGE_CONNECT:
        sendMessageToLowPriorityThread(ETHERNET_CONNECTED, 1);
        break;

    case QUEUE_MESSAGE_CREATE_TCP_SERVER:
        bind(g_scpiPort, g_scpiListenSocket);
        bind(DEBUGGER_TCP_PORT, g_debuggerListenSocket);
        break;

    case QUEUE_MESSAGE_DESTROY_TCP_SERVER:
#ifdef EEZ_PLATFORM_SIMULATOR_WIN32
        if (g_scpiListenSocket != INVALID_SOCKET) {
            closesocket(g_scpiListenSocket);
			g_scpiListenSocket = INVALID_SOCKET;
        }

        if (g_debuggerListenSocket != INVALID_SOCKET) {
            closesocket(g_debuggerListenSocket);
			g_debuggerListenSocket = INVALID_SOCKET;
        }
#else
        close(g_scpiListenSocket);
        g_scpiListenSocket = -1;

        close(g_debuggerListenSocket);
        g_debuggerListenSocket = -1;
#endif    
        break;
    }
}

void onIdle() {
    static bool wasScpiConnected = false;

    if (wasScpiConnected) {
        if (connected(g_scpiClientSocket)) {
            if (!g_scpiInputBufferLength && available(g_scpiClientSocket, g_scpiInputBuffer)) {
                g_scpiInputBufferLength = read(g_scpiClientSocket, g_scpiInputBuffer, INPUT_BUFFER_SIZE);
                sendMessageToLowPriorityThread(ETHERNET_INPUT_AVAILABLE);
            }
        } else {
            sendMessageToLowPriorityThread(ETHERNET_CLIENT_DISCONNECTED);
            wasScpiConnected = false;
        }
    } else {
        if (client_available(g_scpiListenSocket, g_scpiClientSocket)) {
            wasScpiConnected = true;
            sendMessageToLowPriorityThread(ETHERNET_CLIENT_CONNECTED);
        }
    }

    static bool wasDebuggerConnected = false;

    if (wasDebuggerConnected) {
        if (connected(g_debuggerClientSocket)) {
            if (!g_debuggerInputBufferLength && available(g_debuggerClientSocket, g_debuggerInputBuffer)) {
                g_debuggerInputBufferLength = read(g_debuggerClientSocket, g_debuggerInputBuffer, INPUT_BUFFER_SIZE);
                sendMessageToGuiThread(GUI_QUEUE_MESSAGE_DEBUGGER_INPUT_AVAILABLE);
            }
        } else {
            sendMessageToGuiThread(GUI_QUEUE_MESSAGE_DEBUGGER_CLIENT_DISCONNECTED);
            wasDebuggerConnected = false;
        }
    } else {
        if (client_available(g_debuggerListenSocket, g_debuggerClientSocket)) {
            wasDebuggerConnected = true;
            sendMessageToGuiThread(GUI_QUEUE_MESSAGE_DEBUGGER_CLIENT_CONNECTED);
        }
    }
}
#endif

void mainLoop(void *) {
    while (1) {
        ethernetMessageQueueObject obj;
		if (EEZ_MESSAGE_QUEUE_GET(ethernet, obj, 10)) {
            if (obj.type == QUEUE_MESSAGE_PUSH_EVENT) {
                mqtt::pushEvent(obj.pushEvent.eventId, obj.pushEvent.channelIndex);
            } else if (obj.type == QUEUE_MESSAGE_NTP_STATE_TRANSITION) {
                ntp::stateTransition(obj.transition);
            } else {
                onEvent(obj.type);
            }
        } else {
            onIdle();
            mqtt::tick();
            ntp::tick();
        }
    }
}

////////////////////////////////////////////////////////////////////////////////

void begin() {
#if defined(EEZ_PLATFORM_STM32)
	g_connectionState = CONNECTION_STATE_CONNECTING;
#endif

    ethernetMessageQueueObject obj;
    obj.type = QUEUE_MESSAGE_CONNECT;
	EEZ_MESSAGE_QUEUE_PUT(ethernet, obj, osWaitForever);
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
    g_scpiPort = port;

    ethernetMessageQueueObject obj;
    obj.type = QUEUE_MESSAGE_CREATE_TCP_SERVER;
	EEZ_MESSAGE_QUEUE_PUT(ethernet, obj, osWaitForever);
}

void endServer() {
    ethernetMessageQueueObject obj;
    obj.type = QUEUE_MESSAGE_DESTROY_TCP_SERVER;
	EEZ_MESSAGE_QUEUE_PUT(ethernet, obj, osWaitForever);
}

void getInputBuffer(
#if defined(EEZ_PLATFORM_STM32)
	netbuf *&inbuf,
	netconn *&clientConnection,
#else
	char *inputBuffer, uint32_t &inputBufferLength, 
#endif
    char **buffer, uint32_t *length
) {
#if defined(EEZ_PLATFORM_STM32)
	if (!clientConnection) {
        *buffer = nullptr;
    	*length = 0;
		return;
	}

	if (netconn_recv(clientConnection, &inbuf) != ERR_OK) {
		goto fail1;
	}

	if (netconn_err(clientConnection) != ERR_OK) {
		goto fail2;
	}

	uint8_t* data;
	u16_t dataLength;
	netbuf_data(inbuf, (void**)&data, &dataLength);

    if (dataLength > 0) {
    	*buffer = (char *)data;
    	*length = dataLength;
    } else {
        netbuf_delete(inbuf);
        inbuf = nullptr;
    	*buffer = nullptr;
    	*length = 0;
    }

    return;

fail2:
	netbuf_delete(inbuf);
	inbuf = nullptr;

fail1:
	netconn_delete(clientConnection);
	clientConnection = nullptr;
	if (clientConnection == g_scpiClientConnection) {
		sendMessageToLowPriorityThread(ETHERNET_CLIENT_DISCONNECTED);
	} else if (clientConnection == g_debuggerClientConnection) {
        sendMessageToGuiThread(GUI_QUEUE_MESSAGE_DEBUGGER_CLIENT_DISCONNECTED);
    }

	*buffer = nullptr;
	length = 0;
#endif

#if defined(EEZ_PLATFORM_SIMULATOR)
    *buffer = inputBuffer;
    *length = inputBufferLength;
#endif
}

void releaseInputBuffer(
#if defined(EEZ_PLATFORM_STM32)
	netbuf *&inbuf
#else
	char *inputBuffer, uint32_t &inputBufferLength
#endif
) {
#if defined(EEZ_PLATFORM_STM32)
	netbuf_delete(inbuf);
	inbuf = nullptr;
#endif

#if defined(EEZ_PLATFORM_SIMULATOR)
    inputBufferLength = 0;
#endif
}

void getScpiInputBuffer(char **buffer, uint32_t *length) {
	getInputBuffer(
#if defined(EEZ_PLATFORM_STM32)
		g_scpiInbuf,
		g_scpiClientConnection,
#else
		g_scpiInputBuffer, g_scpiInputBufferLength,
#endif
        buffer, length);
}

void releaseScpiInputBuffer() {
    releaseInputBuffer(
#if defined(EEZ_PLATFORM_STM32)
		g_scpiInbuf
#else
		g_scpiInputBuffer, g_scpiInputBufferLength
#endif
    );
}

int writeScpiBuffer(const char *buffer, uint32_t length) {
#if defined(EEZ_PLATFORM_STM32)
    netconn_write(g_scpiClientConnection, (void *)buffer, (uint16_t)length, NETCONN_COPY);
    return length;
#endif

#if defined(EEZ_PLATFORM_SIMULATOR)
    int numWritten = write(g_scpiClientSocket, buffer, length);
    osDelay(1);
    return numWritten;
#endif
}

void getDebuggerInputBuffer(char **buffer, uint32_t *length) {
	getInputBuffer(
#if defined(EEZ_PLATFORM_STM32)
		g_debuggerInbuf,
		g_debuggerClientConnection,
#else
		g_debuggerInputBuffer, g_debuggerInputBufferLength,
#endif
        buffer, length);
}

void releaseDebuggerInputBuffer() {
    releaseInputBuffer(
#if defined(EEZ_PLATFORM_STM32)
		g_debuggerInbuf
#else
		g_debuggerInputBuffer, g_debuggerInputBufferLength
#endif
    );
}

int writeDebuggerBuffer(const char *buffer, uint32_t length) {
#if defined(EEZ_PLATFORM_STM32)
    netconn_write(g_debuggerClientConnection, (void *)buffer, (uint16_t)length, NETCONN_COPY);
    return length;
#endif

#if defined(EEZ_PLATFORM_SIMULATOR)
    int numWritten = write(g_debuggerClientSocket, buffer, length);
    osDelay(1);
    return numWritten;
#endif
}

void disconnectClients() {
#if defined(EEZ_PLATFORM_STM32)
	netconn_delete(g_scpiClientConnection);
	g_scpiClientConnection = nullptr;

	netconn_delete(g_debuggerClientConnection);
	g_debuggerClientConnection = nullptr;
#endif

#if defined(EEZ_PLATFORM_SIMULATOR)
    stop(g_scpiClientSocket);
    stop(g_debuggerClientSocket);
#endif    
}

void pushEvent(int16_t eventId, int8_t channelIndex) {
    if (!g_shutdownInProgress) {
        ethernetMessageQueueObject obj;
        obj.type = QUEUE_MESSAGE_PUSH_EVENT;
        obj.pushEvent.eventId = eventId;
        obj.pushEvent.channelIndex = channelIndex;
		EEZ_MESSAGE_QUEUE_PUT(ethernet, obj, 0);
    }
}

void ntpStateTransition(int transition) {
    if (!g_shutdownInProgress) {
        ethernetMessageQueueObject obj;
        obj.type = QUEUE_MESSAGE_NTP_STATE_TRANSITION;
        obj.transition = transition;
		EEZ_MESSAGE_QUEUE_PUT(ethernet, obj, 0);
    }
}

} // namespace ethernet
} // namespace mcu
} // namespace eez

#endif // EEZ_PLATFORM_SIMULATOR
