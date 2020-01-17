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
#include <eez/modules/psu/init.h>
#include <eez/modules/psu/calibration.h>
#include <eez/modules/psu/channel_dispatcher.h>
#include <eez/modules/psu/event_queue.h>
#include <eez/modules/psu/list_program.h>
#include <eez/scpi/regs.h>
#include <eez/modules/psu/temperature.h>
#include <eez/modules/psu/trigger.h>
#include <eez/index.h>
#include <eez/system.h>
#include <eez/modules/bp3c/relays.h>

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
        if (CH_NUM < 2) {
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
    }

    return true;
}

bool setCouplingType(CouplingType couplingType, int *err) {
    if (g_couplingType != couplingType) {
        if (!isCouplingTypeAllowed(couplingType, err)) {
            return false;
        }

        trigger::abort();

        g_couplingType = couplingType;

        beginOutputEnableSequence();

        for (int i = 0; i < CH_NUM; ++i) {
            Channel &channel = Channel::get(i);

            channel.outputEnable(false);

            if (i < 2 && (couplingType == COUPLING_TYPE_PARALLEL || couplingType == COUPLING_TYPE_SERIES)) {
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

                trigger::setVoltage(channel, getUMin(channel));
                trigger::setCurrent(channel, getIMin(channel));

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
                    channel.flags.displayValue1 = channel1.flags.displayValue1;
                    channel.flags.displayValue2 = channel1.flags.displayValue2;
                    channel.ytViewRate = channel1.ytViewRate;

                }

                channel.setCurrentRangeSelectionMode(CURRENT_RANGE_SELECTION_USE_BOTH);
                channel.enableAutoSelectCurrentRange(false);

                if (g_couplingType == COUPLING_TYPE_SERIES || g_couplingType == COUPLING_TYPE_PARALLEL) { 
                    channel.flags.trackingEnabled = false;
                }

                channel.resetHistory();
            }
        }

        endOutputEnableSequence();

        if (g_couplingType == COUPLING_TYPE_PARALLEL || g_couplingType == COUPLING_TYPE_SERIES) {
            if (persist_conf::getMaxChannelIndex() ==  1) {
                persist_conf::setMaxChannelIndex(0);
            }
        }

        bp3c::relays::switchChannelCoupling(g_couplingType);

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
    }

    return true;
}

void setTrackingChannels(uint16_t trackingEnabled) {
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

        beginOutputEnableSequence();

        for (int i = 0; i < CH_NUM; i++) {
            Channel &trackingChannel = Channel::get(i);
            if (trackingChannel.flags.trackingEnabled) {
            	trackingChannel.outputEnable(false);
                trackingChannel.remoteSensingEnable(false);
                trackingChannel.remoteProgrammingEnable(false);

                trackingChannel.setVoltageTriggerMode(TRIGGER_MODE_FIXED);
                trackingChannel.setCurrentTriggerMode(TRIGGER_MODE_FIXED);
                trackingChannel.setTriggerOutputState(true);
                trackingChannel.setTriggerOnListStop(TRIGGER_ON_LIST_STOP_OUTPUT_OFF);

                list::resetChannelList(trackingChannel);

                trackingChannel.setVoltage(uMin);
                trackingChannel.setVoltageLimit(voltageLimit);

                trackingChannel.setCurrent(iMin);
                trackingChannel.setCurrentLimit(currentLimit);

                trigger::setVoltage(trackingChannel, uDef);
                trigger::setCurrent(trackingChannel, iDef);

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

                trackingChannel.resetHistory();
            }
        }

        endOutputEnableSequence();
    }
}

CouplingType getType() {
    return g_couplingType;
}

////////////////////////////////////////////////////////////////////////////////

#define CONF_GUI_REFRESH_EVERY_MS 250

uint32_t g_lastSnapshotTime;

static struct ChannelSnapshot {
    ChannelMode channelMode;
    float uMon;
    float iMon;
    float pMon;
} g_channelSnapshots[CH_MAX];

