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

#include <eez/apps/psu/psu.h>

#include <math.h>
#include <stdio.h>
#include <assert.h>

#include <eez/system.h>
#if OPTION_WATCHDOG
#include <eez/apps/psu/watchdog.h>
#endif
#include <eez/apps/psu/board.h>
#include <eez/apps/psu/calibration.h>
#include <eez/apps/psu/channel_dispatcher.h>
#include <eez/apps/psu/event_queue.h>
#include <eez/apps/psu/io_pins.h>
#include <eez/apps/psu/list_program.h>
#include <eez/apps/psu/persist_conf.h>
#include <eez/apps/psu/profile.h>
#include <eez/apps/psu/trigger.h>
#include <eez/scpi/regs.h>
#include <eez/sound.h>
#include <eez/index.h>

namespace eez {

using namespace scpi;

namespace psu {

////////////////////////////////////////////////////////////////////////////////

static const char *CH_BOARD_NAMES[] = { "None", "DCP505", "DCP405", "DCP405", "DCM220" };
static const char *CH_REVISION_NAMES[] = { "None", "R1B3", "R1B1", "R2B5", "R1B1" };
static const char *CH_BOARD_AND_REVISION_NAMES[] = { "None", "DCP505_R1B3", "DCP405_R1B1", "DCP405_R2B5", "DCM220_R1B1" };
    
static uint16_t CH_BOARD_REVISION_FEATURES[] = {
    // CH_BOARD_REVISION_NONE
    0,

    // CH_BOARD_REVISION_DCP505_R1B3
    CH_FEATURE_VOLT | CH_FEATURE_CURRENT | CH_FEATURE_POWER | CH_FEATURE_OE | CH_FEATURE_DPROG |
    CH_FEATURE_RPROG | CH_FEATURE_RPOL,

    // CH_BOARD_REVISION_DCP405_R1B1
    CH_FEATURE_VOLT | CH_FEATURE_CURRENT | CH_FEATURE_POWER | CH_FEATURE_OE | CH_FEATURE_DPROG |
    CH_FEATURE_RPROG | CH_FEATURE_RPOL,

    // CH_BOARD_REVISION_DCP405_R2B5
    CH_FEATURE_VOLT | CH_FEATURE_CURRENT | CH_FEATURE_POWER | CH_FEATURE_OE | CH_FEATURE_DPROG |
    CH_FEATURE_RPROG | CH_FEATURE_RPOL,

    // CH_BOARD_REVISION_DCM220_R1B1
    CH_FEATURE_VOLT | CH_FEATURE_CURRENT | CH_FEATURE_POWER | CH_FEATURE_OE
};

static ChannelParams CH_BOARD_REVISION_PARAMS[] = {
                        // VOLTAGE_GND_OFFSET // CURRENT_GND_OFFSET // CALIBRATION_DATA_TOLERANCE_PERCENT // CALIBRATION_MID_TOLERANCE_PERCENT
    { CH_PARAMS_NONE,   0,                    0,                    10.0f,                                1.0f                                 }, // CH_BOARD_REVISION_NONE
    { CH_PARAMS_50V_5A, 1.05f,                0.11f,                10.0f,                                1.0f                                 }, // CH_BOARD_REVISION_DCP505_R1B3
    { CH_PARAMS_40V_5A, 0.86f,                0.11f,                10.0f,                                1.0f                                 }, // CH_BOARD_REVISION_DCP405_R1B1
    { CH_PARAMS_40V_5A, 0.86f,                0.11f,                10.0f,                                1.0f                                 }, // CH_BOARD_REVISION_DCP405_R2B5
    { CH_PARAMS_20V_4A, 0,                    0,                    15.0f,                                2.0f                                 }  // CH_BOARD_REVISION_DCM220_R1B1
};

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
    value = roundPrec(value, prec);

    mon_last = value;

    if (mon_index == -1) {
        mon_index = 0;
        for (int i = 0; i < NUM_ADC_AVERAGING_VALUES; ++i) {
            mon_arr[i] = value;
        }
        mon_total = NUM_ADC_AVERAGING_VALUES * value;
        mon = value;
    } else {
        mon_total -= mon_arr[mon_index];
        mon_total += value;
        mon_arr[mon_index] = value;
        mon_index = (mon_index + 1) % NUM_ADC_AVERAGING_VALUES;
        mon = roundPrec(mon_total / NUM_ADC_AVERAGING_VALUES, prec);
    }

