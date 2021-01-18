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
#include <eez/modules/psu/gui/channel_calibration.h>

namespace eez {
namespace psu {
namespace calibration {

////////////////////////////////////////////////////////////////////////////////

CalibrationEditor g_editor;
CalibrationViewer g_viewer;

////////////////////////////////////////////////////////////////////////////////

Value::Value(CalibrationEditor &editor_, CalibrationValueType type_)
    : editor(editor_)
    , type(type_)
{
}

void Value::reset() {
    currentPointIndex = -1;

    float *points;

    Channel *channel = Channel::getBySlotIndex(editor.m_slotIndex, editor.m_subchannelIndex);
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
        g_slots[editor.m_slotIndex]->getDefaultCalibrationPoints(editor.m_subchannelIndex, type, configuration.numPoints, points);
    }

    for (unsigned int i = 0; i < configuration.numPoints; i++) {
        isPointSet[i] = true;
        configuration.points[i].dac = points[i];
        configuration.points[i].value = points[i];
        configuration.points[i].adc = points[i];
    }
}

void Value::setCurrentPointIndex(int currentPointIndex_) {
    if (currentPointIndex_ < (int)g_editor.getMaxCalibrationPoints()) {
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
    if (currentPointIndex != -1) {
        configuration.points[currentPointIndex].dac = value;
    }

    Channel *channel = Channel::getBySlotIndex(editor.m_slotIndex, editor.m_subchannelIndex);

    if (type == CALIBRATION_VALUE_U) {
        if (channel) {
            channel_dispatcher::setVoltage(*channel, value);
            channel_dispatcher::setCurrent(*channel, channel->params.U_CAL_I_SET);
        } else {
            channel_dispatcher::setVoltage(editor.m_slotIndex, editor.m_subchannelIndex, value, nullptr);
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
            channel_dispatcher::setCurrent(editor.m_slotIndex, editor.m_subchannelIndex, value, nullptr);
        }
    }
}

float Value::getDacValue() {
    if (currentPointIndex == -1) {
        return NAN;
    }

    return configuration.points[currentPointIndex].dac;
}

bool Value::readAdcValue(float &adcValue, int *err) {
    Channel *channel = Channel::getBySlotIndex(editor.m_slotIndex, editor.m_subchannelIndex);
    if (channel) {
    	adcValue = type == CALIBRATION_VALUE_U ? channel->u.mon_last : channel->i.mon_last;
    	return true;
    }
    return g_slots[editor.m_slotIndex]->calibrationReadAdcValue(editor.m_subchannelIndex, adcValue, err);
}

bool Value::checkValueAndAdc(float value, float adc) {
    if (currentPointIndex == -1) {
        return false;
    }

    Channel *channel = Channel::getBySlotIndex(editor.m_slotIndex, editor.m_subchannelIndex);
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

bool Value::measure(float &measuredValue, int *err) {
    return g_slots[editor.m_slotIndex]->calibrationMeasure(editor.m_subchannelIndex, measuredValue, err);
}

////////////////////////////////////////////////////////////////////////////////

void CalibrationBase::getCalibrationChannel(int &slotIndex, int &subchannelIndex) {
    slotIndex = m_slotIndex;
    subchannelIndex = m_subchannelIndex;
}

bool CalibrationBase::hasSupportForCurrentDualRange() {
    int slotIndex;
    int subchannelIndex;
    getCalibrationChannel(slotIndex, subchannelIndex);
    Channel *channel = Channel::getBySlotIndex(slotIndex, subchannelIndex);
    if (channel) {
        return channel->hasSupportForCurrentDualRange();
    }
    return false;
}

CalibrationValueType CalibrationBase::getInitialCalibrationValueType() {
    int slotIndex;
    int subchannelIndex;
    getCalibrationChannel(slotIndex, subchannelIndex);
    Channel *channel = Channel::getBySlotIndex(slotIndex, subchannelIndex);
    if (channel) {
        return CALIBRATION_VALUE_U;
    } else {
        return g_slots[slotIndex]->getCalibrationValueType(subchannelIndex);
    }
}

CalibrationValueType CalibrationBase::getCalibrationValueType() {
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
	return g_slots[slotIndex]->getCalibrationValueType(subchannelIndex);
}

bool CalibrationBase::isCalibrationValueSource() {
    int slotIndex;
    int subchannelIndex;
    getCalibrationChannel(slotIndex, subchannelIndex);
    return g_slots[slotIndex]->isCalibrationValueSource(subchannelIndex);
}

bool CalibrationBase::isCalibrationExists() {
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

void CalibrationBase::getMinValue(CalibrationValueType valueType, float &value, Unit &unit) {
    int slotIndex;
    int subchannelIndex;
    getCalibrationChannel(slotIndex, subchannelIndex);
    Channel *channel = Channel::getBySlotIndex(slotIndex, subchannelIndex);

    if (valueType == CALIBRATION_VALUE_U) {
        value = channel ? channel->u.min : channel_dispatcher::getVoltageMinValue(slotIndex, subchannelIndex);
        unit = UNIT_VOLT;
        return;
    }

    if (valueType == CALIBRATION_VALUE_I_HI_RANGE) {
        value = channel ? channel->i.min : channel_dispatcher::getCurrentMinValue(slotIndex, subchannelIndex);
        unit = UNIT_AMPER;
        return;
    }

    value = 0.0f;
    unit = UNIT_AMPER;
}

void CalibrationBase::getMaxValue(CalibrationValueType valueType, float &value, Unit &unit) {
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
        value = channel ? channel->i.max : channel_dispatcher::getCurrentMaxValue(slotIndex, subchannelIndex);
        unit = UNIT_AMPER;
        return;
    } 

    value = 0.05f;
    unit = UNIT_AMPER;
}

float CalibrationBase::roundCalibrationValue(Unit unit, float value) {
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

bool CalibrationBase::isCalibrationValueTypeSelectable() {
    int slotIndex;
    int subchannelIndex;
    getCalibrationChannel(slotIndex, subchannelIndex);
    Channel *channel = Channel::getBySlotIndex(slotIndex, subchannelIndex);
    return channel != nullptr;
}

float CalibrationBase::getDacValue(CalibrationValueType valueType) {
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

////////////////////////////////////////////////////////////////////////////////

void CalibrationEditor::doStart() {
    g_slots[m_slotIndex]->initChannelCalibration(m_subchannelIndex);

    Channel *channel = Channel::getBySlotIndex(m_slotIndex, m_subchannelIndex);

    if (channel) {
        channel_dispatcher::outputEnable(*channel, false);
        channel_dispatcher::setVoltage(*channel, channel->u.min);
        channel_dispatcher::setCurrent(*channel, channel->i.min);
    } else {
        g_slots[m_slotIndex]->outputEnable(m_subchannelIndex, false, nullptr);
        channel_dispatcher::setVoltage(m_slotIndex, m_subchannelIndex, channel_dispatcher::getVoltageMinValue(m_slotIndex, m_subchannelIndex), nullptr);
        channel_dispatcher::setCurrent(m_slotIndex, m_subchannelIndex, channel_dispatcher::getCurrentMinValue(m_slotIndex, m_subchannelIndex), nullptr);
    }

    profile::saveToLocation(10);
    profile::setFreezeState(true);

    selectCurrentRange(0);

    m_enabled = true;

    if (channel) {
        channel->calibrationEnable(false);
        channel_dispatcher::outputEnable(*channel, true);
        channel->setOperBits(OPER_ISUM_CALI, true);

        m_maxCalibrationPoints = MAX_CALIBRATION_POINTS;
    } else {
        g_slots[m_slotIndex]->enableVoltageCalibration(m_subchannelIndex, false);
        g_slots[m_slotIndex]->enableCurrentCalibration(m_subchannelIndex, false);
        g_slots[m_slotIndex]->outputEnable(m_subchannelIndex, true, nullptr);
        g_slots[m_slotIndex]->startChannelCalibration(m_subchannelIndex);

        m_maxCalibrationPoints = g_slots[m_slotIndex]->getMaxCalibrationPoints(m_subchannelIndex);;
    }

    m_voltageValue.reset();
    m_currentsValue[0].reset();
    if (hasSupportForCurrentDualRange()) {
        m_currentsValue[1].reset();
    }

    m_remarkSet = false;
    m_remark[0] = 0;
}

void CalibrationEditor::start(int slotIndex, int subchannelIndex) {
    if (!isPsuThread()) {
        sendMessageToPsu(PSU_MESSAGE_CALIBRATION_START, (slotIndex << 8) | subchannelIndex);
        return;
    }

    if (!m_enabled) {
        m_slotIndex = slotIndex;
        m_subchannelIndex = subchannelIndex;
        doStart();
    }
}

void CalibrationEditor::stop() {
    if (!isPsuThread()) {
        sendMessageToPsu(PSU_MESSAGE_CALIBRATION_STOP);
        return;
    }

    if (!m_enabled) {
        return;
    }

    m_enabled = false;

    Channel *channel = Channel::getBySlotIndex(m_slotIndex, m_subchannelIndex);

    if (channel) {
        channel->setOperBits(OPER_ISUM_CALI, false);
    }

    profile::recallFromLocation(10);
    profile::setFreezeState(false);

    if (channel) {
        if (channel->isCalibrationExists()) {
            channel->calibrationEnable(true);
        }
    } else {
        if (g_slots[m_slotIndex]->isVoltageCalibrationExists(m_subchannelIndex)) {
            g_slots[m_slotIndex]->enableVoltageCalibration(m_subchannelIndex, true);
        }

        if (g_slots[m_slotIndex]->isCurrentCalibrationExists(m_subchannelIndex)) {
            g_slots[m_slotIndex]->enableCurrentCalibration(m_subchannelIndex, true);
        }

        g_slots[m_slotIndex]->stopChannelCalibration(m_subchannelIndex);
    }
}

unsigned int CalibrationEditor::getMaxCalibrationPoints() {
    return m_maxCalibrationPoints;
}

bool CalibrationEditor::isPowerChannel() {
    int slotIndex;
    int subchannelIndex;
    calibration::g_editor.getCalibrationChannel(slotIndex, subchannelIndex);
    Channel *channel = Channel::getBySlotIndex(slotIndex, subchannelIndex);
    return channel != nullptr;
}

ChannelMode CalibrationEditor::getChannelMode() {
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

float CalibrationEditor::getAdcValue(CalibrationValueType valueType) {
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

void CalibrationEditor::selectCurrentRange(int8_t range) {
    Channel *channel = Channel::getBySlotIndex(m_slotIndex, m_subchannelIndex);
    if (!channel) {
        return;
    }

    if (!isPsuThread()) {
        sendMessageToPsu(PSU_MESSAGE_CALIBRATION_SELECT_CURRENT_RANGE, range);
        return;
    }

    m_currentRangeSelected = range;
    channel->setCurrentRange(range);
    channel->setCurrentRangeSelectionMode(range == CURRENT_RANGE_LOW ? CURRENT_RANGE_SELECTION_ALWAYS_LOW : CURRENT_RANGE_SELECTION_ALWAYS_HIGH);
}

Value &CalibrationEditor::getVoltage() {
    return m_voltageValue;
}

Value &CalibrationEditor::getCurrent() {
    return m_currentsValue[m_currentRangeSelected];
}

void CalibrationEditor::setVoltage(float value) {
    Channel *channel = Channel::getBySlotIndex(m_slotIndex, m_subchannelIndex);
    if (channel) {
        channel_dispatcher::setVoltage(*channel, value);
    } else {
        channel_dispatcher::setVoltage(m_slotIndex, m_subchannelIndex, value, nullptr);
    }
}

void CalibrationEditor::setCurrent(float value) {
    Channel *channel = Channel::getBySlotIndex(m_slotIndex, m_subchannelIndex);
    if (channel) {
        channel_dispatcher::setCurrent(*channel, value);
    } else {
        channel_dispatcher::setCurrent(m_slotIndex, m_subchannelIndex, value, nullptr);
    }
}

static void copyValueFromChannel(CalibrationValueConfiguration &channelValue, Value &value) {
    value.configuration.numPoints = channelValue.numPoints;
    for (unsigned int i = 0; i < channelValue.numPoints; i++) {
        value.isPointSet[i] = true;
        value.configuration.points[i].dac = channelValue.points[i].dac;
        value.configuration.points[i].value = channelValue.points[i].value;
        value.configuration.points[i].adc = channelValue.points[i].adc;
    }
}

void CalibrationEditor::copyValuesFromChannel() {
	Channel *channel = Channel::getBySlotIndex(m_slotIndex, m_subchannelIndex);

	CalibrationConfiguration calConf;
	if (channel) {
        memcpy(&calConf, &channel->cal_conf, sizeof(CalibrationConfiguration));
	} else {
		g_slots[m_slotIndex]->getCalibrationConfiguration(m_subchannelIndex, calConf, nullptr);
	}

	if (calConf.u.numPoints > 1) {
		copyValueFromChannel(calConf.u, m_voltageValue);
	}

	if (calConf.i[0].numPoints > 1) {
		copyValueFromChannel(calConf.i[0], m_currentsValue[0]);
	}

    if (hasSupportForCurrentDualRange()) {
    	if (calConf.i[1].numPoints > 1) {
    		copyValueFromChannel(calConf.i[1], m_currentsValue[1]);
    	}
    }
}

bool CalibrationEditor::isRemarkSet() {
    return m_remarkSet;
}

const char *CalibrationEditor::getRemark() {
    return m_remark;
}

void CalibrationEditor::setRemark(const char *value, size_t len) {
    m_remarkSet = true;
    memset(m_remark, 0, sizeof(m_remark));
    strncpy(m_remark, value, len);
}

static bool checkCalibrationValue(Value &calibrationValue, int16_t &scpiErr) {
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

static bool isCalibrated(Value &value) {
    for (unsigned int i = 0; i < value.configuration.numPoints; i++) {
        if (!value.isPointSet[i]) {
            return false;
        }
    }

    return true;
}

bool CalibrationEditor::canSave(int16_t &scpiErr, int16_t *uiErr) {
    if (!isEnabled()) {
        scpiErr = SCPI_ERROR_CALIBRATION_STATE_IS_OFF;
        if (uiErr) {
            *uiErr = scpiErr;
        }
        return false;
    }

    // at least one value should be calibrated
    bool valueCalibrated = false;

    if (calibration::g_editor.isCalibrationValueTypeSelectable() || calibration::g_editor.getCalibrationValueType() == CALIBRATION_VALUE_U) {
        if (isCalibrated(m_voltageValue)) {
            if (!checkCalibrationValue(m_voltageValue, scpiErr)) {
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
    }

    if (calibration::g_editor.isCalibrationValueTypeSelectable() || calibration::g_editor.getCalibrationValueType() == CALIBRATION_VALUE_I_HI_RANGE) {
        if (isCalibrated(m_currentsValue[0])) {
            if (!checkCalibrationValue(m_currentsValue[0], scpiErr)) {
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
    }

    if (hasSupportForCurrentDualRange()) {
        if (isCalibrated(m_currentsValue[1])) {
            if (!checkCalibrationValue(m_currentsValue[1], scpiErr)) {
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

static bool doSave(int slotIndex, int subchannelIndex) {
    static bool result;

    result = false;

    if (!isPsuThread()) {
        sendMessageToPsu(PSU_MESSAGE_SAVE_CHANNEL_CALIBRATION, (slotIndex << 8) | subchannelIndex);
        return result;
    }

    result = persist_conf::saveChannelCalibration(slotIndex, subchannelIndex);
    return result;
}

bool CalibrationEditor::save() {
    Channel *channel = Channel::getBySlotIndex(m_slotIndex, m_subchannelIndex);

    CalibrationConfiguration calConf;

    calConf.calibrationDate = datetime::now();

    memset(&calConf.calibrationRemark, 0, CALIBRATION_REMARK_MAX_LENGTH + 1);
    strcpy(calConf.calibrationRemark, m_remark);

    if (isCalibrated(m_voltageValue)) {
        memcpy(&calConf.u, &m_voltageValue.configuration, sizeof(CalibrationValueConfiguration));
    }

    if (isCalibrated(m_currentsValue[0])) {
        memcpy(&calConf.i[0], &m_currentsValue[0].configuration, sizeof(CalibrationValueConfiguration));
    }

    if (hasSupportForCurrentDualRange() && isCalibrated(m_currentsValue[1])) {
        memcpy(&calConf.i[1], &m_currentsValue[1].configuration, sizeof(CalibrationValueConfiguration));
    }

    if (channel) {
        memcpy(&channel->cal_conf, &calConf, sizeof(CalibrationConfiguration));
    } else {
        if (!g_slots[m_slotIndex]->setCalibrationConfiguration(m_subchannelIndex, calConf, nullptr)) {
            return false;
        }
    }

    return doSave(m_slotIndex, m_subchannelIndex);
}

////////////////////////////////////////////////////////////////////////////////

void CalibrationViewer::start(int slotIndex, int subchannelIndex) {
    m_slotIndex = slotIndex;
    m_subchannelIndex = subchannelIndex;

    g_slots[m_slotIndex]->initChannelCalibration(m_subchannelIndex);
}

////////////////////////////////////////////////////////////////////////////////

bool clear(int slotIndex, int subchannelIndex, int *err) {
	Channel *channel = Channel::getBySlotIndex(slotIndex, subchannelIndex);

    CalibrationConfiguration calConf;
    clearCalibrationConf(&calConf);

    if (channel) {
        channel->calibrationEnable(false);

		memcpy(&channel->cal_conf, &calConf, sizeof(CalibrationConfiguration));
    } else {
        g_slots[slotIndex]->enableVoltageCalibration(subchannelIndex, false);
        g_slots[slotIndex]->enableCurrentCalibration(subchannelIndex, false);

		if (!g_slots[slotIndex]->setCalibrationConfiguration(subchannelIndex, calConf, err)) {
            return false;
        }
    }

    if (!doSave(slotIndex, subchannelIndex)) {
        if (err) {
            *err = SCPI_ERROR_EXECUTION_ERROR;
        }
        return false;
    }

    return true;
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

bool onHighPriorityThreadMessage(uint8_t type, uint32_t param) {
    if (type == PSU_MESSAGE_CALIBRATION_START) {
        int slotIndex = param >> 8;
        int subchannelIndex = param & 0xFF;
        g_editor.start(slotIndex, subchannelIndex);
    } else if (type == PSU_MESSAGE_CALIBRATION_SELECT_CURRENT_RANGE) {
        g_editor.selectCurrentRange((int8_t)param);
    } else if (type == PSU_MESSAGE_SAVE_CHANNEL_CALIBRATION) {
        int slotIndex = param >> 8;
        int subchannelIndex = param & 0xFF;
        doSave(slotIndex, subchannelIndex);
    } else if (type == PSU_MESSAGE_CALIBRATION_STOP) {
        g_editor.stop();
    } else {
        // not handled
        return false;
    }

    // handled
    return true;
}

} // namespace calibration
} // namespace psu
} // namespace eez