ChannelSnapshot &getChannelSnapshot(int channelIndex) {
    uint32_t currentTime = micros();
    if (!g_lastSnapshotTime || currentTime - g_lastSnapshotTime >= CONF_GUI_REFRESH_EVERY_MS * 1000UL) {
        for (int i = 0; i < CH_NUM; i++) {
            Channel &channel = Channel::get(i);
            ChannelSnapshot &channelSnapshot = g_channelSnapshots[i];
            const char *mode_str = channel.getModeStr();
            channelSnapshot.uMon = channel_dispatcher::getUMon(channel);
            channelSnapshot.iMon = channel_dispatcher::getIMon(channel);
            if (strcmp(mode_str, "CC") == 0) {
                channelSnapshot.channelMode = CHANNEL_MODE_CC;
            } else if (strcmp(mode_str, "CV") == 0) {
                channelSnapshot.channelMode = CHANNEL_MODE_CV;
            } else {
                channelSnapshot.channelMode = CHANNEL_MODE_UR;
            }
            channelSnapshot.pMon = roundPrec(channelSnapshot.uMon * channelSnapshot.iMon, channel.getPowerResolution());
        }

        g_lastSnapshotTime = currentTime;
    }

    return g_channelSnapshots[channelIndex];
}

////////////////////////////////////////////////////////////////////////////////

ChannelMode getChannelMode(const Channel &channel) {
    return getChannelSnapshot(channel.channelIndex).channelMode;
}

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
        return Channel::get(0).u.set + Channel::get(1).u.set;
    }
    return channel.u.set;
}

float getUSetUnbalanced(const Channel &channel) {
    if (channel.channelIndex < 2 && g_couplingType == COUPLING_TYPE_SERIES) {
        return Channel::get(0).getUSetUnbalanced() + Channel::get(1).getUSetUnbalanced();
    }
    return channel.u.set;
}

float getUMon(const Channel &channel) {
    if (channel.channelIndex < 2 && g_couplingType == COUPLING_TYPE_SERIES) {
        return Channel::get(0).u.mon + Channel::get(1).u.mon;
    }
    return channel.u.mon;
}

float getUMonSnapshot(const Channel &channel) {
    if (channel.channelIndex < 2 && g_couplingType == COUPLING_TYPE_SERIES) {
        return getChannelSnapshot(0).uMon + getChannelSnapshot(1).uMon;
    }
    return getChannelSnapshot(channel.channelIndex).uMon;
}

float getUMonLast(const Channel &channel) {
    if (channel.channelIndex < 2 && g_couplingType == COUPLING_TYPE_SERIES) {
        return Channel::get(0).u.mon_last + Channel::get(1).u.mon_last;
    }
    return channel.u.mon_last;
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
                    value = MIN(value, trackingChannel.u.min);
                }
            }
        }
        return value;
    }
    return channel.u.min;
}

float getUDef(const Channel &channel) {
    if (channel.channelIndex < 2 && g_couplingType == COUPLING_TYPE_SERIES) {
        return Channel::get(0).u.def + Channel::get(1).u.def;
    }
    return channel.u.def;
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

float getUMaxOvpLimit(const Channel &channel) {
    if (channel.flags.rprogEnabled) {
        return getUMax(channel) + 0.5f;    
    }
    return getUMax(channel);
}

float getUMaxOvpLevel(const Channel &channel) {
    return getUMax(channel) + 0.5f;
}

float getUProtectionLevel(const Channel &channel) {
    if (channel.channelIndex < 2 && g_couplingType == COUPLING_TYPE_SERIES) {
        return Channel::get(0).prot_conf.u_level + Channel::get(1).prot_conf.u_level;
    }
    return channel.prot_conf.u_level;
}

void setVoltage(Channel &channel, float voltage) {
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
        return Channel::get(0).i.set + Channel::get(1).i.set;
    }
    return channel.i.set;
}

