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
#include <tcpip.h>
#include <api.h>
#include <mqtt.h>
#include <mqtt_priv.h>
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

#include <eez/firmware.h>
#include <eez/debug.h>
#include <eez/mqtt.h>
#include <eez/system.h>

#include <eez/scpi/scpi.h>

#include <eez/modules/psu/psu.h>
#include <eez/modules/psu/trigger.h>
#include <eez/modules/psu/ethernet.h>
#include <eez/modules/psu/channel_dispatcher.h>
#include <eez/modules/psu/event_queue.h>
#include <eez/modules/psu/profile.h>
#include <eez/modules/psu/temperature.h>
#include <eez/modules/psu/ontime.h>
#include <eez/modules/psu/trigger.h>
#include <eez/modules/psu/gui/psu.h>

#include <eez/modules/mcu/battery.h>

#if OPTION_FAN
#include <eez/modules/aux_ps/fan.h>
#endif

namespace eez {

using namespace psu;

namespace mqtt {

static const uint32_t RECONNECT_AFTER_ERROR_MS = 10000;
static const uint32_t CONF_DNS_TIMEOUT_MS = 10000;
static const uint32_t CONF_STARTING_TIMEOUT_MS = 2000; // wait a acouple of seconds, after ethernet is ready, before starting MQTT connect 

static const size_t MAX_PUB_TOPIC_LENGTH = 128;

static const char *PUB_TOPIC_SYSTEM_POW = "%s/system/pow";
static const char *PUB_TOPIC_SYSTEM_EVENT = "%s/system/event";
static const char *PUB_TOPIC_SYSTEM_BATTERY = "%s/system/vbat";
static const char *PUB_TOPIC_SYSTEM_AUXTEMP = "%s/system/auxtemp";
static const char *PUB_TOPIC_SYSTEM_TOTAL_ONTIME = "%s/system/total_ontime";
static const char *PUB_TOPIC_SYSTEM_LAST_ONTIME = "%s/system/last_ontime";
static const char *PUB_TOPIC_SYSTEM_FAN_STATUS = "%s/system/fan";

static const char *PUB_TOPIC_DCPSUPPLY_MODEL = "%s/dcpsupply/ch/%d/model";
static const char *PUB_TOPIC_DCPSUPPLY_OE = "%s/dcpsupply/ch/%d/oe";
static const char *PUB_TOPIC_DCPSUPPLY_U_SET = "%s/dcpsupply/ch/%d/uset";
static const char *PUB_TOPIC_DCPSUPPLY_I_SET = "%s/dcpsupply/ch/%d/iset";
static const char *PUB_TOPIC_DCPSUPPLY_U_MON = "%s/dcpsupply/ch/%d/umon";
static const char *PUB_TOPIC_DCPSUPPLY_I_MON = "%s/dcpsupply/ch/%d/imon";
static const char *PUB_TOPIC_DCPSUPPLY_TEMP = "%s/dcpsupply/ch/%d/temp";
static const char *PUB_TOPIC_DCPSUPPLY_TOTAL_ONTIME = "%s/dcpsupply/ch/%d/total_ontime";
static const char *PUB_TOPIC_DCPSUPPLY_LAST_ONTIME = "%s/dcpsupply/ch/%d/last_ontime";

static const size_t MAX_SUB_TOPIC_LENGTH = 50;

static const char *SUB_TOPIC_SYSTEM_PATTERN = "%s/system/exec/#";
static const char *SUB_TOPIC_DCPSUPPLY_PATTERN = "%s/dcpsupply/ch/+/set/+";

static const size_t MAX_PAYLOAD_LENGTH = 100;

static const size_t MAX_TOPIC_LEN = 128;
static char g_topic[MAX_TOPIC_LEN + 1];
static const size_t MAX_PAYLOAD_LEN = 128;
static char g_payload[MAX_PAYLOAD_LEN + 1];

ConnectionState g_connectionState = CONNECTION_STATE_ETHERNET_NOT_READY;
uint32_t g_connectionStateChangedTickCount;
uint32_t g_ethernetReadyTime;

static int g_powState = -1;
static float g_battery = NAN;
static float g_auxTemperature = NAN;
static uint32_t g_auxTemperatureTick;
static uint32_t g_totalOnTime = 0xFFFFFFFF;
static uint32_t g_lastOnTime = 0xFFFFFFFF;

#if OPTION_FAN
static TestResult g_fanTestResult;
static int g_fanRpm;
static uint32_t g_fanStatusTick;
#endif

static const size_t EVENT_QUEUE_SIZE = 10;
struct {
    int16_t buffer[EVENT_QUEUE_SIZE];
    int head;
    int tail;
    bool full;
} g_eventQueue;

static struct {
    bool modelPublished;

