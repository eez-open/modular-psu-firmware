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

#include <eez/modules/psu/calibration.h>
#include <eez/modules/psu/channel_dispatcher.h>
#include <eez/modules/psu/scpi/psu.h>
#include <eez/modules/psu/trigger.h>

#if OPTION_DISPLAY
#include <eez/modules/psu/gui/psu.h>
#endif

namespace eez {
namespace psu {
namespace scpi {

////////////////////////////////////////////////////////////////////////////////

scpi_choice_def_t calibration_current_range_choice[] = {
    { "HIGH", CURRENT_RANGE_HIGH },
    { "LOW", CURRENT_RANGE_LOW },
    SCPI_CHOICE_LIST_END /* termination of option list */
};

////////////////////////////////////////////////////////////////////////////////

static scpi_result_t calibration_level(scpi_t *context, calibration::Value &calibrationValue) {
    if (!calibration::isEnabled()) {
        SCPI_ErrorPush(context, SCPI_ERROR_CALIBRATION_STATE_IS_OFF);
        return SCPI_RES_ERR;
    }

    int32_t currentPointIndex;
    if (!SCPI_ParamInt32(context, &currentPointIndex, true)) {
        return SCPI_RES_ERR;
    }

    if (currentPointIndex < 1 || currentPointIndex > MAX_CALIBRATION_POINTS) {
        SCPI_ErrorPush(context, SCPI_ERROR_ILLEGAL_PARAMETER_VALUE);
        return SCPI_RES_ERR;
    }

    int slotIndex;
    int subchannelIndex;
    calibration::getCalibrationChannel(slotIndex, subchannelIndex);

    float dacValue;
    if (calibrationValue.type == CALIBRATION_VALUE_U) {
        if (!get_voltage_param(context, dacValue, slotIndex, subchannelIndex, 0)) {
            return SCPI_RES_ERR;
        }
    } else {
        if (!calibration::isCalibrationValueTypeSelectable()) {
            SCPI_ErrorPush(context, SCPI_ERROR_BAD_SEQUENCE_OF_CALIBRATION_COMMANDS);
            return SCPI_RES_ERR;
        }

        if (!get_current_param(context, dacValue, slotIndex, subchannelIndex, 0)) {
            return SCPI_RES_ERR;
        }
    }

    calibrationValue.setCurrentPointIndex(currentPointIndex - 1);
    calibrationValue.setDacValue(dacValue);

    return SCPI_RES_OK;
}
static scpi_result_t calibration_data(scpi_t *context, calibration::Value &calibrationValue) {
    if (!calibration::isEnabled()) {
        SCPI_ErrorPush(context, SCPI_ERROR_CALIBRATION_STATE_IS_OFF);
        return SCPI_RES_ERR;
    }

    if (calibrationValue.currentPointIndex == -1) {
        SCPI_ErrorPush(context, SCPI_ERROR_BAD_SEQUENCE_OF_CALIBRATION_COMMANDS);
        return SCPI_RES_ERR;
    }

    if (calibrationValue.type != CALIBRATION_VALUE_U && !calibration::isCalibrationValueTypeSelectable()) {
        SCPI_ErrorPush(context, SCPI_ERROR_BAD_SEQUENCE_OF_CALIBRATION_COMMANDS);
        return SCPI_RES_ERR;
    }

    scpi_number_t param;
    if (!SCPI_ParamNumber(context, 0, &param, true)) {
        return SCPI_RES_ERR;
    }

    if (param.unit != SCPI_UNIT_NONE &&
        param.unit != (calibrationValue.type == CALIBRATION_VALUE_U ? SCPI_UNIT_VOLT : SCPI_UNIT_AMPER)) {
        SCPI_ErrorPush(context, SCPI_ERROR_INVALID_SUFFIX);
        return SCPI_RES_ERR;
    }

    float value = (float)param.content.value;

    float adc = calibrationValue.readAdcValue();
    if (!calibrationValue.checkValueAndAdc(value, adc)) {
        SCPI_ErrorPush(context, SCPI_ERROR_CAL_VALUE_OUT_OF_RANGE);
        return SCPI_RES_ERR;
    }
    calibrationValue.setValueAndAdc(value, adc);

    return SCPI_RES_OK;
}

////////////////////////////////////////////////////////////////////////////////

scpi_result_t scpi_cmd_calibrationClear(scpi_t *context) {
    if (calibration::isEnabled()) {
        SCPI_ErrorPush(context, SCPI_ERROR_BAD_SEQUENCE_OF_CALIBRATION_COMMANDS);
        return SCPI_RES_ERR;
    }

    if (!checkPassword(context, persist_conf::devConf.calibrationPassword)) {
        return SCPI_RES_ERR;
    }

    SlotAndSubchannelIndex *slotAndSubchannelIndex = getSelectedChannel(context);
    if (!slotAndSubchannelIndex) {
        return SCPI_RES_ERR;
    }

    int err;
    if (!calibration::clear(slotAndSubchannelIndex->slotIndex, slotAndSubchannelIndex->subchannelIndex, &err)) {
        SCPI_ErrorPush(context, err);
        return SCPI_RES_ERR;
    }

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_calibrationMode(scpi_t *context) {
    if (!trigger::isIdle()) {
        SCPI_ErrorPush(context, SCPI_ERROR_CANNOT_CHANGE_TRANSIENT_TRIGGER);
        return SCPI_RES_ERR;
    }

    bool enable;
    if (!SCPI_ParamBool(context, &enable, TRUE)) {
        return SCPI_RES_ERR;
    }

    if (enable == calibration::isEnabled()) {
        SCPI_ErrorPush(context, SCPI_ERROR_BAD_SEQUENCE_OF_CALIBRATION_COMMANDS);
        return SCPI_RES_ERR;
    }

    if (!checkPassword(context, persist_conf::devConf.calibrationPassword)) {
        return SCPI_RES_ERR;
    }

    SlotAndSubchannelIndex *slotAndSubchannelIndex = getSelectedChannel(context);
    if (!slotAndSubchannelIndex) {
        SCPI_ErrorPush(context, SCPI_ERROR_BAD_SEQUENCE_OF_CALIBRATION_COMMANDS);
        return SCPI_RES_ERR;
    }

    Channel *channel = Channel::getBySlotIndex(slotAndSubchannelIndex->slotIndex, slotAndSubchannelIndex->subchannelIndex);
    if (channel) {
        if (channel->channelIndex < 2 && channel_dispatcher::getCouplingType() != channel_dispatcher::COUPLING_TYPE_NONE) {
            SCPI_ErrorPush(context, SCPI_ERROR_EXECUTE_ERROR_CHANNELS_ARE_COUPLED);
            return SCPI_RES_ERR;
        }

        if (channel->flags.trackingEnabled) {
            SCPI_ErrorPush(context, SCPI_ERROR_EXECUTE_ERROR_IN_TRACKING_MODE);
            return SCPI_RES_ERR;
        }

        if (enable) {
            if (!channel->isOutputEnabled()) {
                SCPI_ErrorPush(context, SCPI_ERROR_BAD_SEQUENCE_OF_CALIBRATION_COMMANDS);
                return SCPI_RES_ERR;
            }
        }
    }

    if (enable) {
        calibration::start(slotAndSubchannelIndex->slotIndex, slotAndSubchannelIndex->subchannelIndex);
    } else {
        calibration::stop();
    }

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_calibrationModeQ(scpi_t *context) {
    SCPI_ResultBool(context, calibration::isEnabled());
    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_calibrationCurrentData(scpi_t *context) {
    return calibration_data(context, calibration::getCurrent());
}

scpi_result_t scpi_cmd_calibrationCurrentLevel(scpi_t *context) {
    return calibration_level(context, calibration::getCurrent());
}

scpi_result_t scpi_cmd_calibrationCurrentRange(scpi_t *context) {
    if (!calibration::isEnabled()) {
        SCPI_ErrorPush(context, SCPI_ERROR_CALIBRATION_STATE_IS_OFF);
        return SCPI_RES_ERR;
    }

    if (!calibration::hasSupportForCurrentDualRange()) {
        SCPI_ErrorPush(context, SCPI_ERROR_HARDWARE_MISSING);
        return SCPI_RES_ERR;
    }

    int32_t range;
    if (!SCPI_ParamChoice(context, calibration_current_range_choice, &range, true)) {
        return SCPI_RES_ERR;
    }

    calibration::selectCurrentRange(range);

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_calibrationPasswordNew(scpi_t *context) {
    if (!checkPassword(context, persist_conf::devConf.calibrationPassword)) {
        return SCPI_RES_ERR;
    }

    const char *new_password;
    size_t new_password_len;

    if (!SCPI_ParamCharacters(context, &new_password, &new_password_len, true)) {
        return SCPI_RES_ERR;
    }

    int16_t err;
    if (!persist_conf::isCalibrationPasswordValid(new_password, new_password_len, err)) {
        SCPI_ErrorPush(context, err);
        return SCPI_RES_ERR;
    }

    persist_conf::changeCalibrationPassword(new_password, new_password_len);

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_calibrationRemark(scpi_t *context) {
    if (!calibration::isEnabled()) {
        SCPI_ErrorPush(context, SCPI_ERROR_CALIBRATION_STATE_IS_OFF);
        return SCPI_RES_ERR;
    }

    const char *remark;
    size_t len;
    if (!SCPI_ParamCharacters(context, &remark, &len, true)) {
        return SCPI_RES_ERR;
    }

    if (len > CALIBRATION_REMARK_MAX_LENGTH) {
        SCPI_ErrorPush(context, SCPI_ERROR_TOO_MUCH_DATA);
        return SCPI_RES_ERR;
    }

    calibration::setRemark(remark, len);

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_calibrationRemarkQ(scpi_t *context) {
    const char *remark;

    if (calibration::isEnabled()) {
        remark = calibration::getRemark();
    } else {
        Channel *channel = getSelectedPowerChannel(context);
        if (!channel) {
            return SCPI_RES_ERR;
        }
        remark = channel->cal_conf.calibrationRemark;
    }

    SCPI_ResultText(context, remark);

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_calibrationSave(scpi_t *context) {
    int16_t err;
    if (!calibration::canSave(err)) {
        SCPI_ErrorPush(context, err);
        return SCPI_RES_ERR;
    }

    if (!calibration::save()) {
        SCPI_ErrorPush(context, SCPI_ERROR_EXECUTION_ERROR);
        return SCPI_RES_ERR;
    }

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_calibrationState(scpi_t *context) {
    if (calibration::isEnabled()) {
        SCPI_ErrorPush(context, SCPI_ERROR_BAD_SEQUENCE_OF_CALIBRATION_COMMANDS);
        return SCPI_RES_ERR;
    }

    SlotAndSubchannelIndex *slotAndSubchannelIndex = getSelectedChannel(context);
    if (!slotAndSubchannelIndex) {
        SCPI_ErrorPush(context, SCPI_ERROR_BAD_SEQUENCE_OF_CALIBRATION_COMMANDS);
        return SCPI_RES_ERR;
    }

    Channel *channel = Channel::getBySlotIndex(slotAndSubchannelIndex->slotIndex, slotAndSubchannelIndex->subchannelIndex);
    if (channel) {
        if (channel->channelIndex < 2 && channel_dispatcher::getCouplingType() != channel_dispatcher::COUPLING_TYPE_NONE) {
            SCPI_ErrorPush(context, SCPI_ERROR_EXECUTE_ERROR_CHANNELS_ARE_COUPLED);
            return SCPI_RES_ERR;
        }

        if (channel->flags.trackingEnabled) {
            SCPI_ErrorPush(context, SCPI_ERROR_EXECUTE_ERROR_IN_TRACKING_MODE);
            return SCPI_RES_ERR;
        }

        if (!channel->isCalibrationExists()) {
            SCPI_ErrorPush(context, SCPI_ERROR_CAL_PARAMS_MISSING);
            return SCPI_RES_ERR;
        }
    } else {
        if (!(
            g_slots[slotAndSubchannelIndex->slotIndex]->isVoltageCalibrationExists(slotAndSubchannelIndex->subchannelIndex) ||
            g_slots[slotAndSubchannelIndex->slotIndex]->isCurrentCalibrationExists(slotAndSubchannelIndex->subchannelIndex)
        )) {
            SCPI_ErrorPush(context, SCPI_ERROR_CAL_PARAMS_MISSING);
            return SCPI_RES_ERR;
        }
    }
    
    bool calibrationEnabled;
    if (!SCPI_ParamBool(context, &calibrationEnabled, TRUE)) {
        return SCPI_RES_ERR;
    }

    if (channel) {
        if (calibrationEnabled == channel->isCalibrationEnabled()) {
            SCPI_ErrorPush(context, SCPI_ERROR_BAD_SEQUENCE_OF_CALIBRATION_COMMANDS);
            return SCPI_RES_ERR;
        }

        channel->calibrationEnable(calibrationEnabled);
    } else {
        auto enabled = g_slots[slotAndSubchannelIndex->slotIndex]->isCurrentCalibrationEnabled(slotAndSubchannelIndex->subchannelIndex) ||
            g_slots[slotAndSubchannelIndex->slotIndex]->isVoltageCalibrationEnabled(slotAndSubchannelIndex->subchannelIndex);

        if (calibrationEnabled == enabled) {
            SCPI_ErrorPush(context, SCPI_ERROR_BAD_SEQUENCE_OF_CALIBRATION_COMMANDS);
            return SCPI_RES_ERR;
        }

        g_slots[slotAndSubchannelIndex->slotIndex]->enableVoltageCalibration(slotAndSubchannelIndex->subchannelIndex, calibrationEnabled);
        g_slots[slotAndSubchannelIndex->slotIndex]->enableCurrentCalibration(slotAndSubchannelIndex->subchannelIndex, calibrationEnabled);
    }

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_calibrationStateQ(scpi_t *context) {
    SlotAndSubchannelIndex *slotAndSubchannelIndex = getSelectedChannel(context);
    if (!slotAndSubchannelIndex) {
        SCPI_ErrorPush(context, SCPI_ERROR_BAD_SEQUENCE_OF_CALIBRATION_COMMANDS);
        return SCPI_RES_ERR;
    }

    Channel *channel = Channel::getBySlotIndex(slotAndSubchannelIndex->slotIndex, slotAndSubchannelIndex->subchannelIndex);

    if (channel) {
        SCPI_ResultBool(context, channel->isCalibrationEnabled());
    } else {
        SCPI_ResultBool(context, 
            g_slots[slotAndSubchannelIndex->slotIndex]->isCurrentCalibrationEnabled(slotAndSubchannelIndex->subchannelIndex) ||
            g_slots[slotAndSubchannelIndex->slotIndex]->isVoltageCalibrationEnabled(slotAndSubchannelIndex->subchannelIndex));
    }

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_calibrationVoltageData(scpi_t *context) {
    return calibration_data(context, calibration::getVoltage());
}

scpi_result_t scpi_cmd_calibrationVoltageLevel(scpi_t *context) {
    return calibration_level(context, calibration::getVoltage());
}

scpi_result_t scpi_cmd_calibrationScreenInit(scpi_t *context) {
#if OPTION_DISPLAY
    psu::gui::showPage(PAGE_ID_TOUCH_CALIBRATION_INTRO);
    return SCPI_RES_OK;
#else
    SCPI_ErrorPush(context, SCPI_ERROR_HARDWARE_MISSING);
    return SCPI_RES_ERR;
#endif
}

} // namespace scpi
} // namespace psu
} // namespace eez
