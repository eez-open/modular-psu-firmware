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

static Value g_voltage(CALIBRATION_VALUE_U);

static Value g_currents[2] = {
    Value(CALIBRATION_VALUE_I_HI_RANGE),
    Value(CALIBRATION_VALUE_I_LOW_RANGE)
};

////////////////////////////////////////////////////////////////////////////////

Value::Value(CalibrationValueType type_)
    : type(type_)
{
}

void Value::reset() {
    currentPointIndex = -1;

    float *points;

    if (type == CALIBRATION_VALUE_U) {
        configuration.numPoints = g_channel->params.U_CAL_NUM_POINTS;
        points = g_channel->params.U_CAL_POINTS;
    } else if (type == CALIBRATION_VALUE_I_HI_RANGE) {
        configuration.numPoints = g_channel->params.I_CAL_NUM_POINTS;
        points = g_channel->params.I_CAL_POINTS;
    } else {
        configuration.numPoints = g_channel->params.I_LOW_RANGE_CAL_NUM_POINTS;
        points = g_channel->params.I_LOW_RANGE_CAL_POINTS;
    }

    for (unsigned int i = 0; i < configuration.numPoints; i++) {
        isPointSet[i] = true;
        configuration.points[i].dac = points[i];
        configuration.points[i].value = points[i];
        configuration.points[i].adc = points[i];
    }
}

void Value::setCurrentPointIndex(int currentPointIndex_) {
    if (currentPointIndex_ < MAX_CALIBRATION_POINTS) {
        currentPointIndex = currentPointIndex_;
        while (currentPointIndex >= (int)configuration.numPoints) {
            isPointSet[configuration.numPoints] = false;
            configuration.points[configuration.numPoints].dac = 0;
            configuration.points[configuration.numPoints].value = 0;
            configuration.points[configuration.numPoints].adc = 0;
            configuration.numPoints++;
        }
    }
}

void Value::setDacValue(float value) {
    if (currentPointIndex == -1) {
        return;
    }

    configuration.points[currentPointIndex].dac = value;

    if (type == CALIBRATION_VALUE_U) {
        g_channel->setVoltage(value);
        g_channel->setCurrent(g_channel->params.U_CAL_I_SET);
    } else {
        g_channel->setCurrent(value);
        if (type == CALIBRATION_VALUE_I_HI_RANGE) {
            g_channel->setVoltage(g_channel->params.I_CAL_U_SET);
        } else {
            g_channel->setVoltage(g_channel->params.I_LOW_RANGE_CAL_U_SET);
        }
    }
}

float Value::getDacValue() {
    if (currentPointIndex == -1) {
        return NAN;
    }

    return configuration.points[currentPointIndex].dac;
}

float Value::readAdcValue() {
    return type == CALIBRATION_VALUE_U ? g_channel->u.mon_last : g_channel->i.mon_last;
}