float getISetUnbalanced(const Channel &channel) {
    if (channel.channelIndex < 2 && g_couplingType == COUPLING_TYPE_PARALLEL) {
        return Channel::get(0).getISetUnbalanced() + Channel::get(1).getISetUnbalanced();
    }
    return channel.i.set;
}

float getIMon(const Channel &channel) {
    if (channel.channelIndex < 2 && g_couplingType == COUPLING_TYPE_PARALLEL) {
        return Channel::get(0).i.mon + Channel::get(1).i.mon;
    }
    return channel.i.mon;
}

float getIMonSnapshot(const Channel &channel) {
    if (channel.channelIndex < 2 && g_couplingType == COUPLING_TYPE_PARALLEL) {
        return getChannelSnapshot(0).iMon + getChannelSnapshot(1).iMon;
    }
    return getChannelSnapshot(channel.channelIndex).iMon;
}

float getIMonLast(const Channel &channel) {
    if (channel.channelIndex < 2 && g_couplingType == COUPLING_TYPE_PARALLEL) {
        return Channel::get(0).i.mon_last + Channel::get(1).i.mon_last;
    }
    return channel.i.mon_last;
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

float getIDef(const Channel &channel) {
    if (channel.channelIndex < 2 && g_couplingType == COUPLING_TYPE_PARALLEL) {
        return Channel::get(0).i.def + Channel::get(1).i.def;
    }
    return channel.i.def;
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

void setCurrent(Channel &channel, float current) {
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
    if (channel.channelIndex < 2 && (g_couplingType == COUPLING_TYPE_SERIES || g_couplingType == COUPLING_TYPE_PARALLEL)) {
        return 2 * MIN(Channel::get(0).params.OPP_MAX_LEVEL, Channel::get(1).params.OPP_MAX_LEVEL);
    }
    return channel.params.OPP_MAX_LEVEL;
}

float getOppDefaultLevel(Channel &channel) {
    if (channel.channelIndex < 2 && (g_couplingType == COUPLING_TYPE_SERIES || g_couplingType == COUPLING_TYPE_PARALLEL)) {
        return Channel::get(0).params.OPP_DEFAULT_LEVEL + Channel::get(1).params.OPP_DEFAULT_LEVEL;
    }
    return channel.params.OPP_DEFAULT_LEVEL;
}

void setOppParameters(Channel &channel, int state, float level, float delay) {
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

void outputEnable(Channel &channel, bool enable) {
    if (channel.channelIndex < 2 && (g_couplingType == COUPLING_TYPE_SERIES || g_couplingType == COUPLING_TYPE_PARALLEL)) {
        beginOutputEnableSequence();
        
        Channel::get(0).outputEnable(enable);
        Channel::get(1).outputEnable(enable);

        endOutputEnableSequence();
    } else if (channel.flags.trackingEnabled) {
        beginOutputEnableSequence();

        for (int i = 0; i < CH_NUM; ++i) {
            Channel &trackingChannel = Channel::get(i);
            if (trackingChannel.flags.trackingEnabled) {
                trackingChannel.outputEnable(enable);
            }
        }

        endOutputEnableSequence();
    } else {
        channel.outputEnable(enable);
    }
}

bool outputEnable(Channel &channel, bool enable, int *err) {
    if (enable != channel.isOutputEnabled()) {
        bool triggerModeEnabled =
            channel_dispatcher::getVoltageTriggerMode(channel) != TRIGGER_MODE_FIXED ||
            channel_dispatcher::getCurrentTriggerMode(channel) != TRIGGER_MODE_FIXED;

        if (channel.isOutputEnabled()) {
            if (calibration::isEnabled()) {
                if (err) {
                    *err = SCPI_ERROR_CAL_OUTPUT_DISABLED;
                }
                return false;
            }

            if (triggerModeEnabled) {
                trigger::abort();
            } else {
                channel_dispatcher::outputEnable(channel, false);
            }
        } else {
            if (channel_dispatcher::isTripped(channel)) {
                if (err) {
                    *err = SCPI_ERROR_CANNOT_EXECUTE_BEFORE_CLEARING_PROTECTION;
                }
                return false;
            }

            if (triggerModeEnabled && !trigger::isIdle()) {
                if (trigger::isInitiated()) {
                    trigger::abort();
                } else {
                    if (err) {
                        *err = SCPI_ERROR_CANNOT_CHANGE_TRANSIENT_TRIGGER;
                    }
                    return false;
                }
            }

            channel_dispatcher::outputEnable(channel, true);
        }
    }

    return true;
}

void disableOutputForAllChannels() {
    beginOutputEnableSequence();

    for (int i = 0; i < CH_NUM; i++) {
        if (Channel::get(i).isOutputEnabled()) {
            Channel::get(i).outputEnable(false);
        }
    }

    endOutputEnableSequence();
}

static int g_sequenceCounter;
static bool g_syncPrepared;
static uint16_t g_oeStateBegin;
static uint16_t g_oeStateDiff;
static uint16_t g_syncReady;

static uint16_t getOeStateForAllChannels() {
	uint16_t oeState = 0;
	for (int i = 0; i < CH_NUM; i++) {
		if (Channel::get(i).isOutputEnabled()) {
			oeState |= 1 << i;
		}
	}
	return oeState;
}

static void prepareSync() {
	assert(!g_syncPrepared);

#if defined(EEZ_PLATFORM_STM32)
	HAL_GPIO_WritePin(OE_SYNC_GPIO_Port, OE_SYNC_Pin, GPIO_PIN_RESET);
#endif
	g_syncPrepared = true;
}

static void doSync() {
	assert(g_syncPrepared);

#if defined(EEZ_PLATFORM_STM32)
	HAL_GPIO_WritePin(OE_SYNC_GPIO_Port, OE_SYNC_Pin, GPIO_PIN_SET);
#endif
	g_syncPrepared = false;
	g_syncReady = 0;
	g_oeStateDiff = 0;
}

void beginOutputEnableSequence() {
    lock();

    if (g_sequenceCounter++ == 0) {
    	if (!g_syncPrepared) {
    		prepareSync();
    	}
    	// remember channel output states at sequence begin
    	g_oeStateBegin = getOeStateForAllChannels();
    }

    unlock();
}

void endOutputEnableSequence() {
    lock();

    if (--g_sequenceCounter == 0) {
    	// which channals changed output enable state
    	g_oeStateDiff = g_oeStateBegin ^ getOeStateForAllChannels();
        if (g_oeStateDiff != 0) {
            // is sync ready for all changed channels?
            if (g_syncReady == g_oeStateDiff) {
                doSync();
            }
        }
    }

    assert(g_sequenceCounter >= 0);

    unlock();
}

void outputEnableSyncPrepare(Channel &channel) {
    lock();

    if (!g_syncPrepared) {
    	prepareSync();
    }

    unlock();
}

void outputEnableSyncReady(Channel &channel) {
    lock();

    g_syncReady |= 1 << channel.channelIndex;

    if (g_sequenceCounter == 0) {
    	if (!g_oeStateDiff || g_syncReady == g_oeStateDiff) {
    		doSync();
    	}
    }

    unlock();
}

void remoteSensingEnable(Channel &channel, bool enable) {
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

bool isTripped(Channel &channel) {
    if (channel.channelIndex < 2 && (g_couplingType == COUPLING_TYPE_SERIES || g_couplingType == COUPLING_TYPE_PARALLEL)) {
        return Channel::get(0).isTripped() || Channel::get(1).isTripped();
    } else if (channel.flags.trackingEnabled) {
        for (int i = 0; i < CH_NUM; ++i) {
            Channel &trackingChannel = Channel::get(i);
            if (trackingChannel.flags.trackingEnabled && trackingChannel.isTripped()) {
                return true;
            }
        }
        return false;
    } else {
        return channel.isTripped();
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

void setDisplayViewSettings(Channel &channel, int displayValue1, int displayValue2, float ytViewRate) {
    if (channel.channelIndex < 2 && (g_couplingType == COUPLING_TYPE_SERIES || g_couplingType == COUPLING_TYPE_PARALLEL)) {
        Channel::get(0).flags.displayValue1 = displayValue1;
        Channel::get(0).flags.displayValue2 = displayValue2;
        Channel::get(0).ytViewRate = ytViewRate;

        Channel::get(1).flags.displayValue1 = displayValue1;
        Channel::get(1).flags.displayValue2 = displayValue2;
        Channel::get(1).ytViewRate = ytViewRate;
    } else if (channel.flags.trackingEnabled) {
        for (int i = 0; i < CH_NUM; ++i) {
            Channel &trackingChannel = Channel::get(i);
            if (trackingChannel.flags.trackingEnabled) {
                trackingChannel.flags.displayValue1 = displayValue1;
                trackingChannel.flags.displayValue2 = displayValue2;
                trackingChannel.ytViewRate = ytViewRate;
            }
        }
    } else {
        channel.flags.displayValue1 = displayValue1;
        channel.flags.displayValue2 = displayValue2;
        channel.ytViewRate = ytViewRate;
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
        return trigger::getVoltage(Channel::get(0));
    } else if (channel.flags.trackingEnabled) {
        for (int i = 0; i < CH_NUM; ++i) {
            Channel &trackingChannel = Channel::get(i);
            if (trackingChannel.flags.trackingEnabled) {
                return trigger::getVoltage(trackingChannel);
            }
        }
    }
    return trigger::getVoltage(channel);
}

void setTriggerVoltage(Channel &channel, float value) {
    if (channel.channelIndex < 2 && (g_couplingType == COUPLING_TYPE_SERIES || g_couplingType == COUPLING_TYPE_PARALLEL)) {
        trigger::setVoltage(Channel::get(0), value);
    } else if (channel.flags.trackingEnabled) {
        value = roundTrackingValuePrecision(UNIT_VOLT, value);
        for (int i = 0; i < CH_NUM; ++i) {
            Channel &trackingChannel = Channel::get(i);
            if (trackingChannel.flags.trackingEnabled) {
                trigger::setVoltage(trackingChannel, value);
            }
        }
    } else {
        trigger::setVoltage(channel, value);
    }
}

float getTriggerCurrent(Channel &channel) {
    if (channel.channelIndex < 2 && (g_couplingType == COUPLING_TYPE_SERIES || g_couplingType == COUPLING_TYPE_PARALLEL)) {
        return trigger::getCurrent(Channel::get(0));
    } else if (channel.flags.trackingEnabled) {
        for (int i = 0; i < CH_NUM; ++i) {
            Channel &trackingChannel = Channel::get(i);
            if (trackingChannel.flags.trackingEnabled) {
                return trigger::getCurrent(trackingChannel);
            }
        }
    }
    return trigger::getCurrent(channel);
}

void setTriggerCurrent(Channel &channel, float value) {
    if (channel.channelIndex < 2 && (g_couplingType == COUPLING_TYPE_SERIES || g_couplingType == COUPLING_TYPE_PARALLEL)) {
        trigger::setCurrent(Channel::get(0), value);
    } else if (channel.flags.trackingEnabled) {
        value = roundTrackingValuePrecision(UNIT_AMPER, value);
        for (int i = 0; i < CH_NUM; ++i) {
            Channel &trackingChannel = Channel::get(i);
            if (trackingChannel.flags.trackingEnabled) {
                trigger::setCurrent(trackingChannel, value);
            }
        }
    } else {
        trigger::setCurrent(channel, value);
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
    if (channel.channelIndex < 2 && (g_couplingType == COUPLING_TYPE_SERIES || g_couplingType == COUPLING_TYPE_PARALLEL)) {
        Channel::get(0).setCurrentRangeSelectionMode(mode);
        Channel::get(1).setCurrentRangeSelectionMode(mode);
    } else {
        channel.setCurrentRangeSelectionMode(mode);
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

} // namespace channel_dispatcher
} // namespace psu
} // namespace eez
