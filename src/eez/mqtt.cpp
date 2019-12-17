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
#include <eez/system.h>
#include <eez/modules/psu/psu.h>
#include <eez/modules/psu/trigger.h>
#include <eez/modules/psu/ethernet.h>
#include <eez/modules/psu/channel_dispatcher.h>

namespace eez {

using namespace psu;

namespace mqtt {

static const uint32_t RECONNECT_AFTER_ERROR_MS = 1000;

static const size_t MAX_PUB_TOPIC_LENGTH = 50;
static const char *PUB_TOPIC_OE = "%s/ch/%d/oe";
static const char *PUB_TOPIC_U_SET = "%s/ch/%d/uset";
static const char *PUB_TOPIC_I_SET = "%s/ch/%d/iset";
static const char *PUB_TOPIC_U_MON = "%s/ch/%d/umon";
static const char *PUB_TOPIC_I_MON = "%s/ch/%d/imon";

static const size_t MAX_SUB_TOPIC_LENGTH = 50;
static const char *SUB_TOPIC = "%s/ch/+/+"; // for example: ch/1/set/oe, ch/1/set/u, ch/1/set/i

static const size_t MAX_PAYLOAD_LENGTH = 100;

ConnectionState g_connectionState = CONNECTION_STATE_IDLE;
uint32_t g_connectionStateChangedTickCount;

static struct {
    int oe;

    float uSet;
    uint32_t g_uSetTick;

    float iSet;
    uint32_t g_iSetTick;

    float uMon;
    uint32_t g_uMonTick;