bool Value::checkValueAndAdc(float value, float adc) {
    if (currentPointIndex == -1) {
        return false;
    }

    float range = configuration.points[configuration.numPoints - 1].dac - configuration.points[0].dac;
    float allowedDiff = range * g_channel->params.CALIBRATION_DATA_TOLERANCE_PERCENT / 100;
    float diff;

    float dac = configuration.points[currentPointIndex].dac;

    diff = fabsf(dac - value);
    if (diff > allowedDiff) {
        DebugTrace("Data check failed: level=%f, data=%f, diff=%f, allowedDiff=%f\n", dac, value, diff, allowedDiff);
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

void Value::setValueAndAdc(float value, float adc) {
    if (currentPointIndex == -1) {
        return;
    }

    isPointSet[currentPointIndex] = true;
    configuration.points[currentPointIndex].value = value;
    configuration.points[currentPointIndex].adc = adc;
}

bool Value::checkPoints() {
    //for (int i = 0; i < numPoints; i++) {
    //    if (i > 1) {
    //        const char *prefix = type == CALIBRATION_VALUE_U ? "Volt" : (type == CALIBRATION_VALUE_I_HI_RANGE ? "HI Curr" : "LO Curr");

    //        if (points[i].dac < points[i - 1].dac) {
    //            DebugTrace("%s DAC at point %d < DAC at point %d\n", prefix, i + 1, i);
    //            return false;
    //        }

    //        if (points[i].value < points[i - 1].value) {
    //            DebugTrace("%s data at point %d < data at point %d\n", prefix, i + 1, i);
    //            return false;
    //        }

    //        if (points[i].adc < points[i - 1].adc) {
    //            DebugTrace("%s ADC at point %d < ADC at point %d\n", prefix, i + 1, i);
    //            return false;
    //        }
    //    }
    //}

    return true;
}

////////////////////////////////////////////////////////////////////////////////

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

    channel_dispatcher::outputEnable(channel, false);
    channel_dispatcher::setVoltage(channel, channel.u.min);
    channel_dispatcher::setCurrent(channel, channel.i.min);

    profile::saveToLocation(10);
    profile::setFreezeState(true);

    reset();

    g_channel = &channel;

    selectCurrentRange(0);

    g_voltage.reset();
    g_currents[0].reset();
    if (g_channel->hasSupportForCurrentDualRange()) {
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

void copyValueFromChannel(Channel::CalibrationValueConfiguration &channelValue, Value &value) {
    value.configuration.numPoints = channelValue.numPoints;
    for (unsigned int i = 0; i < channelValue.numPoints; i++) {
        value.isPointSet[i] = true;
        value.configuration.points[i].dac = channelValue.points[i].dac;
        value.configuration.points[i].value = channelValue.points[i].value;
        value.configuration.points[i].adc = channelValue.points[i].adc;
    }
}

void copyValuesFromChannel() {
    if (g_channel->isVoltageCalibrationExists()) {
        copyValueFromChannel(g_channel->cal_conf.u, g_voltage);
    }

    if (g_channel->isCurrentCalibrationExists(0)) {
        copyValueFromChannel(g_channel->cal_conf.i[0], g_currents[0]);
    }

    if (g_channel->hasSupportForCurrentDualRange()) {
        if (g_channel->isCurrentCalibrationExists(1)) {
            copyValueFromChannel(g_channel->cal_conf.i[1], g_currents[1]);
        }
    }
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
    if (!calibrationValue.checkPoints()) {
        scpiErr = SCPI_ERROR_INVALID_CAL_DATA;
        return false;
    }

    return true;
}

bool isCalibrated(Value &value) {
    for (unsigned int i = 0; i < value.configuration.numPoints; i++) {
        if (!value.isPointSet[i]) {
            return false;
        }
    }
    return true;
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

    if (isCalibrated(g_voltage)) {
        if (!checkCalibrationValue(calibration::g_voltage, scpiErr)) {
            if (uiErr) {
                *uiErr = SCPI_ERROR_CALIBRATION_INVALID_VOLTAGE_CAL_DATA;
            }
            return false;
        }
        valueCalibrated = true;
    }

    if (isCalibrated(g_currents[0])) {
        if (!checkCalibrationValue(g_currents[0], scpiErr)) {
            if (uiErr) {
                *uiErr = g_channel->hasSupportForCurrentDualRange() ? SCPI_ERROR_CALIBRATION_INVALID_CURRENT_H_CAL_DATA : SCPI_ERROR_CALIBRATION_INVALID_CURRENT_CAL_DATA;
            }
            return false;
        }
        valueCalibrated = true;
    }

    if (g_channel->hasSupportForCurrentDualRange()) {
        if (isCalibrated(g_currents[1])) {
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
    g_channel->cal_conf.calibrationDate = datetime::now();

    memset(&g_channel->cal_conf.calibrationRemark, 0, sizeof(g_channel->cal_conf.calibrationRemark));
    strcpy(g_channel->cal_conf.calibrationRemark, g_remark);

    if (isCalibrated(g_voltage)) {
        memcpy(&g_channel->cal_conf.u, &g_voltage.configuration, sizeof(Channel::CalibrationValueConfiguration));
    }

    if (isCalibrated(g_currents[0])) {
        memcpy(&g_channel->cal_conf.i[0], &g_currents[0].configuration, sizeof(Channel::CalibrationValueConfiguration));
    }

    if (g_channel->hasSupportForCurrentDualRange() && isCalibrated(g_currents[1])) {
        memcpy(&g_channel->cal_conf.i[1], &g_currents[1].configuration, sizeof(Channel::CalibrationValueConfiguration));
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
