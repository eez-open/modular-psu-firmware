/* / mcu / sound.h
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

#include <eez/modules/psu/psu.h>

#include <eez/index.h>

#include <float.h>

#include <eez/modules/psu/serial_psu.h>

#if OPTION_ETHERNET
#include <eez/modules/psu/ethernet.h>
#endif

#include <eez/modules/psu/channel_dispatcher.h>
#include <eez/modules/psu/event_queue.h>
#include <eez/modules/psu/persist_conf.h>
#include <eez/modules/psu/temperature.h>
#include <eez/sound.h>

namespace eez {
namespace psu {
namespace temperature {

#define TEMP_SENSOR(NAME, QUES_REG_BIT, SCPI_ERROR) TempSensorTemperature(temp_sensor::NAME)
TempSensorTemperature sensors[temp_sensor::NUM_TEMP_SENSORS] = { TEMP_SENSORS };
#undef TEMP_SENSOR

////////////////////////////////////////////////////////////////////////////////

static uint32_t g_lastMeasuredTickMs;
static uint32_t g_maxTempCheckStartTickMs;
static float g_lastMaxChannelTemperature;

void init() {
    for (int i = 0; i < temp_sensor::NUM_TEMP_SENSORS; ++i) {
        temp_sensor::sensors[i].init();
    }
}

bool test() {
    return temp_sensor::sensors[temp_sensor::AUX].test();
}

void tick() {
	uint32_t tickCountMs = millis();
    if (tickCountMs - g_lastMeasuredTickMs >= TEMP_SENSOR_READ_EVERY_MS) {
		g_lastMeasuredTickMs = tickCountMs;

        for (int i = 0; i < temp_sensor::NUM_TEMP_SENSORS; ++i) {
            sensors[i].tick(tickCountMs);
        }

        // find max. channel temperature
        float maxChannelTemperature = FLT_MIN;
        int maxChannelTemperatureIndex = 0;

        for (int i = 0; i < temp_sensor::NUM_TEMP_SENSORS; ++i) {
            temp_sensor::TempSensor &sensor = temp_sensor::sensors[i];
            if (sensor.getChannel()) {
                if (sensor.g_testResult == TEST_OK) {
                    temperature::TempSensorTemperature &sensorTemperature = temperature::sensors[i];
                    if (sensorTemperature.temperature > maxChannelTemperature) {
                        maxChannelTemperature = sensorTemperature.temperature;
                        maxChannelTemperatureIndex = sensor.getChannel()->channelIndex;
                    }
                }
            }
        }

        for (int i = 0; i < NUM_SLOTS; ++i) {
            maxChannelTemperature = MAX(maxChannelTemperature, g_slots[i]->getMaxTemperature());
        }

        // check if max_channel_temperature is too high
        if (isPowerUp() && maxChannelTemperature > FAN_MAX_TEMP) {
            if (g_lastMaxChannelTemperature <= FAN_MAX_TEMP) {
                g_maxTempCheckStartTickMs = tickCountMs;
            }

            if (tickCountMs - g_maxTempCheckStartTickMs > FAN_MAX_TEMP_DELAY * 1000L) {
                // turn off power
                event_queue::pushEvent(event_queue::EVENT_ERROR_HIGH_TEMPERATURE_CH1 + maxChannelTemperatureIndex);
                changePowerState(false);
            }
        } else {
			g_maxTempCheckStartTickMs = tickCountMs;
        }

        g_lastMaxChannelTemperature = maxChannelTemperature;
    }
}

bool isChannelSensorInstalled(Channel *channel) {
    return sensors[temp_sensor::CH1 + channel->channelIndex].isInstalled();
}

bool getChannelSensorState(Channel *channel) {
    return sensors[temp_sensor::CH1 + channel->channelIndex].prot_conf.state;
}

float getChannelSensorLevel(Channel *channel) {
    return sensors[temp_sensor::CH1 + channel->channelIndex].prot_conf.level;
}

float getChannelSensorDelay(Channel *channel) {
    return sensors[temp_sensor::CH1 + channel->channelIndex].prot_conf.delay;
}

bool isAnySensorTripped(Channel *channel) {
    for (int i = 0; i < temp_sensor::NUM_TEMP_SENSORS; ++i) {
        if (sensors[i].isTripped()) {
            return true;
        }
    }

    return false;
}

void clearChannelProtection(Channel *channel) {
    for (int i = 0; i < temp_sensor::NUM_TEMP_SENSORS; ++i) {
        if (sensors[i].getChannel() == channel) {
            sensors[i].clearProtection();
        }
    }
}

void disableChannelProtection(Channel *channel) {
    for (int i = 0; i < temp_sensor::NUM_TEMP_SENSORS; ++i) {
        if (sensors[i].getChannel() == channel) {
            sensors[i].prot_conf.state = 0;
        }
    }
}

float getMaxChannelTemperature() {
    return g_lastMaxChannelTemperature;
}

////////////////////////////////////////////////////////////////////////////////

TempSensorTemperature::TempSensorTemperature(int sensorIndex_)
    : temperature(NAN), sensorIndex(sensorIndex_) {
}

const char *TempSensorTemperature::getName() {
    return temp_sensor::sensors[sensorIndex].name;
}

bool TempSensorTemperature::isInstalled() {
    return temp_sensor::sensors[sensorIndex].isInstalled();
}

bool TempSensorTemperature::isTestOK() {
    return temp_sensor::sensors[sensorIndex].g_testResult == TEST_OK;
}

void TempSensorTemperature::tick(uint32_t tickMs) {
    if (isInstalled() && isTestOK()) {
        measure();
        if (temp_sensor::sensors[sensorIndex].g_testResult == TEST_OK) {
            protection_check(tickMs);
        }
    }
}

Channel *TempSensorTemperature::getChannel() {
    return temp_sensor::sensors[sensorIndex].isInstalled() ? temp_sensor::sensors[sensorIndex].getChannel() : nullptr;
}

float TempSensorTemperature::measure() {
    float newTemperature = temp_sensor::sensors[sensorIndex].read();
    if (!isNaN(newTemperature)) {
		if (isNaN(temperature)) {
			temperature = newTemperature;
		} else {
			temperature = temperature + 1.0f * (newTemperature - temperature);
		}
    }
    return temperature;
}

void TempSensorTemperature::clearProtection() {
    otp_tripped = false;
    set_otp_reg(false);

    if (event_queue::getLastErrorEventId() == event_queue::EVENT_ERROR_AUX_OTP_TRIPPED + sensorIndex) {
        event_queue::markAsRead();
    }
}

bool TempSensorTemperature::isTripped() {
    return otp_tripped;
}

void TempSensorTemperature::set_otp_reg(bool on) {
    Channel *channel = temp_sensor::sensors[sensorIndex].getChannel();
    if (channel) {
        channel->setQuesBits(temp_sensor::sensors[sensorIndex].ques_bit, on);
    } else {
        setQuesBits(temp_sensor::sensors[sensorIndex].ques_bit, on);
    }
}

void TempSensorTemperature::protection_check(uint32_t tickMs) {
    if (temp_sensor::sensors[sensorIndex].isInstalled()) {
        if (!otp_tripped && prot_conf.state && temperature >= prot_conf.level) {
            float delay = prot_conf.delay;
            if (delay > 0) {
                if (otp_alarmed) {
                    if (tickMs - otp_alarmed_started_tick >= delay * 1000) {
                        otp_alarmed = 0;
                        protection_enter(*this);
                    }
                } else {
                    otp_alarmed = 1;
                    otp_alarmed_started_tick = tickMs;
                }
            } else {
                protection_enter(*this);
            }
        } else {
            otp_alarmed = 0;
        }
    }
}

void TempSensorTemperature::protection_enter(TempSensorTemperature &sensor) {
    Channel *channel = temp_sensor::sensors[sensor.sensorIndex].getChannel();
    if (channel) {
        if (channel->channelIndex < 2 && channel_dispatcher::getCouplingType() != channel_dispatcher::COUPLING_TYPE_NONE) {
            for (int i = 0; i < temp_sensor::NUM_TEMP_SENSORS; ++i) {
                TempSensorTemperature &sensor = sensors[i];
                Channel *channel = temp_sensor::sensors[sensor.sensorIndex].getChannel();
                if (channel && (channel->channelIndex == 0 || channel->channelIndex == 1)) {
                    sensor.protection_enter();
                }
            }
        } else if (channel->flags.trackingEnabled) {
            for (int i = 0; i < temp_sensor::NUM_TEMP_SENSORS; ++i) {
                TempSensorTemperature &sensor = sensors[i];
                Channel *trackingChannel = temp_sensor::sensors[sensor.sensorIndex].getChannel();
                if (trackingChannel && trackingChannel->flags.trackingEnabled) {
                    sensor.protection_enter();
                }
            }
        } else {
            sensor.protection_enter();
        }
    } else {
        sensor.protection_enter();
    }

    onProtectionTripped();
}

void TempSensorTemperature::protection_enter() {
    otp_tripped = true;

    Channel *channel = temp_sensor::sensors[sensorIndex].getChannel();
    if (channel) {
        channel_dispatcher::outputEnable(*channel, false);

        event_queue::pushChannelEvent(event_queue::EVENT_ERROR_CH_OTP_TRIPPED, channel->channelIndex);
    } else {
        channel_dispatcher::disableOutputForAllChannels();

        event_queue::pushEvent(event_queue::EVENT_ERROR_AUX_OTP_TRIPPED);
    }

    set_otp_reg(true);
}

} // namespace temperature
} // namespace psu
} // namespace eez
