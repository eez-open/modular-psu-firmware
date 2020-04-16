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

#include <eez/modules/psu/psu.h>

#include <math.h>
#include <stdio.h>

#include <eez/firmware.h>

#include <eez/modules/psu/calibration.h>
#include <eez/modules/psu/channel_dispatcher.h>
#include <eez/modules/psu/datetime.h>
#include <eez/modules/psu/profile.h>
#include <eez/modules/psu/scpi/psu.h>

namespace eez {
namespace psu {
namespace calibration {

////////////////////////////////////////////////////////////////////////////////

static Channel *g_channel;
static bool g_enabled;
static bool g_remarkSet;
static char g_remark[CALIBRATION_REMARK_MAX_LENGTH + 1];

static int8_t g_currentRangeSelected = 0;

static Value g_voltage(true);
static Value g_currents[] = { Value(false, 0), Value(false, 1) };

////////////////////////////////////////////////////////////////////////////////

Value::Value(bool voltOrCurr_, int currentRange_)
    : voltOrCurr(voltOrCurr_), currentRange(currentRange_) {
}

void Value::reset() {
    level = LEVEL_NONE;

    min_set = false;
    mid_set = false;
    max_set = false;

    min_dac = voltOrCurr
                  ? g_channel->params.U_CAL_VAL_MIN
                  : (currentRange == 0 ? g_channel->params.I_CAL_VAL_MIN : g_channel->params.I_CAL_VAL_MIN / 100);
    mid_dac = voltOrCurr
                  ? g_channel->params.U_CAL_VAL_MID
                  : (currentRange == 0 ? g_channel->params.I_CAL_VAL_MID : g_channel->params.I_CAL_VAL_MID / 100);
    max_dac = voltOrCurr
                  ? g_channel->params.U_CAL_VAL_MAX
                  : (currentRange == 0 ? g_channel->params.I_CAL_VAL_MAX : g_channel->params.I_CAL_VAL_MAX / 100);
}

float Value::getLevelValue() {
    if (level == LEVEL_MIN) {
        return min_dac;
    } else if (level == LEVEL_MID) {
        return mid_dac;
    } else {
        return max_dac;
    }
}

float Value::getDacValue() {
    return voltOrCurr ? g_channel->u.set : g_channel->i.set;
}

float Value::getAdcValue() {
    return voltOrCurr ? g_channel->u.mon_last : g_channel->i.mon_last;
}

void Value::setLevel(int8_t value) {
    level = value;
}

void Value::setLevelValue() {
    if (voltOrCurr) {
        g_channel->setVoltage(getLevelValue());
        g_channel->setCurrent(g_channel->params.I_VOLT_CAL);
    } else {
        g_channel->setCurrent(getLevelValue());
        g_channel->setVoltage(g_channel->params.U_CURR_CAL);
    }
}

void Value::setDacValue(float value) {
    if (level == LEVEL_MIN) {
        min_dac = value;
    } else if (level == LEVEL_MID) {
        mid_dac = value;
    } else {
        max_dac = value;
    }

    if (voltOrCurr) {
        g_channel->setVoltage(value);
    } else {
        g_channel->setCurrent(value);
    }
}

bool Value::checkRange(float dac, float data, float adc) {
    float range;
    if (voltOrCurr) {
        range = g_channel->params.U_CAL_VAL_MAX - g_channel->params.U_CAL_VAL_MIN;
    } else {
        range = g_channel->params.I_CAL_VAL_MAX - g_channel->params.I_CAL_VAL_MIN;
        if (currentRange == 1) {
            // range /= 5; // 500mA
        	range /= 25; // 50mA
        }
    }

    float allowedDiff = range * g_channel->params.CALIBRATION_DATA_TOLERANCE_PERCENT / 100;
    float diff;

    diff = fabsf(dac - data);
    if (diff > allowedDiff) {
        DebugTrace("Data check failed: level=%f, data=%f, diff=%f, allowedDiff=%f\n", dac, data, diff, allowedDiff);
        return false;
    }

    if (g_slots[g_channel->slotIndex].moduleInfo->moduleType != MODULE_TYPE_DCM220 && g_slots[g_channel->slotIndex].moduleInfo->moduleType != MODULE_TYPE_DCM224) {
        diff = fabsf(dac - adc);
        if (diff > allowedDiff) {
            DebugTrace("ADC check failed: level=%f, adc=%f, diff=%f, allowedDiff=%f\n", dac, adc, diff, allowedDiff);
            return false;
        }
    }

    return true;
}

void Value::setData(float dac, float data, float adc) {
    if (level == LEVEL_MIN) {
        min_set = true;
        min_dac = dac;
        min_val = data;
        min_adc = adc;
    } else if (level == LEVEL_MID) {
        mid_set = true;
        mid_dac = dac;
        mid_val = data;
        mid_adc = adc;
    } else {
        max_set = true;
        max_dac = dac;
        max_val = data;
        max_adc = adc;
    }

    if (min_set && max_set) {
        DebugTrace("ADC=%f\n", min_adc);
    }
}

bool Value::checkMid() {
    float mid = remap(mid_dac, min_dac, min_val, max_dac, max_val);

    float allowedDiff = g_channel->params.CALIBRATION_MID_TOLERANCE_PERCENT * (max_val - min_val) / 100.0f;

    float diff = fabsf(mid - mid_val);
    if (diff <= allowedDiff) {
        return true;
    } else {
        DebugTrace("%s MID point check failed: level=%f, data=%f, diff=%f, allowedDiff=%f\n",
                    voltOrCurr ? "Volt" : (currentRange == 0 ? "HI Curr" : "LO Curr"), mid, mid_val,
                    diff, allowedDiff);
        return false;
    }
}

////////////////////////////////////////////////////////////////////////////////

void resetChannelToZero() {
    g_channel->setVoltage(g_channel->u.min);
    g_channel->setCurrent(g_channel->i.min);
}

bool isEnabled() {
    return g_enabled;
}

Channel &getCalibrationChannel() {
    return *g_channel;
}

void start(Channel &channel) {
    if (osThreadGetId() != g_psuTaskHandle) {
        osMessagePut(g_psuMessageQueueId, PSU_QUEUE_MESSAGE(PSU_QUEUE_MESSAGE_TYPE_CALIBRATION_START, channel.channelIndex), osWaitForever);
        return;
    }

    if (g_enabled)
        return;

    profile::saveToLocation(10);
    profile::setFreezeState(true);

    reset();

    g_channel = &channel;

    selectCurrentRange(0);

    g_voltage.reset();
    g_currents[0].reset();
    if (hasSupportForCurrentDualRange()) {
        g_currents[1].reset();
    }
    g_remarkSet = false;
    g_remark[0] = 0;

    g_enabled = true;
    g_channel->calibrationEnable(false);

    channel_dispatcher::outputEnable(*g_channel, true);

    g_channel->setOperBits(OPER_ISUM_CALI, true);
}

void stop() {
    if (osThreadGetId() != g_psuTaskHandle) {
        osMessagePut(g_psuMessageQueueId, PSU_QUEUE_MESSAGE(PSU_QUEUE_MESSAGE_TYPE_CALIBRATION_STOP, 0), osWaitForever);
        return;
    }

    if (!g_enabled)
        return;

    g_enabled = false;

    g_channel->setOperBits(OPER_ISUM_CALI, false);

    profile::recallFromLocation(10);
    profile::setFreezeState(false);

    if (g_channel->isCalibrationExists()) {
        g_channel->calibrationEnable(true);
    }
}

bool hasSupportForCurrentDualRange() {
    return g_channel->hasSupportForCurrentDualRange();
}

void selectCurrentRange(int8_t range) {
    g_currentRangeSelected = range;
    g_channel->setCurrentRange(range);
    g_channel->setCurrentRangeSelectionMode(range == CURRENT_RANGE_LOW ? CURRENT_RANGE_SELECTION_ALWAYS_LOW : CURRENT_RANGE_SELECTION_ALWAYS_HIGH);
}

Value &getVoltage() {
    return g_voltage;
}

Value &getCurrent() {
    return g_currents[g_currentRangeSelected];
}

bool isRemarkSet() {
    return g_remarkSet;
}

const char *getRemark() {
    return g_remark;
}

void setRemark(const char *value, size_t len) {
    g_remarkSet = true;
    memset(g_remark, 0, sizeof(g_remark));
    strncpy(g_remark, value, len);
}

static bool checkCalibrationValue(calibration::Value &calibrationValue, int16_t &scpiErr) {
    if (calibrationValue.min_val >= calibrationValue.max_val) {
        scpiErr = SCPI_ERROR_INVALID_CAL_DATA;
        return false;
    }

    if (!calibrationValue.checkMid()) {
        scpiErr = SCPI_ERROR_INVALID_CAL_DATA;
        return false;
    }

    return true;
}

bool isVoltageCalibrated() {
    return g_voltage.min_set && g_voltage.mid_set && g_voltage.max_set;
}

bool isCurrentCalibrated(Value &current) {
    return current.min_set && current.mid_set && current.max_set;
}

bool canSave(int16_t &scpiErr, int16_t *uiErr) {
    if (!isEnabled()) {
        scpiErr = SCPI_ERROR_CALIBRATION_STATE_IS_OFF;
        if (uiErr) {
            *uiErr = scpiErr;
        }
        return false;
    }

    // at least one value should be calibrated
    bool valueCalibrated = false;

    if (isVoltageCalibrated()) {
        if (!checkCalibrationValue(calibration::g_voltage, scpiErr)) {
            if (uiErr) {
                *uiErr = SCPI_ERROR_CALIBRATION_INVALID_VOLTAGE_CAL_DATA;
            }
            return false;
        }
        valueCalibrated = true;
    }

    if (isCurrentCalibrated(g_currents[0])) {
        if (!checkCalibrationValue(g_currents[0], scpiErr)) {
            if (uiErr) {
                *uiErr = hasSupportForCurrentDualRange() ? SCPI_ERROR_CALIBRATION_INVALID_CURRENT_H_CAL_DATA : SCPI_ERROR_CALIBRATION_INVALID_CURRENT_CAL_DATA;
            }
            return false;
        }
        valueCalibrated = true;
    }

    if (hasSupportForCurrentDualRange()) {
        if (isCurrentCalibrated(g_currents[1])) {
            if (!checkCalibrationValue(g_currents[1], scpiErr)) {
                if (uiErr) {
                    *uiErr = SCPI_ERROR_CALIBRATION_INVALID_CURRENT_L_CAL_DATA;
                }
                return false;
            }
            valueCalibrated = true;
        }
    }

    if (!valueCalibrated) {
        scpiErr = SCPI_ERROR_BAD_SEQUENCE_OF_CALIBRATION_COMMANDS;
        if (uiErr) {
            *uiErr = SCPI_ERROR_CALIBRATION_NO_CAL_DATA;
        }
        return false;
    }

    if (!isRemarkSet()) {
        scpiErr = SCPI_ERROR_BAD_SEQUENCE_OF_CALIBRATION_COMMANDS;
        if (uiErr) {
            *uiErr = SCPI_ERROR_CALIBRATION_REMARK_NOT_SET;
        }
        return false;
    }

    return true;
}

bool save() {
    uint8_t year;
    uint8_t month;
    uint8_t day;
    if (datetime::getDate(year, month, day)) {
        sprintf(g_channel->cal_conf.calibration_date, "%d%02d%02d", (int)(2000 + year), (int)month, (int)day);
    } else {
        strcpy(g_channel->cal_conf.calibration_date, "");
    }

    memset(&g_channel->cal_conf.calibration_remark, 0, sizeof(g_channel->cal_conf.calibration_remark));
    strcpy(g_channel->cal_conf.calibration_remark, g_remark);

    if (isVoltageCalibrated()) {
        g_channel->cal_conf.flags.u_cal_params_exists = 1;

        g_channel->cal_conf.u.min.dac = g_voltage.min_dac;
        g_channel->cal_conf.u.min.val = g_voltage.min_val;
        g_channel->cal_conf.u.min.adc = g_voltage.min_adc;

        g_channel->cal_conf.u.mid.dac = g_voltage.mid_dac;
        g_channel->cal_conf.u.mid.val = g_voltage.mid_val;
        g_channel->cal_conf.u.mid.adc = g_voltage.mid_adc;

        g_channel->cal_conf.u.max.dac = g_voltage.max_dac;
        g_channel->cal_conf.u.max.val = g_voltage.max_val;
        g_channel->cal_conf.u.max.adc = g_voltage.max_adc;

        g_voltage.level = LEVEL_NONE;
    }

    if (isCurrentCalibrated(g_currents[0])) {
        g_channel->cal_conf.flags.i_cal_params_exists_range_high = 1;

        g_channel->cal_conf.i[0].min.dac = g_currents[0].min_dac;
        g_channel->cal_conf.i[0].min.val = g_currents[0].min_val;
        g_channel->cal_conf.i[0].min.adc = g_currents[0].min_adc;

        g_channel->cal_conf.i[0].mid.dac = g_currents[0].mid_dac;
        g_channel->cal_conf.i[0].mid.val = g_currents[0].mid_val;
        g_channel->cal_conf.i[0].mid.adc = g_currents[0].mid_adc;

        g_channel->cal_conf.i[0].max.dac = g_currents[0].max_dac;
        g_channel->cal_conf.i[0].max.val = g_currents[0].max_val;
        g_channel->cal_conf.i[0].max.adc = g_currents[0].max_adc;

        g_currents[0].level = LEVEL_NONE;
    }

    if (hasSupportForCurrentDualRange()) {
        if (isCurrentCalibrated(g_currents[1])) {
            g_channel->cal_conf.flags.i_cal_params_exists_range_low = 1;

            g_channel->cal_conf.i[1].min.dac = g_currents[1].min_dac;
            g_channel->cal_conf.i[1].min.val = g_currents[1].min_val;
            g_channel->cal_conf.i[1].min.adc = g_currents[1].min_adc;

            g_channel->cal_conf.i[1].mid.dac = g_currents[1].mid_dac;
            g_channel->cal_conf.i[1].mid.val = g_currents[1].mid_val;
            g_channel->cal_conf.i[1].mid.adc = g_currents[1].mid_adc;

            g_channel->cal_conf.i[1].max.dac = g_currents[1].max_dac;
            g_channel->cal_conf.i[1].max.val = g_currents[1].max_val;
            g_channel->cal_conf.i[1].max.adc = g_currents[1].max_adc;

            g_currents[1].level = LEVEL_NONE;
        }
    }

    // TODO move this to scpi thread
    return persist_conf::saveChannelCalibration(*g_channel);
}

bool clear(Channel *channel) {
    channel->calibrationEnable(false);
    channel->clearCalibrationConf();
    return persist_conf::saveChannelCalibration(*channel);
}

} // namespace calibration
} // namespace psu
} // namespace eez