    int oe;

    float uSet;
    uint32_t uSetTick;

    float iSet;
    uint32_t iSetTick;

    uint32_t uMonTick;

    uint32_t iMonTick;

    float temperature;
    uint32_t temperatureTick;

    uint32_t totalOnTime;
    uint32_t lastOnTime;
} g_channelStates[CH_MAX];

static uint8_t g_lastChannelIndex = 0;
static uint8_t g_lastValueIndex = 0;
static bool g_publishing;

enum {
    EEZ_MQTT_ERROR_NONE,
    EEZ_MQTT_ERROR_DNS,
    EEZ_MQTT_ERROR_CONNECT,
    EEZ_MQTT_ERROR_PUBLISH,
    EEZ_MQTT_ERROR_SOCKET
} g_lastError;

void setState(ConnectionState connectionState);

bool matchSeparator(const char **pTopic) {
    for (const char *p = *pTopic; *p; p++) {
        if (*p == '/') {
            *pTopic = p + 1;
            return true;
        }
    }
    return false;
}

bool match(const char **pTopic, const char *pattern) {
    auto length = strlen(pattern);
    if (strncmp(*pTopic, pattern, length) != 0) {
        return false;
    }
    *pTopic += length;
    return true;
}

void onIncomingPublish(const char *topic, const char *payload) {
    const char *p = topic;
    
    if (!matchSeparator(&p)) {
        return;
    }

    if (match(&p, "system/exec/")) {
        if (strcmp(p, "restart") == 0) {
            restart();
        } else if (strcmp(p, "power") == 0) {
            char *endptr;
            int powerUp = strtol(payload, &endptr, 10);
            if (endptr > payload) {
                if (powerUp && !isPowerUp()) {
                    changePowerState(1);
                } else if (!powerUp && isPowerUp()) {
                    standBy();
                }
            }
        } else if (strcmp(p, "display/window/text") == 0) {
            int length = strlen(payload);
            if (length > 0) {
                psu::gui::setTextMessage(payload, length);
            }
        } else if (strcmp(p, "display/window/text/clear") == 0) {
            psu::gui::clearTextMessage();
        } else if (strcmp(p, "profile/recall") == 0) {
            char *endptr;
            int location = strtol(payload, &endptr, 10);
            if (endptr > payload) {
                using namespace eez::scpi;
                osMessagePut(g_scpiMessageQueueId, SCPI_QUEUE_MESSAGE(SCPI_QUEUE_MESSAGE_TARGET_NONE, SCPI_QUEUE_MESSAGE_TYPE_RECALL_PROFILE, location), 0);
            }
        } else if (strcmp(p, "initiate") == 0) {
            trigger::initiate();
        } else if (strcmp(p, "abort") == 0) {
            trigger::abort();
        }
    } else if (match(&p, "dcpsupply/ch/")) {
        char *endptr;
        int channelIndex = strtol(p, &endptr, 10);

        if (endptr > p && *endptr == '/' && channelIndex > 0 && channelIndex <= CH_NUM) {
            channelIndex--;

            p = endptr + 1;

            if (!match(&p, "set/")) {
                return;
            }

            Channel &channel = Channel::get(channelIndex);

            if (strcmp(p, "oe") == 0) {
                int oe = strtol(payload, &endptr, 10);
                if (endptr > payload) {
                    channel_dispatcher::outputEnable(1 << channel.channelIndex, oe != 0, nullptr);
                }
            } else if (strcmp(p, "u") == 0) {
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
            } else if (strcmp(p, "i") == 0) {
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
}    

#if defined(EEZ_PLATFORM_STM32)
static ip_addr_t g_ipaddr;
static mqtt_client_t g_client;
static size_t g_payloadLen;
static uint32_t g_dnsStartedTick;

static void dnsFoundCallback(const char* hostname, const ip_addr_t *ipaddr, void *arg) {
    if (ipaddr != NULL) {
        g_ipaddr = *ipaddr;
        setState(CONNECTION_STATE_DNS_FOUND);
    } else {
        setState(CONNECTION_STATE_ERROR);
        if (g_lastError != EEZ_MQTT_ERROR_DNS) {
            g_lastError = EEZ_MQTT_ERROR_DNS;
            DebugTrace("mqtt dns error: server not found\n");
        }
    }
}

static void connectCallback(mqtt_client_t *client, void *arg, mqtt_connection_status_t status) {
    if (status == MQTT_CONNECT_ACCEPTED) {
        setState(CONNECTION_STATE_CONNECTED);
    } else {
        setState(CONNECTION_STATE_ERROR);
        if (g_lastError != EEZ_MQTT_ERROR_CONNECT) {
            g_lastError = EEZ_MQTT_ERROR_CONNECT;
            DebugTrace("mqtt connect error: %d\n", (int)status);
        }
    }
}

static void requestCallback(void *arg, err_t err) {
	g_publishing = false;
}

void incomingPublishCallback(void *arg, const char *topic, u32_t tot_len) {
    // DebugTrace("Incoming publish: %s, %d\n", topic, (int)tot_len);
    size_t topicLen = MIN(strlen(topic), MAX_TOPIC_LEN);
    strncpy(g_topic, topic, topicLen);
    g_topic[topicLen] = 0;

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
    size_t topicLen = MIN(published->topic_name_size, MAX_TOPIC_LEN);
    strncpy(g_topic, (const char *)published->topic_name, topicLen);
    g_topic[topicLen] = 0;

    size_t payloadLen = MIN(published->application_message_size, MAX_PAYLOAD_LEN);
    if (payloadLen > 0) {
        strncpy(g_payload, (const char *)published->application_message, payloadLen);
    }
    g_payload[payloadLen] = 0;

    onIncomingPublish(g_topic, g_payload);
}
#endif

bool publish(char *topic, char *payload, bool retain) {
#if defined(EEZ_PLATFORM_STM32)
	g_publishing = true;
    LOCK_TCPIP_CORE();
    err_t result = mqtt_publish(&g_client, topic, payload, strlen(payload), 0, retain ? 1 : 0, requestCallback, nullptr);
    UNLOCK_TCPIP_CORE();
    if (result != ERR_OK) {
    	g_publishing = false;
        if (result != ERR_MEM) {
            if (g_lastError != EEZ_MQTT_ERROR_PUBLISH) {
                g_lastError = EEZ_MQTT_ERROR_PUBLISH;
                DebugTrace("mqtt publish error: %d\n", (int)result);
            }

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
        if (g_lastError != EEZ_MQTT_ERROR_PUBLISH) {
            g_lastError = EEZ_MQTT_ERROR_PUBLISH;
            DebugTrace("mqtt publish error: %s\n", mqtt_error_str(g_client.error));
        }
        reconnect();
        return false;
    }
#endif
    
    return true;
}

bool publish(const char *pubTopic, int value, bool retain) {
    char topic[MAX_PUB_TOPIC_LENGTH + 1];
    sprintf(topic, pubTopic, persist_conf::devConf.ethernetHostName);

    char payload[MAX_PAYLOAD_LENGTH + 1];
    sprintf(payload, "%d", value);

    return publish(topic, payload, retain);
}

bool publishOnTimeCounter(const char *pubTopic, uint32_t value, bool retain) {
    char topic[MAX_PUB_TOPIC_LENGTH + 1];
    sprintf(topic, pubTopic, persist_conf::devConf.ethernetHostName);

    char payload[MAX_PAYLOAD_LENGTH + 1];
    ontime::counterToString(payload, MAX_PAYLOAD_LENGTH, value);

    return publish(topic, payload, retain);
}

bool publish(const char *pubTopic, float value, bool retain) {
    char topic[MAX_PUB_TOPIC_LENGTH + 1];
    sprintf(topic, pubTopic, persist_conf::devConf.ethernetHostName);

    char payload[MAX_PAYLOAD_LENGTH + 1];
    sprintf(payload, "%g", value);

    return publish(topic, payload, retain);
}

bool publishEvent(int16_t eventId, bool retain) {
    char topic[MAX_PUB_TOPIC_LENGTH + 1];
    sprintf(topic, PUB_TOPIC_SYSTEM_EVENT, persist_conf::devConf.ethernetHostName);

    char payload[MAX_PAYLOAD_LENGTH + 1];
    snprintf(payload, MAX_PAYLOAD_LENGTH, "[%d, \"%s\", \"%s\"]", (int)eventId, event_queue::getEventTypeName(eventId), event_queue::getEventMessage(eventId));
    payload[MAX_PAYLOAD_LENGTH] = 0;

    return publish(topic, payload, retain);
}

bool publishFanStatus(const char *pubTopic, TestResult fanTestResult, int rpm, bool retain) {
    char topic[MAX_PUB_TOPIC_LENGTH + 1];
    sprintf(topic, pubTopic, persist_conf::devConf.ethernetHostName);

    char payload[MAX_PAYLOAD_LENGTH + 1];

    if (fanTestResult == TEST_FAILED || fanTestResult == TEST_WARNING) {
        strcpy(payload, "Fault");
    } else if (fanTestResult == TEST_OK) {
        snprintf(payload, MAX_PAYLOAD_LENGTH, "%d rpm", rpm);
    } else if (fanTestResult == TEST_NONE) {
        strcpy(payload, "Testing...");
    } else {
        strcpy(payload, "Not installed");
    }

    payload[MAX_PAYLOAD_LENGTH] = 0;

    return publish(topic, payload, retain);
}

bool publish(int channelIndex, const char *pubTopic, int value, bool retain) {
    char topic[MAX_PUB_TOPIC_LENGTH + 1];
    sprintf(topic, pubTopic, persist_conf::devConf.ethernetHostName, channelIndex + 1);

    char payload[MAX_PAYLOAD_LENGTH + 1];
    sprintf(payload, "%d", value);

    return publish(topic, payload, retain);
}

bool publish(int channelIndex, const char *pubTopic, float value, bool retain) {
    char topic[MAX_PUB_TOPIC_LENGTH + 1];
    sprintf(topic, pubTopic, persist_conf::devConf.ethernetHostName, channelIndex + 1);

    char payload[MAX_PAYLOAD_LENGTH + 1];
    sprintf(payload, "%g", value);

    return publish(topic, payload, retain);
}

bool publish(int channelIndex, const char *pubTopic, char *payload, bool retain) {
    char topic[MAX_PUB_TOPIC_LENGTH + 1];
    sprintf(topic, pubTopic, persist_conf::devConf.ethernetHostName, channelIndex + 1);

    return publish(topic, payload, retain);
}

bool publishOnTimeCounter(int channelIndex, const char *pubTopic, uint32_t value, bool retain) {
    char topic[MAX_PUB_TOPIC_LENGTH + 1];
    sprintf(topic, pubTopic, persist_conf::devConf.ethernetHostName, channelIndex + 1);

    char payload[MAX_PAYLOAD_LENGTH + 1];
    ontime::counterToString(payload, MAX_PAYLOAD_LENGTH, value);

    return publish(topic, payload, retain);
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

bool peekEvent(int16_t &eventId);
bool getEvent(int16_t &eventId);

void setState(ConnectionState connectionState) {
    if (connectionState == CONNECTION_STATE_CONNECTED) {
        if (g_lastError != EEZ_MQTT_ERROR_NONE) {
            DebugTrace("mqtt connected\n");
        }
        g_lastError = EEZ_MQTT_ERROR_NONE;

        char subTopicSystem[MAX_SUB_TOPIC_LENGTH + 1];
        sprintf(subTopicSystem, SUB_TOPIC_SYSTEM_PATTERN, persist_conf::devConf.ethernetHostName);

        char subTopicDcpsupply[MAX_SUB_TOPIC_LENGTH + 1];
        sprintf(subTopicDcpsupply, SUB_TOPIC_DCPSUPPLY_PATTERN, persist_conf::devConf.ethernetHostName);

#if defined(EEZ_PLATFORM_STM32)
        mqtt_set_inpub_callback(&g_client, incomingPublishCallback, incomingDataCallback, nullptr);
        mqtt_subscribe(&g_client, subTopicSystem, 0, requestCallback, nullptr);
        mqtt_subscribe(&g_client, subTopicDcpsupply, 0, requestCallback, nullptr);
#endif

#if defined(EEZ_PLATFORM_SIMULATOR)
        mqtt_subscribe(&g_client, subTopicSystem, 0);
        mqtt_subscribe(&g_client, subTopicDcpsupply, 0);
#endif

        for(int i = 0; i < CH_NUM; i++) {
            g_channelStates[i].modelPublished = false;
            g_channelStates[i].oe = -1;
            g_channelStates[i].uSet = NAN;
            g_channelStates[i].iSet = NAN;
            g_channelStates[i].temperature = NAN;
            g_channelStates[i].totalOnTime = 0xFFFFFFFF;
            g_channelStates[i].lastOnTime = 0xFFFFFFFF;
        }

        g_lastChannelIndex = 0;
        g_lastValueIndex = 0;
    }

    g_connectionState = connectionState;
    g_connectionStateChangedTickCount = millis();
}

void tick() {
    uint32_t tickCount = millis();

    if (ethernet::g_testResult != TEST_OK) {
        // pass
    }

    else if (g_connectionState == CONNECTION_STATE_CONNECTED && !g_publishing) {
        uint32_t period = (uint32_t)roundf(persist_conf::devConf.mqttPeriod * 1000);

        // publish power state
        int powState = isPowerUp() ? 1 : 0;
        if (powState != g_powState) {
            if (publish(PUB_TOPIC_SYSTEM_POW, powState, true)) {
                g_powState = powState;
                if (g_publishing) {
                    return;
                }
            }
        }

        // publish events from event view
        int16_t eventId;
        if (peekEvent(eventId)) {
            if (publishEvent(eventId, true)) {
                getEvent(eventId);
                if (g_publishing) {
                    return;
                }
            }
        }

        // publish battery
        if (mcu::battery::g_battery != g_battery) {
            if (publish(PUB_TOPIC_SYSTEM_BATTERY, mcu::battery::g_battery, true)) {
                g_battery = mcu::battery::g_battery;
                if (g_publishing) {
                    return;
                }
            }
        }

        // publish aux temperature
        if ((tickCount - g_auxTemperatureTick) >= period) {
            float temperature;
            temperature::TempSensorTemperature &tempSensor = temperature::sensors[temp_sensor::AUX];
            if (tempSensor.isInstalled() && tempSensor.isTestOK()) {
                temperature = tempSensor.temperature;
            } else {
                temperature = NAN;
            }
            if (temperature != g_auxTemperature) {
                if (publish(PUB_TOPIC_SYSTEM_AUXTEMP, temperature, true)) {
                    g_auxTemperature = temperature;
                    g_auxTemperatureTick = tickCount;
                    if (g_publishing) {
                        return;
                    }
                }
            }
        }

#if OPTION_FAN
        // publish fan status
        if ((tickCount - g_fanStatusTick) >= period) {
            TestResult fanTestResult = aux_ps::fan::g_testResult;
            int fanRpm = aux_ps::fan::g_rpm;

            if (fanTestResult != g_fanTestResult || fanRpm != g_fanRpm) {
                if (publishFanStatus(PUB_TOPIC_SYSTEM_FAN_STATUS, fanTestResult, fanRpm, true)) {
                    g_fanTestResult = fanTestResult;
                    g_fanRpm = fanRpm;
                    g_fanStatusTick = tickCount;
                    if (g_publishing) {
                        return;
                    }
                }
            }
        }
#endif

        // publish total on-time counter
        uint32_t totalOnTime = ontime::g_mcuCounter.getTotalTime();
        if (totalOnTime != g_totalOnTime) {
            if (publishOnTimeCounter(PUB_TOPIC_SYSTEM_TOTAL_ONTIME, totalOnTime, true)) {
                g_totalOnTime = totalOnTime;
                if (g_publishing) {
                    return;
                }
            }
        }

        // publish last on-time counter
        uint32_t lastOnTime = ontime::g_mcuCounter.getLastTime();
        if (lastOnTime != g_lastOnTime) {
            if (publishOnTimeCounter(PUB_TOPIC_SYSTEM_LAST_ONTIME, lastOnTime, true)) {
                g_lastOnTime = lastOnTime;
                if (g_publishing) {
                    return;
                }
            }
        }

        if (CH_NUM > 0) {
            // publish channel state (oe, u_mon, i_mon, u_set, i_set)
            uint8_t channelIndex = g_lastChannelIndex;
            Channel &channel = Channel::get(channelIndex);

            int oe = channel.isOutputEnabled() ? 1 : 0;

            if (g_lastValueIndex == 0) {
                if (!g_channelStates[channelIndex].modelPublished) {
                    char moduleInfo[50];
                    auto &slot = g_slots[channel.slotIndex];
                    sprintf(moduleInfo, "%s_R%dB%d", slot.moduleInfo->moduleName, (int)(slot.moduleRevision >> 8), (int)(slot.moduleRevision & 0xFF));
                    if (publish(channelIndex, PUB_TOPIC_DCPSUPPLY_MODEL, moduleInfo, true)) {
                        g_channelStates[channelIndex].modelPublished = true;
                    }
                }

                if (oe != g_channelStates[channelIndex].oe) {
                    if (publish(channelIndex, PUB_TOPIC_DCPSUPPLY_OE, oe, true)) {
                        g_channelStates[channelIndex].oe = oe;
                    }
                }
            } else {
                if (g_lastValueIndex == 1) {
                    if (oe && (tickCount - g_channelStates[channelIndex].uMonTick) >= period) {
                        float uMon = channel_dispatcher::getUMonLast(channel);
                        if (publish(channelIndex, PUB_TOPIC_DCPSUPPLY_U_MON, uMon, true)) {
                            g_channelStates[channelIndex].uMonTick = tickCount;
                        }
                    }
                } else if (g_lastValueIndex == 2) {
                    if (oe && (tickCount - g_channelStates[channelIndex].iMonTick) >= period) {
                        float iMon = channel_dispatcher::getIMonLast(channel);
                        if (publish(channelIndex, PUB_TOPIC_DCPSUPPLY_I_MON, iMon, true)) {
                            g_channelStates[channelIndex].iMonTick = tickCount;
                        }
                    }
                } else if (g_lastValueIndex == 3) {
                    if ((tickCount - g_channelStates[channelIndex].uSetTick) >= period) {
                        float uSet = channel_dispatcher::getUSet(channel);
                        if (isNaN(g_channelStates[channelIndex].uSet) || uSet != g_channelStates[channelIndex].uSet) {
                            if (publish(channelIndex, PUB_TOPIC_DCPSUPPLY_U_SET, uSet, true)) {
                                g_channelStates[channelIndex].uSet = uSet;
                                g_channelStates[channelIndex].uSetTick = tickCount;
                            }
                        }
                    }
                } else if (g_lastValueIndex == 4) {
                    if ((tickCount - g_channelStates[channelIndex].iSetTick) >= period) {
                        float iSet = channel_dispatcher::getISet(channel);
                        if (isNaN(g_channelStates[channelIndex].iSet) || iSet != g_channelStates[channelIndex].iSet) {
                            if (publish(channelIndex, PUB_TOPIC_DCPSUPPLY_I_SET, iSet, true)) {
                                g_channelStates[channelIndex].iSet = iSet;
                                g_channelStates[channelIndex].iSetTick = tickCount;
                            }
                        }
                    }
                } else if (g_lastValueIndex == 5) {
                    // publish channel temperature
                    if ((tickCount - g_channelStates[channelIndex].temperatureTick) >= period) {
                        float temperature;
                        temperature::TempSensorTemperature &tempSensor = temperature::sensors[temp_sensor::CH1 + channelIndex];
                        if (tempSensor.isInstalled() && tempSensor.isTestOK()) {
                            temperature = tempSensor.temperature;
                        } else {
                            temperature = NAN;
                        }
                        if (isNaN(g_channelStates[channelIndex].temperature) || temperature != g_channelStates[channelIndex].temperature) {
                            if (publish(channelIndex, PUB_TOPIC_DCPSUPPLY_TEMP, temperature, true)) {
                                g_channelStates[channelIndex].temperature = temperature;
                                g_channelStates[channelIndex].temperatureTick = tickCount;
                            }
                        }
                    }
                } else if (g_lastValueIndex == 6) {
                    // publish total on-time counter
                    uint32_t totalOnTime = ontime::g_moduleCounters[channel.slotIndex].getTotalTime();
                    if (totalOnTime != g_channelStates[channelIndex].totalOnTime) {
                        if (publishOnTimeCounter(channelIndex, PUB_TOPIC_DCPSUPPLY_TOTAL_ONTIME, totalOnTime, true)) {
                            g_channelStates[channelIndex].totalOnTime = totalOnTime;
                            if (g_publishing) {
                                return;
                            }
                        }
                    }
                } else if (g_lastValueIndex == 7) {
                    // publish last on-time counter
                    uint32_t lastOnTime = ontime::g_moduleCounters[channel.slotIndex].getLastTime();
                    if (lastOnTime != g_channelStates[channelIndex].lastOnTime) {
                        if (publishOnTimeCounter(channelIndex, PUB_TOPIC_DCPSUPPLY_LAST_ONTIME, lastOnTime, true)) {
                            g_channelStates[channelIndex].lastOnTime = lastOnTime;
                            if (g_publishing) {
                                return;
                            }
                        }
                    }
                }
            }

            if (++g_lastValueIndex == 8) {
                g_lastValueIndex = 0;
                if (++g_lastChannelIndex == CH_NUM) {
                    g_lastChannelIndex = 0;
                }
            }
        }

#if defined(EEZ_PLATFORM_SIMULATOR)
		mqtt_sync(&g_client);
#endif
    }

    else if (g_connectionState == CONNECTION_STATE_ETHERNET_NOT_READY) {
        g_ethernetReadyTime = millis();
        setState(CONNECTION_STATE_STARTING);
    }

    else if (g_connectionState == CONNECTION_STATE_STARTING) {
        if (millis() - g_ethernetReadyTime > CONF_STARTING_TIMEOUT_MS) {
            setState(CONNECTION_STATE_IDLE);
        }
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
            g_dnsStartedTick = millis();
            setState(CONNECTION_STATE_DNS_IN_PROGRESS);
        } else {
            setState(CONNECTION_STATE_ERROR);
            if (g_lastError != EEZ_MQTT_ERROR_DNS) {
                g_lastError = EEZ_MQTT_ERROR_DNS;
                DebugTrace("mqtt dns error: %d\n", (int)err);
            }
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
            mqtt_connect(&g_client, getClientId(), NULL, NULL, 0, persist_conf::devConf.mqttUsername, persist_conf::devConf.mqttPassword, connect_flags, 400);

            /* check that we don't have any errors */
            if (g_client.error == MQTT_OK) {
                setState(CONNECTION_STATE_CONNECTED);
            } else {
                setState(CONNECTION_STATE_ERROR);
                if (g_lastError != EEZ_MQTT_ERROR_CONNECT) {
                    g_lastError = EEZ_MQTT_ERROR_CONNECT;
                    DebugTrace("mqtt connect error: %s\n", mqtt_error_str(g_client.error));
                }
            }
        } else {
            setState(CONNECTION_STATE_ERROR);
            if (g_lastError != EEZ_MQTT_ERROR_SOCKET) {
                g_lastError = EEZ_MQTT_ERROR_SOCKET;
                DebugTrace("mqtt error: failed to open socket\n");
            }
        }
#endif
    }

    else if (g_connectionState == CONNECTION_STATE_DISCONNECT || g_connectionState == CONNECTION_STATE_RECONNECT) {
#if defined(EEZ_PLATFORM_STM32)
        if (mqtt_client_is_connected(&g_client)) {
            LOCK_TCPIP_CORE();
            mqtt_disconnect(&g_client);
            UNLOCK_TCPIP_CORE();
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
            if (tickCount - g_connectionStateChangedTickCount > RECONNECT_AFTER_ERROR_MS) {
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
    } else if (g_connectionState == CONNECTION_STATE_DNS_IN_PROGRESS) {
        if (tickCount - g_dnsStartedTick > CONF_DNS_TIMEOUT_MS) {
            reconnect();
        }
    }
#endif
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

void pushEvent(int16_t eventId) {
    if (g_connectionState == CONNECTION_STATE_CONNECTED && publishEvent(eventId, true)) {
        return;
    }

    g_eventQueue.buffer[g_eventQueue.head] = eventId;

    // advance
    if (g_eventQueue.full) {
        g_eventQueue.tail = (g_eventQueue.tail + 1) % EVENT_QUEUE_SIZE;
    }
    g_eventQueue.head = (g_eventQueue.head + 1) % EVENT_QUEUE_SIZE;
    g_eventQueue.full = g_eventQueue.head == g_eventQueue.tail;
}

bool peekEvent(int16_t &eventId) {
    if (g_eventQueue.full || g_eventQueue.tail != g_eventQueue.head) {
        eventId = g_eventQueue.buffer[g_eventQueue.tail];
        return true;
    }

    return false;
}

bool getEvent(int16_t &eventId) {
    if (g_eventQueue.full || g_eventQueue.tail != g_eventQueue.head) {
        eventId = g_eventQueue.buffer[g_eventQueue.tail];
        g_eventQueue.tail = (g_eventQueue.tail + 1) % EVENT_QUEUE_SIZE;
        g_eventQueue.full = false;
        return true;
    }

    return false;
}

} // mqtt
} // eez

#endif // OPTION_ETHERNET
