/*
 * EEZ PSU Firmware
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

#include <stdio.h>

#include <eez/apps/psu/calibration.h>
#include <eez/apps/psu/channel_dispatcher.h>
#include <eez/apps/psu/profile.h>
#include <eez/apps/psu/scpi/psu.h>

#include <eez/modules/dcpX05/eeprom.h>

namespace eez {
namespace psu {
namespace scpi {

////////////////////////////////////////////////////////////////////////////////

scpi_choice_def_t channelsCouplingChoice[] = {
    { "NONE", channel_dispatcher::TYPE_NONE },
    { "PARallel", channel_dispatcher::TYPE_PARALLEL },
    { "SERies", channel_dispatcher::TYPE_SERIES },
    { "CGND", channel_dispatcher::TYPE_COMMON_GROUND },
    { "SRAil", channel_dispatcher::TYPE_SPLIT_RAIL },
    SCPI_CHOICE_LIST_END /* termination of option list */
};

////////////////////////////////////////////////////////////////////////////////

scpi_choice_def_t traceValueChoice[] = {
    { "VOLTage", DISPLAY_VALUE_VOLTAGE },
    { "CURRent", DISPLAY_VALUE_CURRENT },
    { "POWer", DISPLAY_VALUE_POWER },
    SCPI_CHOICE_LIST_END /* termination of option list */
};

////////////////////////////////////////////////////////////////////////////////

static void select_channel(scpi_t *context, uint8_t ch) {
    scpi_psu_t *psu_context = (scpi_psu_t *)context->user_context;
    psu_context->selected_channel_index = ch;
}

////////////////////////////////////////////////////////////////////////////////

