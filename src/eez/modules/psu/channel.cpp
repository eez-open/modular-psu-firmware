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
#include <eez/modules/psu/persist_conf.h>
#include <eez/modules/psu/profile.h>
#include <eez/modules/psu/trigger.h>
#include <eez/scpi/regs.h>
#include <eez/sound.h>
#include <eez/index.h>

#include <eez/modules/bp3c/io_exp.h>
#include <eez/modules/bp3c/flash_slave.h>

namespace eez {

using namespace scpi;

namespace psu {

////////////////////////////////////////////////////////////////////////////////

int CH_NUM = 0;
Channel Channel::g_channels[CH_MAX];

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
            float mon_next = mon_total / NUM_ADC_AVERAGING_VALUES;
            if (fabs(mon_prev - mon_next) >= prec) {
                mon = roundPrec(mon_next, prec);
                mon_prev = mon_next;
            }
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

        float mon_dac_next = mon_dac_total / NUM_ADC_AVERAGING_VALUES;

		if (fabs(mon_dac_prev - mon_dac_next) >= prec) {
			mon_dac = roundPrec(mon_dac_next, prec);
			mon_dac_prev = mon_dac_next;
		}
    }
}

////////////////////////////////////////////////////////////////////////////////

static uint16_t g_oeSavedState;

