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
#include <eez/hmi.h>

#include <eez/modules/psu/calibration.h>
#include <eez/modules/psu/channel_dispatcher.h>
#include <eez/modules/psu/datetime.h>
#include <eez/modules/psu/profile.h>
#include <eez/modules/psu/scpi/psu.h>

#include <eez/modules/psu/gui/psu.h>
#include <eez/modules/psu/gui/page_ch_settings.h>

namespace eez {
namespace psu {
namespace calibration {

////////////////////////////////////////////////////////////////////////////////

static int g_slotIndex;
static int g_subchannelIndex;

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

    Channel *channel = Channel::getBySlotIndex(g_slotIndex, g_subchannelIndex);
    if (channel) {
        if (type == CALIBRATION_VALUE_U) {
            configuration.numPoints = channel->params.U_CAL_NUM_POINTS;
            points = channel->params.U_CAL_POINTS;
        } else if (type == CALIBRATION_VALUE_I_HI_RANGE) {
            configuration.numPoints = channel->params.I_CAL_NUM_POINTS;
            points = channel->params.I_CAL_POINTS;
        } else {
            configuration.numPoints = channel->params.I_LOW_RANGE_CAL_NUM_POINTS;
            points = channel->params.I_LOW_RANGE_CAL_POINTS;
        }
    } else {
        g_slots[g_slotIndex]->getCalibrationPoints(type, configuration.numPoints, points);
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

    Channel *channel = Channel::getBySlotIndex(g_slotIndex, g_subchannelIndex);

    if (type == CALIBRATION_VALUE_U) {
        if (channel) {
            channel_dispatcher::setVoltage(*channel, value);
            channel_dispatcher::setCurrent(*channel, channel->params.U_CAL_I_SET);
        } else {
            channel_dispatcher::setVoltage(g_slotIndex, g_subchannelIndex, value, nullptr);
        }
    } else {
        if (channel) {
            channel_dispatcher::setCurrent(*channel, value);
            if (type == CALIBRATION_VALUE_I_HI_RANGE) {
                channel_dispatcher::setVoltage(*channel, channel->params.I_CAL_U_SET);
            } else {
                channel_dispatcher::setVoltage(*channel, channel->params.I_LOW_RANGE_CAL_U_SET);
            }
        } else {
            // TODO
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
    Channel *channel = Channel::getBySlotIndex(g_slotIndex, g_subchannelIndex);
    return channel ? (type == CALIBRATION_VALUE_U ? channel->u.mon_last : channel->i.mon_last) : NAN;
}

bool Value::checkValueAndAdc(float value, float adc) {
    if (currentPointIndex == -1) {
        return false;
    }

    Channel *channel = Channel::getBySlotIndex(g_slotIndex, g_subchannelIndex);
    if (channel) {
        float range = configuration.points[configuration.numPoints - 1].dac - configuration.points[0].dac;
        float allowedDiff = range * channel->params.CALIBRATION_DATA_TOLERANCE_PERCENT / 100;
        float diff;

        float dac = configuration.points[currentPointIndex].dac;

        diff = fabsf(dac - value);
        if (diff > allowedDiff) {
            DebugTrace("Data check failed: level=%f, data=%f, diff=%f, allowedDiff=%f\n", dac, value, diff, allowedDiff);
            return false;
        }

        if (g_slots[channel->slotIndex]->moduleType != MODULE_TYPE_DCM220 && g_slots[channel->slotIndex]->moduleType != MODULE_TYPE_DCM224) {
            diff = fabsf(dac - adc);
            if (diff > allowedDiff) {
                DebugTrace("ADC check failed: level=%f, adc=%f, diff=%f, allowedDiff=%f\n", dac, adc, diff, allowedDiff);
                return false;
            }
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

void getCalibrationChannel(int &slotIndex, int &subchannelIndex) {
    if (isEnabled()) {
        slotIndex = g_slotIndex;
        subchannelIndex = g_subchannelIndex;
    } else {
        slotIndex = hmi::g_selectedSlotIndex;
        subchannelIndex = hmi::g_selectedSubchannelIndex;
    }
}

bool hasSupportForCurrentDualRange() {
    int slotIndex;
    int subchannelIndex;
    getCalibrationChannel(slotIndex, subchannelIndex);
    Channel *channel = Channel::getBySlotIndex(slotIndex, subchannelIndex);
    if (channel) {
        return channel->hasSupportForCurrentDualRange();
    }
    return false;
}

CalibrationValueType getCalibrationValueType() {
    int slotIndex;
    int subchannelIndex;
    getCalibrationChannel(slotIndex, subchannelIndex);
    Channel *channel = Channel::getBySlotIndex(slotIndex, subchannelIndex);
    if (channel) {
        auto editPage = (eez::psu::gui::ChSettingsCalibrationEditPage *)eez::psu::gui::getPage(eez::gui::PAGE_ID_CH_SETTINGS_CALIBRATION_EDIT);
        if (editPage) {
            return editPage->getCalibrationValueType();
        }

        auto viewPage = (eez::psu::gui::ChSettingsCalibrationViewPage *)eez::psu::gui::getPage(eez::gui::PAGE_ID_CH_SETTINGS_CALIBRATION_VIEW);
        return viewPage->getCalibrationValueType();
    }
    return CALIBRATION_VALUE_U;
}

bool isCalibrationExists() {
    int slotIndex;
    int subchannelIndex;
    getCalibrationChannel(slotIndex, subchannelIndex);
    Channel *channel = Channel::getBySlotIndex(slotIndex, subchannelIndex);
    if (channel) {
        return channel->isCalibrationExists();
    } else {
        return g_slots[slotIndex]->isVoltageCalibrationExists(subchannelIndex) || g_slots[slotIndex]->isCurrentCalibrationExists(subchannelIndex);
    }
}

void getMaxValue(CalibrationValueType valueType, float &value, Unit &unit) {
    int slotIndex;
    int subchannelIndex;
    getCalibrationChannel(slotIndex, subchannelIndex);
    Channel *channel = Channel::getBySlotIndex(slotIndex, subchannelIndex);
    
    if (valueType == CALIBRATION_VALUE_U) {
        value = channel ? channel->u.max : channel_dispatcher::getVoltageMaxValue(slotIndex, subchannelIndex);
        unit = UNIT_VOLT;
        return;
    } 
    
    if (valueType == CALIBRATION_VALUE_I_HI_RANGE) {
        value = channel ? channel -> i.max : channel_dispatcher::getCurrentMaxValue(slotIndex, subchannelIndex);
        unit = UNIT_AMPER;
        return;
    } 

    value = 0.05f;
    unit = UNIT_AMPER;
}

float roundCalibrationValue(Unit unit, float value) {
    int slotIndex;
    int subchannelIndex;
    getCalibrationChannel(slotIndex, subchannelIndex);
    Channel *channel = Channel::getBySlotIndex(slotIndex, subchannelIndex);
    if (channel) {
        return channel->roundChannelValue(unit, value);
    }

    return roundPrec(value, unit == UNIT_VOLT ?
        channel_dispatcher::getVoltageResolution(slotIndex, subchannelIndex) : 
        channel_dispatcher::getCurrentResolution(slotIndex, subchannelIndex));
}

bool isCalibrationValueTypeSelectable() {
    int slotIndex;
    int subchannelIndex;
    getCalibrationChannel(slotIndex, subchannelIndex);
    Channel *channel = Channel::getBySlotIndex(slotIndex, subchannelIndex);
    return channel != nullptr;
}

ChannelMode getChannelMode() {
    int slotIndex;
    int subchannelIndex;
    getCalibrationChannel(slotIndex, subchannelIndex);
    Channel *channel = Channel::getBySlotIndex(slotIndex, subchannelIndex);
    if (channel) {
        return channel->getMode();
    }

    if (g_slots[slotIndex]->isConstantVoltageMode(subchannelIndex)) {
        return CHANNEL_MODE_CV;
    }

    return CHANNEL_MODE_UR;
}

float getDacValue(CalibrationValueType valueType) {
    int slotIndex;
    int subchannelIndex;
    getCalibrationChannel(slotIndex, subchannelIndex);
    Channel *channel = Channel::getBySlotIndex(slotIndex, subchannelIndex);
    if (channel) {
        if (valueType == CALIBRATION_VALUE_U) {
            return channel->u.set;
        } else {
            return channel->i.set;
        }
    } 
    
    float value;
    if (valueType == CALIBRATION_VALUE_U) {
        channel_dispatcher::getVoltage(slotIndex, subchannelIndex, value, nullptr);
    } else {
        channel_dispatcher::getCurrent(slotIndex, subchannelIndex, value, nullptr);
    }
    return value;
}

float getAdcValue(CalibrationValueType valueType) {
    int slotIndex;
    int subchannelIndex;
    getCalibrationChannel(slotIndex, subchannelIndex);
    Channel *channel = Channel::getBySlotIndex(slotIndex, subchannelIndex);
    if (channel) {
        if (valueType == CALIBRATION_VALUE_U) {
            return channel->u.mon_last;
        } else {
            return channel->i.mon_last;
        }
    }
    return NAN;
}

void setVoltage(float value) {
    Channel *channel = Channel::getBySlotIndex(g_slotIndex, g_subchannelIndex);
    if (channel) {
        channel_dispatcher::setVoltage(*channel, value);
    } else {
        channel_dispatcher::setVoltage(g_slotIndex, g_subchannelIndex, value, nullptr);
    }
}

void setCurrent(float value) {
    Channel *channel = Channel::getBySlotIndex(g_slotIndex, g_subchannelIndex);
    if (channel) {
        channel_dispatcher::setCurrent(*channel, value);
    } else {
        channel_dispatcher::setCurrent(g_slotIndex, g_subchannelIndex, value, nullptr);
    }
}

void doStart() {
    Channel *channel = Channel::getBySlotIndex(g_slotIndex, g_subchannelIndex);

    if (channel) {
        channel_dispatcher::outputEnable(*channel, false);
        channel_dispatcher::setVoltage(*channel, channel->u.min);
        channel_dispatcher::setCurrent(*channel, channel->i.min);
    } else {
        channel_dispatcher::setVoltage(g_slotIndex, g_subchannelIndex, channel_dispatcher::getVoltageMinValue(g_slotIndex, g_subchannelIndex), nullptr);
        channel_dispatcher::setCurrent(g_slotIndex, g_subchannelIndex, channel_dispatcher::getCurrentMinValue(g_slotIndex, g_subchannelIndex), nullptr);
    }

    profile::saveToLocation(10);
    profile::setFreezeState(true);

    reset();

    selectCurrentRange(0);

    g_voltage.reset();
    g_currents[0].reset();
    if (hasSupportForCurrentDualRange()) {
        g_currents[1].reset();
    }
    
    g_remarkSet = false;
    g_remark[0] = 0;

    g_enabled = true;
    if (channel) {
        channel->calibrationEnable(false);
        channel_dispatcher::outputEnable(*channel, true);
        channel->setOperBits(OPER_ISUM_CALI, true);
    } else {
        g_slots[g_slotIndex]->enableVoltageCalibration(g_subchannelIndex, false);
        g_slots[g_slotIndex]->enableCurrentCalibration(g_subchannelIndex, false);
    }
}

void start(int slotIndex, int subchannelIndex) {
    if (!isPsuThread()) {
        sendMessageToPsu(PSU_MESSAGE_CALIBRATION_START, (slotIndex << 8) | subchannelIndex);
        return;
    }

    if (!g_enabled) {
        g_slotIndex = slotIndex;
        g_subchannelIndex = subchannelIndex;
        doStart();
    }
}

void stop() {
    if (!isPsuThread()) {
        sendMessageToPsu(PSU_MESSAGE_CALIBRATION_STOP);
        return;
    }

    if (!g_enabled)
        return;

    g_enabled = false;

    Channel *channel = Channel::getBySlotIndex(g_slotIndex, g_subchannelIndex);

    if (channel) {
        channel->setOperBits(OPER_ISUM_CALI, false);
    }

    profile::recallFromLocation(10);

    if (!channel) {
        // TODO remove this when recallFromLocation for SMX46 is implemented
        channel_dispatcher::setVoltage(g_slotIndex, g_subchannelIndex, channel_dispatcher::getVoltageMinValue(g_slotIndex, g_subchannelIndex), nullptr);
        channel_dispatcher::setCurrent(g_slotIndex, g_subchannelIndex, channel_dispatcher::getCurrentMinValue(g_slotIndex, g_subchannelIndex), nullptr);
    }

    profile::setFreezeState(false);

    if (channel) {
        if (channel->isCalibrationExists()) {
            channel->calibrationEnable(true);
        }
    } else {
        if (g_slots[g_slotIndex]->isVoltageCalibrationExists(g_subchannelIndex)) {
            g_slots[g_slotIndex]->enableVoltageCalibration(g_subchannelIndex, true);
        }

        if (g_slots[g_slotIndex]->isCurrentCalibrationExists(g_subchannelIndex)) {
            g_slots[g_slotIndex]->enableCurrentCalibration(g_subchannelIndex, true);
        }
    }
}

void copyValueFromChannel(CalibrationValueConfiguration &channelValue, Value &value) {
    value.configuration.numPoints = channelValue.numPoints;
    for (unsigned int i = 0; i < channelValue.numPoints; i++) {
        value.isPointSet[i] = true;
        value.configuration.points[i].dac = channelValue.points[i].dac;
        value.configuration.points[i].value = channelValue.points[i].value;
        value.configuration.points[i].adc = channelValue.points[i].adc;
    }
}

void copyValuesFromChannel() {
	Channel *channel = Channel::getBySlotIndex(g_slotIndex, g_subchannelIndex);

	CalibrationConfiguration *calConf;
	if (channel) {
		calConf = &channel->cal_conf;
	} else {
		calConf = g_slots[g_slotIndex]->getCalibrationConfiguration(g_subchannelIndex);
	}

	if (calConf->u.numPoints > 1) {
		copyValueFromChannel(calConf->u, g_voltage);
	}

	if (calConf->i[0].numPoints > 1) {
		copyValueFromChannel(calConf->i[0], g_currents[0]);
	}

    if (hasSupportForCurrentDualRange()) {
    	if (calConf->i[1].numPoints > 1) {
    		copyValueFromChannel(calConf->i[1], g_currents[1]);
    	}
    }
}

void selectCurrentRange(int8_t range) {
    Channel *channel = Channel::getBySlotIndex(g_slotIndex, g_subchannelIndex);
    if (!channel) {
        return;
    }

    if (!isPsuThread()) {
        sendMessageToPsu(PSU_MESSAGE_CALIBRATION_SELECT_CURRENT_RANGE, range);
        return;
    }

    g_currentRangeSelected = range;
    channel->setCurrentRange(range);
    channel->setCurrentRangeSelectionMode(range == CURRENT_RANGE_LOW ? CURRENT_RANGE_SELECTION_ALWAYS_LOW : CURRENT_RANGE_SELECTION_ALWAYS_HIGH);
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
    if (calibrationValue.configuration.numPoints < 2) {
        scpiErr = SCPI_ERROR_TOO_FEW_CAL_POINTS;
        return false;
    }

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
                if (scpiErr == SCPI_ERROR_TOO_FEW_CAL_POINTS) {
                    *uiErr = SCPI_ERROR_CALIBRATION_TOO_FEW_VOLTAGE_CAL_POINTS;
                } else {
                    *uiErr = SCPI_ERROR_CALIBRATION_INVALID_VOLTAGE_CAL_DATA;
                }
            }
            return false;
        }
        valueCalibrated = true;
    }

    if (isCalibrated(g_currents[0])) {
        if (!checkCalibrationValue(g_currents[0], scpiErr)) {
            if (uiErr) {
                if (scpiErr == SCPI_ERROR_TOO_FEW_CAL_POINTS) {
                    *uiErr = hasSupportForCurrentDualRange() ? SCPI_ERROR_CALIBRATION_TOO_FEW_CURRENT_H_CAL_POINTS : SCPI_ERROR_CALIBRATION_TOO_FEW_CURRENT_CAL_POINTS;
                } else {
                    *uiErr = hasSupportForCurrentDualRange() ? SCPI_ERROR_CALIBRATION_INVALID_CURRENT_H_CAL_DATA : SCPI_ERROR_CALIBRATION_INVALID_CURRENT_CAL_DATA;
                }
            }
            return false;
        }
        valueCalibrated = true;
    }

    if (calibration::hasSupportForCurrentDualRange()) {
        if (isCalibrated(g_currents[1])) {
            if (!checkCalibrationValue(g_currents[1], scpiErr)) {
                if (uiErr) {
                    if (scpiErr == SCPI_ERROR_TOO_FEW_CAL_POINTS) {
                        *uiErr = SCPI_ERROR_CALIBRATION_TOO_FEW_CURRENT_L_CAL_POINTS;
                    } else {
                        *uiErr = SCPI_ERROR_CALIBRATION_INVALID_CURRENT_L_CAL_DATA;
                    }
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

bool doSave(int slotIndex, int subchannelIndex) {
    static bool result;

    result = false;

    if (!isPsuThread()) {
        sendMessageToPsu(PSU_MESSAGE_SAVE_CHANNEL_CALIBRATION, (slotIndex << 8) | subchannelIndex);
        return result;
    }

    result = persist_conf::saveChannelCalibration(slotIndex, subchannelIndex);
    return result;
}

bool save() {
    Channel *channel = Channel::getBySlotIndex(g_slotIndex, g_subchannelIndex);

    CalibrationConfiguration *calConf;
    if (channel) {
        calConf = &channel->cal_conf;
    } else {
        calConf = g_slots[g_slotIndex]->getCalibrationConfiguration(g_subchannelIndex);
    }

    calConf->calibrationDate = datetime::now();

    memset(&calConf->calibrationRemark, 0, CALIBRATION_REMARK_MAX_LENGTH + 1);
    strcpy(calConf->calibrationRemark, g_remark);

    if (isCalibrated(g_voltage)) {
        memcpy(&calConf->u, &g_voltage.configuration, sizeof(CalibrationValueConfiguration));
    }

    if (isCalibrated(g_currents[0])) {
        memcpy(&calConf->i[0], &g_currents[0].configuration, sizeof(CalibrationValueConfiguration));
    }

    if (hasSupportForCurrentDualRange() && isCalibrated(g_currents[1])) {
        memcpy(&calConf->i[1], &g_currents[1].configuration, sizeof(CalibrationValueConfiguration));
    }

    return doSave(g_slotIndex, g_subchannelIndex);
}

bool clear(Channel *channel) {
    channel->calibrationEnable(false);
    clearCalibrationConf(&channel->cal_conf);
    return doSave(channel->slotIndex, channel->subchannelIndex);
}

void clearCalibrationConf(CalibrationConfiguration *calConf) {
    calConf->u.numPoints = 0;
    calConf->i[0].numPoints = 0;
    calConf->i[1].numPoints = 0;
    
    calConf->calibrationDate = 0;
    strcpy(calConf->calibrationRemark, CALIBRATION_REMARK_INIT);
}

float remapValue(float value, CalibrationValueConfiguration &cal) {
    unsigned i;
    unsigned j;

    if (cal.numPoints == 2) {
        i = 0;
        j = 1;
    } else {
        for (j = 1; j < cal.numPoints - 1 && value > cal.points[j].value; j++) {
        }
        i = j - 1;
    }

    if (cal.points[i].value == cal.points[j].value) {
    	return value;
    }

    return remap(value,
        cal.points[i].value,
        cal.points[i].dac,
        cal.points[j].value,
        cal.points[j].dac);
}

} // namespace calibration
} // namespace psu
} // namespace eez