scpi_result_t scpi_cmd_instrumentSelect(scpi_t *context) {
    // TODO migrate to generic firmware
    if (calibration::isEnabled()) {
        SCPI_ErrorPush(context, SCPI_ERROR_BAD_SEQUENCE_OF_CALIBRATION_COMMANDS);
        return SCPI_RES_ERR;
    }

    Channel *channel = param_channel(context, TRUE);
    if (!channel) {
        return SCPI_RES_ERR;
    }

    select_channel(context, channel->index);

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_instrumentSelectQ(scpi_t *context) {
    // TODO migrate to generic firmware
    scpi_psu_t *psu_context = (scpi_psu_t *)context->user_context;

    char buffer[256] = { 0 };
    sprintf(buffer, "CH%d", (int)psu_context->selected_channel_index);
    SCPI_ResultCharacters(context, buffer, strlen(buffer));

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_instrumentNselect(scpi_t *context) {
    // TODO migrate to generic firmware
    if (calibration::isEnabled()) {
        SCPI_ErrorPush(context, SCPI_ERROR_BAD_SEQUENCE_OF_CALIBRATION_COMMANDS);
        return SCPI_RES_ERR;
    }

    int32_t ch;
    if (!SCPI_ParamInt(context, &ch, TRUE)) {
        return SCPI_RES_ERR;
    }

    if (!check_channel(context, ch)) {
        return SCPI_RES_ERR;
    }

    select_channel(context, ch);

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_instrumentNselectQ(scpi_t *context) {
    // TODO migrate to generic firmware
    scpi_psu_t *psu_context = (scpi_psu_t *)context->user_context;

    SCPI_ResultInt(context, psu_context->selected_channel_index);

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_instrumentCoupleTracking(scpi_t *context) {
    // TODO migrate to generic firmware
    if (channel_dispatcher::isTracked()) {
        SCPI_ErrorPush(context, SCPI_ERROR_EXECUTE_ERROR_IN_TRACKING_MODE);
        return SCPI_RES_ERR;
    }

    int32_t type;
    if (!SCPI_ParamChoice(context, channelsCouplingChoice, &type, true)) {
        return SCPI_RES_ERR;
    }

    if (CH_NUM < 2) {
        SCPI_ErrorPush(context, SCPI_ERROR_HARDWARE_MISSING);
        return SCPI_RES_ERR;
    }

    if (!channel_dispatcher::isCouplingOrTrackingAllowed((channel_dispatcher::Type)type)) {
        SCPI_ErrorPush(context, SCPI_ERROR_HARDWARE_MISSING);
        return SCPI_RES_ERR;
    }

    if (channel_dispatcher::setType((channel_dispatcher::Type)type)) {
        profile::save();
    } else {
        SCPI_ErrorPush(context, SCPI_ERROR_EXECUTION_ERROR);
        return SCPI_RES_ERR;
    }

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_instrumentCoupleTrackingQ(scpi_t *context) {
    // TODO migrate to generic firmware
    if (channel_dispatcher::isTracked()) {
        SCPI_ErrorPush(context, SCPI_ERROR_EXECUTE_ERROR_IN_TRACKING_MODE);
        return SCPI_RES_ERR;
    }

    char result[16];

    channel_dispatcher::Type type = channel_dispatcher::getType();
    if (type == channel_dispatcher::TYPE_PARALLEL) {
        strcpy(result, "PARALLEL");
    } else if (type == channel_dispatcher::TYPE_SERIES) {
        strcpy(result, "SERIES");
    } else if (type == channel_dispatcher::TYPE_COMMON_GROUND) {
        strcpy(result, "CGND");
    } else if (type == channel_dispatcher::TYPE_SERIES) {
        strcpy(result, "SRAIL");
    } else {
        strcpy(result, "NONE");
    }

    SCPI_ResultText(context, result);

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_instrumentDisplayTrace(scpi_t *context) {
    // TODO migrate to generic firmware
    scpi_psu_t *psu_context = (scpi_psu_t *)context->user_context;
    Channel *channel = &Channel::get(psu_context->selected_channel_index - 1);

    int32_t traceNumber;
    SCPI_CommandNumbers(context, &traceNumber, 1, 1);
    if (traceNumber < 1 || traceNumber > 2) {
        SCPI_ErrorPush(context, SCPI_ERROR_HEADER_SUFFIX_OUTOFRANGE);
        return SCPI_RES_ERR;
    }

    int32_t traceValue;
    if (!SCPI_ParamChoice(context, traceValueChoice, &traceValue, true)) {
        return SCPI_RES_ERR;
    }

    if (traceNumber == 1) {
        if (traceValue == channel->flags.displayValue2) {
            SCPI_ErrorPush(context, SCPI_ERROR_EXECUTION_ERROR);
            return SCPI_RES_ERR;
        }
        channel_dispatcher::setDisplayViewSettings(
            *channel, traceValue, channel->flags.displayValue2, channel->ytViewRate);
    } else {
        if (traceValue == channel->flags.displayValue1) {
            SCPI_ErrorPush(context, SCPI_ERROR_EXECUTION_ERROR);
            return SCPI_RES_ERR;
        }
        channel_dispatcher::setDisplayViewSettings(*channel, channel->flags.displayValue1,
                                                   traceValue, channel->ytViewRate);
    }

    profile::save();

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_instrumentDisplayTraceQ(scpi_t *context) {
    // TODO migrate to generic firmware
    scpi_psu_t *psu_context = (scpi_psu_t *)context->user_context;
    Channel *channel = &Channel::get(psu_context->selected_channel_index - 1);

    int32_t traceNumber;
    SCPI_CommandNumbers(context, &traceNumber, 1, 1);
    if (traceNumber < 1 || traceNumber > 2) {
        SCPI_ErrorPush(context, SCPI_ERROR_HEADER_SUFFIX_OUTOFRANGE);
        return SCPI_RES_ERR;
    }

    int8_t traceValue;
    if (traceNumber == 1) {
        traceValue = channel->flags.displayValue1;
    } else {
        traceValue = channel->flags.displayValue2;
    }

    char result[16];

    if (traceValue == DISPLAY_VALUE_VOLTAGE) {
        strcpy(result, "VOLT");
    } else if (traceValue == DISPLAY_VALUE_CURRENT) {
        strcpy(result, "CURR");
    } else {
        strcpy(result, "POW");
    }

    SCPI_ResultText(context, result);

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_instrumentDisplayTraceSwap(scpi_t *context) {
    // TODO migrate to generic firmware
    scpi_psu_t *psu_context = (scpi_psu_t *)context->user_context;
    Channel *channel = &Channel::get(psu_context->selected_channel_index - 1);

    channel_dispatcher::setDisplayViewSettings(*channel, channel->flags.displayValue2,
                                               channel->flags.displayValue1, channel->ytViewRate);
    profile::save();

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_instrumentDisplayYtRate(scpi_t *context) {
    // TODO migrate to generic firmware
    scpi_psu_t *psu_context = (scpi_psu_t *)context->user_context;
    Channel *channel = &Channel::get(psu_context->selected_channel_index - 1);

    float ytViewRate;
    if (!get_duration_param(context, ytViewRate, GUI_YT_VIEW_RATE_MIN, GUI_YT_VIEW_RATE_MAX,
                            GUI_YT_VIEW_RATE_DEFAULT)) {
        return SCPI_RES_ERR;
    }

    channel_dispatcher::setDisplayViewSettings(*channel, channel->flags.displayValue1,
                                               channel->flags.displayValue2, ytViewRate);
    profile::save();

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_instrumentDisplayYtRateQ(scpi_t *context) {
    // TODO migrate to generic firmware
    scpi_psu_t *psu_context = (scpi_psu_t *)context->user_context;
    Channel *channel = &Channel::get(psu_context->selected_channel_index - 1);

    SCPI_ResultFloat(context, channel->ytViewRate);

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_instrumentMemory(scpi_t *context) {
    int32_t ch;

    if (!SCPI_ParamChoice(context, channel_choice, &ch, true)) {
        SCPI_ErrorPush(context, SCPI_ERROR_ILLEGAL_PARAMETER_VALUE);
        return SCPI_RES_ERR;
    }

    int32_t address;
    if (!SCPI_ParamInt32(context, &address, true)) {
        SCPI_ErrorPush(context, SCPI_ERROR_MISSING_PARAMETER);
        return SCPI_RES_ERR;
    }
    if (address < 0 || address > 4095) {
        SCPI_ErrorPush(context, SCPI_ERROR_DATA_OUT_OF_RANGE);
        return SCPI_RES_ERR;
    }

    int32_t size;
    if (!SCPI_ParamInt32(context, &size, true)) {
        SCPI_ErrorPush(context, SCPI_ERROR_MISSING_PARAMETER);
        return SCPI_RES_ERR;
    }
    if (size != 1 && size != 2 && size != 4) {
        SCPI_ErrorPush(context, SCPI_ERROR_ILLEGAL_PARAMETER_VALUE);
        return SCPI_RES_ERR;
    }

    int32_t value;
    if (!SCPI_ParamInt32(context, &value, true)) {
        SCPI_ErrorPush(context, SCPI_ERROR_MISSING_PARAMETER);
        return SCPI_RES_ERR;
    }
    if ((size == 1 && (value < 0 || value > 255)) ||
        (size == 2 && (value < 0 || value > 65535))) {
        SCPI_ErrorPush(context, SCPI_ERROR_DATA_OUT_OF_RANGE);
        return SCPI_RES_ERR;
    }

    uint8_t slotIndex = Channel::get(ch - 1).slotIndex;

    if (!dcpX05::eeprom::write(slotIndex, (const uint8_t *)&value, size, (uint16_t)address)) {
        SCPI_ErrorPush(context, SCPI_ERROR_EXECUTION_ERROR);
        return SCPI_RES_ERR;
    }

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_instrumentMemoryQ(scpi_t *context) {
    // TODO migrate to generic firmware
    int32_t address;
    if (!SCPI_ParamInt32(context, &address, true)) {
        return SCPI_RES_ERR;
    }
    if (address < 0 || address > 4095) {
        SCPI_ErrorPush(context, SCPI_ERROR_DATA_OUT_OF_RANGE);
        return SCPI_RES_ERR;
    }

    int32_t size;
    if (!SCPI_ParamInt32(context, &size, true)) {
        return SCPI_RES_ERR;
    }
    if (size != 1 && size != 2 && size != 4) {
        SCPI_ErrorPush(context, SCPI_ERROR_ILLEGAL_PARAMETER_VALUE);
        return SCPI_RES_ERR;
    }

    scpi_psu_t *psu_context = (scpi_psu_t *)context->user_context;
    uint8_t slotIndex = Channel::get(psu_context->selected_channel_index - 1).slotIndex;

    if (size == 1) {
        uint8_t value;
        if (!dcpX05::eeprom::read(slotIndex, &value, 1, (uint16_t)address)) {
            SCPI_ErrorPush(context, SCPI_ERROR_EXECUTION_ERROR);
            return SCPI_RES_ERR;
        }
        SCPI_ResultUInt8(context, value);
    } else if (size == 2) {
        uint16_t value;
        if (!dcpX05::eeprom::read(slotIndex, (uint8_t *)&value, 2, (uint16_t)address)) {
            SCPI_ErrorPush(context, SCPI_ERROR_EXECUTION_ERROR);
            return SCPI_RES_ERR;
        }
        SCPI_ResultUInt16(context, value);
    } else {
        uint32_t value;
        if (!dcpX05::eeprom::read(slotIndex, (uint8_t *)&value, 4, (uint16_t)address)) {
            SCPI_ErrorPush(context, SCPI_ERROR_EXECUTION_ERROR);
            return SCPI_RES_ERR;
        }
        SCPI_ResultUInt32(context, value);
    }

    return SCPI_RES_OK;
}

} // namespace scpi
} // namespace psu
} // namespace eez
