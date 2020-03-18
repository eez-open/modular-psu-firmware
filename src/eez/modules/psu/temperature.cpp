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

static uint32_t last_measured_tick;
static float last_max_channel_temperature;
static uint32_t max_temp_start_tick;
static bool force_power_down = false;

void init() {
    for (int i = 0; i < temp_sensor::NUM_TEMP_SENSORS; ++i) {
        temp_sensor::sensors[i].init();
    }
}

bool test() {
    bool success = true;

    for (int i = 0; i < temp_sensor::NUM_TEMP_SENSORS; ++i) {
        success &= temp_sensor::sensors[i].test();
    }

    return success;
}

void tick(uint32_t tick_usec) {
    if (tick_usec - last_measured_tick >= TEMP_SENSOR_READ_EVERY_MS * 1000L) {
        last_measured_tick = tick_usec;

        for (int i = 0; i < temp_sensor::NUM_TEMP_SENSORS; ++i) {
            sensors[i].tick(tick_usec);
        }

        // find max. channel temperature
        float max_channel_temperature = FLT_MIN;

        for (int i = 0; i < temp_sensor::NUM_TEMP_SENSORS; ++i) {
            temp_sensor::TempSensor &sensor = temp_sensor::sensors[i];
            if (sensor.getChannel()) {
                if (sensor.g_testResult == TEST_OK) {
                    temperature::TempSensorTemperature &sensorTemperature = temperature::sensors[i];
                    if (sensorTemperature.temperature > max_channel_temperature) {
                        max_channel_temperature = sensorTemperature.temperature;
                    }
                }
            }
        }

        // check if max_channel_temperature is too high
        if (max_channel_temperature > FAN_MAX_TEMP) {
            if (last_max_channel_temperature <= FAN_MAX_TEMP) {
                max_temp_start_tick = tick_usec;
            }

            if (tick_usec - max_temp_start_tick > FAN_MAX_TEMP_DELAY * 1000000L) {
                // turn off power
                force_power_down = true;
                changePowerState(false);
            }
        } else if (max_channel_temperature <= FAN_MAX_TEMP - FAN_MAX_TEMP_DROP) {
            force_power_down = false;
        }

        last_max_channel_temperature = max_channel_temperature;
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
        if (sensors[i].isChannelSensor(channel)) {
            sensors[i].clearProtection();
        }
    }
}

void disableChannelProtection(Channel *channel) {
    for (int i = 0; i < temp_sensor::NUM_TEMP_SENSORS; ++i) {
        if (sensors[i].isChannelSensor(channel)) {
            sensors[i].prot_conf.state = 0;
        }
    }
}

float getMaxChannelTemperature() {
    return last_max_channel_temperature;
}

bool isAllowedToPowerUp() {
    return !force_power_down;
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

void TempSensorTemperature::tick(uint32_t tick_usec) {
    if (isInstalled() && isTestOK()) {
        measure();
        if (temp_sensor::sensors[sensorIndex].g_testResult == TEST_OK) {
            protection_check(tick_usec);
        }
    }
}

bool TempSensorTemperature::isChannelSensor(Channel *channel) {
    return temp_sensor::sensors[sensorIndex].isInstalled() && temp_sensor::sensors[sensorIndex].getChannel();
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

void TempSensorTemperature::protection_check(uint32_t tick_usec) {
    if (temp_sensor::sensors[sensorIndex].isInstalled()) {
        if (!otp_tripped && prot_conf.state && temperature >= prot_conf.level) {
            float delay = prot_conf.delay;
            if (delay > 0) {
                if (otp_alarmed) {
                    if (tick_usec - otp_alarmed_started_tick >= delay * 1000000UL) {
                        otp_alarmed = 0;
                        protection_enter(*this);
                    }
                } else {
                    otp_alarmed = 1;
                    otp_alarmed_started_tick = tick_usec;
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

        event_queue::pushEvent(event_queue::EVENT_ERROR_CH1_OTP_TRIPPED + channel->channelIndex);
    } else {
        channel_dispatcher::disableOutputForAllChannels();

        event_queue::pushEvent(event_queue::EVENT_ERROR_AUX_OTP_TRIPPED);
    }

    set_otp_reg(true);
}

} // namespace temperature
} // namespace psu
} // namespace eez