    float iMon;
    uint32_t g_iMonTick;
} g_channelStates[CH_MAX];

static uint8_t g_lastChannelIndex = 0;
static uint8_t g_lastValueIndex = 0;

void setState(ConnectionState connectionState);

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
static mqtt_client_t g_client;
static const size_t MAX_TOPIC_LEN = 128;
static char g_topic[MAX_TOPIC_LEN + 1];
static const size_t MAX_PAYLOAD_LEN = 128;
static char g_payload[MAX_PAYLOAD_LEN + 1];
static size_t g_payloadLen;

static void dnsFoundCallback(const char* hostname, const ip_addr_t *ipaddr, void *arg) {
    if (ipaddr != NULL) {
        g_ipaddr = *ipaddr;
        setState(CONNECTION_STATE_DNS_FOUND);
    } else {
        setState(CONNECTION_STATE_ERROR);
        DebugTrace("mqtt dns error: server not found\n");
    }
}

static void connectCallback(mqtt_client_t *client, void *arg, mqtt_connection_status_t status) {
	if (g_connectionState == CONNECTION_STATE_CONNECTING) {
		if (status == MQTT_CONNECT_ACCEPTED) {
			setState(CONNECTION_STATE_CONNECTED);
		} else {
			setState(CONNECTION_STATE_ERROR);
			DebugTrace("mqtt connect error: %d\n", (int)status);
		}
	}
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
static uint8_t g_sendbuf[4096]; /* sendbuf should be large enough to hold multiple whole mqtt messages */
static uint8_t g_recvbuf[2048]; /* recvbuf should be large enough any whole mqtt message expected to be received */
static struct mqtt_client g_client; /* instantiate the client */

void incomingPublishCallback(void** unused, struct mqtt_response_publish *published) {
    onIncomingPublish((const char *)published->topic_name, (const char *)published->application_message);
}
#endif

bool publish(char *topic, char *payload, bool retain) {
#if defined(EEZ_PLATFORM_STM32)
    err_t result = mqtt_publish(&g_client, topic, payload, strlen(payload), 0, retain ? 1 : 0, requestCallback, nullptr);
    if (result != ERR_OK) {
        if (result != ERR_MEM) {
            DebugTrace("mqtt publish error: %d\n", (int)result);
            if (result == ERR_CONN) {
                reconnect();
            }
        }
        return false;
    }
#endif

#if defined(EEZ_PLATFORM_SIMULATOR)
    mqtt_publish(&g_client, topic, payload, strlen(payload), MQTT_PUBLISH_QOS_0 | (retain ? MQTT_PUBLISH_RETAIN : 0));
    if (g_client.error != MQTT_OK) {
        DebugTrace("mqtt error: %s\n", mqtt_error_str(g_client.error));
        return false;
    }
#endif
    
    return true;
}

bool publish(int channelIndex, const char *pubTopic, int value) {
    char topic[MAX_PUB_TOPIC_LENGTH + 1];
    sprintf(topic, pubTopic, persist_conf::devConf.ethernetHostName, channelIndex + 1);

    char payload[MAX_PAYLOAD_LENGTH + 1];
    sprintf(payload, "%d", value);

    return publish(topic, payload, true);
}

bool publish(int channelIndex, const char *pubTopic, float value) {
    char topic[MAX_PUB_TOPIC_LENGTH + 1];
    sprintf(topic, pubTopic, persist_conf::devConf.ethernetHostName, channelIndex + 1);

    char payload[MAX_PAYLOAD_LENGTH + 1];
    sprintf(payload, "%g", value);

    return publish(topic, payload, false);
}

const char *getSubTopic() {
    static char g_subTopic[MAX_SUB_TOPIC_LENGTH + 1] = { 0 };
    if (!g_subTopic[0]) {
        sprintf(g_subTopic, SUB_TOPIC, persist_conf::devConf.ethernetHostName);
    }
    return g_subTopic;
}

const char *getClientId() {
    static char g_clientId[50 + 1] = { 0 };

    if (!g_clientId[0]) {
#if defined(EEZ_PLATFORM_STM32)
        sprintf(g_clientId, "BB3_STM32_%s", getSerialNumber());
#endif

#if defined(EEZ_PLATFORM_SIMULATOR)
        static const char *CLIENT_ID = "BB3_Simulator";
        sprintf(g_clientId, "BB3_Simulator_%s", getSerialNumber());
#endif
    }
    
    return g_clientId;
}

void setState(ConnectionState connectionState) {
    if (connectionState == CONNECTION_STATE_CONNECTED) {
#if defined(EEZ_PLATFORM_STM32)
        mqtt_set_inpub_callback(&g_client, incomingPublishCallback, incomingDataCallback, nullptr);
        mqtt_subscribe(&g_client, getSubTopic(), 0, requestCallback, nullptr);
#endif

#if defined(EEZ_PLATFORM_SIMULATOR)
        mqtt_subscribe(&g_client, getSubTopic(), 0);
#endif

        for(int i = 0; i < CH_NUM; i++) {
            g_channelStates[i].oe = -1;
            g_channelStates[i].uSet = NAN;
            g_channelStates[i].iSet = NAN;
            g_channelStates[i].uMon = NAN;
            g_channelStates[i].iMon = NAN;
        }

        g_lastChannelIndex = 0;
        g_lastValueIndex = 0;
    }

    g_connectionState = connectionState;
    g_connectionStateChangedTickCount = millis();
}

void tick(uint32_t tickCount) {
    if (ethernet::g_testResult != TEST_OK) {
        // pass
    }

    else if (g_connectionState == CONNECTION_STATE_IDLE) {
        if (persist_conf::devConf.mqttEnabled) {
            setState(CONNECTION_STATE_CONNECT);
        }
    }

    else if (g_connectionState == CONNECTION_STATE_CONNECT) {
#if defined(EEZ_PLATFORM_STM32)
        ip_addr_set_zero(&g_ipaddr);
        ip_addr_t ipaddr;
        err_t err = dns_gethostbyname(persist_conf::devConf.mqttHost, &ipaddr, dnsFoundCallback, NULL);
        if (err == ERR_OK) {
            setState(CONNECTION_STATE_DNS_FOUND);
            g_ipaddr = ipaddr;
        } else if (err == ERR_INPROGRESS) {
            setState(CONNECTION_STATE_DNS_IN_PROGRESS);
        } else {
            setState(CONNECTION_STATE_ERROR);
            DebugTrace("mqtt dns error: %d\n", (int)err);
        }
#endif

#if defined(EEZ_PLATFORM_SIMULATOR)
        char port[16];
        sprintf(port, "%d", persist_conf::devConf.mqttPort);
        g_sockfd = open_nb_socket(persist_conf::devConf.mqttHost, port);
        if (g_sockfd != -1) {
            /* initialize the client */
            mqtt_init(&g_client, g_sockfd, g_sendbuf, sizeof(g_sendbuf), g_recvbuf, sizeof(g_recvbuf), incomingPublishCallback);

            /* Ensure we have a clean session */
            uint8_t connect_flags = MQTT_CONNECT_CLEAN_SESSION;

            /* Send connection request to the broker. */
            mqtt_connect(&g_client, CLIENT_ID, NULL, NULL, 0, persist_conf::devConf.mqttUsername, persist_conf::devConf.mqttPassword, connect_flags, 400);

            /* check that we don't have any errors */
            if (g_client.error == MQTT_OK) {
                setState(CONNECTION_STATE_CONNECTED);
            } else {
                setState(CONNECTION_STATE_ERROR);
                DebugTrace("mqtt error: %s\n", mqtt_error_str(g_client.error));
            }
        } else {
            setState(CONNECTION_STATE_ERROR);
            DebugTrace("mqtt error: failed to open socket\n");
        }
#endif
    }

    else if (g_connectionState == CONNECTION_STATE_DISCONNECT || g_connectionState == CONNECTION_STATE_RECONNECT) {
#if defined(EEZ_PLATFORM_STM32)
        if (mqtt_client_is_connected(&g_client)) {
            mqtt_disconnect(&g_client);
        }
#endif

#if defined(EEZ_PLATFORM_SIMULATOR)
        mqtt_disconnect(&g_client);
#endif

        setState(CONNECTION_STATE_IDLE);

        if (g_connectionState == CONNECTION_STATE_RECONNECT) {
            setState(CONNECTION_STATE_CONNECT);
        }
    }

    else if (g_connectionState == CONNECTION_STATE_ERROR) {
        if (persist_conf::devConf.mqttEnabled) {
            if (millis() - g_connectionStateChangedTickCount > RECONNECT_AFTER_ERROR_MS) {
                setState(CONNECTION_STATE_CONNECT);
            }
        }
    }

#if defined(EEZ_PLATFORM_STM32)
    else if (g_connectionState == CONNECTION_STATE_DNS_FOUND) {
        mqtt_connect_client_info_t clientInfo;
        clientInfo.client_id = getClientId();
        clientInfo.client_user = persist_conf::devConf.mqttUsername;
        clientInfo.client_pass = persist_conf::devConf.mqttPassword;
        clientInfo.keep_alive = 60; // seconds
        clientInfo.will_topic = nullptr; // not used

        memset(&g_client, 0, sizeof(g_client));

        err_t result = mqtt_client_connect(&g_client, &g_ipaddr, persist_conf::devConf.mqttPort, connectCallback, nullptr, &clientInfo);
        if (result == ERR_OK) {
            setState(CONNECTION_STATE_CONNECTING);
        } else {
            setState(CONNECTION_STATE_ERROR);
            DebugTrace("mqtt connect error: %d\n", (int)result);
        }
    }
#endif

    else if (g_connectionState == CONNECTION_STATE_CONNECTED) {
		uint8_t lastChannelIndexAtStart = g_lastChannelIndex;
		uint8_t lastValueIndexAtStart = g_lastValueIndex;

		do {
			uint8_t channelIndex = g_lastChannelIndex;
			Channel &channel = Channel::get(channelIndex);

			uint32_t period = (uint32_t)roundf(persist_conf::devConf.mqttPeriod * 1000000);

			if (g_lastValueIndex == 0) {
				int oe = channel.isOutputEnabled() ? 1 : 0;
				if (oe != g_channelStates[channelIndex].oe) {
					if (!publish(channelIndex, PUB_TOPIC_OE, oe)) {
						break;
					}
					g_channelStates[channelIndex].oe = oe;
				}
			} else if (g_lastValueIndex == 1) {
				float uSet = channel_dispatcher::getUSet(channel);
				if ((isNaN(g_channelStates[channelIndex].uSet) || uSet != g_channelStates[channelIndex].uSet) && (tickCount - g_channelStates[channelIndex].g_uSetTick) >= period) {
					if (!publish(channelIndex, PUB_TOPIC_U_SET, uSet)) {
						break;
					}
					g_channelStates[channelIndex].uSet = uSet;
					g_channelStates[channelIndex].g_uSetTick = tickCount;
				}
			} else if (g_lastValueIndex == 2) {
				float iSet = channel_dispatcher::getISet(channel);
				if ((isNaN(g_channelStates[channelIndex].iSet) || iSet != g_channelStates[channelIndex].iSet) && (tickCount - g_channelStates[channelIndex].g_iSetTick) >= period) {
					if (!publish(channelIndex, PUB_TOPIC_I_SET, iSet)) {
						break;
					}
					g_channelStates[channelIndex].iSet = iSet;
					g_channelStates[channelIndex].g_iSetTick = tickCount;
				}
			} else if (g_lastValueIndex == 3) {
				float uMon = channel_dispatcher::getUMonLast(channel);
				if ((isNaN(g_channelStates[channelIndex].uMon) || uMon != g_channelStates[channelIndex].uMon) && (tickCount - g_channelStates[channelIndex].g_uMonTick) >= period) {
					if (!publish(channelIndex, PUB_TOPIC_U_MON, uMon)) {
						break;
					}
					g_channelStates[channelIndex].uMon = uMon;
					g_channelStates[channelIndex].g_uMonTick = tickCount;
				}
			} else {
				float iMon = channel_dispatcher::getIMonLast(channel);
				if ((isNaN(g_channelStates[channelIndex].iMon) || iMon != g_channelStates[channelIndex].iMon) && (tickCount - g_channelStates[channelIndex].g_iMonTick) >= period) {
					if (!publish(channelIndex, PUB_TOPIC_I_MON, iMon)) {
						break;
					}
					g_channelStates[channelIndex].iMon = iMon;
					g_channelStates[channelIndex].g_iMonTick = tickCount;
				}
			}

			if (++g_lastValueIndex == 5) {
				g_lastValueIndex = 0;
				if (++g_lastChannelIndex == CH_NUM) {
					g_lastChannelIndex = 0;
				}
			}
		} while (g_lastChannelIndex != lastChannelIndexAtStart || g_lastValueIndex != lastValueIndexAtStart);

#if defined(EEZ_PLATFORM_SIMULATOR)
		mqtt_sync(&g_client);
#endif
    }
}

void reconnect() {
    if (persist_conf::devConf.mqttEnabled) {
        if (g_connectionState == CONNECTION_STATE_IDLE || g_connectionState == CONNECTION_STATE_ERROR) {
            setState(CONNECTION_STATE_CONNECT);
        } else {
            setState(CONNECTION_STATE_RECONNECT);
        }
    } else {
        if (g_connectionState != CONNECTION_STATE_IDLE && g_connectionState != CONNECTION_STATE_ERROR) {
            setState(CONNECTION_STATE_DISCONNECT);
        }
    }
}

} // mqtt
} // eez

#endif // OPTION_ETHERNET
