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

#include <math.h>
#include <stdio.h>
#include <assert.h>

#include <eez/firmware.h>
#include <eez/system.h>
#include <eez/modules/psu/board.h>
#include <eez/modules/psu/calibration.h>
#include <eez/modules/psu/channel_dispatcher.h>
#include <eez/modules/psu/event_queue.h>
#include <eez/modules/psu/io_pins.h>
#include <eez/modules/psu/list_program.h>
#include <eez/modules/psu/ontime.h>
#include <eez/modules/psu/persist_conf.h>
#include <eez/modules/psu/profile.h>
#include <eez/modules/psu/ramp.h>
#include <eez/modules/psu/trigger.h>
#include <eez/scpi/regs.h>
#include <eez/sound.h>
#include <eez/index.h>
#include <eez/gui/gui.h>

#include <eez/modules/bp3c/io_exp.h>
#include <eez/modules/bp3c/flash_slave.h>

namespace eez {

using namespace scpi;

namespace psu {

void ChannelHistory::reset() {
    for (int i = 0; i < CHANNEL_HISTORY_SIZE; ++i) {
        uHistory[i] = 0;
        iHistory[i] = 0;
    }
    historyStarted = 0;
    historyPosition = 1;
}

void ChannelHistory::update(uint32_t tickCount) {
    if (!historyStarted) {
        historyStarted = 1;
        historyLastTick = tickCount;
        historyPosition = 1;
    } else {
        uint32_t ytViewRateMicroseconds = (int)round(channel.ytViewRate * 1000000L);
        while (tickCount - historyLastTick >= ytViewRateMicroseconds) {
            uint32_t historyIndex = historyPosition % CHANNEL_HISTORY_SIZE;
            uHistory[historyIndex] = channel_dispatcher::getUMonLast(channel);
            iHistory[historyIndex] = channel_dispatcher::getIMonLast(channel);
            historyPosition++;
            historyLastTick += ytViewRateMicroseconds;
        }
    }
}

float ChannelHistory::getChannel0HistoryValue(uint32_t rowIndex, uint8_t columnIndex, float *max) {
    Channel &channel = *Channel::g_channels[0];
    ChannelHistory *channelHistory = channel.channelHistory;
    if (!channelHistory) {
        return NAN;
    }

    uint32_t position = rowIndex % CHANNEL_HISTORY_SIZE;
    
    if (columnIndex == 0) {
        if (channel.flags.displayValue1 == DISPLAY_VALUE_VOLTAGE) {
            return channelHistory->uHistory[position];
        }
        if (channel.flags.displayValue1 == DISPLAY_VALUE_CURRENT) {
            return channelHistory->iHistory[position];
        }
    } else {
        if (channel.flags.displayValue2 == DISPLAY_VALUE_VOLTAGE) {
            return channelHistory->uHistory[position];
        }
        if (channel.flags.displayValue2 == DISPLAY_VALUE_CURRENT) {
            return channelHistory->iHistory[position];
        }
    }

    return channelHistory->uHistory[position] * channelHistory->iHistory[position];
}

float ChannelHistory::getChannel1HistoryValue(uint32_t rowIndex, uint8_t columnIndex, float *max) {
    Channel &channel = *Channel::g_channels[1];
    ChannelHistory *channelHistory = channel.channelHistory;
    if (!channelHistory) {
        return NAN;
    }

    uint32_t position = rowIndex % CHANNEL_HISTORY_SIZE;

    if (columnIndex == 0) {
        if (channel.flags.displayValue1 == DISPLAY_VALUE_VOLTAGE) {
            return channelHistory->uHistory[position];
        }
        if (channel.flags.displayValue1 == DISPLAY_VALUE_CURRENT) {
            return channelHistory->iHistory[position];
        }
    } else {
        if (channel.flags.displayValue2 == DISPLAY_VALUE_VOLTAGE) {
            return channelHistory->uHistory[position];
        }
        if (channel.flags.displayValue2 == DISPLAY_VALUE_CURRENT) {
            return channelHistory->iHistory[position];
        }
    }

    return channelHistory->uHistory[position] * channelHistory->iHistory[position];
}

float ChannelHistory::getChannel2HistoryValue(uint32_t rowIndex, uint8_t columnIndex, float *max) {
    Channel &channel = *Channel::g_channels[2];
    ChannelHistory *channelHistory = channel.channelHistory;
    if (!channelHistory) {
        return NAN;
    }

    uint32_t position = rowIndex % CHANNEL_HISTORY_SIZE;

    if (columnIndex == 0) {
        if (channel.flags.displayValue1 == DISPLAY_VALUE_VOLTAGE) {
            return channelHistory->uHistory[position];
        }
        if (channel.flags.displayValue1 == DISPLAY_VALUE_CURRENT) {
            return channelHistory->iHistory[position];
        }
    } else {
        if (channel.flags.displayValue2 == DISPLAY_VALUE_VOLTAGE) {
            return channelHistory->uHistory[position];
        }
        if (channel.flags.displayValue2 == DISPLAY_VALUE_CURRENT) {
            return channelHistory->iHistory[position];
        }
    }

    return channelHistory->uHistory[position] * channelHistory->iHistory[position];
}

float ChannelHistory::getChannel3HistoryValue(uint32_t rowIndex, uint8_t columnIndex, float *max) {
    Channel &channel = *Channel::g_channels[3];
    ChannelHistory *channelHistory = channel.channelHistory;
    if (!channelHistory) {
        return NAN;
    }

    uint32_t position = rowIndex % CHANNEL_HISTORY_SIZE;

    if (columnIndex == 0) {
        if (channel.flags.displayValue1 == DISPLAY_VALUE_VOLTAGE) {
            return channelHistory->uHistory[position];
        }
        if (channel.flags.displayValue1 == DISPLAY_VALUE_CURRENT) {
            return channelHistory->iHistory[position];
        }
    } else {
        if (channel.flags.displayValue2 == DISPLAY_VALUE_VOLTAGE) {
            return channelHistory->uHistory[position];
        }
        if (channel.flags.displayValue2 == DISPLAY_VALUE_CURRENT) {
            return channelHistory->iHistory[position];
        }
    }

    return channelHistory->uHistory[position] * channelHistory->iHistory[position];
}

float ChannelHistory::getChannel4HistoryValue(uint32_t rowIndex, uint8_t columnIndex, float *max) {
    Channel &channel = *Channel::g_channels[4];
    ChannelHistory *channelHistory = channel.channelHistory;
    if (!channelHistory) {
        return NAN;
    }

    uint32_t position = rowIndex % CHANNEL_HISTORY_SIZE;

    if (columnIndex == 0) {
        if (channel.flags.displayValue1 == DISPLAY_VALUE_VOLTAGE) {
            return channelHistory->uHistory[position];
        }
        if (channel.flags.displayValue1 == DISPLAY_VALUE_CURRENT) {
            return channelHistory->iHistory[position];
        }
    } else {
        if (channel.flags.displayValue2 == DISPLAY_VALUE_VOLTAGE) {
            return channelHistory->uHistory[position];
        }
        if (channel.flags.displayValue2 == DISPLAY_VALUE_CURRENT) {
            return channelHistory->iHistory[position];
        }
    }

    return channelHistory->uHistory[position] * channelHistory->iHistory[position];
}

float ChannelHistory::getChannel5HistoryValue(uint32_t rowIndex, uint8_t columnIndex, float *max) {
    Channel &channel = *Channel::g_channels[5];
    ChannelHistory *channelHistory = channel.channelHistory;
    if (!channelHistory) {
        return NAN;
    }

    uint32_t position = rowIndex % CHANNEL_HISTORY_SIZE;

    if (columnIndex == 0) {
        if (channel.flags.displayValue1 == DISPLAY_VALUE_VOLTAGE) {
            return channelHistory->uHistory[position];
        }
        if (channel.flags.displayValue1 == DISPLAY_VALUE_CURRENT) {
            return channelHistory->iHistory[position];
        }
    } else {
        if (channel.flags.displayValue2 == DISPLAY_VALUE_CURRENT) {
            return channelHistory->iHistory[position];
        }
        if (channel.flags.displayValue2 == DISPLAY_VALUE_VOLTAGE) {
            return channelHistory->uHistory[position];
        }
    }

    return channelHistory->uHistory[position] * channelHistory->iHistory[position];
}

YtDataGetValueFunctionPointer ChannelHistory::getChannelHistoryValueFuncs(int channelIndex) {
    if (channelIndex == 0) {
        return ChannelHistory::getChannel0HistoryValue;
    } else if (channelIndex == 1) {
        return ChannelHistory::getChannel1HistoryValue;
    } else if (channelIndex == 2) {
        return ChannelHistory::getChannel2HistoryValue;
    } else if (channelIndex == 3) {
        return ChannelHistory::getChannel3HistoryValue;
    } else if (channelIndex == 4) {
        return ChannelHistory::getChannel4HistoryValue;
    } else {
        return ChannelHistory::getChannel5HistoryValue;
    }
}

////////////////////////////////////////////////////////////////////////////////

void Channel::Value::init(float set_, float step_, float limit_) {
    set = set_;
    step = step_;
    limit = limit_;
    resetMonValues();
}

void Channel::Value::resetMonValues() {
    mon_adc = 0;
    mon = 0;
    mon_last = 0;
    mon_dac = 0;

    mon_index = -1;
    mon_dac_index = -1;

    mon_measured = false;
}

void Channel::Value::addMonValue(float value, float prec) {
    if (io_pins::isInhibited()) {
        value = 0;
    }
    
    mon_last = roundPrec(value, prec);

    if (mon_index == -1) {
        mon_index = 0;
        for (int i = 0; i < NUM_ADC_AVERAGING_VALUES; ++i) {
            mon_arr[i] = value;
        }
        mon_total = NUM_ADC_AVERAGING_VALUES * value;
        mon = mon_last;
        mon_prev = mon_last;
    } else {
        mon_total -= mon_arr[mon_index];
        mon_total += value;
        mon_arr[mon_index] = value;
        mon_index = (mon_index + 1) % NUM_ADC_AVERAGING_VALUES;

        if (io_pins::isInhibited()) {
            mon = 0;
            mon_prev = 0;
        } else {
#if defined(EEZ_PLATFORM_STM32)
            float mon_next = mon_total / NUM_ADC_AVERAGING_VALUES;
            if (fabs(mon_prev - mon_next) >= prec) {
                mon = roundPrec(mon_next, prec);
                mon_prev = mon_next;
            }
#endif
            
#if defined(EEZ_PLATFORM_SIMULATOR)
            float mon_next = roundPrec(mon_total / NUM_ADC_AVERAGING_VALUES, prec);
            mon = mon_next;
            mon_prev = mon_next;
#endif
        }
    }

    mon_measured = true;
}

void Channel::Value::addMonDacValue(float value, float prec) {
    mon_dac_last = roundPrec(value, prec);

    if (mon_dac_index == -1) {
        mon_dac_index = 0;
        for (int i = 0; i < NUM_ADC_AVERAGING_VALUES; ++i) {
            mon_dac_arr[i] = value;
        }
        mon_dac_total = NUM_ADC_AVERAGING_VALUES * value;
        mon_dac = mon_dac_last;
        mon_dac_prev = mon_dac_last;
    } else {
        mon_dac_total -= mon_dac_arr[mon_dac_index];
        mon_dac_total += value;
        mon_dac_arr[mon_dac_index] = value;
        mon_dac_index = (mon_dac_index + 1) % NUM_ADC_AVERAGING_VALUES;

#if defined(EEZ_PLATFORM_STM32)
        float mon_dac_next = mon_dac_total / NUM_ADC_AVERAGING_VALUES;
		if (fabs(mon_dac_prev - mon_dac_next) >= prec) {
			mon_dac = roundPrec(mon_dac_next, prec);
			mon_dac_prev = mon_dac_next;
		}
#endif
        
#if defined(EEZ_PLATFORM_SIMULATOR)
        float mon_next = roundPrec(mon_total / NUM_ADC_AVERAGING_VALUES, prec);
        mon = mon_next;
        mon_prev = mon_next;
#endif
    }
}

////////////////////////////////////////////////////////////////////////////////

int CH_NUM = 0;
Channel *Channel::g_channels[CH_MAX];
int8_t Channel::g_slotIndexToChannelIndex[NUM_SLOTS] = { -1, -1, -1 };

int g_errorChannelIndex = -1;

////////////////////////////////////////////////////////////////////////////////

static uint16_t g_oeSavedState;

void Channel::saveAndDisableOE() {
    if (!isPsuThread()) {
        sendMessageToPsu(PSU_MESSAGE_TRIGGER_CHANNEL_SAVE_AND_DISABLE_OE, 0, 0);
    } else {
        if (!g_oeSavedState) {
            for (int i = 0; i < CH_NUM; i++)  {
                Channel& channel = Channel::get(i);
                if (channel.isOutputEnabled()) {
                    g_oeSavedState |= (1 << (i + 1));
                    channel_dispatcher::outputEnableOnNextSync(channel, false);
                }
            }
            channel_dispatcher::syncOutputEnable();
            g_oeSavedState |= 1;
        }
    }
}

void Channel::restoreOE() {
    if (!isPsuThread()) {
        sendMessageToPsu(PSU_MESSAGE_TRIGGER_CHANNEL_RESTORE_OE, 0, 0);
    } else {
        if (g_oeSavedState) {
            for (int i = 0; i < CH_NUM; i++)  {
                if ((g_oeSavedState & (1 << (i + 1))) != 0) {
                    channel_dispatcher::outputEnableOnNextSync(Channel::get(i), true);
                }
            }
            channel_dispatcher::syncOutputEnable();
            g_oeSavedState = 0;
        }
    }
}

////////////////////////////////////////////////////////////////////////////////

#ifdef EEZ_PLATFORM_SIMULATOR

void Channel::Simulator::setLoadEnabled(bool value) {
    load_enabled = value;
}

bool Channel::Simulator::getLoadEnabled() {
    return load_enabled;
}

void Channel::Simulator::setLoad(float value) {
    load = value;
}

float Channel::Simulator::getLoad() {
    return load;
}

void Channel::Simulator::setVoltProgExt(float value) {
    voltProgExt = value;
}

float Channel::Simulator::getVoltProgExt() {
    return voltProgExt;
}

#endif

////////////////////////////////////////////////////////////////////////////////

Channel::Channel(uint8_t slotIndex_, uint8_t channelIndex_, uint8_t subchannelIndex_) {
	slotIndex = slotIndex_;
    channelIndex = channelIndex_;
    subchannelIndex = subchannelIndex_;
}

void Channel::initParams(uint16_t moduleRevision) {
    getParams(moduleRevision);

    u.min = roundChannelValue(UNIT_VOLT, params.U_MIN);
    u.max = roundChannelValue(UNIT_VOLT, params.U_MAX);
    u.def = roundChannelValue(UNIT_VOLT, params.U_DEF);

    i.min = roundChannelValue(UNIT_AMPER, params.I_MIN);
    i.max = roundChannelValue(UNIT_AMPER, params.I_MAX);
    i.def = roundChannelValue(UNIT_AMPER, params.I_DEF);

#ifdef EEZ_PLATFORM_SIMULATOR
    simulator.load_enabled = true;
    simulator.load = 10;
#endif

    flags.currentCurrentRange = CURRENT_RANGE_HIGH;
    flags.currentRangeSelectionMode = CURRENT_RANGE_SELECTION_ALWAYS_HIGH;
    flags.autoSelectCurrentRange = 0;

    flags.displayValue1 = DISPLAY_VALUE_VOLTAGE;
    flags.displayValue2 = DISPLAY_VALUE_CURRENT;
    ytViewRate = GUI_YT_VIEW_RATE_DEFAULT;

    autoRangeCheckLastTickCount = 0;

    flags.cvMode = 0;
    flags.ccMode = 0;
}

int Channel::reg_get_ques_isum_bit_mask_for_channel_protection_value(ProtectionValue &cpv) {
    if (IS_OVP_VALUE(this, cpv))
        return QUES_ISUM_OVP;
    if (IS_OCP_VALUE(this, cpv))
        return QUES_ISUM_OCP;
    return QUES_ISUM_OPP;
}

void Channel::protectionEnter(ProtectionValue &cpv, bool hwOvp) {
    if (IS_OVP_VALUE(this, cpv)) {
        if (hwOvp) {
            DebugTrace("HW OVP detected\n");
        } else {
            if (flags.rprogEnabled) {
                DebugTrace("OVP condition: %f (U_MON_DAC) > %f (OVP level)\n", channel_dispatcher::getUMonDacLast(*this), getSwOvpProtectionLevel());
            } else {
                DebugTrace("OVP condition: %f (U_MON) > %f (OVP level)\n", channel_dispatcher::getUMonLast(*this), getSwOvpProtectionLevel());
            }
        }
    } else if (IS_OCP_VALUE(this, cpv)) {
        DebugTrace("OCP condition: %f (I_MON) > %f (I_SET)\n", channel_dispatcher::getIMonLast(*this), channel_dispatcher::getISet(*this));
    } else if (IS_OPP_VALUE(this, cpv)) {
        DebugTrace("OPP condition: %f (U_MON) * %f (I_MON) > %f (OPP level)\n", channel_dispatcher::getUMonLast(*this), channel_dispatcher::getIMonLast(*this), channel_dispatcher::getPowerProtectionLevel(*this));
    }

    if (getVoltageTriggerMode() != TRIGGER_MODE_FIXED) {
        trigger::abort();
    }

    channel_dispatcher::outputEnable(*this, false);

    cpv.flags.tripped = 1;

    int bit_mask = reg_get_ques_isum_bit_mask_for_channel_protection_value(cpv);
    setQuesBits(bit_mask, true);

    int16_t eventId;

    if (IS_OVP_VALUE(this, cpv)) {
        if (flags.rprogEnabled && channel_dispatcher::getUProtectionLevel(*this) == channel_dispatcher::getUMax(*this)) {
            g_rprogAlarm = true;
        }
        doRemoteProgrammingEnable(false);

        eventId = event_queue::EVENT_ERROR_CH_OVP_TRIPPED;
    } else if (IS_OCP_VALUE(this, cpv)) {
        eventId = event_queue::EVENT_ERROR_CH_OCP_TRIPPED;
    } else {
        eventId = event_queue::EVENT_ERROR_CH_OPP_TRIPPED;
    }

    event_queue::pushChannelEvent(eventId, channelIndex);

    onProtectionTripped();
}

float Channel::getSwOvpProtectionLevel() {
    if (prot_conf.flags.u_type) {
    	const float CONF_MIN_SW_OVP_PROTECTION_LEVEL = 0.35f; // 350 mV
        return MAX(channel_dispatcher::getUSet(*this) * 1.03f, CONF_MIN_SW_OVP_PROTECTION_LEVEL); // 3%
    }
    return channel_dispatcher::getUProtectionLevel(*this);
}

bool Channel::checkSwOvpCondition() {
    float uProtectionLevel = getSwOvpProtectionLevel();
    return channel_dispatcher::getUMonLast(*this) > uProtectionLevel || (flags.rprogEnabled && channel_dispatcher::getUMonDacLast(*this) > uProtectionLevel);
}

void Channel::protectionCheck(ProtectionValue &cpv) {
    bool state;
    bool condition;
    float delay;

    if (IS_OVP_VALUE(this, cpv)) {
        state = (flags.rprogEnabled || prot_conf.flags.u_state) && !((params.features & CH_FEATURE_HW_OVP) && prot_conf.flags.u_type && !prot_conf.flags.u_hwOvpDeactivated);
        condition = checkSwOvpCondition();
        delay = prot_conf.u_delay;
        delay -= PROT_DELAY_CORRECTION;
    } else if (IS_OCP_VALUE(this, cpv)) {
        state = prot_conf.flags.i_state;
        condition = channel_dispatcher::getIMonLast(*this) >= channel_dispatcher::getISet(*this);
        delay = prot_conf.i_delay;
        delay -= PROT_DELAY_CORRECTION;
    } else {
        state = prot_conf.flags.p_state;
        condition = channel_dispatcher::getUMonLast(*this) * channel_dispatcher::getIMonLast(*this) > channel_dispatcher::getPowerProtectionLevel(*this);
        delay = prot_conf.p_delay;
    }

    if (state && isOutputEnabled() && condition) {
        if (delay > 0) {
            if (cpv.flags.alarmed) {
                if (micros() - cpv.alarm_started >= delay * 1000000UL) {
                    cpv.flags.alarmed = 0;
                    protectionEnter(cpv, false);
                }
            } else {
                cpv.flags.alarmed = 1;
                cpv.alarm_started = micros();
            }
        } else {
            protectionEnter(cpv, false);
        }
    } else {
        cpv.flags.alarmed = 0;
    }
}

////////////////////////////////////////////////////////////////////////////////

void Channel::onPowerDown() {
    doRemoteSensingEnable(false);
    doRemoteProgrammingEnable(false);

    clearProtection(false);
}

void Channel::reset() {
    flags.outputEnabled = 0;
    flags.senseEnabled = 0;
    flags.rprogEnabled = 0;

    flags.cvMode = 0;
    flags.ccMode = 0;

    ovp.flags.tripped = 0;
    ovp.flags.alarmed = 0;

    ocp.flags.tripped = 0;
    ocp.flags.alarmed = 0;

    opp.flags.tripped = 0;
    opp.flags.alarmed = 0;

    flags.currentCurrentRange = CURRENT_RANGE_HIGH;
    flags.currentRangeSelectionMode = CURRENT_RANGE_SELECTION_USE_BOTH;
    flags.autoSelectCurrentRange = 0;

    flags.dprogState = DPROG_STATE_ON;

    // CAL:STAT ON if valid calibrating data for both voltage and current exists in the nonvolatile
    // memory, otherwise OFF.
    doCalibrationEnable(isCalibrationExists());

    // OUTP:PROT:CLE OFF
    // [SOUR[n]]:VOLT:PROT:TRIP? 0
    // [SOUR[n]]:CURR:PROT:TRIP? 0
    // [SOUR[n]]:POW:PROT:TRIP? 0
    clearProtection(false);

    // [SOUR[n]]:VOLT:SENS INTernal
    doRemoteSensingEnable(false);

    // [SOUR[n]]:VOLT:PROG INTernal
    doRemoteProgrammingEnable(false);

    // [SOUR[n]]:VOLT:PROT:DEL
    // [SOUR[n]]:VOLT:PROT:STAT
    // [SOUR[n]]:CURR:PROT:DEL
    // [SOUR[n]]:CURR:PROT:STAT
    // [SOUR[n]]:POW:PROT[:LEV]
    // [SOUR[n]]:POW:PROT:DEL
    // [SOUR[n]]:POW:PROT:STAT -> set all to default
    clearProtectionConf();

    // [SOUR[n]]:CURR
    // [SOUR[n]]:CURR:STEP
    // [SOUR[n]]:VOLT
    // [SOUR[n]]:VOLT:STEP -> set all to default
    u.init(params.U_MIN, params.U_DEF_STEP, u.max);
    i.init(params.I_MIN, params.I_DEF_STEP, i.max);

    u.rampDuration = RAMP_DURATION_DEF_VALUE_U;
    i.rampDuration = RAMP_DURATION_DEF_VALUE_I;

    maxCurrentLimitCause = MAX_CURRENT_LIMIT_CAUSE_NONE;
    p_limit = roundChannelValue(UNIT_WATT, params.PTOT);

    resetHistory();

    flags.displayValue1 = DISPLAY_VALUE_VOLTAGE;
    flags.displayValue2 = DISPLAY_VALUE_CURRENT;
    ytViewRate = GUI_YT_VIEW_RATE_DEFAULT;

    flags.voltageTriggerMode = TRIGGER_MODE_FIXED;
    flags.currentTriggerMode = TRIGGER_MODE_FIXED;
    flags.triggerOutputState = 1;
    flags.triggerOnListStop = TRIGGER_ON_LIST_STOP_OUTPUT_OFF;
    u.triggerLevel = params.U_MIN;
    i.triggerLevel = params.I_MIN;
    list::resetChannelList(*this);

    outputDelayDuration = 0;

#ifdef EEZ_PLATFORM_SIMULATOR
    simulator.setLoadEnabled(false);
    simulator.load = 10;
#endif
}

uint32_t Channel::getCurrentHistoryValuePosition() {
    return channelHistory ? channelHistory->historyPosition : 0;
}

void Channel::resetHistoryForAllChannels() {
    for (int i = 0; i < CH_NUM; i++) {
        Channel::get(i).resetHistory();
    }
}

void Channel::resetHistory() {
    if (channelHistory) {
        channelHistory->reset();
    }
}

void Channel::clearProtectionConf() {
    prot_conf.flags.u_state = params.OVP_DEFAULT_STATE;
    if (params.features & CH_FEATURE_HW_OVP) {
        prot_conf.flags.u_type = 1; // HW
    } else {
        prot_conf.flags.u_type = 0; // SW
    }
    prot_conf.flags.i_state = params.OCP_DEFAULT_STATE;
    prot_conf.flags.p_state = params.OPP_DEFAULT_STATE;

    prot_conf.u_delay = params.OVP_DEFAULT_DELAY;
    prot_conf.u_level = u.max;
    prot_conf.i_delay = params.OCP_DEFAULT_DELAY;
    prot_conf.p_delay = params.OPP_DEFAULT_DELAY;
    prot_conf.p_level = params.OPP_DEFAULT_LEVEL;

    temperature::sensors[temp_sensor::CH1 + channelIndex].prot_conf.state = OTP_CH_DEFAULT_STATE;
    temperature::sensors[temp_sensor::CH1 + channelIndex].prot_conf.level = OTP_CH_DEFAULT_LEVEL;
    temperature::sensors[temp_sensor::CH1 + channelIndex].prot_conf.delay = OTP_CH_DEFAULT_DELAY;
}

bool Channel::isPowerOk() {
    return flags.powerOk;
}

bool Channel::isTestFailed() {
    return getTestResult() == TEST_FAILED;
}

bool Channel::isTestOk() {
    return getTestResult() == TEST_OK;
}

bool Channel::isOk() {
    return g_slots[slotIndex]->enabled && isPowerUp() && isPowerOk() && isTestOk() && !bp3c::flash_slave::g_bootloaderMode;
}

void Channel::tick(uint32_t tick_usec) {
    if (!isOk()) {
        return;
    }

    tickSpecific(tick_usec);

    if (params.features & CH_FEATURE_RPOL) {
        unsigned rpol = 0;
            
        rpol = getRPol();

        if (rpol != flags.rpol) {
            flags.rpol = rpol;
            setQuesBits(QUES_ISUM_RPOL, flags.rpol ? true : false);
        }

        if (rpol && isOutputEnabled()) {
            channel_dispatcher::outputEnable(*this, false);
            event_queue::pushChannelEvent(event_queue::EVENT_ERROR_CH_REMOTE_SENSE_REVERSE_POLARITY_DETECTED, channelIndex);
            onProtectionTripped();
        }
    }

    if (!io_pins::isInhibited()) {
        setCvMode(isInCvMode());
        setCcMode(isInCcMode());
    }

    if (channelHistory) {
        channelHistory->update(tick_usec);
    }

    doAutoSelectCurrentRange(tick_usec);
}

float Channel::getValuePrecision(Unit unit, float value) const {
    if (unit == UNIT_VOLT) {
        return getVoltageResolution();
    } else if (unit == UNIT_AMPER) {
        return getCurrentResolution(value);
    } else if (unit == UNIT_WATT) {
        return getPowerResolution();
    } else if (unit == UNIT_CELSIUS) {
        return 1;
    } else if (unit == UNIT_SECOND) {
        return 0.001f;
    }
    return 1;
}

float Channel::getVoltageResolution() const {
    return calibration::g_editor.isEnabled() ? params.U_RESOLUTION_DURING_CALIBRATION : params.U_RESOLUTION;
}

float Channel::getCurrentResolution(float value) const {
    float precision = calibration::g_editor.isEnabled() ? params.I_RESOLUTION_DURING_CALIBRATION : params.I_RESOLUTION; // 0.5mA

    if (hasSupportForCurrentDualRange()) {
        if (!isNaN(value) && value <= 0.05f && isMicroAmperAllowed()) {
            precision = calibration::g_editor.isEnabled() ? params.I_LOW_RESOLUTION_DURING_CALIBRATION : params.I_LOW_RESOLUTION; // 5uA
        }
    }
    
    return precision;
}

float Channel::getPowerResolution() const {
    return params.P_RESOLUTION; // 1 mW;
}

bool Channel::isMicroAmperAllowed() const {
    return hasSupportForCurrentDualRange() ? flags.currentRangeSelectionMode != CURRENT_RANGE_SELECTION_ALWAYS_HIGH : false;
}

bool Channel::isAmperAllowed() const {
    return hasSupportForCurrentDualRange() ? flags.currentRangeSelectionMode != CURRENT_RANGE_SELECTION_ALWAYS_LOW : true;
}

float Channel::roundChannelValue(Unit unit, float value) const {
    return roundPrec(value, getValuePrecision(unit, value));
}

float remapAdcValue(float value, CalibrationValueConfiguration &cal) {
    unsigned i;
    unsigned j;

    if (cal.numPoints == 2) {
        i = 0;
        j = 1;
    } else {
        for (j = 1; j < cal.numPoints - 1 && value > cal.points[j].adc; j++) {
        }
        i = j - 1;
    }

    if (cal.points[i].adc == cal.points[j].adc) {
    	return value;
    }

    return remap(value,
        cal.points[i].adc,
        cal.points[i].value,
        cal.points[j].adc,
        cal.points[j].value);
}

void Channel::addUMonAdcValue(float value) {
    if (isVoltageCalibrationEnabled()) {
        value = remapAdcValue(value, cal_conf.u);
    }
    u.addMonValue(value, getVoltageResolution());
}

void Channel::addIMonAdcValue(float value) {
    if (isCurrentCalibrationEnabled()) {
        value = remapAdcValue(value, cal_conf.i[flags.currentCurrentRange]);
    }
    i.addMonValue(value, getCurrentResolution(value));
}

void Channel::addUMonDacAdcValue(float value) {
    u.addMonDacValue(value, getVoltageResolution());
}

void Channel::addIMonDacAdcValue(float value) {
    i.addMonDacValue(value, getCurrentResolution(value));
}

void Channel::onAdcData(AdcDataType adcDataType, float value) {
    switch (adcDataType) {
    case ADC_DATA_TYPE_U_MON:
        addUMonAdcValue(value);
        break;

    case ADC_DATA_TYPE_I_MON:
        addIMonAdcValue(value);
        break;

    case ADC_DATA_TYPE_U_MON_DAC:
        addUMonDacAdcValue(value);
        break;

    case ADC_DATA_TYPE_I_MON_DAC:
        addIMonDacAdcValue(value);
        break;
    }

    protectionCheck();
}

void Channel::setCcMode(bool cc_mode) {
    if (cc_mode != flags.ccMode) {
        flags.ccMode = cc_mode;

        setOperBits(OPER_ISUM_CC, cc_mode);
        setQuesBits(QUES_ISUM_VOLT, cc_mode);
    }
}

void Channel::setCvMode(bool cv_mode) {
    if (cv_mode != flags.cvMode) {
        flags.cvMode = cv_mode;

        setOperBits(OPER_ISUM_CV, cv_mode);
        setQuesBits(QUES_ISUM_CURR, cv_mode);
    }
}

void Channel::protectionCheck() {
    protectionCheck(ovp);
    protectionCheck(ocp);
    protectionCheck(opp);
}

void Channel::updateAllChannels() {
    for (int i = 0; i < CH_NUM; ++i) {
        Channel &channel = Channel::get(i);
        if (channel.isOk()) {
            channel.doCalibrationEnable(persist_conf::isChannelCalibrationEnabled(channel.slotIndex, channel.subchannelIndex) && channel.isCalibrationExists());
            channel.setVoltage(channel.u.set);
            channel.setCurrent(channel.i.set);
        }
    }

    for (int i = 0; i < CH_NUM; ++i) {
        Channel &channel = Channel::get(i);
        if (channel.isOk()) {
            channel.flags.outputEnabledValueOnNextSync = channel.flags.outputEnabled;
            channel.flags.outputEnabled = !channel.flags.outputEnabled;
            channel.flags.doOutputEnableOnNextSync = 1;
        }
    }

    Channel::syncOutputEnable();

    for (int i = 0; i < CH_NUM; ++i) {
        Channel &channel = Channel::get(i);
        if (channel.isOk()) {
            channel.doRemoteSensingEnable(channel.flags.senseEnabled);
            channel.doRemoteProgrammingEnable(channel.flags.rprogEnabled);
        }
    }
}

void Channel::executeOutputEnable(bool enable, uint16_t tasks) {
    u.resetMonValues();
    i.resetMonValues();
    setOutputEnable(enable, tasks);

    if (tasks & OUTPUT_ENABLE_TASK_FINALIZE) {
        setOperBits(OPER_ISUM_OE_OFF, !enable);

        if (!enable) {
            setCvMode(false);
            setCcMode(false);

            if (calibration::g_editor.isEnabled()) {
                calibration::g_editor.stop();
            }
        }
    }
}

void Channel::executeOutputEnable(bool inhibited) {
    bool anyToEnable = false;
    bool anyToDisable = false;

    // before SYNC
    for (int i = 0; i < CH_NUM; i++) {
        Channel &channel = Channel::get(i);
        if (channel.flags.doOutputEnableOnNextSync) {
            if (channel.flags.outputEnabled && !inhibited) {
                anyToEnable = true;
                // OE for enabled
                channel.executeOutputEnable(true, OUTPUT_ENABLE_TASK_OE);
            } else if ((channel.flags.outputEnabled && inhibited) || (!channel.flags.outputEnabled && !inhibited)) {
                anyToDisable = true;
                // OVP, DAC, and OE for disabled
                channel.executeOutputEnable(false, OUTPUT_ENABLE_TASK_OVP | OUTPUT_ENABLE_TASK_DAC | OUTPUT_ENABLE_TASK_OE);
            }
        }
    }

    if (anyToEnable || anyToDisable) {
        // SYNC set
#if defined(EEZ_PLATFORM_STM32)
        HAL_GPIO_WritePin(OE_SYNC_GPIO_Port, OE_SYNC_Pin, GPIO_PIN_SET);
#endif

        // AFTER sync
        if (anyToEnable) {
            delayMicroseconds(3000);

            // DAC and current range for enabled
            for (int i = 0; i < CH_NUM; i++) {
                Channel &channel = Channel::get(i);
                if (channel.flags.doOutputEnableOnNextSync) {
                    if (channel.flags.outputEnabled && !inhibited) {
                        channel.executeOutputEnable(true, OUTPUT_ENABLE_TASK_DAC | OUTPUT_ENABLE_TASK_CURRENT_RANGE);
                    }
                }
            }

            delayMicroseconds(500);
        }

        for (int i = 0; i < CH_NUM; i++) {
            Channel &channel = Channel::get(i);
            if (channel.flags.doOutputEnableOnNextSync) {
                if (channel.flags.outputEnabled && !inhibited) {
                    // DP and ADC start for enabled
                    channel.executeOutputEnable(true, OUTPUT_ENABLE_TASK_DP | OUTPUT_ENABLE_TASK_ADC_START);
                } else if ((channel.flags.outputEnabled && inhibited) || (!channel.flags.outputEnabled && !inhibited)) {
                    // current range and DP for disabled
                    channel.executeOutputEnable(false, OUTPUT_ENABLE_TASK_CURRENT_RANGE | OUTPUT_ENABLE_TASK_DP);
                }
            }
        }

        if (anyToEnable) {
            for (int i = 0; i < CH_NUM; i++) {
                Channel &channel = Channel::get(i);
                if (channel.flags.doOutputEnableOnNextSync) {
                    if (channel.flags.outputEnabled && !inhibited) {
                        // OVP for enabled
                        channel.executeOutputEnable(true, OUTPUT_ENABLE_TASK_OVP);
                    }
                }
            }
        }

        // FINALIZE
        for (int i = 0; i < CH_NUM; i++) {
            Channel &channel = Channel::get(i);
            if (channel.flags.doOutputEnableOnNextSync) {
                if (channel.flags.outputEnabled && !inhibited) {
                    channel.executeOutputEnable(true, OUTPUT_ENABLE_TASK_FINALIZE);
                } else if ((channel.flags.outputEnabled && inhibited) || (!channel.flags.outputEnabled && !inhibited)) {
                    channel.executeOutputEnable(false, OUTPUT_ENABLE_TASK_FINALIZE);
                }

                channel.flags.doOutputEnableOnNextSync = 0;
            }
        }

        // SYNC reset
#if defined(EEZ_PLATFORM_STM32)
        HAL_GPIO_WritePin(OE_SYNC_GPIO_Port, OE_SYNC_Pin, GPIO_PIN_RESET);
#endif
    }
}

void Channel::syncOutputEnable() {
    for (int i = 0; i < CH_NUM; i++) {
        Channel &channel = Channel::get(i);
        if (channel.flags.doOutputEnableOnNextSync) { 
            if (channel.flags.outputEnabled != channel.flags.outputEnabledValueOnNextSync) {
                if (channel.flags.outputEnabledValueOnNextSync && !channel.isOk()) {
                    channel.flags.doOutputEnableOnNextSync = 0;
                } else {
                    channel.flags.outputEnabled = channel.flags.outputEnabledValueOnNextSync;
                    if (channel.flags.outputEnabled || g_isBooted) {
                        event_queue::pushChannelEvent((channel.flags.outputEnabled ? event_queue::EVENT_INFO_CH_OUTPUT_ENABLED : event_queue::EVENT_INFO_CH_OUTPUT_DISABLED), channel.channelIndex);
                    }
                }
            } else {
                channel.flags.doOutputEnableOnNextSync = 0;
            }
        }
    }

    if (!io_pins::isInhibited()) {
        executeOutputEnable(false);
    } else {
        for (int i = 0; i < CH_NUM; i++) {
            Channel &channel = Channel::get(i);
            channel.flags.doOutputEnableOnNextSync = 0;
        }
    }
}

void Channel::onInhibitedChanged(bool inhibited) {
    for (int i = 0; i < CH_NUM; i++) {
        Channel &channel = Channel::get(i);
        if (channel.isOutputEnabled()) {
            channel.flags.doOutputEnableOnNextSync = 1;
        }
    }
    executeOutputEnable(inhibited);
}

void Channel::doRemoteSensingEnable(bool enable) {
    if (!(params.features & CH_FEATURE_RPOL)) {
        return;
    }
    if (enable && !isOk()) {
        return;
    }
    flags.senseEnabled = enable;
    setRemoteSense(enable);
    setOperBits(OPER_ISUM_RSENS_ON, enable);
}

void Channel::doRemoteProgrammingEnable(bool enable) {
    if (!(params.features & CH_FEATURE_RPROG)) {
        return;
    }

    if (enable && !isOk()) {
        return;
    }

    flags.rprogEnabled = enable;

    if (enable) {
    	channel_dispatcher::setVoltageLimit(*this, channel_dispatcher::getUMaxOvpLimit(*this));
    	channel_dispatcher::setVoltage(*this, u.min);
    	channel_dispatcher::setOvpLevel(*this, channel_dispatcher::getUMaxOvpLevel(*this));
    	channel_dispatcher::setOvpState(*this, 1);
    } else {
    	if (channel_dispatcher::getULimit(*this) > channel_dispatcher::getUMaxOvpLimit(*this)) {
    		channel_dispatcher::setVoltageLimit(*this, channel_dispatcher::getUMaxOvpLimit(*this));
    	}
    	if (channel_dispatcher::getUProtectionLevel(*this) > channel_dispatcher::getUMaxOvpLevel(*this)) {
    		channel_dispatcher::setOvpLevel(*this, channel_dispatcher::getUMaxOvpLevel(*this));
    	}
    }

    setRemoteProgramming(enable);
    setOperBits(OPER_ISUM_RPROG_ON, enable);
}

bool Channel::isOutputEnabled() {
    return isPowerUp() && flags.outputEnabled;
}

void Channel::doCalibrationEnable(bool enable) {
    flags.calEnabled = enable;

    if (g_isBooted) {
    	setVoltage(u.set);
    	setCurrent(i.set);
    }
}

void Channel::calibrationEnable(bool enabled) {
    if (enabled != isCalibrationEnabled()) {
        doCalibrationEnable(enabled);
        event_queue::pushChannelEvent((enabled ? event_queue::EVENT_INFO_CH_CALIBRATION_ENABLED : event_queue::EVENT_WARNING_CH_CALIBRATION_DISABLED), channelIndex);
        persist_conf::saveCalibrationEnabledFlag(slotIndex, subchannelIndex, enabled);
    }
}

void Channel::calibrationEnableNoEvent(bool enabled) {
    if (enabled != isCalibrationEnabled()) {
        doCalibrationEnable(enabled);
    }
}

bool Channel::isCalibrationEnabled() {
    return flags.calEnabled;
}

bool Channel::isVoltageCalibrationEnabled() {
    return flags.calEnabled && isVoltageCalibrationExists();
}

bool Channel::isCurrentCalibrationEnabled() {
    return flags.calEnabled && (
        (flags.currentCurrentRange == CURRENT_RANGE_HIGH && isCurrentCalibrationExists(0)) ||
        (flags.currentCurrentRange == CURRENT_RANGE_LOW && isCurrentCalibrationExists(1))
    );
}

void Channel::remoteSensingEnable(bool enable) {
    if (enable != flags.senseEnabled) {
        doRemoteSensingEnable(enable);
        event_queue::pushChannelEvent((enable ? event_queue::EVENT_INFO_CH_REMOTE_SENSE_ENABLED : event_queue::EVENT_INFO_CH_REMOTE_SENSE_DISABLED), channelIndex);
    }
}

bool Channel::isRemoteSensingEnabled() {
    return flags.senseEnabled;
}

void Channel::remoteProgrammingEnable(bool enable) {
    if (!isPsuThread()) {
        sendMessageToPsu(PSU_MESSAGE_REMOTE_PROGRAMMING_ENABLE, (channelIndex << 8) | (enable ? 1 : 0));
    } else {
        if (enable != flags.rprogEnabled) {
            if (trigger::isActive()) {
                trigger::abort();
            }
            doRemoteProgrammingEnable(enable);
            event_queue::pushChannelEvent((enable ? event_queue::EVENT_INFO_CH_REMOTE_PROG_ENABLED : event_queue::EVENT_INFO_CH_REMOTE_PROG_DISABLED), channelIndex);
        }
    }
}

bool Channel::isRemoteProgrammingEnabled() {
    return flags.rprogEnabled;
}

float Channel::getCalibratedVoltage(float value) {
    if (isVoltageCalibrationEnabled()) {
        value = calibration::remapValue(value, cal_conf.u);
    }

#if !defined(EEZ_PLATFORM_SIMULATOR)
    value += params.VOLTAGE_GND_OFFSET;
#endif

    return value;
}

void Channel::doSetVoltage(float value) {
    u.set = value;
    u.mon_dac = 0;

    if (prot_conf.u_level < u.set) {
        prot_conf.u_level = u.set;
    }

	value = getCalibratedVoltage(value);

    setDacVoltageFloat(value);
}

void Channel::setVoltage(float value) {
    if (!calibration::g_editor.isEnabled()) {
        value = roundPrec(value, getVoltageResolution());
    }

    doSetVoltage(value);
}

void Channel::doSetCurrent(float value) {
    if (!calibration::g_editor.isEnabled()) {
        if (hasSupportForCurrentDualRange()) {
            if (flags.currentRangeSelectionMode == CURRENT_RANGE_SELECTION_USE_BOTH) {
                setCurrentRange(value > 0.05f ? CURRENT_RANGE_HIGH : CURRENT_RANGE_LOW);
            } else if (flags.currentRangeSelectionMode == CURRENT_RANGE_SELECTION_ALWAYS_HIGH) {
                setCurrentRange(CURRENT_RANGE_HIGH);
            } else {
                setCurrentRange(CURRENT_RANGE_LOW);
            }
        }
    }

    i.set = value;
    i.mon_dac = 0;

    if (isCurrentCalibrationEnabled()) {
        value = calibration::remapValue(value, cal_conf.i[flags.currentCurrentRange]);
    }

    value += getDualRangeGndOffset();

    setDacCurrentFloat(value);
}

void Channel::setCurrent(float value) {
    if (!calibration::g_editor.isEnabled()) {
        value = roundPrec(value, getCurrentResolution(value));
    }

    doSetCurrent(value);
}

bool Channel::isCalibrationExists() {
    return (flags.currentCurrentRange == CURRENT_RANGE_HIGH && isCurrentCalibrationExists(0)) ||
           (flags.currentCurrentRange == CURRENT_RANGE_LOW && isCurrentCalibrationExists(1)) ||
           isVoltageCalibrationExists();
}

bool Channel::isVoltageCalibrationExists() {
    return cal_conf.u.numPoints > 1;
}

bool Channel::isCurrentCalibrationExists(uint8_t currentRange) {
    return cal_conf.i[currentRange].numPoints > 1;
}

bool Channel::isTripped() {
    return ovp.flags.tripped || ocp.flags.tripped || opp.flags.tripped ||
           temperature::isAnySensorTripped(this);
}

void Channel::clearProtection(bool clearOTP) {
    auto lastErrorEventId = event_queue::getLastErrorEventId();
    auto lastErrorEventChannelIndex = event_queue::getLastErrorEventChannelIndex();

    ovp.flags.tripped = 0;
    ovp.flags.alarmed = 0;
    setQuesBits(QUES_ISUM_OVP, false);
    if (lastErrorEventId == event_queue::EVENT_ERROR_CH_OVP_TRIPPED && lastErrorEventChannelIndex == channelIndex) {
        event_queue::markAsRead();
    }

    ocp.flags.tripped = 0;
    ocp.flags.alarmed = 0;
    setQuesBits(QUES_ISUM_OCP, false);
    if (lastErrorEventId == event_queue::EVENT_ERROR_CH_OCP_TRIPPED && lastErrorEventChannelIndex == channelIndex) {
        event_queue::markAsRead();
    }

    opp.flags.tripped = 0;
    opp.flags.alarmed = 0;
    setQuesBits(QUES_ISUM_OPP, false);
    if (lastErrorEventId == event_queue::EVENT_ERROR_CH_OPP_TRIPPED && lastErrorEventChannelIndex == channelIndex) {
        event_queue::markAsRead();
    }

    if (clearOTP) {
        temperature::clearChannelProtection(this);
    }
}

void Channel::disableProtection() {
    if (!isTripped()) {
        prot_conf.flags.u_state = 0;
        prot_conf.flags.i_state = 0;
        prot_conf.flags.p_state = 0;
        temperature::disableChannelProtection(this);
    }
}

void Channel::setQuesBits(int bit_mask, bool on) {
    reg_set_ques_isum_bit(channelIndex, bit_mask, on);
}

void Channel::setOperBits(int bit_mask, bool on) {
    reg_set_oper_isum_bit(channelIndex, bit_mask, on);
}

ChannelMode Channel::getMode() const {
    if (isCvMode()) {
        return CHANNEL_MODE_CV;;
    }
    if (isCcMode()) {
        return CHANNEL_MODE_CC;
    }
    return CHANNEL_MODE_UR;
}

const char *Channel::getModeStr() const {
    ChannelMode mode = getMode();
    if (mode == CHANNEL_MODE_CV) {
        return "CV";
    }
    if (mode == CHANNEL_MODE_CC) {
        return "CC";
    }
    return "UR";
}

float Channel::getVoltageLimit() const {
    return u.limit;
}

float Channel::getVoltageMaxLimit() const {
    return u.max;
}

void Channel::setVoltageLimit(float limit) {
    limit = roundPrec(limit, getVoltageResolution());

    u.limit = limit;
    if (u.set > u.limit) {
        setVoltage(u.limit);
    }
}

float Channel::getCurrentLimit() const {
    return i.limit;
}

void Channel::setCurrentLimit(float limit) {
    limit = roundPrec(limit, getCurrentResolution(limit));

    if (limit > getMaxCurrentLimit()) {
        limit = getMaxCurrentLimit();
    }
    i.limit = limit;
    if (i.set > i.limit) {
        setCurrent(i.limit);
    }
}

float Channel::getMaxCurrentLimit() const {
    float limit;
    if (hasSupportForCurrentDualRange() && flags.currentRangeSelectionMode == CURRENT_RANGE_SELECTION_ALWAYS_LOW) {
        limit = 0.05f;
    } else {
        limit = isMaxCurrentLimited() ? ERR_MAX_CURRENT : i.max;
    }
    return roundChannelValue(UNIT_AMPER, limit);
}

bool Channel::isMaxCurrentLimited() const {
    return getMaxCurrentLimitCause() != MAX_CURRENT_LIMIT_CAUSE_NONE;
}

MaxCurrentLimitCause Channel::getMaxCurrentLimitCause() const {
    if (psu::isMaxCurrentLimited()) {
        return psu::getMaxCurrentLimitCause();
    }
    return maxCurrentLimitCause;
}

void Channel::limitMaxCurrent(MaxCurrentLimitCause cause) {
    if (cause != maxCurrentLimitCause) {
        maxCurrentLimitCause = cause;

        if (isMaxCurrentLimited()) {
            if (i.limit > ERR_MAX_CURRENT) {
                channel_dispatcher::setCurrentLimit(*this, ERR_MAX_CURRENT);
            }
        }
    }
}

void Channel::unlimitMaxCurrent() {
    limitMaxCurrent(MAX_CURRENT_LIMIT_CAUSE_NONE);
}

float Channel::getPowerLimit() const {
    return p_limit;
}

float Channel::getPowerMaxLimit() const {
    return params.PTOT;
}

void Channel::setPowerLimit(float limit) {
    limit = roundPrec(limit, getPowerResolution());

    p_limit = limit;
    if (u.set * i.set > p_limit) {
        // setVoltage(p_limit / i.set);
        setCurrent(p_limit / u.set);
    }
}

bool Channel::isVoltageWithinRange(float u) {
    u = channel_dispatcher::roundChannelValue(*this, UNIT_VOLT, u);
    float min = channel_dispatcher::roundChannelValue(*this, UNIT_VOLT, channel_dispatcher::getUMin(*this));
    float max = channel_dispatcher::roundChannelValue(*this, UNIT_VOLT, channel_dispatcher::getUMax(*this));
    return u >= min && u <= max;
}

bool Channel::isVoltageLimitExceeded(float u) {
    return channel_dispatcher::roundChannelValue(*this, UNIT_VOLT, u) >
        channel_dispatcher::roundChannelValue(*this, UNIT_VOLT, channel_dispatcher::getULimit(*this));
}

bool Channel::isCurrentWithinRange(float i) {
    i = channel_dispatcher::roundChannelValue(*this, UNIT_AMPER, i);
    float min =channel_dispatcher::roundChannelValue(*this, UNIT_AMPER, channel_dispatcher::getIMin(*this));
    float max =channel_dispatcher::roundChannelValue(*this, UNIT_AMPER, channel_dispatcher::getIMax(*this));
    return i >= min && i <= max;
}

bool Channel::isCurrentLimitExceeded(float i) {
    return channel_dispatcher::roundChannelValue(*this, UNIT_AMPER, i) >
        channel_dispatcher::roundChannelValue(*this, UNIT_AMPER, channel_dispatcher::getILimit(*this));
}


TriggerMode Channel::getVoltageTriggerMode() {
    return (TriggerMode)flags.voltageTriggerMode;
}

void Channel::setVoltageTriggerMode(TriggerMode mode) {
    flags.voltageTriggerMode = mode;
}

TriggerMode Channel::getCurrentTriggerMode() {
    return (TriggerMode)flags.currentTriggerMode;
}

void Channel::setCurrentTriggerMode(TriggerMode mode) {
    flags.currentTriggerMode = mode;
}

bool Channel::getTriggerOutputState() {
    return flags.triggerOutputState ? true : false;
}

void Channel::setTriggerOutputState(bool enabled) {
    flags.triggerOutputState = enabled ? 1 : 0;
}

TriggerOnListStop Channel::getTriggerOnListStop() {
    return (TriggerOnListStop)flags.triggerOnListStop;
}

void Channel::setTriggerOnListStop(TriggerOnListStop value) {
    flags.triggerOnListStop = value;
}

float Channel::getDualRangeGndOffset() {
#ifdef EEZ_PLATFORM_SIMULATOR
    return 0;
#else
    return flags.currentCurrentRange == CURRENT_RANGE_LOW ? (params.CURRENT_GND_OFFSET / 100) : params.CURRENT_GND_OFFSET;
#endif
}

bool Channel::hasSupportForCurrentDualRange() const {
    return params.features & CH_FEATURE_CURRENT_DUAL_RANGE ? true : false;
}

void Channel::setCurrentRangeSelectionMode(CurrentRangeSelectionMode mode) {
    flags.currentRangeSelectionMode = mode;

    if (flags.currentRangeSelectionMode == CURRENT_RANGE_SELECTION_ALWAYS_LOW) {
        float limit = 0.05f;
        if (i.set > limit) {
            i.set = limit;
        }
        if (i.limit > limit) {
            i.limit = limit;
        }
    }

    setCurrent(i.set);
}

void Channel::enableAutoSelectCurrentRange(bool enable) {
    flags.autoSelectCurrentRange = enable;

    if (!flags.autoSelectCurrentRange) {
        setCurrent(i.set);
    }
}

float Channel::getDualRangeMax() {
    return flags.currentCurrentRange == CURRENT_RANGE_LOW ? (params.I_MAX / 100) : params.I_MAX;
}

void Channel::setCurrentRange(uint8_t currentCurrentRange) {
    if (hasSupportForCurrentDualRange()) {
        if (currentCurrentRange != flags.currentCurrentRange) {
            flags.currentCurrentRange = currentCurrentRange;
            doSetCurrentRange();
        }
    }
}

void Channel::doAutoSelectCurrentRange(uint32_t tickCount) {
    if (calibration::g_editor.isEnabled()) {
        return;
    }

    if (isOutputEnabled()) {
        if (autoRangeCheckLastTickCount != 0) {
            if (tickCount - autoRangeCheckLastTickCount > CURRENT_AUTO_RANGE_SWITCHING_DELAY_MS * 1000L) {
                if (flags.autoSelectCurrentRange &&
                    flags.currentRangeSelectionMode == CURRENT_RANGE_SELECTION_USE_BOTH &&
                    hasSupportForCurrentDualRange() && 
                    !isDacTesting() &&
                    !calibration::g_editor.isEnabled()) {
                    if (flags.currentCurrentRange == CURRENT_RANGE_LOW) {
                        if (i.set > 0.05f && isCcMode()) {
                            doSetCurrent(i.set);
                        }
                    } else if (i.mon_measured) {
                        if (i.mon_last < 0.05f) {
                            setCurrentRange(1);
                            setDacCurrent((uint16_t)65535);
                        }
                    }
                }
                autoRangeCheckLastTickCount = tickCount;
            }
        } else {
            autoRangeCheckLastTickCount = tickCount;
        }
    } else {
        autoRangeCheckLastTickCount = 0;
    }
}

void Channel::setDprogState(DprogState dprogState) {
    flags.dprogState = dprogState;
}

unsigned Channel::getRPol() {
    return 0;
}

void Channel::setRemoteSense(bool enable) {
}

void Channel::setRemoteProgramming(bool enable) {
}

void Channel::doSetCurrentRange() {
}

bool Channel::isVoltageBalanced() {
	return false;
}

bool Channel::isCurrentBalanced() {
	return false;
}

float Channel::getUSetUnbalanced() {
    return u.set;
}

float Channel::getISetUnbalanced() {
    return i.set;
}

void Channel::readAllRegisters(uint8_t ioexpRegisters[], uint8_t adcRegisters[]) {
}

#if defined(DEBUG) && defined(EEZ_PLATFORM_STM32)
int Channel::getIoExpBitDirection(int io_bit) {
	return 0;
}

bool Channel::testIoExpBit(int io_bit) {
	return false;
}

void Channel::changeIoExpBit(int io_bit, bool set) {
}
#endif

int Channel::getAdvancedOptionsPageId() {
    return gui::PAGE_ID_NONE;
}

} // namespace psu
} // namespace eez
