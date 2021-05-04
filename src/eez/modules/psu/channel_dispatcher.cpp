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

#include <float.h>
#include <assert.h>

#include <eez/modules/psu/psu.h>
#include <eez/modules/psu/calibration.h>
#include <eez/modules/psu/channel_dispatcher.h>
#include <eez/modules/psu/event_queue.h>
#include <eez/modules/psu/list_program.h>
#include <eez/modules/psu/gui/psu.h>
#include <eez/scpi/regs.h>
#include <eez/modules/psu/temperature.h>
#include <eez/modules/psu/trigger.h>
#include <eez/index.h>
#include <eez/system.h>
#include <eez/modules/bp3c/io_exp.h>

namespace eez {
namespace psu {
namespace channel_dispatcher {

static CouplingType g_couplingType = COUPLING_TYPE_NONE;

CouplingType getCouplingType() {
    return g_couplingType;
}

bool isTrackingAllowed(Channel &channel, int *err) {
    if (!channel.isOk()) {
        if (err) {
            *err = SCPI_ERROR_HARDWARE_ERROR;
        }
        return false;
    }

    if (CH_NUM < 2) {
        if (err) {
            *err = SCPI_ERROR_HARDWARE_MISSING;
        }
        return false;
    }

    if (channel.channelIndex < 2 && (g_couplingType == COUPLING_TYPE_SERIES || g_couplingType == COUPLING_TYPE_PARALLEL)) {
        if (err) {
            *err = SCPI_ERROR_EXECUTE_ERROR_CHANNELS_ARE_COUPLED;
        }
        return false;
    }

    return true;
}

bool isCouplingTypeAllowed(CouplingType couplingType, int *err) {
    if (couplingType == COUPLING_TYPE_NONE) {
        return true;
    }

    if (couplingType == COUPLING_TYPE_COMMON_GND) {
        int n = 0;

        for (int i = 0; i < CH_NUM; i++) {
            Channel &channel = Channel::get(i);
            if (channel.isOk() && channel.subchannelIndex == 0) {
                n++;
            }
        }

        if (n < 2) {
            if (err) {
                *err = SCPI_ERROR_HARDWARE_ERROR;
            }
            return false;
        }
    } else {
        if (CH_NUM < 2 || Channel::get(0).slotIndex != 0 || Channel::get(1).slotIndex != 1) {
            if (err) {
                *err = SCPI_ERROR_HARDWARE_MISSING;
            }
            return false;
        }

        if (
            !Channel::get(0).isOk() || 
            !(Channel::get(0).params.features & CH_FEATURE_COUPLING) || 
            !Channel::get(1).isOk() ||
            !(Channel::get(1).params.features & CH_FEATURE_COUPLING)
        ) {
            if (err) {
                *err = SCPI_ERROR_HARDWARE_ERROR;
            }
            return false;
        }

        if (couplingType == COUPLING_TYPE_PARALLEL || couplingType == COUPLING_TYPE_SERIES) {
            if (Channel::get(0).flags.trackingEnabled || Channel::get(1).flags.trackingEnabled) {
                if (err) {
                    *err = SCPI_ERROR_EXECUTION_ERROR;
                }
                return false;
            }
        }
    }

    return true;
}

static int g_setCouplingTypeErr;

bool setCouplingType(CouplingType couplingType, int *err) {
    if (g_couplingType != couplingType) {
        if (!isCouplingTypeAllowed(couplingType, err)) {
            return false;
        }

        if (!isPsuThread()) {
            sendMessageToPsu(PSU_MESSAGE_SET_COUPLING_TYPE, couplingType);
        } else {
            setCouplingTypeInPsuThread(couplingType);
        }

        if (g_setCouplingTypeErr != SCPI_RES_OK) {
            if (err) {
                *err = g_setCouplingTypeErr;
            }
            return false;
        }

        return true;
    }

    return true;
}

bool additionalCheckForCouplingType(CouplingType couplingType, int *err) {
    Channel &ch1 = Channel::get(0);
    Channel &ch2 = Channel::get(1);

    if (!ch1.isVoltageCalibrationExists() || !ch1.isCurrentCalibrationExists(0) || !ch1.isCurrentCalibrationExists(1)) {
        if (err) {
            *err = SCPI_ERROR_CH1_NOT_CALIBRATED;
        }
        return false;
    }

    if (!ch2.isVoltageCalibrationExists() || !ch2.isCurrentCalibrationExists(0) || !ch2.isCurrentCalibrationExists(1)) {
        if (err) {
            *err = SCPI_ERROR_CH2_NOT_CALIBRATED;
        }
        return false;
    }

    ch1.calibrationEnableNoEvent(true);
    ch2.calibrationEnableNoEvent(true);

    return true;
}

void setCouplingTypeInPsuThread(CouplingType couplingType) {
    trigger::abort();

    disableOutputForAllChannels();

    if (couplingType == COUPLING_TYPE_PARALLEL || couplingType == COUPLING_TYPE_SERIES) {
        for (int i = 0; i < 2; ++i) {
            Channel &channel = Channel::get(i);

            channel.remoteSensingEnable(false);
            channel.remoteProgrammingEnable(false);

            channel.setVoltageTriggerMode(TRIGGER_MODE_FIXED);
            channel.setCurrentTriggerMode(TRIGGER_MODE_FIXED);
            channel.setTriggerOutputState(true);
            channel.setTriggerOnListStop(TRIGGER_ON_LIST_STOP_OUTPUT_OFF);

            list::resetChannelList(channel);

            channel.setVoltage(getUMin(channel));
            channel.setVoltageLimit(MIN(Channel::get(0).getVoltageLimit(), Channel::get(1).getVoltageLimit()));

            channel.setCurrent(getIMin(channel));
            channel.setCurrentLimit(MIN(Channel::get(0).getCurrentLimit(), Channel::get(1).getCurrentLimit()));

            channel.u.triggerLevel = getUMin(channel);
            channel.i.triggerLevel = getIMin(channel);

            channel.prot_conf.flags.u_state = Channel::get(0).prot_conf.flags.u_state || Channel::get(1).prot_conf.flags.u_state ? 1 : 0;
            if (channel.params.features & CH_FEATURE_HW_OVP) {
                channel.prot_conf.flags.u_type = Channel::get(0).prot_conf.flags.u_type || Channel::get(1).prot_conf.flags.u_type ? 1 : 0;
            }
            channel.prot_conf.u_level = MIN(Channel::get(0).prot_conf.u_level, Channel::get(1).prot_conf.u_level);
            channel.prot_conf.u_delay = MIN(Channel::get(0).prot_conf.u_delay, Channel::get(1).prot_conf.u_delay);

            channel.prot_conf.flags.i_state = Channel::get(0).prot_conf.flags.i_state || Channel::get(1).prot_conf.flags.i_state ? 1 : 0;
            channel.prot_conf.i_delay = MIN(Channel::get(0).prot_conf.i_delay, Channel::get(1).prot_conf.i_delay);

            channel.prot_conf.flags.p_state = Channel::get(0).prot_conf.flags.p_state || Channel::get(1).prot_conf.flags.p_state ? 1 : 0;
            channel.prot_conf.p_level = MIN(Channel::get(0).prot_conf.p_level, Channel::get(1).prot_conf.p_level);
            channel.prot_conf.p_delay = MIN(Channel::get(0).prot_conf.p_delay, Channel::get(1).prot_conf.p_delay);

            temperature::sensors[temp_sensor::CH1 + channel.channelIndex].prot_conf.state = temperature::sensors[temp_sensor::CH1].prot_conf.state || temperature::sensors[temp_sensor::CH2].prot_conf.state ? 1 : 0;
            temperature::sensors[temp_sensor::CH1 + channel.channelIndex].prot_conf.level = MIN(temperature::sensors[temp_sensor::CH1].prot_conf.level, temperature::sensors[temp_sensor::CH2].prot_conf.level);
            temperature::sensors[temp_sensor::CH1 + channel.channelIndex].prot_conf.delay = MIN(temperature::sensors[temp_sensor::CH1].prot_conf.delay, temperature::sensors[temp_sensor::CH2].prot_conf.delay);

            if (i == 1) {
                Channel &channel1 = Channel::get(0);
                channel.displayValues[0] = channel1.displayValues[0];
				channel.displayValues[1] = channel1.displayValues[1];
                channel.ytViewRate = channel1.ytViewRate;

                channel.u.rampDuration = channel1.u.rampDuration;
            }

            channel.setCurrentRangeSelectionMode(CURRENT_RANGE_SELECTION_USE_BOTH);
            channel.enableAutoSelectCurrentRange(false);

            channel.flags.trackingEnabled = false;

            channel.resetHistory();
        }

        if (persist_conf::isMaxView() && persist_conf::getMaxSlotIndex() == Channel::get(1).slotIndex) {
            persist_conf::setMaxSlotIndex(Channel::get(0).slotIndex);
        }

        // disable tracking if only 1 channel left in tracking mode
        int numTrackingChannels = 0;
        for (int i = 0; i < CH_NUM; i++) {
            Channel &channel = Channel::get(i);
            if (channel.flags.trackingEnabled) {
                numTrackingChannels++;
            }
        }
        if (numTrackingChannels == 1) {
            for (int i = 0; i < CH_NUM; i++) {
                Channel &channel = Channel::get(i);
                if (channel.flags.trackingEnabled) {
                    channel.flags.trackingEnabled = false;
                    break;
                }
            }
        }

        if (!additionalCheckForCouplingType(couplingType, &g_setCouplingTypeErr)) {
            return;
        }
    } else {
        if (g_couplingType == COUPLING_TYPE_PARALLEL || g_couplingType == COUPLING_TYPE_SERIES) {
            for (int i = 0; i < 2; ++i) {
                Channel &channel = Channel::get(i);
                channel.reset(false /* do not reset label and color */);
            }
        }
    }

    g_couplingType = couplingType;
    bp3c::io_exp::switchChannelCoupling(g_couplingType);

    if (g_couplingType == COUPLING_TYPE_PARALLEL) {
        event_queue::pushEvent(event_queue::EVENT_INFO_COUPLED_IN_PARALLEL);
    } else if (g_couplingType == COUPLING_TYPE_SERIES) {
        event_queue::pushEvent(event_queue::EVENT_INFO_COUPLED_IN_SERIES);
    } else if (g_couplingType == COUPLING_TYPE_COMMON_GND) {
        event_queue::pushEvent(event_queue::EVENT_INFO_COUPLED_IN_COMMON_GND);
    } else if (g_couplingType == COUPLING_TYPE_SPLIT_RAILS) {
        event_queue::pushEvent(event_queue::EVENT_INFO_COUPLED_IN_SPLIT_RAILS);
    } else {
        event_queue::pushEvent(event_queue::EVENT_INFO_CHANNELS_UNCOUPLED);
    }

    setOperBits(OPER_GROUP_PARALLEL, g_couplingType == COUPLING_TYPE_PARALLEL);
    setOperBits(OPER_GROUP_SERIAL, g_couplingType == COUPLING_TYPE_SERIES);
    setOperBits(OPER_GROUP_COMMON_GND, g_couplingType == COUPLING_TYPE_COMMON_GND);
    setOperBits(OPER_GROUP_SPLIT_RAILS, g_couplingType == COUPLING_TYPE_SPLIT_RAILS);

    delay(100); // Huge pause that allows relay contacts to debounce

    g_setCouplingTypeErr = SCPI_RES_OK;
}

void setTrackingChannels(uint16_t trackingEnabled) {
    if (!isPsuThread()) {
        sendMessageToPsu(PSU_MESSAGE_SET_TRACKING_CHANNELS, trackingEnabled);
    } else {
        bool resetTrackingChannels = false;
        for (int i = 0; i < CH_NUM; i++) {
            Channel &trackingChannel = Channel::get(i);
            unsigned wasEnabled = trackingChannel.flags.trackingEnabled;
            trackingChannel.flags.trackingEnabled = (trackingEnabled & (1 << i)) ? 1 : 0;
            if (!wasEnabled && trackingChannel.flags.trackingEnabled) {
                resetTrackingChannels = true;
            }
        }

        if (resetTrackingChannels) {
            event_queue::pushEvent(event_queue::EVENT_INFO_CHANNELS_TRACKED);

            trigger::abort();

            float uMin = 0;
            float iMin = 0;

            float voltageLimit = FLT_MAX;
            float currentLimit = FLT_MAX;

            float uDef = FLT_MAX;
            float iDef = FLT_MAX;

            int u_state = 0;
            int u_type = 0;
            float u_level = FLT_MAX;
            float u_delay = FLT_MAX;

            int i_state = 0;
            float i_delay = FLT_MAX;

            int p_state = 0;
            float p_level = FLT_MAX;
            float p_delay = FLT_MAX;

            int t_state = 0;
            float t_level = FLT_MAX;
            float t_delay = FLT_MAX;

            for (int i = 0; i < CH_NUM; i++) {
                Channel &trackingChannel = Channel::get(i);
                if (trackingChannel.flags.trackingEnabled) {
                    uMin = MAX(uMin, getUMin(trackingChannel));
                    iMin = MAX(iMin, getIMin(trackingChannel));
                    
                    voltageLimit = MIN(voltageLimit, trackingChannel.getVoltageLimit());
                    currentLimit = MIN(currentLimit, trackingChannel.getCurrentLimit());
                    
                    uDef = MIN(uDef, trackingChannel.u.def);
                    iDef = MIN(iDef, trackingChannel.i.def);

                    if (trackingChannel.prot_conf.flags.u_state) {
                        u_state = 1;
                    }
                    if (trackingChannel.prot_conf.flags.u_type) {
                        u_type = 1;
                    }
                    u_level = MIN(u_level, trackingChannel.prot_conf.u_level);
                    u_delay = MIN(u_delay, trackingChannel.prot_conf.u_delay);

                    if (trackingChannel.prot_conf.flags.i_state) {
                        i_state = 1;
                    }
                    i_delay = MIN(i_delay, trackingChannel.prot_conf.i_delay);

                    if (trackingChannel.prot_conf.flags.p_state) {
                        p_state = 1;
                    }
                    p_level = MIN(p_level, trackingChannel.prot_conf.p_level);
                    p_delay = MIN(p_delay, trackingChannel.prot_conf.p_delay);

                    if (temperature::sensors[temp_sensor::CH1 + i].prot_conf.state) {
                        t_state = 1;
                    }
                    t_level = MIN(t_level, temperature::sensors[temp_sensor::CH1 + i].prot_conf.level);
                    t_delay = MIN(t_delay, temperature::sensors[temp_sensor::CH1 + i].prot_conf.delay);
                }
            }

            disableOutputForAllTrackingChannels();

            for (int i = 0; i < CH_NUM; i++) {
                Channel &trackingChannel = Channel::get(i);
                if (trackingChannel.flags.trackingEnabled) {
                    trackingChannel.remoteSensingEnable(false);
                    trackingChannel.remoteProgrammingEnable(false);

                    trackingChannel.setVoltageTriggerMode(TRIGGER_MODE_FIXED);
                    trackingChannel.setCurrentTriggerMode(TRIGGER_MODE_FIXED);
                    trackingChannel.setTriggerOutputState(true);
                    trackingChannel.setTriggerOnListStop(TRIGGER_ON_LIST_STOP_OUTPUT_OFF);

                    list::resetChannelList(trackingChannel);

                    trackingChannel.setVoltage(MAX(uMin, getUMin(trackingChannel)));
                    trackingChannel.setVoltageLimit(MAX(voltageLimit, getUMin(trackingChannel)));

                    trackingChannel.setCurrent(MAX(iMin, getIMin(trackingChannel)));
                    trackingChannel.setCurrentLimit(MAX(currentLimit, getIMin(trackingChannel)));

                    trackingChannel.u.triggerLevel = uDef;
                    trackingChannel.i.triggerLevel = iDef;

                    trackingChannel.prot_conf.flags.u_state = u_state;
                    if (trackingChannel.params.features & CH_FEATURE_HW_OVP) {
                        trackingChannel.prot_conf.flags.u_type = u_type;
                    }
                    trackingChannel.prot_conf.u_level = u_level;
                    trackingChannel.prot_conf.u_delay = u_delay;

                    trackingChannel.prot_conf.flags.i_state = i_state;
                    trackingChannel.prot_conf.i_delay = i_delay;

                    trackingChannel.prot_conf.flags.p_state = p_state;
                    trackingChannel.prot_conf.p_level = p_level;
                    trackingChannel.prot_conf.p_delay = p_delay;

                    temperature::sensors[temp_sensor::CH1 + i].prot_conf.state = t_state;
                    temperature::sensors[temp_sensor::CH1 + i].prot_conf.level = t_level;
                    temperature::sensors[temp_sensor::CH1 + i].prot_conf.delay = t_delay;

                    trackingChannel.u.rampDuration = RAMP_DURATION_DEF_VALUE_U;

                    trackingChannel.i.rampDuration = RAMP_DURATION_DEF_VALUE_I;

                    trackingChannel.resetHistory();
                }
            }
        }
    }
}

CouplingType getType() {
    return g_couplingType;
}

////////////////////////////////////////////////////////////////////////////////

float getTrackingValuePrecision(Unit unit, float value) {
    float precision = 0;
    for (int i = 0; i < CH_NUM; ++i) {
        Channel &trackingChannel = Channel::get(i);
        if (trackingChannel.flags.trackingEnabled) {
            precision = MAX(precision, trackingChannel.getValuePrecision(unit, value));
        }
    }
    return precision;
}

float roundTrackingValuePrecision(Unit unit, float value) {
    return roundPrec(value, getTrackingValuePrecision(unit, value));
}

float getValuePrecision(const Channel &channel, Unit unit, float value) {
    if (channel.flags.trackingEnabled) {
        return getTrackingValuePrecision(unit, value);
    }
    return channel.getValuePrecision(unit, value);
}

float roundChannelValue(const Channel &channel, Unit unit, float value) {
    return roundPrec(value, getValuePrecision(channel, unit, value));
}

float getUSet(const Channel &channel) {
    if (channel.channelIndex < 2 && g_couplingType == COUPLING_TYPE_SERIES) {
        return Channel::get(0).getUSet() + Channel::get(1).getUSet();
    }
    return channel.getUSet();
}

float getUSet(int slotIndex, int subchannelIndex) {
    Channel *channel = Channel::getBySlotIndex(slotIndex, subchannelIndex);
    if (channel) {
        return getUSet(*channel);
    }

    float value;
    getVoltage(slotIndex, subchannelIndex, value, nullptr);
    return value;
}

float getUMon(const Channel &channel) {
    if (channel.channelIndex < 2 && g_couplingType == COUPLING_TYPE_SERIES) {
        return Channel::get(0).u.mon + Channel::get(1).u.mon;
    }
    return channel.u.mon;
}

float getUMonLast(const Channel &channel) {
    if (channel.channelIndex < 2 && g_couplingType == COUPLING_TYPE_SERIES) {
        return Channel::get(0).u.mon_last + Channel::get(1).u.mon_last;
    }
    return channel.u.mon_last;
}

float getUMonLast(int slotIndex, int subchannelIndex) {
    Channel *channel = Channel::getBySlotIndex(slotIndex, subchannelIndex);
    if (channel) {
        return getUMonLast(*channel);
    }

    float value;
    getMeasuredVoltage(slotIndex, subchannelIndex, value, nullptr);
    return value;
}

float getUMonDac(const Channel &channel) {
    if (channel.channelIndex < 2 && g_couplingType == COUPLING_TYPE_SERIES) {
        return Channel::get(0).u.mon_dac + Channel::get(1).u.mon_dac;
    }
    return channel.u.mon_dac;
}

float getUMonDacLast(const Channel &channel) {
    if (channel.channelIndex < 2 && g_couplingType == COUPLING_TYPE_SERIES) {
        return Channel::get(0).u.mon_dac_last + Channel::get(1).u.mon_dac_last;
    }
    return channel.u.mon_dac_last;
}

float getULimit(const Channel &channel) {
    if (channel.channelIndex < 2 && g_couplingType == COUPLING_TYPE_SERIES) {
        return 2 * MIN(Channel::get(0).getVoltageLimit(), Channel::get(1).getVoltageLimit());
    }
    return channel.getVoltageLimit();
}

float getUMaxLimit(const Channel &channel) {
    if (channel.channelIndex < 2 && g_couplingType == COUPLING_TYPE_SERIES) {
        return 2 * MIN(Channel::get(0).getVoltageMaxLimit(), Channel::get(1).getVoltageMaxLimit());
    } else if (channel.channelIndex < 2 && g_couplingType == COUPLING_TYPE_PARALLEL) {
        return MIN(Channel::get(0).getVoltageMaxLimit(), Channel::get(1).getVoltageMaxLimit());
    } else if (channel.flags.trackingEnabled) {
        float value = channel.getVoltageMaxLimit();
        for (int i = 0; i < CH_NUM; ++i) {
            if (i != channel.channelIndex) {
                const Channel &trackingChannel = Channel::get(i);
                if (trackingChannel.flags.trackingEnabled) {
                    value = MIN(value, trackingChannel.getVoltageMaxLimit());
                }
            }
        }
        return value;
    }
    return channel.getVoltageMaxLimit();
}

float getUMin(const Channel &channel) {
    if (channel.channelIndex < 2 && g_couplingType == COUPLING_TYPE_SERIES) {
        return 2 * MAX(Channel::get(0).u.min, Channel::get(1).u.min);
    } else if (channel.channelIndex < 2 && g_couplingType == COUPLING_TYPE_PARALLEL) {
        return MAX(Channel::get(0).u.min, Channel::get(1).u.min);
    } else if (channel.flags.trackingEnabled) {
        float value = channel.u.min;
        for (int i = 0; i < CH_NUM; ++i) {
            if (i != channel.channelIndex) {
                const Channel &trackingChannel = Channel::get(i);
                if (trackingChannel.flags.trackingEnabled) {
                    value = MAX(value, trackingChannel.u.min);
                }
            }
        }
        return value;
    }
    return channel.u.min;
}

float getUMin(int slotIndex, int subchannelIndex) {
    Channel *channel = Channel::getBySlotIndex(slotIndex, subchannelIndex);
    if (channel) {
        return getUMin(*channel);
    }

    return getVoltageMinValue(slotIndex, subchannelIndex);
}

float getUDef(const Channel &channel) {
    if (channel.channelIndex < 2 && g_couplingType == COUPLING_TYPE_SERIES) {
        return Channel::get(0).u.def + Channel::get(1).u.def;
    }
    return channel.u.def;
}

float getUDef(int slotIndex, int subchannelIndex) {
    Channel *channel = Channel::getBySlotIndex(slotIndex, subchannelIndex);
    if (channel) {
        return getUDef(*channel);
    }

    return getVoltageMinValue(slotIndex, subchannelIndex);
}

float getUMax(const Channel &channel) {
    if (channel.channelIndex < 2 && g_couplingType == COUPLING_TYPE_SERIES) {
        return 2 * MIN(Channel::get(0).u.max, Channel::get(1).u.max);
    } else if (channel.channelIndex < 2 && g_couplingType == COUPLING_TYPE_PARALLEL) {
        return MIN(Channel::get(0).u.max, Channel::get(1).u.max);
    } else if (channel.flags.trackingEnabled) {
        float value = channel.u.max;
        for (int i = 0; i < CH_NUM; ++i) {
            if (i != channel.channelIndex) {
                const Channel &trackingChannel = Channel::get(i);
                if (trackingChannel.flags.trackingEnabled) {
                    value = MIN(value, trackingChannel.u.max);
                }
            }
        }
        return value;
    }
    return channel.u.max;
}

float getUMax(int slotIndex, int subchannelIndex) {
    Channel *channel = Channel::getBySlotIndex(slotIndex, subchannelIndex);
    if (channel) {
        return getUMax(*channel);
    }

    return getVoltageMaxValue(slotIndex, subchannelIndex);
}

float getUMaxOvpLimit(const Channel &channel) {
    if (channel.flags.rprogEnabled) {
        return getUMax(channel) + 0.05f;    
    }
    return getUMax(channel);
}

float getUMaxOvpLevel(const Channel &channel) {
    return getUMax(channel) + 0.05f;
}

float getUProtectionLevel(const Channel &channel) {
    if (channel.channelIndex < 2 && g_couplingType == COUPLING_TYPE_SERIES) {
        return Channel::get(0).prot_conf.u_level + Channel::get(1).prot_conf.u_level + 0.01;
    }
    return channel.prot_conf.u_level;
}

static float g_setVoltageValues[CH_MAX];

void setVoltageInPsuThread(int channelIndex) {
    setVoltage(Channel::get(channelIndex), g_setVoltageValues[channelIndex]);
}

void setVoltage(Channel &channel, float voltage) {
    if (!isPsuThread()) {
        g_setVoltageValues[channel.channelIndex] = voltage;
        sendMessageToPsu(PSU_MESSAGE_SET_VOLTAGE, channel.channelIndex);
        return;
    }

    if (channel.channelIndex < 2 && g_couplingType == COUPLING_TYPE_SERIES) {
        Channel::get(0).setVoltage(voltage / 2);
        Channel::get(1).setVoltage(voltage / 2);
    } else if (channel.channelIndex < 2 && g_couplingType == COUPLING_TYPE_PARALLEL) {
        Channel::get(0).setVoltage(voltage);
        Channel::get(1).setVoltage(voltage);
    } else if (channel.flags.trackingEnabled) {
        voltage = roundTrackingValuePrecision(UNIT_VOLT, voltage);

        for (int i = 0; i < CH_NUM; ++i) {
            Channel &trackingChannel = Channel::get(i);
            if (trackingChannel.flags.trackingEnabled) {
                trackingChannel.setVoltage(voltage);
            }
        }
    } else {
        channel.setVoltage(voltage);
    }
}

void setVoltageStep(Channel &channel, float voltageStep) {
    if (channel.channelIndex < 2 && (g_couplingType == COUPLING_TYPE_SERIES || g_couplingType == COUPLING_TYPE_PARALLEL)) {
        Channel::get(0).u.step = voltageStep;
        Channel::get(1).u.step = voltageStep;
    } else if (channel.flags.trackingEnabled) {
        voltageStep = roundTrackingValuePrecision(UNIT_VOLT, voltageStep);

        for (int i = 0; i < CH_NUM; ++i) {
            Channel &trackingChannel = Channel::get(i);
            if (trackingChannel.flags.trackingEnabled) {
                trackingChannel.u.step = voltageStep;
            }
        }
    } else {
        channel.u.step = voltageStep;
    }
}

void setVoltageLimit(Channel &channel, float limit) {
    if (channel.channelIndex < 2 && g_couplingType == COUPLING_TYPE_SERIES) {
        Channel::get(0).setVoltageLimit(limit / 2);
        Channel::get(1).setVoltageLimit(limit / 2);
    } else if (channel.channelIndex < 2 && g_couplingType == COUPLING_TYPE_PARALLEL) {
        Channel::get(0).setVoltageLimit(limit);
        Channel::get(1).setVoltageLimit(limit);
    } else if (channel.flags.trackingEnabled) {
        limit = roundTrackingValuePrecision(UNIT_VOLT, limit);
        for (int i = 0; i < CH_NUM; ++i) {
            Channel &trackingChannel = Channel::get(i);
            if (trackingChannel.flags.trackingEnabled) {
                trackingChannel.setVoltageLimit(limit);
            }
        }
    } else {
        channel.setVoltageLimit(limit);
    }
}

void setOvpParameters(Channel &channel, int state, int type, float level, float delay) {
    if (!state && channel_dispatcher::isOvpTripped(channel)) {
        clearOvpProtection(channel);
    }

    delay = roundPrec(delay, 0.001f);

    if (channel.channelIndex < 2 && (g_couplingType == COUPLING_TYPE_SERIES || g_couplingType == COUPLING_TYPE_PARALLEL)) {
        float coupledLevel = g_couplingType == COUPLING_TYPE_SERIES ? level / 2 : level;

        coupledLevel = roundPrec(coupledLevel, Channel::get(0).getVoltageResolution());

        Channel::get(0).prot_conf.flags.u_state = state;
        if (Channel::get(0).params.features & CH_FEATURE_HW_OVP) {
            Channel::get(0).prot_conf.flags.u_type = type;
        }
        Channel::get(0).prot_conf.u_level = coupledLevel;
        Channel::get(0).prot_conf.u_delay = delay;

        Channel::get(1).prot_conf.flags.u_state = state;
        if (Channel::get(1).params.features & CH_FEATURE_HW_OVP) {
            Channel::get(1).prot_conf.flags.u_type = type;
        }
        Channel::get(1).prot_conf.u_level = coupledLevel;
        Channel::get(1).prot_conf.u_delay = delay;
    } else if (channel.flags.trackingEnabled) {
        level = roundTrackingValuePrecision(UNIT_VOLT, level);
        for (int i = 0; i < CH_NUM; ++i) {
            Channel &trackingChannel = Channel::get(i);
            if (trackingChannel.flags.trackingEnabled) {
                trackingChannel.prot_conf.flags.u_state = state;
                if (trackingChannel.params.features & CH_FEATURE_HW_OVP) {
                    trackingChannel.prot_conf.flags.u_type = type;
                }
                trackingChannel.prot_conf.u_level = level;
                trackingChannel.prot_conf.u_delay = delay;
            }
        }
    } else {
        channel.prot_conf.flags.u_state = state;
        if (channel.params.features & CH_FEATURE_HW_OVP) {
            channel.prot_conf.flags.u_type = type;
        }
        channel.prot_conf.u_level = roundPrec(level, channel.getVoltageResolution());
        channel.prot_conf.u_delay = delay;
    }
}

void setOvpState(Channel &channel, int state) {
    if (channel.channelIndex < 2 && (g_couplingType == COUPLING_TYPE_SERIES || g_couplingType == COUPLING_TYPE_PARALLEL)) {
        Channel::get(0).prot_conf.flags.u_state = state;
        Channel::get(1).prot_conf.flags.u_state = state;
    } else if (channel.flags.trackingEnabled) {
        for (int i = 0; i < CH_NUM; ++i) {
            Channel &trackingChannel = Channel::get(i);
            if (trackingChannel.flags.trackingEnabled) {
                trackingChannel.prot_conf.flags.u_state = state;
            }
        }
    } else {
        channel.prot_conf.flags.u_state = state;
    }
}

void setOvpType(Channel &channel, int type) {
    if (channel.channelIndex < 2 && (g_couplingType == COUPLING_TYPE_SERIES || g_couplingType == COUPLING_TYPE_PARALLEL)) {
        if (Channel::get(0).params.features & CH_FEATURE_HW_OVP) {
            Channel::get(0).prot_conf.flags.u_type = type;
        }
        if (Channel::get(1).params.features & CH_FEATURE_HW_OVP) {
            Channel::get(1).prot_conf.flags.u_type = type;
        }
    } else if (channel.flags.trackingEnabled) {
        for (int i = 0; i < CH_NUM; ++i) {
            Channel &trackingChannel = Channel::get(i);
            if (trackingChannel.params.features & CH_FEATURE_HW_OVP) {
                if (trackingChannel.flags.trackingEnabled) {
                    trackingChannel.prot_conf.flags.u_type = type;
                }
            }
        }
    } else {
        if (channel.params.features & CH_FEATURE_HW_OVP) {
            channel.prot_conf.flags.u_type = type;
        }
    }
}

void setOvpLevel(Channel &channel, float level) {
    if (channel.channelIndex < 2 && (g_couplingType == COUPLING_TYPE_SERIES || g_couplingType == COUPLING_TYPE_PARALLEL)) {
        float coupledLevel = g_couplingType == COUPLING_TYPE_SERIES ? level / 2 : level;
        coupledLevel = roundPrec(coupledLevel, Channel::get(0).getVoltageResolution());
        Channel::get(0).prot_conf.u_level = coupledLevel;
        Channel::get(1).prot_conf.u_level = coupledLevel;
    } else if (channel.flags.trackingEnabled) {
        level = roundTrackingValuePrecision(UNIT_VOLT, level);
        for (int i = 0; i < CH_NUM; ++i) {
            Channel &trackingChannel = Channel::get(i);
            if (trackingChannel.flags.trackingEnabled) {
                trackingChannel.prot_conf.u_level = level;
            }
        }
    } else {
        channel.prot_conf.u_level = roundPrec(level, channel.getVoltageResolution());
    }
}

void setOvpDelay(Channel &channel, float delay) {
    delay = roundPrec(delay, 0.001f);

    if (channel.channelIndex < 2 && (g_couplingType == COUPLING_TYPE_SERIES || g_couplingType == COUPLING_TYPE_PARALLEL)) {
        Channel::get(0).prot_conf.u_delay = delay;
        Channel::get(1).prot_conf.u_delay = delay;
    } else if (channel.flags.trackingEnabled) {
        for (int i = 0; i < CH_NUM; ++i) {
            Channel &trackingChannel = Channel::get(i);
            if (trackingChannel.flags.trackingEnabled) {
                trackingChannel.prot_conf.u_delay = delay;
            }
        }
    } else {
        channel.prot_conf.u_delay = delay;
    }
}

float getISet(const Channel &channel) {
    if (channel.channelIndex < 2 && g_couplingType == COUPLING_TYPE_PARALLEL) {
        return Channel::get(0).getISet() + Channel::get(1).getISet();
    }
    return channel.getISet();
}

float getISet(int slotIndex, int subchannelIndex) {
    Channel *channel = Channel::getBySlotIndex(slotIndex, subchannelIndex);
    if (channel) {
        return getISet(*channel);
    }

    float value;
    getCurrent(slotIndex, subchannelIndex, value, nullptr);
    return value;
}

float getIMon(const Channel &channel) {
    if (channel.channelIndex < 2 && g_couplingType == COUPLING_TYPE_PARALLEL) {
        return Channel::get(0).i.mon + Channel::get(1).i.mon;
    }
    return channel.i.mon;
}

float getIMonLast(const Channel &channel) {
    if (channel.channelIndex < 2 && g_couplingType == COUPLING_TYPE_PARALLEL) {
        return Channel::get(0).i.mon_last + Channel::get(1).i.mon_last;
    }
    return channel.i.mon_last;
}

float getIMonLast(int slotIndex, int subchannelIndex) {
    Channel *channel = Channel::getBySlotIndex(slotIndex, subchannelIndex);
    if (channel) {
        return getIMonLast(*channel);
    }

    float value;
    getMeasuredCurrent(slotIndex, subchannelIndex, value, nullptr);
    return value;
}

float getIMonDac(const Channel &channel) {
    if (channel.channelIndex < 2 && g_couplingType == COUPLING_TYPE_PARALLEL) {
        return Channel::get(0).i.mon_dac + Channel::get(1).i.mon_dac;
    }
    return channel.i.mon_dac;
}

float getILimit(const Channel &channel) {
    if (channel.channelIndex < 2 && g_couplingType == COUPLING_TYPE_PARALLEL) {
        return 2 * MIN(Channel::get(0).getCurrentLimit(), Channel::get(1).getCurrentLimit());
    }
    return channel.getCurrentLimit();
}

float getIMaxLimit(const Channel &channel) {
    if (channel.channelIndex < 2 && g_couplingType == COUPLING_TYPE_PARALLEL) {
        return 2 * MIN(Channel::get(0).getMaxCurrentLimit(), Channel::get(1).getMaxCurrentLimit());
    } else if (channel.channelIndex < 2 && g_couplingType == COUPLING_TYPE_SERIES) {
        return MIN(Channel::get(0).getMaxCurrentLimit(), Channel::get(1).getMaxCurrentLimit());
    } else if (channel.flags.trackingEnabled) {
        float value = channel.getMaxCurrentLimit();
        for (int i = 0; i < CH_NUM; ++i) {
            if (i != channel.channelIndex) {
                const Channel &trackingChannel = Channel::get(i);
                if (trackingChannel.flags.trackingEnabled) {
                    value = MIN(value, trackingChannel.getMaxCurrentLimit());
                }
            }
        }
        return value;
    }
    return channel.getMaxCurrentLimit();
}

float getIMaxLimit(int slotIndex, int subchannelIndex) {
    Channel *channel = Channel::getBySlotIndex(slotIndex, subchannelIndex);
    if (channel) {
        return getIMaxLimit(*channel);
    }

    return getCurrentMaxValue(slotIndex, subchannelIndex);
}

float getIMin(const Channel &channel) {
    if (channel.channelIndex < 2 && g_couplingType == COUPLING_TYPE_PARALLEL) {
        return 2 * MAX(Channel::get(0).i.min, Channel::get(1).i.min);
    } else if (channel.channelIndex < 2 && g_couplingType == COUPLING_TYPE_SERIES) {
        return MAX(Channel::get(0).i.min, Channel::get(1).i.min);
    } else if (channel.flags.trackingEnabled) {
        float value = channel.i.min;
        for (int i = 0; i < CH_NUM; ++i) {
            if (i != channel.channelIndex) {
                const Channel &trackingChannel = Channel::get(i);
                if (trackingChannel.flags.trackingEnabled) {
                    value = MAX(value, trackingChannel.i.min);
                }
            }
        }
        return value;
    }
    return channel.i.min;
}

float getIMin(int slotIndex, int subchannelIndex) {
    Channel *channel = Channel::getBySlotIndex(slotIndex, subchannelIndex);
    if (channel) {
        return getIMin(*channel);
    }

    return getCurrentMinValue(slotIndex, subchannelIndex);
}

float getIDef(const Channel &channel) {
    if (channel.channelIndex < 2 && g_couplingType == COUPLING_TYPE_PARALLEL) {
        return Channel::get(0).i.def + Channel::get(1).i.def;
    }
    return channel.i.def;
}

float getIDef(int slotIndex, int subchannelIndex) {
    Channel *channel = Channel::getBySlotIndex(slotIndex, subchannelIndex);
    if (channel) {
        return getIDef(*channel);
    }

    return getCurrentMinValue(slotIndex, subchannelIndex);
}

float getIMax(const Channel &channel) {
    if (channel.channelIndex < 2 && g_couplingType == COUPLING_TYPE_PARALLEL) {
        return 2 * MIN(Channel::get(0).i.max, Channel::get(1).i.max);
    } else if (channel.channelIndex < 2 && g_couplingType == COUPLING_TYPE_SERIES) {
        return MIN(Channel::get(0).i.max, Channel::get(1).i.max);
    } else if (channel.flags.trackingEnabled) {
        float value = channel.i.max;
        for (int i = 0; i < CH_NUM; ++i) {
            if (i != channel.channelIndex) {
                const Channel &trackingChannel = Channel::get(i);
                if (trackingChannel.flags.trackingEnabled) {
                    value = MIN(value, trackingChannel.i.max);
                }
            }
        }
        return value;
    }
    return channel.i.max;
}

float getIMax(int slotIndex, int subchannelIndex) {
    Channel *channel = Channel::getBySlotIndex(slotIndex, subchannelIndex);
    if (channel) {
        return getIMax(*channel);
    }

    return getCurrentMaxValue(slotIndex, subchannelIndex);
}

static float g_setCurrentValues[CH_MAX];

void setCurrentInPsuThread(int channelIndex) {
    setCurrent(Channel::get(channelIndex), g_setCurrentValues[channelIndex]);
}

void setCurrent(Channel &channel, float current) {
    if (!isPsuThread()) {
        g_setCurrentValues[channel.channelIndex] = current;
        sendMessageToPsu(PSU_MESSAGE_SET_CURRENT, channel.channelIndex);
        return;
    }

    if (channel.channelIndex < 2 && g_couplingType == COUPLING_TYPE_PARALLEL) {
        Channel::get(0).setCurrent(current / 2);
        Channel::get(1).setCurrent(current / 2);
    } else if (channel.channelIndex < 2 && g_couplingType == COUPLING_TYPE_SERIES) {
        Channel::get(0).setCurrent(current);
        Channel::get(1).setCurrent(current);
    } else if (channel.flags.trackingEnabled) {
        current = roundTrackingValuePrecision(UNIT_AMPER, current);
        for (int i = 0; i < CH_NUM; ++i) {
            Channel &trackingChannel = Channel::get(i);
            if (trackingChannel.flags.trackingEnabled) {
                trackingChannel.setCurrent(current);
            }
        }
    } else {
        channel.setCurrent(current);
    }
}

void setCurrentStep(Channel &channel, float currentStep) {
    if (channel.channelIndex < 2 && (g_couplingType == COUPLING_TYPE_SERIES || g_couplingType == COUPLING_TYPE_PARALLEL)) {
        Channel::get(0).i.step = currentStep;
        Channel::get(1).i.step = currentStep;
    } else if (channel.flags.trackingEnabled) {
        currentStep = roundTrackingValuePrecision(UNIT_AMPER, currentStep);

        for (int i = 0; i < CH_NUM; ++i) {
            Channel &trackingChannel = Channel::get(i);
            if (trackingChannel.flags.trackingEnabled) {
                trackingChannel.i.step = currentStep;
            }
        }
    } else {
        channel.i.step = currentStep;
    }
}

void setCurrentLimit(Channel &channel, float limit) {
    if (channel.channelIndex < 2 && g_couplingType == COUPLING_TYPE_PARALLEL) {
        Channel::get(0).setCurrentLimit(limit / 2);
        Channel::get(1).setCurrentLimit(limit / 2);
    } else if (channel.channelIndex < 2 && g_couplingType == COUPLING_TYPE_SERIES) {
        Channel::get(0).setCurrentLimit(limit);
        Channel::get(1).setCurrentLimit(limit);
    } else if (channel.flags.trackingEnabled) {
        limit = roundTrackingValuePrecision(UNIT_AMPER, limit);
        for (int i = 0; i < CH_NUM; ++i) {
            Channel &trackingChannel = Channel::get(i);
            if (trackingChannel.flags.trackingEnabled) {
                trackingChannel.setCurrentLimit(limit);
            }
        }
    } else {
        channel.setCurrentLimit(limit);
    }
}

void setOcpParameters(Channel &channel, int state, float delay) {
    if (!state && channel_dispatcher::isOcpTripped(channel)) {
        clearOcpProtection(channel);
    }

    delay = roundPrec(delay, 0.001f);

    if (channel.channelIndex < 2 && (g_couplingType == COUPLING_TYPE_SERIES || g_couplingType == COUPLING_TYPE_PARALLEL)) {
        Channel::get(0).prot_conf.flags.i_state = state;
        Channel::get(0).prot_conf.i_delay = delay;

        Channel::get(1).prot_conf.flags.i_state = state;
        Channel::get(1).prot_conf.i_delay = delay;
    } else if (channel.flags.trackingEnabled) {
        for (int i = 0; i < CH_NUM; ++i) {
            Channel &trackingChannel = Channel::get(i);
            if (trackingChannel.flags.trackingEnabled) {
                trackingChannel.prot_conf.flags.i_state = state;
                trackingChannel.prot_conf.i_delay = delay;
            }
        }
    } else {
        channel.prot_conf.flags.i_state = state;
        channel.prot_conf.i_delay = delay;
    }
}

void setOcpState(Channel &channel, int state) {
    if (channel.channelIndex < 2 && (g_couplingType == COUPLING_TYPE_SERIES || g_couplingType == COUPLING_TYPE_PARALLEL)) {
        Channel::get(0).prot_conf.flags.i_state = state;
    } else if (channel.flags.trackingEnabled) {
        for (int i = 0; i < CH_NUM; ++i) {
            Channel &trackingChannel = Channel::get(i);
            if (trackingChannel.flags.trackingEnabled) {
                trackingChannel.prot_conf.flags.i_state = state;
            }
        }
    } else {
        channel.prot_conf.flags.i_state = state;
    }
}

void setOcpDelay(Channel &channel, float delay) {
    delay = roundPrec(delay, 0.001f);

    if (channel.channelIndex < 2 && (g_couplingType == COUPLING_TYPE_SERIES || g_couplingType == COUPLING_TYPE_PARALLEL)) {
        Channel::get(0).prot_conf.i_delay = delay;
        Channel::get(1).prot_conf.i_delay = delay;
    } else if (channel.flags.trackingEnabled) {
        for (int i = 0; i < CH_NUM; ++i) {
            Channel &trackingChannel = Channel::get(i);
            if (trackingChannel.flags.trackingEnabled) {
                trackingChannel.prot_conf.i_delay = delay;
            }
        }
    } else {
        channel.prot_conf.i_delay = delay;
    }
}

float getPowerLimit(const Channel &channel) {
    if (channel.channelIndex < 2 && (g_couplingType == COUPLING_TYPE_SERIES || g_couplingType == COUPLING_TYPE_PARALLEL)) {
        return 2 * MIN(Channel::get(0).getPowerLimit(), Channel::get(1).getPowerLimit());
    }
    return channel.getPowerLimit();
}

float getPowerMinLimit(const Channel &channel) {
    return 0;
}

float getPowerMaxLimit(const Channel &channel) {
    if (channel.channelIndex < 2 && (g_couplingType == COUPLING_TYPE_SERIES || g_couplingType == COUPLING_TYPE_PARALLEL)) {
        return 2 * MIN(Channel::get(0).params.PTOT, Channel::get(1).params.PTOT);
    }
    return channel.params.PTOT;
}

float getPowerDefaultLimit(const Channel &channel) {
    return getPowerMaxLimit(channel);
}

float getPowerProtectionLevel(const Channel &channel) {
    if (channel.channelIndex < 2 && (g_couplingType == COUPLING_TYPE_SERIES || g_couplingType == COUPLING_TYPE_PARALLEL)) {
        return Channel::get(0).prot_conf.p_level + Channel::get(1).prot_conf.p_level;
    }
    return channel.prot_conf.p_level;
}

void setPowerLimit(Channel &channel, float limit) {
    if (channel.channelIndex < 2 && (g_couplingType == COUPLING_TYPE_SERIES || g_couplingType == COUPLING_TYPE_PARALLEL)) {
        Channel::get(0).setPowerLimit(limit / 2);
        Channel::get(1).setPowerLimit(limit / 2);
    } else if (channel.flags.trackingEnabled) {
        limit = roundTrackingValuePrecision(UNIT_WATT, limit);
        for (int i = 0; i < CH_NUM; ++i) {
            Channel &trackingChannel = Channel::get(i);
            if (trackingChannel.flags.trackingEnabled) {
                trackingChannel.setPowerLimit(limit);
            }
        }
    } else {
        channel.setPowerLimit(limit);
    }

    if (getOppLevel(channel) > getPowerLimit(channel)) {
        setOppLevel(channel, getPowerLimit(channel));
    }
}

float getOppLevel(Channel &channel) {
    if (channel.channelIndex < 2 && (g_couplingType == COUPLING_TYPE_SERIES || g_couplingType == COUPLING_TYPE_PARALLEL)) {
        return Channel::get(0).prot_conf.p_level + Channel::get(1).prot_conf.p_level;
    }
    return channel.prot_conf.p_level;
}

float getOppMinLevel(Channel &channel) {
    if (channel.channelIndex < 2 && (g_couplingType == COUPLING_TYPE_SERIES || g_couplingType == COUPLING_TYPE_PARALLEL)) {
        return 2 * MAX(Channel::get(0).params.OPP_MIN_LEVEL, Channel::get(1).params.OPP_MIN_LEVEL);
    }
    return channel.params.OPP_MIN_LEVEL;
}

float getOppMaxLevel(Channel &channel) {
    return getPowerLimit(channel);
}

float getOppDefaultLevel(Channel &channel) {
    if (channel.channelIndex < 2 && (g_couplingType == COUPLING_TYPE_SERIES || g_couplingType == COUPLING_TYPE_PARALLEL)) {
        return Channel::get(0).params.OPP_DEFAULT_LEVEL + Channel::get(1).params.OPP_DEFAULT_LEVEL;
    }
    return channel.params.OPP_DEFAULT_LEVEL;
}

void setOppParameters(Channel &channel, int state, float level, float delay) {
    if (!state && channel_dispatcher::isOppTripped(channel)) {
        clearOppProtection(channel);
    }
    
    delay = roundPrec(delay, 0.001f);

    if (channel.channelIndex < 2 && (g_couplingType == COUPLING_TYPE_SERIES || g_couplingType == COUPLING_TYPE_PARALLEL)) {
        float coupledLevel = level / 2;

        coupledLevel = roundPrec(coupledLevel, Channel::get(0).getPowerResolution());

        Channel::get(0).prot_conf.flags.p_state = state;
        Channel::get(0).prot_conf.p_level = coupledLevel;
        Channel::get(0).prot_conf.p_delay = delay;

        Channel::get(1).prot_conf.flags.p_state = state;
        Channel::get(1).prot_conf.p_level = coupledLevel;
        Channel::get(1).prot_conf.p_delay = delay;
    } else if (channel.flags.trackingEnabled) {
        level = roundTrackingValuePrecision(UNIT_WATT, level);
        for (int i = 0; i < CH_NUM; ++i) {
            Channel &trackingChannel = Channel::get(i);
            if (trackingChannel.flags.trackingEnabled) {
                trackingChannel.prot_conf.flags.p_state = state;
                trackingChannel.prot_conf.p_level = level;
                trackingChannel.prot_conf.p_delay = delay;
            }
        }
    } else {
        channel.prot_conf.flags.p_state = state;
        channel.prot_conf.p_level = roundPrec(level, channel.getPowerResolution());
        channel.prot_conf.p_delay = delay;
    }
}

void setOppState(Channel &channel, int state) {
    if (channel.channelIndex < 2 && (g_couplingType == COUPLING_TYPE_SERIES || g_couplingType == COUPLING_TYPE_PARALLEL)) {
        Channel::get(0).prot_conf.flags.p_state = state;
        Channel::get(1).prot_conf.flags.p_state = state;
    } else if (channel.flags.trackingEnabled) {
        for (int i = 0; i < CH_NUM; ++i) {
            Channel &trackingChannel = Channel::get(i);
            if (trackingChannel.flags.trackingEnabled) {
                trackingChannel.prot_conf.flags.p_state = state;
            }
        }
    } else {
        channel.prot_conf.flags.p_state = state;
    }
}

void setOppLevel(Channel &channel, float level) {
    if (channel.channelIndex < 2 && (g_couplingType == COUPLING_TYPE_SERIES || g_couplingType == COUPLING_TYPE_PARALLEL)) {
        float coupledLevel = level / 2;

        coupledLevel = roundPrec(coupledLevel, Channel::get(0).getPowerResolution());

        Channel::get(0).prot_conf.p_level = coupledLevel;
        Channel::get(1).prot_conf.p_level = coupledLevel;
    } else if (channel.flags.trackingEnabled) {
        level = roundTrackingValuePrecision(UNIT_WATT, level);
        for (int i = 0; i < CH_NUM; ++i) {
            Channel &trackingChannel = Channel::get(i);
            if (trackingChannel.flags.trackingEnabled) {
                trackingChannel.prot_conf.p_level = level;
            }
        }
    } else {
        channel.prot_conf.p_level = roundPrec(level, channel.getPowerResolution());
    }
}

void setOppDelay(Channel &channel, float delay) {
    delay = roundPrec(delay, 0.001f);

    if (channel.channelIndex < 2 && (g_couplingType == COUPLING_TYPE_SERIES || g_couplingType == COUPLING_TYPE_PARALLEL)) {
        Channel::get(0).prot_conf.p_delay = delay;
        Channel::get(1).prot_conf.p_delay = delay;
    } else if (channel.flags.trackingEnabled) {
        for (int i = 0; i < CH_NUM; ++i) {
            Channel &trackingChannel = Channel::get(i);
            if (trackingChannel.flags.trackingEnabled) {
                trackingChannel.prot_conf.p_delay = delay;
            }
        }
    } else {
        channel.prot_conf.p_delay = delay;
    }
}

void setVoltageRampDuration(Channel &channel, float duration) {
    duration = roundPrec(duration, RAMP_DURATION_PREC);

    if (channel.channelIndex < 2 && (g_couplingType == COUPLING_TYPE_SERIES || g_couplingType == COUPLING_TYPE_PARALLEL)) {
        Channel::get(0).u.rampDuration = duration;
        Channel::get(1).u.rampDuration = duration;
    } else if (channel.flags.trackingEnabled) {
        for (int i = 0; i < CH_NUM; ++i) {
            Channel &trackingChannel = Channel::get(i);
            if (trackingChannel.flags.trackingEnabled) {
                trackingChannel.u.rampDuration = duration;
            }
        }
    } else {
        channel.u.rampDuration = duration;
    }
}

void setCurrentRampDuration(Channel &channel, float duration) {
    duration = roundPrec(duration, RAMP_DURATION_PREC);

    if (channel.channelIndex < 2 && (g_couplingType == COUPLING_TYPE_SERIES || g_couplingType == COUPLING_TYPE_PARALLEL)) {
        Channel::get(0).i.rampDuration = duration;
        Channel::get(1).i.rampDuration = duration;
    } else if (channel.flags.trackingEnabled) {
        for (int i = 0; i < CH_NUM; ++i) {
            Channel &trackingChannel = Channel::get(i);
            if (trackingChannel.flags.trackingEnabled) {
                trackingChannel.i.rampDuration = duration;
            }
        }
    } else {
        channel.i.rampDuration = duration;
    }
}

void setOutputDelayDuration(Channel &channel, float duration) {
    duration = roundPrec(duration, RAMP_DURATION_PREC);

    if (channel.channelIndex < 2 && (g_couplingType == COUPLING_TYPE_SERIES || g_couplingType == COUPLING_TYPE_PARALLEL)) {
        Channel::get(0).outputDelayDuration = duration;
        Channel::get(1).outputDelayDuration = duration;
    } else if (channel.flags.trackingEnabled) {
        for (int i = 0; i < CH_NUM; ++i) {
            Channel &trackingChannel = Channel::get(i);
            if (trackingChannel.flags.trackingEnabled) {
                trackingChannel.outputDelayDuration = duration;
            }
        }
    } else {
        channel.outputDelayDuration = duration;
    }
}

void outputEnable(Channel &channel, bool enable) {
    outputEnableOnNextSync(channel, enable);
    syncOutputEnable();
}

void outputEnableOnNextSync(Channel &channel, bool enable) {
    if (channel.channelIndex < 2 && (g_couplingType == COUPLING_TYPE_SERIES || g_couplingType == COUPLING_TYPE_PARALLEL)) {
        Channel::get(0).flags.doOutputEnableOnNextSync = 1;
        Channel::get(0).flags.outputEnabledValueOnNextSync = enable;

        Channel::get(1).flags.doOutputEnableOnNextSync = 1;
        Channel::get(1).flags.outputEnabledValueOnNextSync = enable;
    } else if (channel.flags.trackingEnabled) {
        for (int i = 0; i < CH_NUM; ++i) {
            Channel &trackingChannel = Channel::get(i);
            if (trackingChannel.flags.trackingEnabled) {
                trackingChannel.flags.doOutputEnableOnNextSync = 1;
                trackingChannel.flags.outputEnabledValueOnNextSync = enable;
            }
        }
    } else {
        channel.flags.doOutputEnableOnNextSync = 1;
        channel.flags.outputEnabledValueOnNextSync = enable;
    }
}

void syncOutputEnable() {
    if (!isPsuThread()) {
        sendMessageToPsu(PSU_MESSAGE_SYNC_OUTPUT_ENABLE);
    } else {
        Channel::syncOutputEnable();
    }
}

bool testOutputEnable(Channel &channel, bool enable, bool &callTriggerAbort, int *err) {
    if (enable != channel.isOutputEnabled()) {
        bool triggerModeEnabled = getVoltageTriggerMode(channel) != TRIGGER_MODE_FIXED || getCurrentTriggerMode(channel) != TRIGGER_MODE_FIXED;

        if (channel.isOutputEnabled()) {
            if (calibration::isChannelCalibrating(channel)) {
                if (err) {
                    *err = SCPI_ERROR_CAL_OUTPUT_DISABLED;
                }
                return false;
            }

            if (triggerModeEnabled && !trigger::isIdle()) {
                callTriggerAbort = true;
            }
        } else {
            int channelIndex;
            if (isTripped(channel, channelIndex)) {
                if (err) {
                    *err = SCPI_ERROR_CANNOT_EXECUTE_BEFORE_CLEARING_PROTECTION;
                }
                return false;
            }

            if (triggerModeEnabled && !trigger::isIdle()) {
                if (trigger::isInitiated()) {
                    callTriggerAbort = true;
                } else {
                    if (err) {
                        *err = SCPI_ERROR_CANNOT_CHANGE_TRANSIENT_TRIGGER;
                    }
                    return false;
                }
            }
        }
    }

    return true;
}

bool outputEnable(int numChannels, uint8_t *channels, bool enable, int *err) {
    bool callTriggerAbort = false;

    for (int i = 0; i < numChannels; i++) {
        if (!testOutputEnable(Channel::get(channels[i]), enable, callTriggerAbort, err)) {
            return false;
        }
    }

    if (callTriggerAbort) {
        trigger::abort();
    } else {
        for (int i = 0; i < numChannels; i++) {
            outputEnableOnNextSync(Channel::get(channels[i]), enable);
        }
        syncOutputEnable();
    }

    return true;
}

void disableOutputForAllChannels() {
    for (int i = 0; i < CH_NUM; i++) {
        Channel &channel = Channel::get(i);
        if (channel.isOutputEnabled()) {
            outputEnableOnNextSync(channel, false);
        }
    }

    syncOutputEnable();
}

void disableOutputForAllTrackingChannels() {
    for (int i = 0; i < CH_NUM; i++) {
        Channel &channel = Channel::get(i);
        if (channel.flags.trackingEnabled && channel.isOutputEnabled()) {
            outputEnableOnNextSync(channel, false);
        }
    }

    syncOutputEnable();
}

void calibrationEnable(Channel &channel, bool enable) {
    if (!isPsuThread()) {
        sendMessageToPsu(PSU_MESSAGE_CALIBRATION_ENABLE, (channel.channelIndex << 8) | (enable ? 1 : 0));
    } else {
        channel.calibrationEnable(enable);
    }
}

void remoteSensingEnable(Channel &channel, bool enable) {
    if (!isPsuThread()) {
        sendMessageToPsu(PSU_MESSAGE_REMOTE_SENSING_EANBLE, (channel.channelIndex << 8) | (enable ? 1 : 0));
    } else {
        if (channel.channelIndex < 2 && (g_couplingType == COUPLING_TYPE_SERIES || g_couplingType == COUPLING_TYPE_PARALLEL)) {
            Channel::get(0).remoteSensingEnable(enable);
            Channel::get(1).remoteSensingEnable(enable);
        } else if (channel.flags.trackingEnabled) {
            for (int i = 0; i < CH_NUM; ++i) {
                Channel &trackingChannel = Channel::get(i);
                if (trackingChannel.flags.trackingEnabled) {
                    trackingChannel.remoteSensingEnable(enable);
                }
            }
        } else {
            channel.remoteSensingEnable(enable);
        }
    }
}

bool isTripped(Channel &channel, int &channelIndex) {
    if (channel.channelIndex < 2 && (g_couplingType == COUPLING_TYPE_SERIES || g_couplingType == COUPLING_TYPE_PARALLEL)) {
        if (Channel::get(0).isTripped()) {
            channelIndex = 0;
            return true;
        }
        channelIndex = 1;
        return Channel::get(1).isTripped();
    } else if (channel.flags.trackingEnabled) {
        for (int i = 0; i < CH_NUM; ++i) {
            Channel &trackingChannel = Channel::get(i);
            if (trackingChannel.flags.trackingEnabled && trackingChannel.isTripped()) {
                channelIndex = i;
                return true;
            }
        }
        return false;
    } else {
        if (channel.isTripped()) {
            channelIndex = channel.channelIndex;
            return true;
        }

        bool triggerModeEnabled = getVoltageTriggerMode(channel) != TRIGGER_MODE_FIXED || getCurrentTriggerMode(channel) != TRIGGER_MODE_FIXED;
        if (triggerModeEnabled) {
            for (int i = 0; i < CH_NUM; ++i) {
                if (i != channel.channelIndex) {
                    Channel &otherChannel = Channel::get(i);
                    bool triggerModeEnabled = getVoltageTriggerMode(otherChannel) != TRIGGER_MODE_FIXED || getCurrentTriggerMode(otherChannel) != TRIGGER_MODE_FIXED;
                    if (triggerModeEnabled && otherChannel.isTripped()) {
                        channelIndex = i;
                        return true;
                    }
                }
            }
        }
        return false;
    }
}

void clearProtection(Channel &channel) {
    if (channel.channelIndex < 2 && (g_couplingType == COUPLING_TYPE_SERIES || g_couplingType == COUPLING_TYPE_PARALLEL)) {
        Channel::get(0).clearProtection();
        Channel::get(1).clearProtection();
    } else if (channel.flags.trackingEnabled) {
        for (int i = 0; i < CH_NUM; ++i) {
            Channel &trackingChannel = Channel::get(i);
            if (trackingChannel.flags.trackingEnabled) {
                trackingChannel.clearProtection();
            }
        }
    } else {
        channel.clearProtection();
    }
}

void clearOvpProtection(Channel &channel) {
    if (channel.channelIndex < 2 && (g_couplingType == COUPLING_TYPE_SERIES || g_couplingType == COUPLING_TYPE_PARALLEL)) {
        Channel::get(0).clearOvpProtection();
        Channel::get(1).clearOvpProtection();
    } else if (channel.flags.trackingEnabled) {
        for (int i = 0; i < CH_NUM; ++i) {
            Channel &trackingChannel = Channel::get(i);
            if (trackingChannel.flags.trackingEnabled) {
                trackingChannel.clearOvpProtection();
            }
        }
    } else {
        channel.clearOvpProtection();
    }
}

void clearOcpProtection(Channel &channel) {
    if (channel.channelIndex < 2 && (g_couplingType == COUPLING_TYPE_SERIES || g_couplingType == COUPLING_TYPE_PARALLEL)) {
        Channel::get(0).clearOcpProtection();
        Channel::get(1).clearOcpProtection();
    } else if (channel.flags.trackingEnabled) {
        for (int i = 0; i < CH_NUM; ++i) {
            Channel &trackingChannel = Channel::get(i);
            if (trackingChannel.flags.trackingEnabled) {
                trackingChannel.clearOcpProtection();
            }
        }
    } else {
        channel.clearOcpProtection();
    }
}

void clearOppProtection(Channel &channel) {
    if (channel.channelIndex < 2 && (g_couplingType == COUPLING_TYPE_SERIES || g_couplingType == COUPLING_TYPE_PARALLEL)) {
        Channel::get(0).clearOppProtection();
        Channel::get(1).clearOppProtection();
    } else if (channel.flags.trackingEnabled) {
        for (int i = 0; i < CH_NUM; ++i) {
            Channel &trackingChannel = Channel::get(i);
            if (trackingChannel.flags.trackingEnabled) {
                trackingChannel.clearOppProtection();
            }
        }
    } else {
        channel.clearOppProtection();
    }
}

void clearOtpProtection(Channel &channel) {
    if (channel.channelIndex < 2 && (g_couplingType == COUPLING_TYPE_SERIES || g_couplingType == COUPLING_TYPE_PARALLEL)) {
        Channel::get(0).clearOtpProtection();
        Channel::get(1).clearOtpProtection();
    } else if (channel.flags.trackingEnabled) {
        for (int i = 0; i < CH_NUM; ++i) {
            Channel &trackingChannel = Channel::get(i);
            if (trackingChannel.flags.trackingEnabled) {
                trackingChannel.clearOtpProtection();
            }
        }
    } else {
        channel.clearOtpProtection();
    }
}

void disableProtection(Channel &channel) {
    if (channel.channelIndex < 2 && (g_couplingType == COUPLING_TYPE_SERIES || g_couplingType == COUPLING_TYPE_PARALLEL)) {
        Channel::get(0).disableProtection();
        Channel::get(1).disableProtection();
    } else if (channel.flags.trackingEnabled) {
        for (int i = 0; i < CH_NUM; ++i) {
            Channel &trackingChannel = Channel::get(i);
            if (trackingChannel.flags.trackingEnabled) {
                trackingChannel.disableProtection();
            }
        }
    } else {
        channel.disableProtection();
    }
}

bool isOvpTripped(Channel &channel) {
    if (channel.channelIndex < 2 && (g_couplingType == COUPLING_TYPE_SERIES || g_couplingType == COUPLING_TYPE_PARALLEL)) {
        return Channel::get(0).ovp.flags.tripped || Channel::get(1).ovp.flags.tripped;
    } else if (channel.flags.trackingEnabled) {
        for (int i = 0; i < CH_NUM; ++i) {
            Channel &trackingChannel = Channel::get(i);
            if (trackingChannel.flags.trackingEnabled && trackingChannel.ovp.flags.tripped) {
                return true;
            }
        }
        return false;
    } else {
        return channel.ovp.flags.tripped;
    }
}

bool isOcpTripped(Channel &channel) {
    if (channel.channelIndex < 2 && (g_couplingType == COUPLING_TYPE_SERIES || g_couplingType == COUPLING_TYPE_PARALLEL)) {
        return Channel::get(0).ocp.flags.tripped || Channel::get(1).ocp.flags.tripped;
    } else if (channel.flags.trackingEnabled) {
        for (int i = 0; i < CH_NUM; ++i) {
            Channel &trackingChannel = Channel::get(i);
            if (trackingChannel.flags.trackingEnabled && trackingChannel.ocp.flags.tripped) {
                return true;
            }
        }
        return false;
    } else {
        return channel.ocp.flags.tripped;
    }
}

bool isOppTripped(Channel &channel) {
    if (channel.channelIndex < 2 && (g_couplingType == COUPLING_TYPE_SERIES || g_couplingType == COUPLING_TYPE_PARALLEL)) {
        return Channel::get(0).opp.flags.tripped || Channel::get(1).opp.flags.tripped;
    } else if (channel.flags.trackingEnabled) {
        for (int i = 0; i < CH_NUM; ++i) {
            Channel &trackingChannel = Channel::get(i);
            if (trackingChannel.flags.trackingEnabled && trackingChannel.opp.flags.tripped) {
                return true;
            }
        }
        return false;
    } else {
        return channel.opp.flags.tripped;
    }
}

bool isOtpTripped(Channel &channel) {
    if (channel.channelIndex < 2 && (g_couplingType == COUPLING_TYPE_SERIES || g_couplingType == COUPLING_TYPE_PARALLEL)) {
        return temperature::sensors[temp_sensor::CH1].isTripped() || temperature::sensors[temp_sensor::CH2].isTripped();
    } else if (channel.flags.trackingEnabled) {
        for (int i = 0; i < CH_NUM; ++i) {
            Channel &trackingChannel = Channel::get(i);
            if (trackingChannel.flags.trackingEnabled && temperature::sensors[temp_sensor::CH1 + i].isTripped()) {
                return true;
            }
        }
        return false;
    } else {
        return temperature::sensors[temp_sensor::CH1 + channel.channelIndex].isTripped();
    }
}

void clearOtpProtection(int sensor) {
    if ((sensor == temp_sensor::CH1 || sensor == temp_sensor::CH2) && (g_couplingType == COUPLING_TYPE_SERIES || g_couplingType == COUPLING_TYPE_PARALLEL)) {
        temperature::sensors[temp_sensor::CH1].clearProtection();
        temperature::sensors[temp_sensor::CH2].clearProtection();
    } else if (sensor >= temp_sensor::CH1 && Channel::get(sensor - temp_sensor::CH1).flags.trackingEnabled) {
        for (int i = 0; i < CH_NUM; ++i) {
            Channel &trackingChannel = Channel::get(i);
            if (trackingChannel.flags.trackingEnabled) {
                temperature::sensors[temp_sensor::CH1 + i].clearProtection();
            }
        }
    } else {
        temperature::sensors[sensor].clearProtection();
    }
}

void setOtpParameters(Channel &channel, int state, float level, float delay) {
    if (!state && channel_dispatcher::isOtpTripped(channel)) {
        clearOtpProtection(channel);
    }
    
    delay = roundPrec(delay, 0.001f);
    level = roundPrec(level, 1);

    if (channel.channelIndex < 2 && (g_couplingType == COUPLING_TYPE_SERIES || g_couplingType == COUPLING_TYPE_PARALLEL)) {
        temperature::sensors[temp_sensor::CH1].prot_conf.state = state ? true : false;
        temperature::sensors[temp_sensor::CH1].prot_conf.level = level;
        temperature::sensors[temp_sensor::CH1].prot_conf.delay = delay;

        temperature::sensors[temp_sensor::CH2].prot_conf.state = state ? true : false;
        temperature::sensors[temp_sensor::CH2].prot_conf.level = level;
        temperature::sensors[temp_sensor::CH2].prot_conf.delay = delay;
    } else if (channel.flags.trackingEnabled) {
        for (int i = 0; i < CH_NUM; ++i) {
            Channel &trackingChannel = Channel::get(i);
            if (trackingChannel.flags.trackingEnabled) {
                temperature::sensors[temp_sensor::CH1 + i].prot_conf.state = state ? true : false;
                temperature::sensors[temp_sensor::CH1 + i].prot_conf.level = level;
                temperature::sensors[temp_sensor::CH1 + i].prot_conf.delay = delay;
            }
        }
    } else {
        temperature::sensors[temp_sensor::CH1 + channel.channelIndex].prot_conf.state = state ? true : false;
        temperature::sensors[temp_sensor::CH1 + channel.channelIndex].prot_conf.level = level;
        temperature::sensors[temp_sensor::CH1 + channel.channelIndex].prot_conf.delay = delay;
    }
}

void setOtpState(int sensor, int state) {
    if ((sensor == temp_sensor::CH1 || sensor == temp_sensor::CH2) && (g_couplingType == COUPLING_TYPE_SERIES || g_couplingType == COUPLING_TYPE_PARALLEL)) {
        temperature::sensors[temp_sensor::CH1].prot_conf.state = state ? true : false;
        temperature::sensors[temp_sensor::CH2].prot_conf.state = state ? true : false;
    } else if (sensor >= temp_sensor::CH1 && Channel::get(sensor - temp_sensor::CH1).flags.trackingEnabled) {
        for (int i = 0; i < CH_NUM; ++i) {
            Channel &trackingChannel = Channel::get(i);
            if (trackingChannel.flags.trackingEnabled) {
                temperature::sensors[temp_sensor::CH1 + i].prot_conf.state = state ? true : false;
            }
        }
    } else {
        temperature::sensors[sensor].prot_conf.state = state ? true : false;
    }
}

void setOtpLevel(int sensor, float level) {
    level = roundPrec(level, 1);

    if ((sensor == temp_sensor::CH1 || sensor == temp_sensor::CH2) && (g_couplingType == COUPLING_TYPE_SERIES || g_couplingType == COUPLING_TYPE_PARALLEL)) {
        temperature::sensors[temp_sensor::CH1].prot_conf.level = level;
        temperature::sensors[temp_sensor::CH2].prot_conf.level = level;
    } else if (sensor >= temp_sensor::CH1 && Channel::get(sensor - temp_sensor::CH1).flags.trackingEnabled) {
        for (int i = 0; i < CH_NUM; ++i) {
            Channel &trackingChannel = Channel::get(i);
            if (trackingChannel.flags.trackingEnabled) {
                temperature::sensors[temp_sensor::CH1 + i].prot_conf.level = level;
            }
        }
    } else {
        temperature::sensors[sensor].prot_conf.level = level;
    }
}

void setOtpDelay(int sensor, float delay) {
    delay = roundPrec(delay, 0.001f);

    if ((sensor == temp_sensor::CH1 || sensor == temp_sensor::CH2) && (g_couplingType == COUPLING_TYPE_SERIES || g_couplingType == COUPLING_TYPE_PARALLEL)) {
        temperature::sensors[temp_sensor::CH1].prot_conf.delay = delay;
        temperature::sensors[temp_sensor::CH2].prot_conf.delay = delay;
    } else if (sensor >= temp_sensor::CH1 && Channel::get(sensor - temp_sensor::CH1).flags.trackingEnabled) {
        for (int i = 0; i < CH_NUM; ++i) {
            Channel &trackingChannel = Channel::get(i);
            if (trackingChannel.flags.trackingEnabled) {
                temperature::sensors[temp_sensor::CH1 + i].prot_conf.delay = delay;
            }
        }
    } else {
        temperature::sensors[sensor].prot_conf.delay = delay;
    }
}

void setDisplayViewSettings(Channel &channel, DisplayValue *displayValues, float ytViewRate) {
    bool resetHistory = false;

    if (channel.channelIndex < 2 && (g_couplingType == COUPLING_TYPE_SERIES || g_couplingType == COUPLING_TYPE_PARALLEL)) {
        Channel::get(0).displayValues[0] = displayValues[0];
		Channel::get(0).displayValues[1] = displayValues[1];
        if (Channel::get(0).ytViewRate != ytViewRate) {
            Channel::get(0).ytViewRate = ytViewRate;
            resetHistory = true;
        }

		Channel::get(1).displayValues[0] = displayValues[0];
		Channel::get(1).displayValues[1] = displayValues[1];
		if (Channel::get(1).ytViewRate != ytViewRate) {
            Channel::get(1).ytViewRate = ytViewRate;
            resetHistory = true;
        }
    } else if (channel.flags.trackingEnabled) {
        for (int i = 0; i < CH_NUM; ++i) {
            Channel &trackingChannel = Channel::get(i);
            if (trackingChannel.flags.trackingEnabled) {
                trackingChannel.displayValues[1] = displayValues[0];
				trackingChannel.displayValues[1] = displayValues[1];
                if (trackingChannel.ytViewRate != ytViewRate) {
                    trackingChannel.ytViewRate = ytViewRate;
                    resetHistory = true;
                }
            }
        }
    } else {
        channel.displayValues[0] = displayValues[0];
		channel.displayValues[1] = displayValues[1];
        if (channel.ytViewRate != ytViewRate) {
            channel.ytViewRate = ytViewRate;
            resetHistory = true;
        }
    }
    
    if (resetHistory) {
        if (!isPsuThread()) {
            sendMessageToPsu(PSU_MESSAGE_RESET_CHANNELS_HISTORY);
        } else {
            Channel::resetHistoryForAllChannels();
        }
    }
}

TriggerMode getVoltageTriggerMode(Channel &channel) {
    if (channel.channelIndex < 2 && (g_couplingType == COUPLING_TYPE_SERIES || g_couplingType == COUPLING_TYPE_PARALLEL)) {
        return Channel::get(0).getVoltageTriggerMode();
    } else if (channel.flags.trackingEnabled) {
        for (int i = 0; i < CH_NUM; ++i) {
            Channel &trackingChannel = Channel::get(i);
            if (trackingChannel.flags.trackingEnabled) {
                return trackingChannel.getVoltageTriggerMode();
            }
        }
    }
    
    return channel.getVoltageTriggerMode();
}

void setVoltageTriggerMode(Channel &channel, TriggerMode mode) {
    if (channel.channelIndex < 2 && (g_couplingType == COUPLING_TYPE_SERIES || g_couplingType == COUPLING_TYPE_PARALLEL)) {
        Channel::get(0).setVoltageTriggerMode(mode);
    } else if (channel.flags.trackingEnabled) {
        for (int i = 0; i < CH_NUM; ++i) {
            Channel &trackingChannel = Channel::get(i);
            if (trackingChannel.flags.trackingEnabled) {
                trackingChannel.setVoltageTriggerMode(mode);
            }
        }
    } else {
        channel.setVoltageTriggerMode(mode);
    }
}

TriggerMode getCurrentTriggerMode(Channel &channel) {
    if (channel.channelIndex < 2 && (g_couplingType == COUPLING_TYPE_SERIES || g_couplingType == COUPLING_TYPE_PARALLEL)) {
        return Channel::get(0).getCurrentTriggerMode();
    } else if (channel.flags.trackingEnabled) {
        for (int i = 0; i < CH_NUM; ++i) {
            Channel &trackingChannel = Channel::get(i);
            if (trackingChannel.flags.trackingEnabled) {
                return trackingChannel.getCurrentTriggerMode();
            }
        }
    }
    return channel.getCurrentTriggerMode();
}

void setCurrentTriggerMode(Channel &channel, TriggerMode mode) {
    if (channel.channelIndex < 2 && (g_couplingType == COUPLING_TYPE_SERIES || g_couplingType == COUPLING_TYPE_PARALLEL)) {
        Channel::get(0).setCurrentTriggerMode(mode);
    } else if (channel.flags.trackingEnabled) {
        for (int i = 0; i < CH_NUM; ++i) {
            Channel &trackingChannel = Channel::get(i);
            if (trackingChannel.flags.trackingEnabled) {
                trackingChannel.setCurrentTriggerMode(mode);
            }
        }
    } else {
        channel.setCurrentTriggerMode(mode);
    }
}

bool getTriggerOutputState(Channel &channel) {
    if (channel.channelIndex < 2 && (g_couplingType == COUPLING_TYPE_SERIES || g_couplingType == COUPLING_TYPE_PARALLEL)) {
        return Channel::get(0).getTriggerOutputState();
    } else if (channel.flags.trackingEnabled) {
        for (int i = 0; i < CH_NUM; ++i) {
            Channel &trackingChannel = Channel::get(i);
            if (trackingChannel.flags.trackingEnabled) {
                return trackingChannel.getTriggerOutputState();
            }
        }
    }
    return channel.getTriggerOutputState();
}

void setTriggerOutputState(Channel &channel, bool enable) {
    if (channel.channelIndex < 2 && (g_couplingType == COUPLING_TYPE_SERIES || g_couplingType == COUPLING_TYPE_PARALLEL)) {
        Channel::get(0).setTriggerOutputState(enable);
    } else if (channel.flags.trackingEnabled) {
        for (int i = 0; i < CH_NUM; ++i) {
            Channel &trackingChannel = Channel::get(i);
            if (trackingChannel.flags.trackingEnabled) {
                trackingChannel.setTriggerOutputState(enable);
            }
        }
    } else {
        channel.setTriggerOutputState(enable);
    }
}

TriggerOnListStop getTriggerOnListStop(Channel &channel) {
    if (channel.channelIndex < 2 && (g_couplingType == COUPLING_TYPE_SERIES || g_couplingType == COUPLING_TYPE_PARALLEL)) {
        return Channel::get(0).getTriggerOnListStop();
    } else if (channel.flags.trackingEnabled) {
        for (int i = 0; i < CH_NUM; ++i) {
            Channel &trackingChannel = Channel::get(i);
            if (trackingChannel.flags.trackingEnabled) {
                return trackingChannel.getTriggerOnListStop();
            }
        }
    }
    return channel.getTriggerOnListStop();
}

void setTriggerOnListStop(Channel &channel, TriggerOnListStop value) {
    if (channel.channelIndex < 2 && (g_couplingType == COUPLING_TYPE_SERIES || g_couplingType == COUPLING_TYPE_PARALLEL)) {
        Channel::get(0).setTriggerOnListStop(value);
    } else if (channel.flags.trackingEnabled) {
        for (int i = 0; i < CH_NUM; ++i) {
            Channel &trackingChannel = Channel::get(i);
            if (trackingChannel.flags.trackingEnabled) {
                trackingChannel.setTriggerOnListStop(value);
            }
        }
    } else {
        channel.setTriggerOnListStop(value);
    }
}

float getTriggerVoltage(Channel &channel) {
    if (channel.channelIndex < 2 && (g_couplingType == COUPLING_TYPE_SERIES || g_couplingType == COUPLING_TYPE_PARALLEL)) {
        return Channel::get(0).u.triggerLevel;
    } else if (channel.flags.trackingEnabled) {
        for (int i = 0; i < CH_NUM; ++i) {
            Channel &trackingChannel = Channel::get(i);
            if (trackingChannel.flags.trackingEnabled) {
                return trackingChannel.u.triggerLevel;
            }
        }
    }
    return channel.u.triggerLevel;
}

void setTriggerVoltage(Channel &channel, float value) {
    value = roundChannelValue(channel, UNIT_VOLT, value);

    if (channel.channelIndex < 2 && (g_couplingType == COUPLING_TYPE_SERIES || g_couplingType == COUPLING_TYPE_PARALLEL)) {
        Channel::get(0).u.triggerLevel = value;
    } else if (channel.flags.trackingEnabled) {
        value = roundTrackingValuePrecision(UNIT_VOLT, value);
        for (int i = 0; i < CH_NUM; ++i) {
            Channel &trackingChannel = Channel::get(i);
            if (trackingChannel.flags.trackingEnabled) {
                trackingChannel.u.triggerLevel = value;
            }
        }
    } else {
        channel.u.triggerLevel = value;
    }
}

float getTriggerCurrent(Channel &channel) {
    if (channel.channelIndex < 2 && (g_couplingType == COUPLING_TYPE_SERIES || g_couplingType == COUPLING_TYPE_PARALLEL)) {
        return Channel::get(0).i.triggerLevel;
    } else if (channel.flags.trackingEnabled) {
        for (int i = 0; i < CH_NUM; ++i) {
            Channel &trackingChannel = Channel::get(i);
            if (trackingChannel.flags.trackingEnabled) {
                return trackingChannel.i.triggerLevel;
            }
        }
    }
    return channel.i.triggerLevel;
}

void setTriggerCurrent(Channel &channel, float value) {
    value = roundChannelValue(channel, UNIT_AMPER, value);

    if (channel.channelIndex < 2 && (g_couplingType == COUPLING_TYPE_SERIES || g_couplingType == COUPLING_TYPE_PARALLEL)) {
        Channel::get(0).i.triggerLevel = value;
    } else if (channel.flags.trackingEnabled) {
        value = roundTrackingValuePrecision(UNIT_AMPER, value);
        for (int i = 0; i < CH_NUM; ++i) {
            Channel &trackingChannel = Channel::get(i);
            if (trackingChannel.flags.trackingEnabled) {
                trackingChannel.i.triggerLevel = value;
            }
        }
    } else {
        channel.i.triggerLevel = value;
    }
}

void setDwellList(Channel &channel, float *list, uint16_t listLength) {
    if (channel.channelIndex < 2 && (g_couplingType == COUPLING_TYPE_SERIES || g_couplingType == COUPLING_TYPE_PARALLEL)) {
        list::setDwellList(Channel::get(0), list, listLength);
        list::setDwellList(Channel::get(1), list, listLength);
    } else if (channel.flags.trackingEnabled) {
        for (int i = 0; i < CH_NUM; ++i) {
            Channel &trackingChannel = Channel::get(i);
            if (trackingChannel.flags.trackingEnabled) {
                list::setDwellList(trackingChannel, list, listLength);
            }
        }
    } else {
        list::setDwellList(channel, list, listLength);
    }
}

void setVoltageList(Channel &channel, float *list, uint16_t listLength) {
    if (channel.channelIndex < 2 && (g_couplingType == COUPLING_TYPE_SERIES || g_couplingType == COUPLING_TYPE_PARALLEL)) {
        list::setVoltageList(Channel::get(0), list, listLength);
        list::setVoltageList(Channel::get(1), list, listLength);
    } else if (channel.flags.trackingEnabled) {
        for (int i = 0; i < CH_NUM; ++i) {
            Channel &trackingChannel = Channel::get(i);
            if (trackingChannel.flags.trackingEnabled) {
                list::setVoltageList(trackingChannel, list, listLength);
            }
        }
    } else {
        list::setVoltageList(channel, list, listLength);
    }
}

void setCurrentList(Channel &channel, float *list, uint16_t listLength) {
    if (channel.channelIndex < 2 && (g_couplingType == COUPLING_TYPE_SERIES || g_couplingType == COUPLING_TYPE_PARALLEL)) {
        list::setCurrentList(Channel::get(0), list, listLength);
        list::setCurrentList(Channel::get(1), list, listLength);
    } else if (channel.flags.trackingEnabled) {
        for (int i = 0; i < CH_NUM; ++i) {
            Channel &trackingChannel = Channel::get(i);
            if (trackingChannel.flags.trackingEnabled) {
                list::setCurrentList(trackingChannel, list, listLength);
            }
        }
    } else {
        list::setCurrentList(channel, list, listLength);
    }
}

void setListCount(Channel &channel, uint16_t value) {
    if (channel.channelIndex < 2 && (g_couplingType == COUPLING_TYPE_SERIES || g_couplingType == COUPLING_TYPE_PARALLEL)) {
        list::setListCount(Channel::get(0), value);
        list::setListCount(Channel::get(1), value);
    } else if (channel.flags.trackingEnabled) {
        for (int i = 0; i < CH_NUM; ++i) {
            Channel &trackingChannel = Channel::get(i);
            if (trackingChannel.flags.trackingEnabled) {
                list::setListCount(trackingChannel, value);
            }
        }
    } else {
        list::setListCount(channel, value);
    }
}

void setCurrentRangeSelectionMode(Channel &channel, CurrentRangeSelectionMode mode) {
    if (!isPsuThread()) {
        sendMessageToPsu(PSU_MESSAGE_SET_CURRENT_RANGE_SELECTION_MODE, (channel.channelIndex << 8) | mode );
    } else {
        if (channel.channelIndex < 2 && (g_couplingType == COUPLING_TYPE_SERIES || g_couplingType == COUPLING_TYPE_PARALLEL)) {
            Channel::get(0).setCurrentRangeSelectionMode(mode);
            Channel::get(1).setCurrentRangeSelectionMode(mode);
        } else {
            channel.setCurrentRangeSelectionMode(mode);
        }
    }
}

void enableAutoSelectCurrentRange(Channel &channel, bool enable) {
    if (channel.channelIndex < 2 && (g_couplingType == COUPLING_TYPE_SERIES || g_couplingType == COUPLING_TYPE_PARALLEL)) {
        Channel::get(0).enableAutoSelectCurrentRange(enable);
        Channel::get(1).enableAutoSelectCurrentRange(enable);
    } else {
        channel.enableAutoSelectCurrentRange(enable);
    }
}

#ifdef EEZ_PLATFORM_SIMULATOR
void setLoadEnabled(Channel &channel, bool state) {
    channel.simulator.setLoadEnabled(state);
}

void setLoad(Channel &channel, float load) {
    load = roundPrec(load, 0.001f);
    channel.simulator.setLoad(load);
}
#endif

const char *copyChannelToChannel(int srcChannelIndex, int dstChannelIndex) {
    Channel &srcChannel = Channel::get(srcChannelIndex);
    Channel &dstChannel = Channel::get(dstChannelIndex);

    float voltageLimit;
    if (srcChannel.u.limit < channel_dispatcher::getUMaxLimit(dstChannel)) {
        voltageLimit = srcChannel.u.limit;
    } else {
        voltageLimit = channel_dispatcher::getUMaxLimit(dstChannel);
    }

    float currentLimit;
    if (srcChannel.i.limit < channel_dispatcher::getIMaxLimit(dstChannel)) {
        currentLimit = srcChannel.i.limit;
    } else {
        currentLimit = channel_dispatcher::getIMaxLimit(dstChannel);
    }

    float powerLimit;
    if (srcChannel.p_limit < channel_dispatcher::getPowerMaxLimit(dstChannel)) {
        powerLimit = srcChannel.p_limit;
    } else {
        powerLimit = channel_dispatcher::getPowerMaxLimit(dstChannel);
    }

    if (srcChannel.getUSet() > voltageLimit) {
        return "Voltage overflow.";
    }

    if (srcChannel.getISet() > currentLimit) {
        return "Current overflow.";
    }

    if (srcChannel.getUSet() * srcChannel.getISet() > powerLimit) {
        return "Power overflow.";
    }

    if (
        srcChannel.flags.rprogEnabled &&
        (dstChannel.params.features & CH_FEATURE_RPROG) &&
        (
            dstChannel.flags.trackingEnabled || 
            (dstChannel.channelIndex < 2 && (g_couplingType == COUPLING_TYPE_SERIES || g_couplingType == COUPLING_TYPE_PARALLEL))
        )
    ) {
        return "Can not enable remote programming.";
    }

    uint16_t dwellListLength;
    float *dwellList = list::getDwellList(srcChannel, &dwellListLength);

    uint16_t voltageListLength;
    float *voltageList = list::getVoltageList(srcChannel, &voltageListLength);

    for (int i = 0; i < voltageListLength; i++) {
        if (voltageList[i] > channel_dispatcher::getUMaxLimit(dstChannel)) {
            return "Voltage list value overflow.";
        }
    }

    uint16_t currentListLength;
    float *currentList = list::getCurrentList(srcChannel, &currentListLength);

    for (int i = 0; i < currentListLength; i++) {
        if (currentList[i] > channel_dispatcher::getIMaxLimit(dstChannel)) {
            return "Current list value overflow.";
        }
    }

    if (!isPsuThread()) {
        sendMessageToPsu(PSU_MESSAGE_COPY_CHANNEL_TO_CHANNEL, (srcChannelIndex << 8) | dstChannelIndex);
        return nullptr;
    }

    channel_dispatcher::outputEnable(dstChannel, false);

    channel_dispatcher::setVoltage(dstChannel, srcChannel.getUSet());
    channel_dispatcher::setVoltageStep(dstChannel, srcChannel.u.step);
    channel_dispatcher::setVoltageLimit(dstChannel, voltageLimit);

    channel_dispatcher::setCurrent(dstChannel, srcChannel.getISet());
    channel_dispatcher::setCurrentStep(dstChannel, srcChannel.i.step);
    channel_dispatcher::setCurrentLimit(dstChannel, currentLimit);

    channel_dispatcher::setPowerLimit(dstChannel, powerLimit);

    channel_dispatcher::setOvpParameters(dstChannel, srcChannel.prot_conf.flags.u_state, srcChannel.prot_conf.flags.u_type, srcChannel.prot_conf.u_level, srcChannel.prot_conf.u_delay);
    channel_dispatcher::setOcpParameters(dstChannel, srcChannel.prot_conf.flags.i_state, srcChannel.prot_conf.i_delay);
    channel_dispatcher::setOppParameters(dstChannel, srcChannel.prot_conf.flags.p_state, srcChannel.prot_conf.p_level, srcChannel.prot_conf.p_delay);

#ifdef EEZ_PLATFORM_SIMULATOR
    channel_dispatcher::setLoadEnabled(dstChannel, srcChannel.simulator.load_enabled);
    channel_dispatcher::setLoad(dstChannel, srcChannel.simulator.load);
#endif

    channel_dispatcher::remoteSensingEnable(dstChannel, srcChannel.flags.senseEnabled);

    if (dstChannel.params.features & CH_FEATURE_RPROG) {
        dstChannel.flags.rprogEnabled = srcChannel.flags.rprogEnabled;
    }

    channel_dispatcher::setDisplayViewSettings(dstChannel, srcChannel.displayValues, srcChannel.ytViewRate);

    channel_dispatcher::setVoltageTriggerMode(dstChannel, (TriggerMode)srcChannel.flags.voltageTriggerMode);
    channel_dispatcher::setCurrentTriggerMode(dstChannel, (TriggerMode)srcChannel.flags.currentTriggerMode);
    channel_dispatcher::setTriggerOutputState(dstChannel, srcChannel.flags.triggerOutputState);
    channel_dispatcher::setTriggerOnListStop(dstChannel, (TriggerOnListStop)srcChannel.flags.triggerOnListStop);

    channel_dispatcher::setTriggerVoltage(dstChannel, srcChannel.u.triggerLevel);
    channel_dispatcher::setTriggerCurrent(dstChannel, srcChannel.i.triggerLevel);

    channel_dispatcher::setVoltageRampDuration(dstChannel, srcChannel.u.rampDuration);
    channel_dispatcher::setCurrentRampDuration(dstChannel, srcChannel.i.rampDuration);

    channel_dispatcher::setOutputDelayDuration(dstChannel, srcChannel.outputDelayDuration);

    channel_dispatcher::setListCount(dstChannel, list::getListCount(srcChannel));

    channel_dispatcher::setCurrentRangeSelectionMode(dstChannel, (CurrentRangeSelectionMode)srcChannel.flags.currentRangeSelectionMode);
    channel_dispatcher::enableAutoSelectCurrentRange(dstChannel, srcChannel.flags.autoSelectCurrentRange);

    dstChannel.setDprogState((DprogState)srcChannel.flags.dprogState);

    channel_dispatcher::setDwellList(dstChannel, dwellList, dwellListLength);
    channel_dispatcher::setVoltageList(dstChannel, voltageList, voltageListLength);
    channel_dispatcher::setCurrentList(dstChannel, currentList, currentListLength);

    return nullptr;
}

bool isEditEnabled(const WidgetCursor &widgetCursor) {
    if (widgetCursor.widget->data == DATA_ID_CHANNEL_U_EDIT || widgetCursor.widget->data == DATA_ID_CHANNEL_I_EDIT) {
        auto &channel = Channel::get(widgetCursor.cursor);

        if (channel.flags.rprogEnabled && widgetCursor.widget->data == DATA_ID_CHANNEL_U_EDIT) {
            return false;
        }

        if (getVoltageTriggerMode(channel) != TRIGGER_MODE_FIXED || getCurrentTriggerMode(channel) != TRIGGER_MODE_FIXED) {
            return false;
        }
    } else if (
        widgetCursor.widget->data == DATA_ID_CHANNEL_PROTECTION_OVP_LIMIT ||
        widgetCursor.widget->data == DATA_ID_CHANNEL_PROTECTION_OPP_LIMIT
    ) {
		using namespace psu::gui;
		auto cursor = widgetCursor.cursor;
		int iChannel = cursor >= 0 ? cursor : (g_channel ? g_channel->channelIndex : 0);
        auto &channel = Channel::get(iChannel);
        if (channel.flags.rprogEnabled) {
            return false;
        }
    }

    return true;
}

bool getDigitalInputData(int slotIndex, int subchannelIndex, uint8_t &data, int *err) {
    return g_slots[slotIndex]->getDigitalInputData(subchannelIndex, data, err);
}
#ifdef EEZ_PLATFORM_SIMULATOR
bool setDigitalInputData(int slotIndex, int subchannelIndex, uint8_t data, int *err) {
    return g_slots[slotIndex]->setDigitalInputData(subchannelIndex, data, err);
}
#endif

bool getDigitalInputRange(int slotIndex, int subchannelIndex, uint8_t pin, uint8_t &range, int *err) {
    return g_slots[slotIndex]->getDigitalInputRange(subchannelIndex, pin, range, err);
}

bool setDigitalInputRange(int slotIndex, int subchannelIndex, uint8_t pin, uint8_t range, int *err) {
    return g_slots[slotIndex]->setDigitalInputRange(subchannelIndex, pin, range, err);
}

bool getDigitalInputSpeed(int slotIndex, int subchannelIndex, uint8_t pin, uint8_t &speed, int *err) {
    return g_slots[slotIndex]->getDigitalInputSpeed(subchannelIndex, pin, speed, err);
}

bool setDigitalInputSpeed(int slotIndex, int subchannelIndex, uint8_t pin, uint8_t speed, int *err) {
    return g_slots[slotIndex]->setDigitalInputSpeed(subchannelIndex, pin, speed, err);
}

bool getDigitalOutputData(int slotIndex, int subchannelIndex, uint8_t &data, int *err) {
    return g_slots[slotIndex]->getDigitalOutputData(subchannelIndex, data, err);
}

bool setDigitalOutputData(int slotIndex, int subchannelIndex, uint8_t data, int *err) {
    return g_slots[slotIndex]->setDigitalOutputData(subchannelIndex, data, err);
}

bool getSourceMode(int slotIndex, int subchannelIndex, SourceMode &mode, int *err) {
    return g_slots[slotIndex]->getSourceMode(subchannelIndex, mode, err);
}

bool setSourceMode(int slotIndex, int subchannelIndex, SourceMode mode, int *err) {
    return g_slots[slotIndex]->setSourceMode(subchannelIndex, mode, err);
}

bool getSourceCurrentRange(int slotIndex, int subchannelIndex, uint8_t &range, int *err) {
    return g_slots[slotIndex]->getSourceCurrentRange(subchannelIndex, range, err);
}

bool setSourceCurrentRange(int slotIndex, int subchannelIndex, uint8_t range, int *err) {
    return g_slots[slotIndex]->setSourceCurrentRange(subchannelIndex, range, err);
}

bool getSourceVoltageRange(int slotIndex, int subchannelIndex, uint8_t &range, int *err) {
    return g_slots[slotIndex]->getSourceVoltageRange(subchannelIndex, range, err);
}

bool setSourceVoltageRange(int slotIndex, int subchannelIndex, uint8_t range, int *err) {
    return g_slots[slotIndex]->setSourceVoltageRange(subchannelIndex, range, err);
}

bool getMeasureMode(int slotIndex, int subchannelIndex, MeasureMode &mode, int *err) {
    return g_slots[slotIndex]->getMeasureMode(subchannelIndex, mode, err);
}

bool setMeasureMode(int slotIndex, int subchannelIndex, MeasureMode mode, int *err) {
    return g_slots[slotIndex]->setMeasureMode(subchannelIndex, mode, err);
}

bool getMeasureCurrentRange(int slotIndex, int subchannelIndex, uint8_t &range, int *err) {
    return g_slots[slotIndex]->getMeasureCurrentRange(subchannelIndex, range, err);
}

bool setMeasureCurrentRange(int slotIndex, int subchannelIndex, uint8_t range, int *err) {
    return g_slots[slotIndex]->setMeasureCurrentRange(subchannelIndex, range, err);
}

bool getMeasureVoltageRange(int slotIndex, int subchannelIndex, uint8_t &range, int *err) {
    return g_slots[slotIndex]->getMeasureVoltageRange(subchannelIndex, range, err);
}

bool setMeasureVoltageRange(int slotIndex, int subchannelIndex, uint8_t range, int *err) {
    return g_slots[slotIndex]->setMeasureVoltageRange(subchannelIndex, range, err);
}

bool getMeasureCurrentNPLC(int slotIndex, int subchannelIndex, float &nplc, int *err) {
    return g_slots[slotIndex]->getMeasureCurrentNPLC(subchannelIndex, nplc, err);
}

bool setMeasureCurrentNPLC(int slotIndex, int subchannelIndex, float nplc, int *err) {
    return g_slots[slotIndex]->setMeasureCurrentNPLC(subchannelIndex, nplc, err);
}

bool getMeasureVoltageNPLC(int slotIndex, int subchannelIndex, float &nplc, int *err) {
    return g_slots[slotIndex]->getMeasureVoltageNPLC(subchannelIndex, nplc, err);
}

bool setMeasureVoltageNPLC(int slotIndex, int subchannelIndex, float nplc, int *err) {
    return g_slots[slotIndex]->setMeasureVoltageNPLC(subchannelIndex, nplc, err);
}

bool routeOpen(ChannelList channelList, int *err) {
    int slotIndex = channelList.channels[0].slotIndex;

    for (int i = 1; i < channelList.numChannels; i++) {
        if (channelList.channels[i].slotIndex != channelList.channels[0].slotIndex) {
            if (err) {
                *err = SCPI_ERROR_ILLEGAL_PARAMETER_VALUE;
            }
            return false;
        }
    }

    return g_slots[slotIndex]->routeOpen(channelList, err);
}

bool routeClose(ChannelList channelList, int *err) {
    int slotIndex = channelList.channels[0].slotIndex;

    for (int i = 1; i < channelList.numChannels; i++) {
        if (channelList.channels[i].slotIndex != slotIndex) {
            if (err) {
                *err = SCPI_ERROR_ILLEGAL_PARAMETER_VALUE;
            }
            return false;
        }
    }

    return g_slots[slotIndex]->routeClose(channelList, err);
}

bool routeCloseExclusive(ChannelList channelList, int *err) {
    int slotIndex = channelList.channels[0].slotIndex;

    for (int i = 1; i < channelList.numChannels; i++) {
        if (channelList.channels[i].slotIndex != slotIndex) {
            if (err) {
                *err = SCPI_ERROR_ILLEGAL_PARAMETER_VALUE;
            }
            return false;
        }
    }

    return g_slots[slotIndex]->routeCloseExclusive(channelList, err);
}

bool getVoltage(int slotIndex, int subchannelIndex, float &value, int *err) {
    return g_slots[slotIndex]->getVoltage(subchannelIndex, value, err);
}

bool setVoltage(int slotIndex, int subchannelIndex, float value, int *err) {
    return g_slots[slotIndex]->setVoltage(subchannelIndex, value, err);
}

void getVoltageStepValues(int slotIndex, int subchannelIndex, StepValues *stepValues, bool calibrationMode) {
    Channel *channel = Channel::getBySlotIndex(slotIndex, subchannelIndex);
    if (channel) {
        channel->getVoltageStepValues(stepValues, calibrationMode);
    } else {
        g_slots[slotIndex]->getVoltageStepValues(subchannelIndex, stepValues, calibrationMode);
    }
}

void setVoltageEncoderMode(int slotIndex, int subchannelIndex, EncoderMode encoderMode) {
    Channel *channel = Channel::getBySlotIndex(slotIndex, subchannelIndex);
    if (channel) {
        channel->setVoltageEncoderMode(encoderMode);
    } else {
        g_slots[slotIndex]->setVoltageEncoderMode(subchannelIndex, encoderMode);
    }
}

float getVoltageResolution(int slotIndex, int subchannelIndex) {
    Channel *channel = Channel::getBySlotIndex(slotIndex, subchannelIndex);
    if (channel) {
        return channel->getVoltageResolution();
    } else {
        return g_slots[slotIndex]->getVoltageResolution(subchannelIndex);
    }
}

float getVoltageMinValue(int slotIndex, int subchannelIndex) {
    return g_slots[slotIndex]->getVoltageMinValue(subchannelIndex);
}

float getVoltageMaxValue(int slotIndex, int subchannelIndex) {
    return g_slots[slotIndex]->getVoltageMaxValue(subchannelIndex);
}

bool getCurrent(int slotIndex, int subchannelIndex, float &value, int *err) {
    return g_slots[slotIndex]->getCurrent(subchannelIndex, value, err);
}

bool setCurrent(int slotIndex, int subchannelIndex, float value, int *err) {
    return g_slots[slotIndex]->setCurrent(subchannelIndex, value, err);
}

void getCurrentStepValues(int slotIndex, int subchannelIndex, StepValues *stepValues, bool calibrationMode) {
    Channel *channel = Channel::getBySlotIndex(slotIndex, subchannelIndex);
    if (channel) {
        channel->getCurrentStepValues(stepValues, calibrationMode);
    } else {
        g_slots[slotIndex]->getCurrentStepValues(subchannelIndex, stepValues, calibrationMode);
    }
}

void setCurrentEncoderMode(int slotIndex, int subchannelIndex, EncoderMode encoderMode) {
    Channel *channel = Channel::getBySlotIndex(slotIndex, subchannelIndex);
    if (channel) {
        channel->setCurrentEncoderMode(encoderMode);
    } else {
        g_slots[slotIndex]->setCurrentEncoderMode(subchannelIndex, encoderMode);
    }
}

float getCurrentResolution(int slotIndex, int subchannelIndex) {
    Channel *channel = Channel::getBySlotIndex(slotIndex, subchannelIndex);
    if (channel) {
        return channel->getCurrentResolution(NAN);
    } else {
        return g_slots[slotIndex]->getCurrentResolution(subchannelIndex);
    }
}

float getCurrentMinValue(int slotIndex, int subchannelIndex) {
    return g_slots[slotIndex]->getCurrentMinValue(subchannelIndex);
}

float getCurrentMaxValue(int slotIndex, int subchannelIndex) {
    return g_slots[slotIndex]->getCurrentMaxValue(subchannelIndex);
}

bool getMeasuredVoltage(int slotIndex, int subchannelIndex, float &value, int *err) {
    Channel *channel = Channel::getBySlotIndex(slotIndex, subchannelIndex);
    if (channel) {
        value = getUMonLast(*channel);
        return true;
    } else {
        return g_slots[slotIndex]->getMeasuredVoltage(subchannelIndex, value, err);
    }
}

bool getMeasuredCurrent(int slotIndex, int subchannelIndex, float &value, int *err) {
    Channel *channel = Channel::getBySlotIndex(slotIndex, subchannelIndex);
    if (channel) {
        value = getIMonLast(*channel);
        return true;
    } else {
        return g_slots[slotIndex]->getMeasuredCurrent(subchannelIndex, value, err);
    }
}

bool getSourcePwmState(int slotIndex, int subchannelIndex, bool &enabled, int *err) {
    return g_slots[slotIndex]->getSourcePwmState(subchannelIndex, enabled, err);
}

bool setSourcePwmState(int slotIndex, int subchannelIndex, bool enabled, int *err) {
	return g_slots[slotIndex]->setSourcePwmState(subchannelIndex, enabled, err);
}

bool getSourcePwmFrequency(int slotIndex, int subchannelIndex, float &frequency, int *err) {
	return g_slots[slotIndex]->getSourcePwmFrequency(subchannelIndex, frequency, err);
}

bool setSourcePwmFrequency(int slotIndex, int subchannelIndex, float frequency, int *err) {
	return g_slots[slotIndex]->setSourcePwmFrequency(subchannelIndex, frequency, err);
}

bool getSourcePwmDuty(int slotIndex, int subchannelIndex, float &duty, int *err) {
	return g_slots[slotIndex]->getSourcePwmDuty(subchannelIndex, duty, err);
}

bool setSourcePwmDuty(int slotIndex, int subchannelIndex, float duty, int *err) {
	return g_slots[slotIndex]->setSourcePwmDuty(subchannelIndex, duty, err);
}


} // namespace channel_dispatcher
} // namespace psu
} // namespace eez
