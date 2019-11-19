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

#include <eez/modules/psu/psu.h>

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

    if (CH_NUM < 2) {
        if (err) {
            *err = SCPI_ERROR_HARDWARE_MISSING;
        }
        return false;
    }

    if (!Channel::get(0).isOk() || !Channel::get(1).isOk()) {
        if (err) {
            *err = SCPI_ERROR_HARDWARE_ERROR;
        }
        return false;
    }

    if (couplingType == COUPLING_TYPE_PARALLEL || couplingType == COUPLING_TYPE_SERIES) {
        if (!(Channel::get(0).params.features & CH_FEATURE_COUPLING) || !(Channel::get(1).params.features & CH_FEATURE_COUPLING)) {
            if (err) {
                *err = SCPI_ERROR_HARDWARE_MISSING;
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

        for (int i = 0; i < 2; ++i) {
            Channel &channel = Channel::get(i);
            channel.outputEnable(false);
            channel.remoteSensingEnable(false);

            if (channel.params.features & CH_FEATURE_RPROG) {
                channel.remoteProgrammingEnable(false);
            }

            channel.setVoltageTriggerMode(TRIGGER_MODE_FIXED);
            channel.setCurrentTriggerMode(TRIGGER_MODE_FIXED);
            channel.setTriggerOutputState(true);
            channel.setTriggerOnListStop(TRIGGER_ON_LIST_STOP_OUTPUT_OFF);

            list::resetChannelList(channel);

            if (g_couplingType != COUPLING_TYPE_NONE) {
                channel.setVoltage(getUMin(channel));
                channel.setVoltageLimit(MIN(Channel::get(0).getVoltageLimit(),
                                            Channel::get(1).getVoltageLimit()));

                channel.setCurrent(getIMin(channel));
                channel.setCurrentLimit(MIN(Channel::get(0).getCurrentLimit(),
                                            Channel::get(1).getCurrentLimit()));

                trigger::setVoltage(channel, getUMin(channel));
                trigger::setCurrent(channel, getIMin(channel));

#ifdef EEZ_PLATFORM_SIMULATOR
                channel.simulator.setLoadEnabled(false);
                channel.simulator.setLoad(Channel::get(0).simulator.getLoad());
#endif

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

                channel.setVoltageTriggerMode(TRIGGER_MODE_FIXED);
                channel.setCurrentTriggerMode(TRIGGER_MODE_FIXED);
                channel.setTriggerOutputState(true);
                channel.setTriggerOnListStop(TRIGGER_ON_LIST_STOP_OUTPUT_OFF);
            }

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
    bool channelsTracked = false;
    for (int i = 0; i < CH_NUM; i++) {
        Channel &trackingChannel = Channel::get(i);
        trackingChannel.flags.trackingEnabled = (trackingEnabled & (1 << i)) ? 1 : 0;
        if (trackingChannel.flags.trackingEnabled) {
            channelsTracked = true;
        }
    }

    if (channelsTracked) {
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

        for (int i = 0; i < CH_NUM; i++) {
            Channel &trackingChannel = Channel::get(i);
            if (trackingChannel.flags.trackingEnabled) {
            	trackingChannel.outputEnable(false);

                trackingChannel.setVoltage(uMin);
                trackingChannel.setCurrent(iMin);

                trackingChannel.setVoltageLimit(voltageLimit);
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

                trackingChannel.setVoltageTriggerMode(TRIGGER_MODE_FIXED);
                trackingChannel.setCurrentTriggerMode(TRIGGER_MODE_FIXED);
                trackingChannel.setTriggerOutputState(true);
                trackingChannel.setTriggerOnListStop(TRIGGER_ON_LIST_STOP_OUTPUT_OFF);
            }
        }
    }
}

CouplingType getType() {
    return g_couplingType;
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

float getUMaxOvpLevel(const Channel &channel) {
    return getUMax(channel) + 0.5f;
}

float getUProtectionLevel(const Channel &channel) {
    if (channel.channelIndex < 2 && g_couplingType == COUPLING_TYPE_SERIES) {
        return Channel::get(0).prot_conf.u_level + Channel::get(1).prot_conf.u_level;
    }
    return channel.prot_conf.u_level;
}

float getTrackingValuePrecision(Unit unit, float value) {
    float precision = 0;
    for (int i = 0; i < CH_NUM; ++i) {
        Channel &trackingChannel = Channel::get(i);
        if (trackingChannel.flags.trackingEnabled) {
            precision = MAX(precision, trackingChannel.getValuePrecision(unit, value));
        }
    }
    return roundPrec(value, precision);
}

void setVoltage(Channel &channel, float voltage) {
    if (channel.channelIndex < 2 && g_couplingType == COUPLING_TYPE_SERIES) {
        Channel::get(0).setVoltage(voltage / 2);
        Channel::get(1).setVoltage(voltage / 2);
    } else if (channel.channelIndex < 2 && g_couplingType == COUPLING_TYPE_PARALLEL) {
        Channel::get(0).setVoltage(voltage);
        Channel::get(1).setVoltage(voltage);
    } else if (channel.flags.trackingEnabled) {
        voltage = getTrackingValuePrecision(UNIT_VOLT, voltage);

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
        limit = getTrackingValuePrecision(UNIT_VOLT, limit);
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
        level = getTrackingValuePrecision(UNIT_VOLT, level);
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
        level = getTrackingValuePrecision(UNIT_VOLT, level);
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
        current = getTrackingValuePrecision(UNIT_AMPER, current);
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
        limit = getTrackingValuePrecision(UNIT_AMPER, limit);
        for (int i = 0; i < CH_NUM; ++i) {
            Channel &trackingChannel = Channel::get(i);
            if (trackingChannel.flags.trackingEnabled) {
                trackingChannel.setCurrent(limit);
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
        limit = getTrackingValuePrecision(UNIT_WATT, limit);
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
        level = getTrackingValuePrecision(UNIT_WATT, level);
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
        level = getTrackingValuePrecision(UNIT_WATT, level);
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
        Channel::get(0).outputEnable(enable);
        Channel::get(1).outputEnable(enable);
    } else if (channel.flags.trackingEnabled) {
        for (int i = 0; i < CH_NUM; ++i) {
            Channel &trackingChannel = Channel::get(i);
            if (trackingChannel.flags.trackingEnabled) {
                trackingChannel.outputEnable(enable);
            }
        }
    } else {
        channel.outputEnable(enable);
    }
}

void disableOutputForAllChannels() {
    for (int i = 0; i < CH_NUM; i++) {
        if (Channel::get(i).isOutputEnabled()) {
            Channel::get(i).outputEnable(false);
        }
    }
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

void remoteProgrammingEnable(Channel &channel, bool enable) {
    if (channel.channelIndex < 2 && (g_couplingType == COUPLING_TYPE_SERIES || g_couplingType == COUPLING_TYPE_PARALLEL)) {
        Channel::get(0).remoteProgrammingEnable(enable);
        Channel::get(1).remoteProgrammingEnable(enable);
    } else if (channel.flags.trackingEnabled) {
        for (int i = 0; i < CH_NUM; ++i) {
            Channel &trackingChannel = Channel::get(i);
            if (trackingChannel.flags.trackingEnabled) {
                trackingChannel.remoteProgrammingEnable(enable);
            }
        }
    } else {
        channel.remoteProgrammingEnable(enable);
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
                return;
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
                return;
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
                return;
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
                return;
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
        value = getTrackingValuePrecision(UNIT_VOLT, value);
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
        value = getTrackingValuePrecision(UNIT_AMPER, value);
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

#ifdef EEZ_PLATFORM_SIMULATOR
void setLoadEnabled(Channel &channel, bool state) {
    if (channel.channelIndex < 2 && (g_couplingType == COUPLING_TYPE_SERIES || g_couplingType == COUPLING_TYPE_PARALLEL)) {
        Channel::get(0).simulator.setLoadEnabled(state);
        Channel::get(1).simulator.setLoadEnabled(state);
    } else {
        channel.simulator.setLoadEnabled(state);
    }
}

void setLoad(Channel &channel, float load) {
    load = roundPrec(load, 0.001f);

    if (channel.channelIndex < 2 && (g_couplingType == COUPLING_TYPE_SERIES || g_couplingType == COUPLING_TYPE_PARALLEL)) {
        Channel::get(0).simulator.setLoad(load);
        Channel::get(1).simulator.setLoad(load);
    } else {
        channel.simulator.setLoad(load);
    }
}
#endif

} // namespace channel_dispatcher
} // namespace psu
} // namespace eez