void Channel::saveAndDisableOE() {
    if (osThreadGetId() != g_psuTaskHandle) {
        osMessagePut(g_psuMessageQueueId, PSU_QUEUE_MESSAGE(PSU_QUEUE_TRIGGER_CHANNEL_SAVE_AND_DISABLE_OE, 0), 0);
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
    if (osThreadGetId() != g_psuTaskHandle) {
        osMessagePut(g_psuMessageQueueId, PSU_QUEUE_MESSAGE(PSU_QUEUE_TRIGGER_CHANNEL_RESTORE_OE, 0), 0);
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

float Channel::getChannel0HistoryValue(int rowIndex, int columnIndex, float *max) {
    Channel &channel = g_channels[0];

    uint32_t position = rowIndex % CHANNEL_HISTORY_SIZE;
    
    if (columnIndex == 0) {
        if (channel.flags.displayValue1 == DISPLAY_VALUE_VOLTAGE) {
            return channel.uHistory[position];
        }
        if (channel.flags.displayValue1 == DISPLAY_VALUE_CURRENT) {
            return channel.iHistory[position];
        }
    } else {
        if (channel.flags.displayValue2 == DISPLAY_VALUE_VOLTAGE) {
            return channel.uHistory[position];
        }
        if (g_channels[0].flags.displayValue2 == DISPLAY_VALUE_CURRENT) {
            return channel.iHistory[position];
        }
    }

    return channel.uHistory[position] * channel.iHistory[position];
}

float Channel::getChannel1HistoryValue(int rowIndex, int columnIndex, float *max) {
    Channel &channel = g_channels[1];

    uint32_t position = rowIndex % CHANNEL_HISTORY_SIZE;

    if (columnIndex == 0) {
        if (channel.flags.displayValue1 == DISPLAY_VALUE_VOLTAGE) {
            return channel.uHistory[position];
        }
        if (channel.flags.displayValue1 == DISPLAY_VALUE_CURRENT) {
            return channel.iHistory[position];
        }
    } else {
        if (channel.flags.displayValue2 == DISPLAY_VALUE_VOLTAGE) {
            return channel.uHistory[position];
        }
        if (g_channels[0].flags.displayValue2 == DISPLAY_VALUE_CURRENT) {
            return channel.iHistory[position];
        }
    }

    return channel.uHistory[position] * channel.iHistory[position];
}

float Channel::getChannel2HistoryValue(int rowIndex, int columnIndex, float *max) {
    Channel &channel = g_channels[2];

    uint32_t position = rowIndex % CHANNEL_HISTORY_SIZE;

    if (columnIndex == 0) {
        if (channel.flags.displayValue1 == DISPLAY_VALUE_VOLTAGE) {
            return channel.uHistory[position];
        }
        if (channel.flags.displayValue1 == DISPLAY_VALUE_CURRENT) {
            return channel.iHistory[position];
        }
    } else {
        if (channel.flags.displayValue2 == DISPLAY_VALUE_VOLTAGE) {
            return channel.uHistory[position];
        }
        if (g_channels[0].flags.displayValue2 == DISPLAY_VALUE_CURRENT) {
            return channel.iHistory[position];
        }
    }

    return channel.uHistory[position] * channel.iHistory[position];
}

float Channel::getChannel3HistoryValue(int rowIndex, int columnIndex, float *max) {
    Channel &channel = g_channels[3];

    uint32_t position = rowIndex % CHANNEL_HISTORY_SIZE;

    if (columnIndex == 0) {
        if (channel.flags.displayValue1 == DISPLAY_VALUE_VOLTAGE) {
            return channel.uHistory[position];
        }
        if (channel.flags.displayValue1 == DISPLAY_VALUE_CURRENT) {
            return channel.iHistory[position];
        }
    } else {
        if (channel.flags.displayValue2 == DISPLAY_VALUE_VOLTAGE) {
            return channel.uHistory[position];
        }
        if (g_channels[0].flags.displayValue2 == DISPLAY_VALUE_CURRENT) {
            return channel.iHistory[position];
        }
    }

    return channel.uHistory[position] * channel.iHistory[position];
}

float Channel::getChannel4HistoryValue(int rowIndex, int columnIndex, float *max) {
    Channel &channel = g_channels[4];

    uint32_t position = rowIndex % CHANNEL_HISTORY_SIZE;

    if (columnIndex == 0) {
        if (channel.flags.displayValue1 == DISPLAY_VALUE_VOLTAGE) {
            return channel.uHistory[position];
        }
        if (channel.flags.displayValue1 == DISPLAY_VALUE_CURRENT) {
            return channel.iHistory[position];
        }
    } else {
        if (channel.flags.displayValue2 == DISPLAY_VALUE_VOLTAGE) {
            return channel.uHistory[position];
        }
        if (g_channels[0].flags.displayValue2 == DISPLAY_VALUE_CURRENT) {
            return channel.iHistory[position];
        }
    }

    return channel.uHistory[position] * channel.iHistory[position];
}

float Channel::getChannel5HistoryValue(int rowIndex, int columnIndex, float *max) {
    Channel &channel = g_channels[5];

    uint32_t position = rowIndex % CHANNEL_HISTORY_SIZE;

    if (columnIndex == 0) {
        if (channel.flags.displayValue1 == DISPLAY_VALUE_VOLTAGE) {
            return channel.uHistory[position];
        }
        if (channel.flags.displayValue1 == DISPLAY_VALUE_CURRENT) {
            return channel.iHistory[position];
        }
    } else {
        if (channel.flags.displayValue2 == DISPLAY_VALUE_VOLTAGE) {
            return channel.uHistory[position];
        }
        if (g_channels[0].flags.displayValue2 == DISPLAY_VALUE_CURRENT) {
            return channel.iHistory[position];
        }
    }

    return channel.uHistory[position] * channel.iHistory[position];
}

Channel::YtDataGetValueFunctionPointer Channel::getChannelHistoryValueFuncs(int channelIndex) {
    if (channelIndex == 0) {
        return Channel::getChannel0HistoryValue;
    } else if (channelIndex == 1) {
        return Channel::getChannel1HistoryValue;
    } else if (channelIndex == 2) {
        return Channel::getChannel2HistoryValue;
    } else if (channelIndex == 3) {
        return Channel::getChannel3HistoryValue;
    } else if (channelIndex == 4) {
        return Channel::getChannel4HistoryValue;
    } else {
        return Channel::getChannel5HistoryValue;
    }
}

////////////////////////////////////////////////////////////////////////////////

#ifdef EEZ_PLATFORM_SIMULATOR

void Channel::Simulator::setLoadEnabled(bool value) {
    load_enabled = value;
    profile::save();
}

bool Channel::Simulator::getLoadEnabled() {
    return load_enabled;
}

void Channel::Simulator::setLoad(float value) {
    load = value;
    profile::save();
}

float Channel::Simulator::getLoad() {
    return load;
}

void Channel::Simulator::setVoltProgExt(float value) {
    voltProgExt = value;
    profile::save();
}

float Channel::Simulator::getVoltProgExt() {
    return voltProgExt;
}

#endif

////////////////////////////////////////////////////////////////////////////////

void Channel::set(uint8_t slotIndex_, uint8_t subchannelIndex_) {
    auto slot = g_slots[slotIndex_];

	slotIndex = slotIndex_;
    subchannelIndex = subchannelIndex_;

    channelInterface = slot.moduleInfo->channelInterfaces ? slot.moduleInfo->channelInterfaces[slotIndex] : nullptr;

    if (!channelInterface) {
        return;
    }

    channelInterface->getParams(subchannelIndex, params);

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

void Channel::setChannelIndex(uint8_t channelIndex_) {
    channelIndex = channelIndex_;
}

int Channel::reg_get_ques_isum_bit_mask_for_channel_protection_value(ProtectionValue &cpv) {
    if (IS_OVP_VALUE(this, cpv))
        return QUES_ISUM_OVP;
    if (IS_OCP_VALUE(this, cpv))
        return QUES_ISUM_OCP;
    return QUES_ISUM_OPP;
}

void Channel::protectionEnter(ProtectionValue &cpv) {
    trigger::abort();

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

        eventId = event_queue::EVENT_ERROR_CH1_OVP_TRIPPED + channelIndex;
    } else if (IS_OCP_VALUE(this, cpv)) {
        eventId = event_queue::EVENT_ERROR_CH1_OCP_TRIPPED + channelIndex;
    } else {
        eventId = event_queue::EVENT_ERROR_CH1_OPP_TRIPPED + channelIndex;
    }

    event_queue::pushEvent(eventId);

    onProtectionTripped();
}

void Channel::enterOvpProtection() {
   protectionEnter(ovp);
}

bool Channel::checkSwOvpCondition(float uProtectionLevel) {
    return channel_dispatcher::getUMonLast(*this) >= uProtectionLevel || (flags.rprogEnabled && channel_dispatcher::getUMonDacLast(*this) >= uProtectionLevel);
}

void Channel::protectionCheck(ProtectionValue &cpv) {
    bool state;
    bool condition;
    float delay;

    if (IS_OVP_VALUE(this, cpv)) {
        state = (flags.rprogEnabled || prot_conf.flags.u_state) && !((params.features & CH_FEATURE_HW_OVP) && prot_conf.flags.u_type);
        condition = checkSwOvpCondition(channel_dispatcher::getUProtectionLevel(*this));
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

                    // if (IS_OVP_VALUE(this, cpv)) {
                    //    DebugTrace("OVP condition: CV_MODE=%d, CC_MODE=%d, I DIFF=%d mA, I MON=%d
                    //    mA", (int)flags.cvMode, (int)flags.ccMode, (int)(fabs(i.mon_last - i.set)
                    //    * 1000), (int)(i.mon_last * 1000));
                    //}
                    // else if (IS_OCP_VALUE(this, cpv)) {
                    //    DebugTrace("OCP condition: CC_MODE=%d, CV_MODE=%d, U DIFF=%d mV",
                    //    (int)flags.ccMode, (int)flags.cvMode, (int)(fabs(u.mon_last - u.set) *
                    //    1000));
                    //}

                    protectionEnter(cpv);
                }
            } else {
                cpv.flags.alarmed = 1;
                cpv.alarm_started = micros();
            }
        } else {
            // if (IS_OVP_VALUE(this, cpv)) {
            //    DebugTrace("OVP condition: CV_MODE=%d, CC_MODE=%d, I DIFF=%d mA",
            //    (int)flags.cvMode, (int)flags.ccMode, (int)(fabs(i.mon_last - i.set) * 1000));
            //}
            // else if (IS_OCP_VALUE(this, cpv)) {
            //    DebugTrace("OCP condition: CC_MODE=%d, CV_MODE=%d, U DIFF=%d mV",
            //    (int)flags.ccMode, (int)flags.cvMode, (int)(fabs(u.mon_last - u.set) * 1000));
            //}

            protectionEnter(cpv);
        }
    } else {
        cpv.flags.alarmed = 0;
    }
}

