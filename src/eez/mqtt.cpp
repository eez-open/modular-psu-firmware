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

#if OPTION_ETHERNET

#include <stdlib.h>

#if defined(EEZ_PLATFORM_STM32)
#include <api.h>
#include <mqtt.h>
#include <ip_addr.h>
#include <dns.h>
#endif

#if defined(EEZ_PLATFORM_SIMULATOR)
extern "C" {
#include <mqtt.h>
}
#include <unistd.h>
#include <stdio.h>
#if defined(WIN32)
#include <ws2tcpip.h>
#endif
#include <posix_sockets.h>
#endif

#include <eez/debug.h>
#include <eez/mqtt.h>
#include <eez/modules/psu/psu.h>
#include <eez/modules/psu/trigger.h>
#include <eez/modules/psu/ethernet.h>
#include <eez/modules/psu/channel_dispatcher.h>

namespace eez {

using namespace psu;

namespace mqtt {

#if defined(EEZ_PLATFORM_STM32)
static const char *CLIENT_ID = "BB3_STM32";
#endif

#if defined(EEZ_PLATFORM_SIMULATOR)
static const char *CLIENT_ID = "BB3_Simulator";
#endif

static const char *PUB_TOPIC = "ch/%d";
static const char *SUB_TOPIC = "ch/+/+"; // for example: ch/1/oe, ch/1/setu, ch/1/seti

ConnectionState g_connectionState = CONNECTION_STATE_IDLE;

static char g_addr[64];
static int g_port;
static char g_user[32];
static char g_pass[32];

static uint32_t g_lastTickCount;

static struct {
    int oe;
    float uSet;
    float iSet;
} g_channelStates[CH_MAX];

void onIncomingPublish(const char *topic, const char *payload) {
    const char *p = topic + 3;

    char *endptr;
    int channelIndex = strtol(p, &endptr, 10);
    if (endptr > p && channelIndex > 0 && channelIndex <= CH_NUM) {
        channelIndex--;

        while (*p != '/') {
            p++;
        }
        p++;

        Channel &channel = Channel::get(channelIndex);

        if (strncmp(p, "oe", 2) == 0) {
            int oe = strtol(payload, &endptr, 10);
            if (endptr > payload) {
                channel_dispatcher::outputEnable(channel, oe != 0);
            }
        } else if (strncmp(p, "setu", 4) == 0) {
            if (channel_dispatcher::getVoltageTriggerMode(channel) != TRIGGER_MODE_FIXED && !trigger::isIdle()) {
                return;
            }

            if (channel.isRemoteProgrammingEnabled()) {
                return;
            }

            float voltage = (float)strtof(payload, &endptr);

            if (endptr == payload) {
                return;
            }

            if (isNaN(voltage)) {
                return;
            }

            if (voltage < channel_dispatcher::getUMin(channel)) {
                return;
            }

            if (voltage > channel_dispatcher::getULimit(channel)) {
                return;
            }

            if (voltage * channel_dispatcher::getISetUnbalanced(channel) > channel_dispatcher::getPowerLimit(channel)) {
                return;
            }

            channel_dispatcher::setVoltage(channel, voltage);
        } else if (strncmp(p, "seti", 4) == 0) {
            if (channel_dispatcher::getVoltageTriggerMode(channel) != TRIGGER_MODE_FIXED && !trigger::isIdle()) {
                return;
            }

            float current = (float)strtof(payload, &endptr);

            if (endptr == payload) {
                return;
            }

            if (isNaN(current)) {
                return;
            }

            if (current < channel_dispatcher::getIMin(channel)) {
                return;
            }

            if (current > channel_dispatcher::getILimit(channel)) {
                return;
            }

            if (current * channel_dispatcher::getUSetUnbalanced(channel) > channel_dispatcher::getPowerLimit(channel)) {
                return;
            }

            channel_dispatcher::setCurrent(channel, current);
        }
    }
}    

#if defined(EEZ_PLATFORM_STM32)
static ip_addr_t g_ipaddr;
static mqtt_client_t *g_client;
static const size_t MAX_TOPIC_LEN = 128;
static char g_topic[MAX_TOPIC_LEN + 1];
static const size_t MAX_PAYLOAD_LEN = 128;
static char g_payload[MAX_PAYLOAD_LEN + 1];
static size_t g_payloadLen;

static void dnsFoundCallback(const char* hostname, const ip_addr_t *ipaddr, void *arg) {
    if (ipaddr != NULL) {
        g_ipaddr = *ipaddr;
        g_connectionState = CONNECTION_STATE_DNS_FOUND;
    } else {
        g_connectionState = CONNECTION_STATE_ERROR;
        DebugTrace("mqtt dns error: server not found\n");
    }
}

static void connectCallback(mqtt_client_t *client, void *arg, mqtt_connection_status_t status) {
}

static void requestCallback(void *arg, err_t err) {
}

void incomingPublishCallback(void *arg, const char *topic, u32_t tot_len) {
    // DebugTrace("Incoming publish: %s, %d\n", topic, (int)tot_len);
    if (tot_len < MAX_TOPIC_LEN) {
        strncpy(g_topic, topic, tot_len);
        g_topic[tot_len] = 0;
    } else {
        g_topic[0] = 0;
    }
    g_payloadLen = 0;
}

void incomingDataCallback(void *arg, const u8_t *data, u16_t len, u8_t flags) {
    // DebugTrace("Incoming data: %.*s, %d\n", len, data, (int)flags);
    if (g_payloadLen + len < MAX_PAYLOAD_LEN) {
        memcpy(g_payload + g_payloadLen, data, len);
        g_payloadLen += len;
        if (flags == MQTT_DATA_FLAG_LAST) {
            g_payload[g_payloadLen] = 0;
            onIncomingPublish(g_topic, g_payload);
        }
    }
}
#endif

#if defined(EEZ_PLATFORM_SIMULATOR)
static int g_sockfd;
static uint8_t g_sendbuf[2048]; /* sendbuf should be large enough to hold multiple whole mqtt messages */
static uint8_t g_recvbuf[1024]; /* recvbuf should be large enough any whole mqtt message expected to be received */
static struct mqtt_client g_client; /* instantiate the client */

void incomingPublishCallback(void** unused, struct mqtt_response_publish *published) {
    onIncomingPublish((const char *)published->topic_name, (const char *)published->application_message);
}
#endif

void publish(char *topic, char *payload, bool retain) {
#if defined(EEZ_PLATFORM_STM32)
    err_t result = mqtt_publish(g_client, topic, payload, strlen(payload), 0, retain ? 1 : 0, requestCallback, nullptr);
    if (result != ERR_OK) {
        DebugTrace("mqtt publish error: %d\n", (int)result);
    }
#endif

#if defined(EEZ_PLATFORM_SIMULATOR)
    mqtt_publish(&g_client, topic, payload, strlen(payload), MQTT_PUBLISH_QOS_0 | (retain ? MQTT_PUBLISH_RETAIN : 0));
    if (g_client.error != MQTT_OK) {
        DebugTrace("mqtt error: %s\n", mqtt_error_str(g_client.error));
    }
#endif
}

void publish(int channelIndex, float uMon, float iMon) {
    char topic[20];
    sprintf(topic, PUB_TOPIC, channelIndex + 1);

    char payload[100];
    sprintf(payload, "{\"umon\":%g,\"imon\":%g}", uMon, iMon);

    publish(topic, payload, false);
}

void publish(int channelIndex, int oeState, float uSet, float iSet) {
    char topic[20];
    sprintf(topic, PUB_TOPIC, channelIndex + 1);

    char payload[100];
    sprintf(payload, "{\"oe\":%d,\"uset\":%g,\"iset\":%g}", oeState, uSet, iSet);

    publish(topic, payload, true);
}

void setConnected(uint32_t tickCount) {
    g_connectionState = CONNECTION_STATE_CONNECTED;
    g_lastTickCount = tickCount;

#if defined(EEZ_PLATFORM_STM32)
    mqtt_set_inpub_callback(g_client, incomingPublishCallback, incomingDataCallback, nullptr);
    mqtt_subscribe(g_client, SUB_TOPIC, 0, requestCallback, nullptr);
#endif

#if defined(EEZ_PLATFORM_SIMULATOR)
    mqtt_subscribe(&g_client, SUB_TOPIC, 0);
#endif

    for(int i = 0; i < CH_MAX; i++) {
        g_channelStates[i].oe = -1;
    }
}

void tick(uint32_t tickCount) {
    if (ethernet::g_testResult != TEST_OK) {
        // pass
    }

    else if (g_connectionState == CONNECTION_STATE_IDLE) {
        // pass
    }

    else if (g_connectionState == CONNECTION_STATE_CONNECT) {
#if defined(EEZ_PLATFORM_STM32)
        ip_addr_set_zero(&g_ipaddr);
        ip_addr_t ipaddr;
        err_t err = dns_gethostbyname(g_addr, &ipaddr, dnsFoundCallback, NULL);
        if (err == ERR_OK) {
            g_connectionState = CONNECTION_STATE_DNS_FOUND;
            g_ipaddr = ipaddr;
        } else if (err == ERR_INPROGRESS) {
            g_connectionState = CONNECTION_STATE_DNS_IN_PROGRESS;
        } else {
            g_connectionState = CONNECTION_STATE_ERROR;
            DebugTrace("mqtt dns error: %d\n", (int)err);
        }
#endif

#if defined(EEZ_PLATFORM_SIMULATOR)
        char port[16];
        sprintf(port, "%d", g_port);
        g_sockfd = open_nb_socket(g_addr, port);
        if (g_sockfd != -1) {
            /* initialize the client */
            mqtt_init(&g_client, g_sockfd, g_sendbuf, sizeof(g_sendbuf), g_recvbuf, sizeof(g_recvbuf), incomingPublishCallback);

            /* Ensure we have a clean session */
            uint8_t connect_flags = MQTT_CONNECT_CLEAN_SESSION;

            /* Send connection request to the broker. */
            mqtt_connect(&g_client, CLIENT_ID, NULL, NULL, 0, g_user, g_pass, connect_flags, 400);

            /* check that we don't have any errors */
            if (g_client.error == MQTT_OK) {
                setConnected(tickCount);
            } else {
                g_connectionState = CONNECTION_STATE_ERROR;
                DebugTrace("mqtt error: %s\n", mqtt_error_str(g_client.error));
            }
        } else {
            g_connectionState = CONNECTION_STATE_ERROR;
            DebugTrace("mqtt error: failed to open socket\n");
        }
#endif
    }

    else if (g_connectionState == CONNECTION_STATE_DISCONNECT) {
#if defined(EEZ_PLATFORM_STM32)
        mqtt_disconnect(g_client);
#endif

#if defined(EEZ_PLATFORM_SIMULATOR)
        mqtt_disconnect(&g_client);
#endif

        g_connectionState = CONNECTION_STATE_IDLE;
    }

    else if (g_connectionState == CONNECTION_STATE_ERROR) {
        // pass
    }

#if defined(EEZ_PLATFORM_STM32)
    else if (g_connectionState == CONNECTION_STATE_DNS_FOUND) {
        if (!g_client) {
            g_client = mqtt_client_new();
        }
        if (g_client) {
            mqtt_connect_client_info_t clientInfo;
            clientInfo.client_id = CLIENT_ID;
            clientInfo.client_user = g_user;
            clientInfo.client_pass = g_pass;
            clientInfo.keep_alive = 60; // seconds
            clientInfo.will_topic = nullptr; // not used
            err_t result = mqtt_client_connect(g_client, &g_ipaddr, g_port, connectCallback, nullptr, &clientInfo);
            if (result == ERR_OK) {
                g_connectionState = CONNECTION_STATE_CONNECTING;
                g_lastTickCount = tickCount;
            } else {
                g_connectionState = CONNECTION_STATE_ERROR;
                DebugTrace("mqtt connect error: %d\n", (int)result);
            }
        } else {
            g_connectionState = CONNECTION_STATE_ERROR;
            DebugTrace("mqtt error: failed to create a client\n");
        }
        return;
    }
#endif

    else if (g_connectionState == CONNECTION_STATE_CONNECTING) {
#if defined(EEZ_PLATFORM_STM32)
        if (mqtt_client_is_connected(g_client)) {
            setConnected(tickCount);
        }
#endif
    }

    else if (g_connectionState == CONNECTION_STATE_CONNECTED) {
        bool callPublish = false;
        int32_t diff = tickCount - g_lastTickCount;
        if (diff >= 1000000L) { // 1 sec
            g_lastTickCount = tickCount;
            callPublish = true;
        }

        for (uint8_t channelIndex = 0; channelIndex < CH_NUM; channelIndex++) {
			Channel &channel = Channel::get(channelIndex);
			if (channel.isInstalled()) {
				int oe = channel.isOutputEnabled() ? 1 : 0;
				float uSet = channel_dispatcher::getUSet(channel);
				float iSet = channel_dispatcher::getISet(channel);
				if (oe != g_channelStates[channelIndex].oe || (callPublish && (uSet != g_channelStates[channelIndex].uSet || iSet != g_channelStates[channelIndex].iSet))) {
					g_channelStates[channelIndex].oe = oe;
					g_channelStates[channelIndex].uSet = uSet;
					g_channelStates[channelIndex].iSet = iSet;
					publish(channelIndex, oe, uSet, iSet);
				}

				if (oe && callPublish) {
					publish(channelIndex, channel_dispatcher::getUMonLast(channel), channel_dispatcher::getIMonLast(channel));
				}
			}
        }

#if defined(EEZ_PLATFORM_SIMULATOR)
        mqtt_sync(&g_client);
#endif
    }
}

bool connect(const char *addr, int port, const char *user, const char *pass, int16_t *err) {
    if (g_connectionState != CONNECTION_STATE_IDLE && g_connectionState == CONNECTION_STATE_ERROR) {
        if (err != nullptr) {
            // TODO find better error
            *err = SCPI_ERROR_EXECUTION_ERROR;
        }
        return false;
    }

    strcpy(g_addr, addr);
    
    g_port = port;
    
    if (user) {
        strcpy(g_user, user);
    } else {
        g_user[0] = 0;
    }
    
    if (pass) {
        strcpy(g_pass, pass);
    } else {
        g_pass[0] = 0;
    }

    g_connectionState = CONNECTION_STATE_CONNECT;
    return true;
}

bool disconnect(int16_t *err) {
    if (g_connectionState != CONNECTION_STATE_CONNECTED && g_connectionState != CONNECTION_STATE_CONNECTING && g_connectionState != CONNECTION_STATE_DNS_IN_PROGRESS) {
        if (err != nullptr) {
            // TODO find better error
            *err = SCPI_ERROR_EXECUTION_ERROR;
        }
        return false;
    }

    g_connectionState = CONNECTION_STATE_DISCONNECT;
    return true;
}

} // mqtt
} // eez

#endif // OPTION_ETHERNET