    mon_measured = true;
}

void Channel::Value::addMonDacValue(float value, float prec) {
    value = roundPrec(value, prec);

    if (mon_dac_index == -1) {
        mon_dac_index = 0;
        for (int i = 0; i < NUM_ADC_AVERAGING_VALUES; ++i) {
            mon_dac_arr[i] = value;
        }
        mon_dac_total = NUM_ADC_AVERAGING_VALUES * value;
        mon_dac = value;
    } else {
        mon_dac_total -= mon_dac_arr[mon_dac_index];
        mon_dac_total += value;
        mon_dac_arr[mon_dac_index] = value;
        mon_dac_index = (mon_dac_index + 1) % NUM_ADC_AVERAGING_VALUES;
        mon_dac = roundPrec(mon_dac_total / NUM_ADC_AVERAGING_VALUES, prec);
    }
}

////////////////////////////////////////////////////////////////////////////////

static struct {
    unsigned OE_SAVED : 1;
    unsigned CH1_OE : 1;
    unsigned CH2_OE : 1;
} g_savedOE;

void Channel::saveAndDisableOE() {
    if (!g_savedOE.OE_SAVED) {
        if (CH_NUM > 0) {
            g_savedOE.CH1_OE = Channel::get(0).isOutputEnabled() ? 1 : 0;
            Channel::get(0).outputEnable(false);

            if (CH_NUM > 1) {
                g_savedOE.CH2_OE = Channel::get(1).isOutputEnabled() ? 1 : 0;
                Channel::get(1).outputEnable(false);
            }
        }
        g_savedOE.OE_SAVED = 1;
    }
}

void Channel::restoreOE() {
    if (g_savedOE.OE_SAVED) {
        if (CH_NUM > 0) {
            Channel::get(0).outputEnable(g_savedOE.CH1_OE ? true : false);
            if (CH_NUM > 1) {
                Channel::get(1).outputEnable(g_savedOE.CH2_OE ? true : false);
            }
        }
        g_savedOE.OE_SAVED = 0;
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

void Channel::set(uint8_t slotIndex_, uint8_t subchannelIndex_, uint8_t boardRevision_) {
	slotIndex = slotIndex_;
    boardRevision = boardRevision_;
    subchannelIndex = subchannelIndex_;

    channelInterface = g_modules[g_slots[slotIndex].moduleType].channelInterface[slotIndex];

    params = &CH_BOARD_REVISION_PARAMS[boardRevision];

    u.min = roundChannelValue(UNIT_VOLT, params->U_MIN);
    u.max = roundChannelValue(UNIT_VOLT, params->U_MAX);
    u.def = roundChannelValue(UNIT_VOLT, params->U_DEF);

    i.min = roundChannelValue(UNIT_AMPER, params->I_MIN);
    i.max = roundChannelValue(UNIT_AMPER, params->I_MAX);
    i.def = roundChannelValue(UNIT_AMPER, params->I_DEF);

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
    channel_dispatcher::outputEnable(*this, false);

    cpv.flags.tripped = 1;

    int bit_mask = reg_get_ques_isum_bit_mask_for_channel_protection_value(cpv);
    setQuesBits(bit_mask, true);

    int16_t eventId = event_queue::EVENT_ERROR_CH1_OVP_TRIPPED + channelIndex;

    if (IS_OVP_VALUE(this, cpv)) {
        if (flags.rprogEnabled && channel_dispatcher::getUProtectionLevel(*this) == channel_dispatcher::getUMax(*this)) {
            g_rprogAlarm = true;
        }
        doRemoteProgrammingEnable(false);
    } else if (IS_OCP_VALUE(this, cpv)) {
        eventId += 1;
    } else {
        eventId += 2;
    }

    event_queue::pushEvent(eventId);

    onProtectionTripped();
}

void Channel::enterOvpProtection() {
   protectionEnter(ovp);
}

void Channel::protectionCheck(ProtectionValue &cpv) {
    bool state;
    bool condition;
    float delay;

    if (IS_OVP_VALUE(this, cpv)) {
        state = (flags.rprogEnabled || prot_conf.flags.u_state) && !prot_conf.flags.u_type;
        condition = channel_dispatcher::getUMon(*this) >= channel_dispatcher::getUProtectionLevel(*this) ||
            (flags.rprogEnabled && channel_dispatcher::getUMonDac(*this) >= channel_dispatcher::getUProtectionLevel(*this));
        delay = prot_conf.u_delay;
        delay -= PROT_DELAY_CORRECTION;
    } else if (IS_OCP_VALUE(this, cpv)) {
        state = prot_conf.flags.i_state;
        condition = channel_dispatcher::getIMon(*this) >= channel_dispatcher::getISet(*this);
        delay = prot_conf.i_delay;
        delay -= PROT_DELAY_CORRECTION;
    } else {
        state = prot_conf.flags.p_state;
        condition = channel_dispatcher::getUMon(*this) * channel_dispatcher::getIMon(*this) >
                    channel_dispatcher::getPowerProtectionLevel(*this);
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
    bool last_save_enabled = profile::enableSave(false);

    channelInterface->init(subchannelIndex);

    profile::enableSave(last_save_enabled);
}

void Channel::onPowerDown() {
    if (!isInstalled()) {
        return;
    }

    bool last_save_enabled = profile::enableSave(false);

    outputEnable(false);
    doRemoteSensingEnable(false);
    if (getFeatures() & CH_FEATURE_RPROG) {
        doRemoteProgrammingEnable(false);
    }

    clearProtection(false);

    profile::enableSave(last_save_enabled);
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
    flags.currentRangeSelectionMode = CURRENT_RANGE_SELECTION_ALWAYS_HIGH;
    flags.autoSelectCurrentRange = 0;

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

    if (getFeatures() & CH_FEATURE_RPROG) {
        // [SOUR[n]]:VOLT:PROG INTernal
        doRemoteProgrammingEnable(false);
    }

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
    u.init(params->U_MIN, params->U_DEF_STEP, u.max);
    i.init(params->I_MIN, params->I_DEF_STEP, i.max);

    maxCurrentLimitCause = MAX_CURRENT_LIMIT_CAUSE_NONE;
    p_limit = roundChannelValue(UNIT_WATT, params->PTOT);

    resetHistory();

    flags.displayValue1 = DISPLAY_VALUE_VOLTAGE;
    flags.displayValue2 = DISPLAY_VALUE_CURRENT;
    ytViewRate = GUI_YT_VIEW_RATE_DEFAULT;

    flags.voltageTriggerMode = TRIGGER_MODE_FIXED;
    flags.currentTriggerMode = TRIGGER_MODE_FIXED;
    flags.triggerOutputState = 1;
    flags.triggerOnListStop = TRIGGER_ON_LIST_STOP_OUTPUT_OFF;
    trigger::setVoltage(*this, params->U_MIN);
    trigger::setCurrent(*this, params->I_MIN);
    list::resetChannelList(*this);

#ifdef EEZ_PLATFORM_SIMULATOR
    simulator.setLoadEnabled(false);
    simulator.load = 10;
#endif

    channelInterface->reset(subchannelIndex);
}

void Channel::resetHistory() {
    uHistory[0] = u.mon_last;
    iHistory[0] = i.mon_last;

    for (int i = 1; i < CHANNEL_HISTORY_SIZE; ++i) {
        uHistory[i] = 0;
        iHistory[i] = 0;
    }

    historyPosition = 0;
    historyLastTick = micros();
}

void Channel::clearCalibrationConf() {
    cal_conf.flags.u_cal_params_exists = 0;
    cal_conf.flags.i_cal_params_exists_range_high = 0;
    cal_conf.flags.i_cal_params_exists_range_low = 0;

    cal_conf.u.min.dac = cal_conf.u.min.val = cal_conf.u.min.adc = params->U_CAL_VAL_MIN;
    cal_conf.u.mid.dac = cal_conf.u.mid.val = cal_conf.u.mid.adc = (params->U_CAL_VAL_MIN + params->U_CAL_VAL_MAX) / 2;
    cal_conf.u.max.dac = cal_conf.u.max.val = cal_conf.u.max.adc = params->U_CAL_VAL_MAX;
    cal_conf.u.minPossible = params->U_MIN;
    cal_conf.u.maxPossible = params->U_MAX;

    cal_conf.i[0].min.dac = cal_conf.i[0].min.val = cal_conf.i[0].min.adc = params->I_CAL_VAL_MIN;
    cal_conf.i[0].mid.dac = cal_conf.i[0].mid.val = cal_conf.i[0].mid.adc = (params->I_CAL_VAL_MIN + params->I_CAL_VAL_MAX) / 2;
    cal_conf.i[0].max.dac = cal_conf.i[0].max.val = cal_conf.i[0].max.adc = params->I_CAL_VAL_MAX;
    cal_conf.i[0].minPossible = params->I_MIN;
    cal_conf.i[0].maxPossible = params->I_MAX;

    cal_conf.i[1].min.dac = cal_conf.i[1].min.val = cal_conf.i[1].min.adc = params->I_CAL_VAL_MIN / 100;
    cal_conf.i[1].mid.dac = cal_conf.i[1].mid.val = cal_conf.i[1].mid.adc = (params->I_CAL_VAL_MIN + params->I_CAL_VAL_MAX) / 2 / 100;
    cal_conf.i[1].max.dac = cal_conf.i[1].max.val = cal_conf.i[1].max.adc = params->I_CAL_VAL_MAX / 100;
    cal_conf.i[1].minPossible = params->I_MIN;
    cal_conf.i[1].maxPossible = params->I_MAX / 100;

    strcpy(cal_conf.calibration_date, "");
    strcpy(cal_conf.calibration_remark, CALIBRATION_REMARK_INIT);
}

void Channel::clearProtectionConf() {
    prot_conf.flags.u_state = params->OVP_DEFAULT_STATE;
    if (g_slots[slotIndex].moduleType == MODULE_TYPE_DCP405) {
        prot_conf.flags.u_type = 1; // HW
    } else {
        prot_conf.flags.u_type = 0; // SW
    }
    prot_conf.flags.i_state = params->OCP_DEFAULT_STATE;
    prot_conf.flags.p_state = params->OPP_DEFAULT_STATE;

    prot_conf.u_delay = params->OVP_DEFAULT_DELAY;
    prot_conf.u_level = u.max;
    prot_conf.i_delay = params->OCP_DEFAULT_DELAY;
    prot_conf.p_delay = params->OPP_DEFAULT_DELAY;
    prot_conf.p_level = params->OPP_DEFAULT_LEVEL;

    temperature::sensors[temp_sensor::CH1 + channelIndex].prot_conf.state = OTP_CH_DEFAULT_STATE;
    temperature::sensors[temp_sensor::CH1 + channelIndex].prot_conf.level = OTP_CH_DEFAULT_LEVEL;
    temperature::sensors[temp_sensor::CH1 + channelIndex].prot_conf.delay = OTP_CH_DEFAULT_DELAY;
}

bool Channel::test() {
    if (!isInstalled()) {
        return true;
    }

    bool last_save_enabled = profile::enableSave(false);

    flags.powerOk = 0;

    outputEnable(false);
    doRemoteSensingEnable(false);
    if (getFeatures() & CH_FEATURE_RPROG) {
        doRemoteProgrammingEnable(false);
    }

    channelInterface->test(subchannelIndex);

    profile::enableSave(last_save_enabled);
    profile::save();

    return isOk();
}

bool Channel::isInstalled() {
    return boardRevision != CH_BOARD_REVISION_NONE;
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
    return isPowerUp() && isPowerOk() && isTestOk();
}

void Channel::tick(uint32_t tick_usec) {
    if (!isOk()) {
        return;
    }

    channelInterface->tick(subchannelIndex, tick_usec);

    if (getFeatures() & CH_FEATURE_RPOL) {
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
    uint32_t ytViewRateMicroseconds = (int)round(ytViewRate * 1000000L);
    while (tick_usec - historyLastTick >= ytViewRateMicroseconds) {
        uint32_t historyIndex = historyPosition % CHANNEL_HISTORY_SIZE;
        uHistory[historyIndex] = u.mon_last;
        iHistory[historyIndex] = i.mon_last;
        ++historyPosition;
        historyLastTick += ytViewRateMicroseconds;
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
    float precision = params->U_RESOLUTION; // 5 mV;

    if (calibration::isEnabled()) {
        precision /= 10;
    }

    return precision;
}

float Channel::getCurrentResolution(float value) const {
    float precision = params->I_RESOLUTION; // 0.5mA

    if (hasSupportForCurrentDualRange()) {
        if ((!isNaN(value) && value <= 0.05f && isMicroAmperAllowed()) || flags.currentCurrentRange == CURRENT_RANGE_LOW) {
            precision = params->I_LOW_RESOLUTION; // 5uA
        }
    }
    
    if (calibration::isEnabled()) {
        precision /= 10;
    }

    return precision;
}

float Channel::getPowerResolution() const {
    return params->P_RESOLUTION; // 1 mW;
}

bool Channel::isMicroAmperAllowed() const {
    return flags.currentRangeSelectionMode != CURRENT_RANGE_SELECTION_ALWAYS_HIGH;
}

float Channel::roundChannelValue(Unit unit, float value) const {
    if (unit == UNIT_VOLT) {
        return roundPrec(value, getVoltageResolution());
    }
    
    if (unit == UNIT_AMPER) {
        return roundPrec(value, getCurrentResolution(value));
    }
    
    if (unit == UNIT_WATT) {
        return roundPrec(value, getPowerResolution());
    }
    
    return value;
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
    if (channel_dispatcher::isCoupled() && channelIndex == 1) {
        // protections of coupled channels are checked on channel 1
        return;
    }

    protectionCheck(ovp);
    protectionCheck(ocp);
    protectionCheck(opp);
}

void Channel::adcMeasureMonDac() {
    if (isOk()) {
        channelInterface->adcMeasureMonDac(subchannelIndex);
    }
}

void Channel::adcMeasureAll() {
    if (isOk()) {
        channelInterface->adcMeasureAll(subchannelIndex);
    }
}

void Channel::executeOutputEnable(bool enable) {
    channelInterface->setOutputEnable(subchannelIndex, enable);

    setOperBits(OPER_ISUM_OE_OFF, !enable);

    if (enable) {
    } else {
        setCvMode(false);
        setCcMode(false);

        if (calibration::isEnabled()) {
            calibration::stop();
        }
    }
}

void Channel::doOutputEnable(bool enable) {
    if (enable && !isOk()) {
        return;
    }

    flags.outputEnabled = enable;

    if (!io_pins::isInhibited()) {
        executeOutputEnable(enable);
    }
}

void Channel::onInhibitedChanged(bool inhibited) {
    if (isOutputEnabled()) {
        if (inhibited) {
            executeOutputEnable(false);
        } else {
            executeOutputEnable(true);
        }
    }
}

void Channel::doRemoteSensingEnable(bool enable) {
    if (enable && !isOk()) {
        return;
    }
    flags.senseEnabled = enable;
    channelInterface->setRemoteSense(subchannelIndex, enable);
    setOperBits(OPER_ISUM_RSENS_ON, enable);
}

void Channel::doRemoteProgrammingEnable(bool enable) {
    if (enable && !isOk()) {
        return;
    }
    flags.rprogEnabled = enable;
    if (enable) {
        setVoltageLimit(u.max);
        setVoltage(u.min);
        prot_conf.u_level = u.max;
        prot_conf.flags.u_state = 1;
    }
    channelInterface->setRemoteProgramming(subchannelIndex, enable);
    setOperBits(OPER_ISUM_RPROG_ON, enable);
}

void Channel::update() {
    if (!isOk()) {
        return;
    }

    doCalibrationEnable(persist_conf::isChannelCalibrationEnabled(*this) && isCalibrationExists());

    bool last_save_enabled = profile::enableSave(false);

    setVoltage(u.set);
    setCurrent(i.set);
    doOutputEnable(flags.outputEnabled);
    doRemoteSensingEnable(flags.senseEnabled);
    if (getFeatures() & CH_FEATURE_RPROG) {
        doRemoteProgrammingEnable(flags.rprogEnabled);
    }

    profile::enableSave(last_save_enabled);
}

void Channel::outputEnable(bool enable) {
    if (enable != flags.outputEnabled) {
        doOutputEnable(enable);
        event_queue::pushEvent((enable ? event_queue::EVENT_INFO_CH1_OUTPUT_ENABLED : event_queue::EVENT_INFO_CH1_OUTPUT_DISABLED) + channelIndex);
        profile::save();
    }
}

bool Channel::isOutputEnabled() {
    return isPowerUp() && flags.outputEnabled;
}

void Channel::doCalibrationEnable(bool enable) {
    flags._calEnabled = enable;

    if (enable) {
        u.min = roundChannelValue(UNIT_VOLT, MAX(cal_conf.u.minPossible, params->U_MIN));
        if (u.limit < u.min)
            u.limit = u.min;
        if (u.set < u.min)
            setVoltage(u.min);

        u.max = roundChannelValue(UNIT_VOLT, MIN(cal_conf.u.maxPossible, params->U_MAX));
        if (u.limit > u.max)
            u.limit = u.max;
        if (u.set > u.max)
            setVoltage(u.max);

        i.min = roundChannelValue(UNIT_AMPER, MAX(cal_conf.i[0].minPossible, params->I_MIN));
        if (i.min < params->I_MIN)
            i.min = params->I_MIN;
        if (i.limit < i.min)
            i.limit = i.min;
        if (i.set < i.min)
            setCurrent(i.min);

        i.max = roundChannelValue(UNIT_AMPER, MIN(cal_conf.i[0].maxPossible, params->I_MAX));
        if (i.limit > i.max)
            i.limit = i.max;
        if (i.set > i.max)
            setCurrent(i.max);
    } else {
        u.min = roundChannelValue(UNIT_VOLT, params->U_MIN);
        u.max = roundChannelValue(UNIT_VOLT, params->U_MAX);

        i.min = roundChannelValue(UNIT_AMPER, params->I_MIN);
        i.max = roundChannelValue(UNIT_AMPER, params->I_MAX);
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
        doRemoteProgrammingEnable(enable);
        event_queue::pushEvent((enable ? event_queue::EVENT_INFO_CH1_REMOTE_PROG_ENABLED : event_queue::EVENT_INFO_CH1_REMOTE_PROG_DISABLED) + channelIndex);
        profile::save();
    }
}

bool Channel::isRemoteProgrammingEnabled() {
    return flags.rprogEnabled;
}

void Channel::doSetVoltage(float value) {
    u.set = value;
    u.mon_dac = 0;

    if (prot_conf.u_level < u.set) {
        prot_conf.u_level = u.set;
    }

    if (params->U_MAX != params->U_MAX_CONF) {
        value = remap(value, 0, 0, params->U_MAX_CONF, params->U_MAX);
    }

    if (isVoltageCalibrationEnabled()) {
        value = remap(value, cal_conf.u.min.val, cal_conf.u.min.dac, cal_conf.u.max.val,
                      cal_conf.u.max.dac);
    }

#if !defined(EEZ_PLATFORM_SIMULATOR)
    value += params->VOLTAGE_GND_OFFSET;
#endif

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

    if (params->I_MAX != params->I_MAX_CONF) {
        value = remap(value, 0, 0, params->I_MAX_CONF, params->I_MAX);
    }

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

const char *Channel::getCvModeStr() {
    if (isCvMode())
        return "CV";
    else if (isCcMode())
        return "CC";
    else
        return "UR";
}

const char *Channel::getBoardName() {
    return CH_BOARD_NAMES[boardRevision];
}

const char *Channel::getRevisionName() {
    return CH_REVISION_NAMES[boardRevision];
}

const char *Channel::getBoardAndRevisionName() {
    return CH_BOARD_AND_REVISION_NAMES[boardRevision];
}

uint16_t Channel::getFeatures() {
    return CH_BOARD_REVISION_FEATURES[boardRevision];
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
            if (isOutputEnabled() && i.mon_last > ERR_MAX_CURRENT) {
                setCurrent(0);
            }

            if (i.limit > ERR_MAX_CURRENT) {
                setCurrentLimit(ERR_MAX_CURRENT);
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
    return params->PTOT;
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
    return flags.currentCurrentRange == CURRENT_RANGE_LOW ? (params->CURRENT_GND_OFFSET / 100) : params->CURRENT_GND_OFFSET;
#endif
}

bool Channel::hasSupportForCurrentDualRange() const {
    return (g_slots[slotIndex].moduleType == MODULE_TYPE_DCP405) && 
        (boardRevision == CH_BOARD_REVISION_DCP405_R1B1 || boardRevision == CH_BOARD_REVISION_DCP405_R2B5);
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
    return flags.currentCurrentRange == CURRENT_RANGE_LOW ? (params->I_MAX / 100) : params->I_MAX;
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

} // namespace psu
} // namespace eez