////////////////////////////////////////////////////////////////////////////////

void Channel::init() {
    if (!isInstalled()) {
        return;
    }
    bool wasSaveProfileEnabled = profile::enableSave(false);

    channelInterface->init(subchannelIndex);

    profile::enableSave(wasSaveProfileEnabled);
}

void Channel::onPowerDown() {
    if (!isInstalled()) {
        return;
    }

    doRemoteSensingEnable(false);
    doRemoteProgrammingEnable(false);

    clearProtection(false);

    channelInterface->onPowerDown(subchannelIndex);
}

void Channel::reset() {
    if (!isInstalled()) {
        return;
    }

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
    trigger::setVoltage(*this, params.U_MIN);
    trigger::setCurrent(*this, params.I_MIN);
    list::resetChannelList(*this);

#ifdef EEZ_PLATFORM_SIMULATOR
    simulator.setLoadEnabled(false);
    simulator.load = 10;
#endif

    channelInterface->reset(subchannelIndex);
}

void Channel::resetHistory() {
    for (int i = 0; i < CHANNEL_HISTORY_SIZE; ++i) {
        uHistory[i] = 0;
        iHistory[i] = 0;
    }
    flags.historyStarted = 0;
    historyPosition = 0;
}

void Channel::clearCalibrationConf() {
    cal_conf.flags.u_cal_params_exists = 0;
    cal_conf.flags.i_cal_params_exists_range_high = 0;
    cal_conf.flags.i_cal_params_exists_range_low = 0;

    cal_conf.u.min.dac = cal_conf.u.min.val = cal_conf.u.min.adc = params.U_CAL_VAL_MIN;
    cal_conf.u.mid.dac = cal_conf.u.mid.val = cal_conf.u.mid.adc = (params.U_CAL_VAL_MIN + params.U_CAL_VAL_MAX) / 2;
    cal_conf.u.max.dac = cal_conf.u.max.val = cal_conf.u.max.adc = params.U_CAL_VAL_MAX;
    cal_conf.u.minPossible = params.U_MIN;
    cal_conf.u.maxPossible = params.U_MAX;

    cal_conf.i[0].min.dac = cal_conf.i[0].min.val = cal_conf.i[0].min.adc = params.I_CAL_VAL_MIN;
    cal_conf.i[0].mid.dac = cal_conf.i[0].mid.val = cal_conf.i[0].mid.adc = (params.I_CAL_VAL_MIN + params.I_CAL_VAL_MAX) / 2;
    cal_conf.i[0].max.dac = cal_conf.i[0].max.val = cal_conf.i[0].max.adc = params.I_CAL_VAL_MAX;
    cal_conf.i[0].minPossible = params.I_MIN;
    cal_conf.i[0].maxPossible = params.I_MAX;

    cal_conf.i[1].min.dac = cal_conf.i[1].min.val = cal_conf.i[1].min.adc = params.I_CAL_VAL_MIN / 100;
    cal_conf.i[1].mid.dac = cal_conf.i[1].mid.val = cal_conf.i[1].mid.adc = (params.I_CAL_VAL_MIN + params.I_CAL_VAL_MAX) / 2 / 100;
    cal_conf.i[1].max.dac = cal_conf.i[1].max.val = cal_conf.i[1].max.adc = params.I_CAL_VAL_MAX / 100;
    cal_conf.i[1].minPossible = params.I_MIN;
    cal_conf.i[1].maxPossible = params.I_MAX / 100;

    strcpy(cal_conf.calibration_date, "");
    strcpy(cal_conf.calibration_remark, CALIBRATION_REMARK_INIT);
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

bool Channel::test() {
    if (!isInstalled()) {
        return true;
    }

    flags.powerOk = 0;

    doRemoteSensingEnable(false);
    doRemoteProgrammingEnable(false);

    channelInterface->test(subchannelIndex);

    return isOk();
}

bool Channel::isInstalled() {
    return channelInterface != nullptr;
}

bool Channel::isPowerOk() {
    return flags.powerOk;
}

TestResult Channel::getTestResult() {
    if (!isInstalled()) {
        return TEST_SKIPPED;
    }
    return channelInterface->getTestResult(subchannelIndex);
}

bool Channel::isTestFailed() {
    return getTestResult() == TEST_FAILED;
}

bool Channel::isTestOk() {
    return getTestResult() == TEST_OK;
}

bool Channel::isOk() {
    return isPowerUp() && isPowerOk() && isTestOk() && !bp3c::flash_slave::g_bootloaderMode;
}

void Channel::tick(uint32_t tick_usec) {
    if (!isOk()) {
        return;
    }

    channelInterface->tick(subchannelIndex, tick_usec);

    if (params.features & CH_FEATURE_RPOL) {
        unsigned rpol = 0;
            
        rpol = channelInterface->getRPol(subchannelIndex);

        if (rpol != flags.rpol) {
            flags.rpol = rpol;
            setQuesBits(QUES_ISUM_RPOL, flags.rpol ? true : false);
        }

        if (rpol && isOutputEnabled()) {
            channel_dispatcher::outputEnable(*this, false);
            event_queue::pushEvent(event_queue::EVENT_ERROR_CH1_REMOTE_SENSE_REVERSE_POLARITY_DETECTED + channelIndex);
            onProtectionTripped();
        }
    }

    if (!io_pins::isInhibited()) {
        setCvMode(channelInterface->isCcMode(subchannelIndex));
        setCcMode(channelInterface->isCvMode(subchannelIndex));
    }

    // update history values
    if (!flags.historyStarted) {
        flags.historyStarted = 1;
        historyLastTick = tick_usec;
        historyPosition = 0;
    } else {
        uint32_t ytViewRateMicroseconds = (int)round(ytViewRate * 1000000L);
        while (tick_usec - historyLastTick >= ytViewRateMicroseconds) {
            uint32_t historyIndex = historyPosition % CHANNEL_HISTORY_SIZE;
            uHistory[historyIndex] = channel_dispatcher::getUMonLast(*this);
            iHistory[historyIndex] = channel_dispatcher::getIMonLast(*this);
            ++historyPosition;
            historyLastTick += ytViewRateMicroseconds;
        }
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
    float precision = params.U_RESOLUTION; // 5 mV;

    if (calibration::isEnabled()) {
        precision /= 10;
    }

    return precision;
}

float Channel::getCurrentResolution(float value) const {
    float precision = params.I_RESOLUTION; // 0.5mA

    if (hasSupportForCurrentDualRange()) {
        if ((!isNaN(value) && value <= 0.05f && isMicroAmperAllowed()) || flags.currentCurrentRange == CURRENT_RANGE_LOW) {
            precision = params.I_LOW_RESOLUTION; // 5uA
        }
    }
    
    if (calibration::isEnabled()) {
        precision /= 10;
    }

    return precision;
}

float Channel::getPowerResolution() const {
    return params.P_RESOLUTION; // 1 mW;
}

bool Channel::isMicroAmperAllowed() const {
    return flags.currentRangeSelectionMode != CURRENT_RANGE_SELECTION_ALWAYS_HIGH;
}

float Channel::roundChannelValue(Unit unit, float value) const {
    return roundPrec(value, getValuePrecision(unit, value));
}

void Channel::addUMonAdcValue(float value) {
    if (isVoltageCalibrationEnabled()) {
        value = remap(value, cal_conf.u.min.adc, cal_conf.u.min.val, cal_conf.u.max.adc, cal_conf.u.max.val);
    }

    u.addMonValue(value, getVoltageResolution());
}

void Channel::addIMonAdcValue(float value) {
    if (isCurrentCalibrationEnabled()) {
        value = remap(value,
            cal_conf.i[flags.currentCurrentRange].min.adc, cal_conf.i[flags.currentCurrentRange].min.val,
            cal_conf.i[flags.currentCurrentRange].max.adc, cal_conf.i[flags.currentCurrentRange].max.val);
    }

    i.addMonValue(value, getCurrentResolution());
}

void Channel::addUMonDacAdcValue(float value) {
    u.addMonDacValue(value, getVoltageResolution());
}

void Channel::addIMonDacAdcValue(float value) {
    i.addMonDacValue(value, getCurrentResolution());
}

AdcDataType Channel::onAdcData(AdcDataType adcDataType, float value) {
    AdcDataType nextAdcDataType = ADC_DATA_TYPE_NONE;

    if (isPowerUp()) {
        switch (adcDataType) {
        case ADC_DATA_TYPE_NONE:
        	break;

        case ADC_DATA_TYPE_U_MON:
            addUMonAdcValue(value);
            nextAdcDataType = ADC_DATA_TYPE_I_MON;
            break;

        case ADC_DATA_TYPE_I_MON:
            addIMonAdcValue(value);

            if (isOutputEnabled()) {
                if (isRemoteProgrammingEnabled()) {
                    nextAdcDataType = ADC_DATA_TYPE_U_MON_DAC;
                } else {
                    nextAdcDataType = ADC_DATA_TYPE_U_MON;
                }
            } else {
                u.resetMonValues();
                i.resetMonValues();
                nextAdcDataType = ADC_DATA_TYPE_I_MON_DAC;
            }

            break;

        case ADC_DATA_TYPE_U_MON_DAC:
            addUMonDacAdcValue(value);

            if (isOutputEnabled() && isRemoteProgrammingEnabled()) {
                nextAdcDataType = ADC_DATA_TYPE_U_MON;
            } else {
                nextAdcDataType = ADC_DATA_TYPE_I_MON_DAC;
            }

            break;

        case ADC_DATA_TYPE_I_MON_DAC:
            addIMonDacAdcValue(value);

            if (isOutputEnabled()) {
                nextAdcDataType = ADC_DATA_TYPE_U_MON;
            }

            break;
        }

        protectionCheck();
    }

    return nextAdcDataType;
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

void Channel::adcMeasureMonDac() {
    channelInterface->adcMeasureMonDac(subchannelIndex);
}

void Channel::adcMeasureAll() {
    channelInterface->adcMeasureAll(subchannelIndex);
}

void Channel::updateAllChannels() {
    bool wasSaveProfileEnabled = profile::enableSave(false);

    for (int i = 0; i < CH_NUM; ++i) {
        Channel &channel = Channel::get(i);
        if (channel.isOk()) {
            channel.doCalibrationEnable(persist_conf::isChannelCalibrationEnabled(channel) && channel.isCalibrationExists());
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

    profile::enableSave(wasSaveProfileEnabled);
}

void Channel::executeOutputEnable(bool enable, uint16_t tasks) {
    channelInterface->setOutputEnable(subchannelIndex, enable, tasks);

    if (tasks & OUTPUT_ENABLE_TASK_FINALIZE) {
        setOperBits(OPER_ISUM_OE_OFF, !enable);

        if (!enable) {
            setCvMode(false);
            setCcMode(false);

            if (calibration::isEnabled()) {
                calibration::stop();
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
            delayMicroseconds(500);

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

        // FINALIZE
        for (int i = 0; i < CH_NUM; i++) {
            Channel &channel = Channel::get(i);
            if (channel.flags.doOutputEnableOnNextSync) {
                if (channel.flags.outputEnabled && !inhibited) {
                    // OVP, DP for enabled
                    channel.executeOutputEnable(true, OUTPUT_ENABLE_TASK_OVP | OUTPUT_ENABLE_TASK_DP | OUTPUT_ENABLE_TASK_FINALIZE);
                } else if ((channel.flags.outputEnabled && inhibited) || (!channel.flags.outputEnabled && !inhibited)) {
                    // current range and DP for disabled
                    channel.executeOutputEnable(false, OUTPUT_ENABLE_TASK_CURRENT_RANGE | OUTPUT_ENABLE_TASK_DP | OUTPUT_ENABLE_TASK_FINALIZE);
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
                        event_queue::pushEvent((channel.flags.outputEnabled ? event_queue::EVENT_INFO_CH1_OUTPUT_ENABLED : event_queue::EVENT_INFO_CH1_OUTPUT_DISABLED) + channel.channelIndex);
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

    profile::save();
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
    channelInterface->setRemoteSense(subchannelIndex, enable);
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

    channelInterface->setRemoteProgramming(subchannelIndex, enable);
    setOperBits(OPER_ISUM_RPROG_ON, enable);
}

bool Channel::isOutputEnabled() {
    return isPowerUp() && flags.outputEnabled;
}

void Channel::doCalibrationEnable(bool enable) {
    flags._calEnabled = enable;

    if (enable) {
        u.min = roundChannelValue(UNIT_VOLT, MAX(cal_conf.u.minPossible, params.U_MIN));
        if (u.limit < u.min)
            u.limit = u.min;
        if (u.set < u.min)
            setVoltage(u.min);

        u.max = roundChannelValue(UNIT_VOLT, MIN(cal_conf.u.maxPossible, params.U_MAX));
        if (u.limit > u.max)
            u.limit = u.max;
        if (u.set > u.max)
            setVoltage(u.max);

        i.min = roundChannelValue(UNIT_AMPER, MAX(cal_conf.i[0].minPossible, params.I_MIN));
        if (i.min < params.I_MIN)
            i.min = params.I_MIN;
        if (i.limit < i.min)
            i.limit = i.min;
        if (i.set < i.min)
            setCurrent(i.min);

        i.max = roundChannelValue(UNIT_AMPER, MIN(cal_conf.i[0].maxPossible, params.I_MAX));
        if (i.limit > i.max)
            i.limit = i.max;
        if (i.set > i.max)
            setCurrent(i.max);
    } else {
        u.min = roundChannelValue(UNIT_VOLT, params.U_MIN);
        u.max = roundChannelValue(UNIT_VOLT, params.U_MAX);

        i.min = roundChannelValue(UNIT_AMPER, params.I_MIN);
        i.max = roundChannelValue(UNIT_AMPER, params.I_MAX);
    }

    u.def = roundChannelValue(UNIT_VOLT, u.min);
    i.def = roundChannelValue(UNIT_AMPER, i.min);

    if (g_isBooted) {
    	setVoltage(u.set);
    	setCurrent(i.set);
    }
}

void Channel::calibrationEnable(bool enabled) {
    if (enabled != isCalibrationEnabled()) {
        doCalibrationEnable(enabled);
        event_queue::pushEvent((enabled ? event_queue::EVENT_INFO_CH1_CALIBRATION_ENABLED : event_queue::EVENT_WARNING_CH1_CALIBRATION_DISABLED) + channelIndex);
        persist_conf::saveCalibrationEnabledFlag(*this, enabled);
    }
}

void Channel::calibrationEnableNoEvent(bool enabled) {
    if (enabled != isCalibrationEnabled()) {
        doCalibrationEnable(enabled);
    }
}

bool Channel::isCalibrationEnabled() {
    return flags._calEnabled;
}

bool Channel::isVoltageCalibrationEnabled() {
    return flags._calEnabled && cal_conf.flags.u_cal_params_exists;
}

bool Channel::isCurrentCalibrationEnabled() {
    return flags._calEnabled && ((flags.currentCurrentRange == CURRENT_RANGE_HIGH &&
                                  cal_conf.flags.i_cal_params_exists_range_high) ||
                                 (flags.currentCurrentRange == CURRENT_RANGE_LOW &&
                                  cal_conf.flags.i_cal_params_exists_range_low));
}

void Channel::remoteSensingEnable(bool enable) {
    if (enable != flags.senseEnabled) {
        doRemoteSensingEnable(enable);
        event_queue::pushEvent((enable ? event_queue::EVENT_INFO_CH1_REMOTE_SENSE_ENABLED : event_queue::EVENT_INFO_CH1_REMOTE_SENSE_DISABLED) + channelIndex);
        profile::save();
    }
}

bool Channel::isRemoteSensingEnabled() {
    return flags.senseEnabled;
}

void Channel::remoteProgrammingEnable(bool enable) {
    if (enable != flags.rprogEnabled) {
        if (list::isActive(*this)) {
            trigger::abort();
        }
        doRemoteProgrammingEnable(enable);
        event_queue::pushEvent((enable ? event_queue::EVENT_INFO_CH1_REMOTE_PROG_ENABLED : event_queue::EVENT_INFO_CH1_REMOTE_PROG_DISABLED) + channelIndex);
        profile::save();
    }
}

bool Channel::isRemoteProgrammingEnabled() {
    return flags.rprogEnabled;
}

float Channel::getCalibratedVoltage(float value) {
    if (isVoltageCalibrationEnabled()) {
        value = remap(value, cal_conf.u.min.val, cal_conf.u.min.dac, cal_conf.u.max.val, cal_conf.u.max.dac);
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

    channelInterface->setDacVoltageFloat(subchannelIndex, value);
}

void Channel::setVoltage(float value) {
    value = roundPrec(value, getVoltageResolution());

    doSetVoltage(value);

    profile::save();
}

void Channel::doSetCurrent(float value) {
    if (!calibration::isEnabled()) {
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
        value = remap(value, cal_conf.i[flags.currentCurrentRange].min.val,
                      cal_conf.i[flags.currentCurrentRange].min.dac,
                      cal_conf.i[flags.currentCurrentRange].max.val,
                      cal_conf.i[flags.currentCurrentRange].max.dac);
    }

    value += getDualRangeGndOffset();

    channelInterface->setDacCurrentFloat(subchannelIndex, value);
}

void Channel::setCurrent(float value) {
    value = roundPrec(value, getCurrentResolution(value));

    doSetCurrent(value);

    profile::save();
}

bool Channel::isCalibrationExists() {
    return (flags.currentCurrentRange == CURRENT_RANGE_HIGH && cal_conf.flags.i_cal_params_exists_range_high) ||
           (flags.currentCurrentRange == CURRENT_RANGE_LOW && cal_conf.flags.i_cal_params_exists_range_low) ||
           cal_conf.flags.u_cal_params_exists;
}

bool Channel::isTripped() {
    return ovp.flags.tripped || ocp.flags.tripped || opp.flags.tripped ||
           temperature::isAnySensorTripped(this);
}

void Channel::clearProtection(bool clearOTP) {
    auto lastEvent = event_queue::getLastErrorEvent();

    ovp.flags.tripped = 0;
    ovp.flags.alarmed = 0;
    setQuesBits(QUES_ISUM_OVP, false);
    if (lastEvent && lastEvent->eventId == event_queue::EVENT_ERROR_CH1_OVP_TRIPPED + channelIndex) {
        event_queue::markAsRead();
    }

    ocp.flags.tripped = 0;
    ocp.flags.alarmed = 0;
    setQuesBits(QUES_ISUM_OCP, false);
    if (lastEvent && lastEvent->eventId == event_queue::EVENT_ERROR_CH1_OCP_TRIPPED + channelIndex) {
        event_queue::markAsRead();
    }

    opp.flags.tripped = 0;
    opp.flags.alarmed = 0;
    setQuesBits(QUES_ISUM_OPP, false);
    if (lastEvent && lastEvent->eventId == event_queue::EVENT_ERROR_CH1_OPP_TRIPPED + channelIndex) {
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

const char *Channel::getModeStr() const {
    if (isCvMode())
        return "CV";
    else if (isCcMode())
        return "CC";
    else
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
    profile::save();
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
    profile::save();
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
    profile::save();
}

bool Channel::isVoltageBalanced() {
	if (!isInstalled()) {
		return false;
	}
    return channelInterface->isVoltageBalanced(subchannelIndex);
}

bool Channel::isCurrentBalanced() {
	if (!isInstalled()) {
		return false;
	}
    return channelInterface->isCurrentBalanced(subchannelIndex);
}

float Channel::getUSetUnbalanced() {
	if (!isInstalled()) {
		return 0;
	}
    return channelInterface->getUSetUnbalanced(subchannelIndex);
}

float Channel::getISetUnbalanced() {
	if (!isInstalled()) {
		return 0;
	}
    return channelInterface->getISetUnbalanced(subchannelIndex);
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
    profile::save();

    if (flags.currentRangeSelectionMode == CURRENT_RANGE_SELECTION_ALWAYS_LOW) {
        if (i.set > 0.05f) {
            i.set = 0.05f;
        }

        if (i.limit > 0.05f) {
            i.limit = 0.05f;
        }
    }

    setCurrent(i.set);
}

void Channel::enableAutoSelectCurrentRange(bool enable) {
    flags.autoSelectCurrentRange = enable;
    profile::save();

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
            channelInterface->setCurrentRange(subchannelIndex);
        }
    }
}

void Channel::doAutoSelectCurrentRange(uint32_t tickCount) {
    if (calibration::isEnabled()) {
        return;
    }

    if (isOutputEnabled()) {
        if (autoRangeCheckLastTickCount != 0) {
            if (tickCount - autoRangeCheckLastTickCount > CURRENT_AUTO_RANGE_SWITCHING_DELAY_MS * 1000L) {
                if (flags.autoSelectCurrentRange &&
                    flags.currentRangeSelectionMode == CURRENT_RANGE_SELECTION_USE_BOTH &&
                    hasSupportForCurrentDualRange() && 
                    !channelInterface->isDacTesting(subchannelIndex) &&
                    !calibration::isEnabled()) {
                    if (flags.currentCurrentRange == CURRENT_RANGE_LOW) {
                        if (i.set > 0.05f && isCcMode()) {
                            doSetCurrent(i.set);
                        }
                    } else if (i.mon_measured) {
                        if (i.mon_last < 0.05f) {
                            setCurrentRange(1);
                            channelInterface->setDacCurrent(subchannelIndex, (uint16_t)65535);
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
    channelInterface->setDprogState(dprogState);
}

void Channel::getFirmwareVersion(uint8_t &majorVersion, uint8_t &minorVersion) {
    channelInterface->getFirmwareVersion(majorVersion, minorVersion);
}

} // namespace psu
} // namespace eez
