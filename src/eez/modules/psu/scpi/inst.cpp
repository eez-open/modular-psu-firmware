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

#include <stdio.h>

#if OPTION_DISPLAY
#include <eez/gui/gui.h>
#endif

#include <eez/modules/psu/calibration.h>
#include <eez/modules/psu/channel_dispatcher.h>
#include <eez/modules/psu/profile.h>
#include <eez/modules/psu/scpi/psu.h>

#include <eez/modules/bp3c/eeprom.h>

namespace eez {
namespace psu {
namespace scpi {

////////////////////////////////////////////////////////////////////////////////

scpi_choice_def_t channelsCouplingChoice[] = {
    { "NONE", channel_dispatcher::COUPLING_TYPE_NONE },
    { "PARallel", channel_dispatcher::COUPLING_TYPE_PARALLEL },
    { "SERies", channel_dispatcher::COUPLING_TYPE_SERIES },
    { "CGND", channel_dispatcher::COUPLING_TYPE_COMMON_GND },
    { "SRAil", channel_dispatcher::COUPLING_TYPE_SPLIT_RAILS },
    SCPI_CHOICE_LIST_END /* termination of option list */
};

////////////////////////////////////////////////////////////////////////////////

scpi_choice_def_t traceValueChoice[] = {
    { "VOLTage", DISPLAY_VALUE_VOLTAGE },
    { "CURRent", DISPLAY_VALUE_CURRENT },
    { "POWer", DISPLAY_VALUE_POWER },
    SCPI_CHOICE_LIST_END /* termination of option list */
};

scpi_choice_def_t displayValueScaleChoice[] = {
    { "MAXimum", DISPLAY_VALUE_SCALE_MAXIMUM },
    { "LIMit", DISPLAY_VALUE_SCALE_LIMIT },
    { "AUTO", DISPLAY_VALUE_SCALE_AUTO },
    SCPI_CHOICE_LIST_END /* termination of option list */
};

////////////////////////////////////////////////////////////////////////////////

static void selectChannel(scpi_t *context, SlotAndSubchannelIndex &slotAndSubchannelIndex) {
    scpi_psu_t *psu_context = (scpi_psu_t *)context->user_context;
    psu_context->selectedChannels.numChannels = 1;
    psu_context->selectedChannels.channels[0] = slotAndSubchannelIndex;
}

////////////////////////////////////////////////////////////////////////////////

scpi_result_t scpi_cmd_instrumentSelect(scpi_t *context) {
    SlotAndSubchannelIndex slotAndSubchannelIndex;
    if (!getChannelFromParam(context, slotAndSubchannelIndex)) {
		return SCPI_RES_ERR;
    }

    Channel *channel = Channel::getBySlotIndex(slotAndSubchannelIndex.slotIndex, slotAndSubchannelIndex.subchannelIndex);
    if (channel) {
        if (!checkPowerChannel(context, channel->channelIndex)) {
            return SCPI_RES_ERR;
        }
    }

    selectChannel(context, slotAndSubchannelIndex);

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_instrumentSelectQ(scpi_t *context) {
    scpi_psu_t *psu_context = (scpi_psu_t *)context->user_context;

    for (int i = 0; i < psu_context->selectedChannels.numChannels; i++) {
        char buffer[20];
        snprintf(buffer, sizeof(buffer), "(@%d%02d)", psu_context->selectedChannels.channels[i].slotIndex + 1, psu_context->selectedChannels.channels[i].subchannelIndex + 1);
        SCPI_ResultCharacters(context, buffer, strlen(buffer));
    }

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_instrumentNselect(scpi_t *context) {
    int32_t channelIndex;
    if (!SCPI_ParamInt(context, &channelIndex, TRUE)) {
        return SCPI_RES_ERR;
    }

    channelIndex--;

    SlotAndSubchannelIndex slotAndSubchannelIndex;
    if (!absoluteChannelIndexToSlotAndSubchannelIndex(channelIndex, slotAndSubchannelIndex)) {
        return SCPI_RES_ERR;
    }

    Channel *channel = Channel::getBySlotIndex(slotAndSubchannelIndex.slotIndex, slotAndSubchannelIndex.subchannelIndex);
    if (channel) {
        if (!checkPowerChannel(context, channel->channelIndex)) {
            return SCPI_RES_ERR;
        }
    }

    selectChannel(context, slotAndSubchannelIndex);

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_instrumentNselectQ(scpi_t *context) {
    scpi_psu_t *psu_context = (scpi_psu_t *)context->user_context;

    for (int i = 0; i < psu_context->selectedChannels.numChannels; i++) {
        auto channel = Channel::getBySlotIndex(psu_context->selectedChannels.channels[i].slotIndex, psu_context->selectedChannels.channels[i].subchannelIndex);
        if (channel) {
            SCPI_ResultInt(context, channel->channelIndex + 1);
        } else {
            SCPI_ResultInt(context, 0);
        }
    }

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_instrumentCoupleTracking(scpi_t *context) {
    int32_t type;
    if (!SCPI_ParamChoice(context, channelsCouplingChoice, &type, true)) {
        return SCPI_RES_ERR;
    }

    int err;
    if (!channel_dispatcher::setCouplingType((channel_dispatcher::CouplingType)type, &err)) {
        SCPI_ErrorPush(context, err);
        return SCPI_RES_ERR;
    }

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_instrumentCoupleTrackingQ(scpi_t *context) {
    char result[16];

    channel_dispatcher::CouplingType couplingType = channel_dispatcher::getCouplingType();
    if (couplingType == channel_dispatcher::COUPLING_TYPE_PARALLEL) {
        stringCopy(result, sizeof(result), "PARALLEL");
    } else if (couplingType == channel_dispatcher::COUPLING_TYPE_SERIES) {
        stringCopy(result, sizeof(result), "SERIES");
    } else if (couplingType == channel_dispatcher::COUPLING_TYPE_COMMON_GND) {
        stringCopy(result, sizeof(result), "CGND");
    } else if (couplingType == channel_dispatcher::COUPLING_TYPE_SPLIT_RAILS) {
        stringCopy(result, sizeof(result), "SRAIL");
    } else {
        stringCopy(result, sizeof(result), "NONE");
    }

    SCPI_ResultText(context, result);

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_instrumentDisplayTrace(scpi_t *context) {
    Channel *channel = getSelectedPowerChannel(context);
    if (!channel) {
        return SCPI_RES_ERR;
    }

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

    int displayValueIndex = traceNumber - 1;
    int otherDisplayValueIndex = displayValueIndex == 0 ? 1 : 0;

    if (traceValue == channel->displayValues[otherDisplayValueIndex].type) {
        SCPI_ErrorPush(context, SCPI_ERROR_EXECUTION_ERROR);
        return SCPI_RES_ERR;
    }

	DisplayValue displayValues[2];
	displayValues[0] = channel->displayValues[0];
	displayValues[1] = channel->displayValues[1];
    displayValues[displayValueIndex].type = (DisplayValueType)traceValue;

    channel_dispatcher::setDisplayViewSettings(*channel, displayValues, channel->ytViewRate);

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_instrumentDisplayTraceQ(scpi_t *context) {
    Channel *channel = getSelectedPowerChannel(context);
    if (!channel) {
        return SCPI_RES_ERR;
    }

    int32_t traceNumber;
    SCPI_CommandNumbers(context, &traceNumber, 1, 1);
    if (traceNumber < 1 || traceNumber > 2) {
        SCPI_ErrorPush(context, SCPI_ERROR_HEADER_SUFFIX_OUTOFRANGE);
        return SCPI_RES_ERR;
    }

    int displayValueIndex = traceNumber - 1;
    auto type = channel->displayValues[displayValueIndex].type;

    char result[16];

    if (type == DISPLAY_VALUE_VOLTAGE) {
        stringCopy(result, sizeof(result), "VOLT");
    } else if (type == DISPLAY_VALUE_CURRENT) {
        stringCopy(result, sizeof(result), "CURR");
    } else {
        stringCopy(result, sizeof(result), "POW");
    }

    SCPI_ResultText(context, result);

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_instrumentDisplayScale(scpi_t *context) {
    Channel *channel = getSelectedPowerChannel(context);
    if (!channel) {
        return SCPI_RES_ERR;
    }

    int32_t displayValueIndex;
    SCPI_CommandNumbers(context, &displayValueIndex, 1, 1);
    if (displayValueIndex < 1 || displayValueIndex > 2) {
        SCPI_ErrorPush(context, SCPI_ERROR_HEADER_SUFFIX_OUTOFRANGE);
        return SCPI_RES_ERR;
    }
	displayValueIndex--;

    scpi_parameter_t parameter;
    if (!SCPI_Parameter(context, &parameter, true)) {
        return SCPI_RES_ERR;
    }

	DisplayValue displayValues[2];
	displayValues[0] = channel->displayValues[0];
	displayValues[1] = channel->displayValues[1];

    if (SCPI_ParamIsNumber(&parameter, true)) {
        float range;
        if (!SCPI_ParamToFloat(context, &parameter, &range)) {
            return SCPI_RES_ERR;
        }

        if (range < 0 || range > displayValues[displayValueIndex].getMaxRange(channel)) {
            SCPI_ErrorPush(context, SCPI_ERROR_DATA_OUT_OF_RANGE);
            return SCPI_RES_ERR;
        }
        
        displayValues[displayValueIndex].scale = DISPLAY_VALUE_SCALE_CUSTOM;
        displayValues[displayValueIndex].range = range;
    } else {
        int32_t scale;
        if (!SCPI_ParamToChoice(context, &parameter, displayValueScaleChoice, &scale)) {
            return SCPI_RES_ERR;
        }
        displayValues[displayValueIndex].scale = (DisplayValueScale)scale;
    }

    channel_dispatcher::setDisplayViewSettings(*channel, displayValues, channel->ytViewRate);

#if OPTION_DISPLAY
	gui::refreshScreen();
#endif

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_instrumentDisplayScaleQ(scpi_t *context) {
    Channel *channel = getSelectedPowerChannel(context);
    if (!channel) {
        return SCPI_RES_ERR;
    }

    int32_t displayValueIndex;
    SCPI_CommandNumbers(context, &displayValueIndex, 1, 1);
    if (displayValueIndex < 1 || displayValueIndex > 2) {
        SCPI_ErrorPush(context, SCPI_ERROR_HEADER_SUFFIX_OUTOFRANGE);
        return SCPI_RES_ERR;
    }
	displayValueIndex--;

    auto scale = channel->displayValues[displayValueIndex].scale;

    if (scale == DISPLAY_VALUE_SCALE_MAXIMUM) {
        SCPI_ResultText(context, "MAX");
    } else if (scale == DISPLAY_VALUE_SCALE_LIMIT) {
        SCPI_ResultText(context, "LIM");
    } else if (scale == DISPLAY_VALUE_SCALE_AUTO) {
        SCPI_ResultText(context, "AUTO");
    } else {
        SCPI_ResultFloat(context, channel->displayValues[displayValueIndex].range);
    }

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_instrumentDisplayTraceSwap(scpi_t *context) {
    Channel *channel = getSelectedPowerChannel(context);
    if (!channel) {
        return SCPI_RES_ERR;
    }

	DisplayValue displayValues[2];
	displayValues[0] = channel->displayValues[1];
	displayValues[1] = channel->displayValues[0];

	channel_dispatcher::setDisplayViewSettings(*channel, displayValues, channel->ytViewRate);

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_instrumentDisplayYtRate(scpi_t *context) {
    Channel *channel = getSelectedPowerChannel(context);
    if (!channel) {
        return SCPI_RES_ERR;
    }

    float ytViewRate;
    if (!get_duration_param(context, ytViewRate, GUI_YT_VIEW_RATE_MIN, GUI_YT_VIEW_RATE_MAX,
                            GUI_YT_VIEW_RATE_DEFAULT)) {
        return SCPI_RES_ERR;
    }

    channel_dispatcher::setDisplayViewSettings(*channel, channel->displayValues, ytViewRate);

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_instrumentDisplayYtRateQ(scpi_t *context) {
    Channel *channel = getSelectedPowerChannel(context);
    if (!channel) {
        return SCPI_RES_ERR;
    }

    SCPI_ResultFloat(context, channel->ytViewRate);

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_instrumentMemory(scpi_t *context) {
    int32_t slotIndex;
    if (!SCPI_ParamInt32(context, &slotIndex, true)) {
        SCPI_ErrorPush(context, SCPI_ERROR_ILLEGAL_PARAMETER_VALUE);
        return SCPI_RES_ERR;
    }
    slotIndex--;

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

    if (!bp3c::eeprom::write(slotIndex, (const uint8_t *)&value, size, (uint16_t)address)) {
        SCPI_ErrorPush(context, SCPI_ERROR_EXECUTION_ERROR);
        return SCPI_RES_ERR;
    }

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_instrumentMemoryQ(scpi_t *context) {
    int32_t slotIndex;
    if (!SCPI_ParamInt32(context, &slotIndex, true)) {
        SCPI_ErrorPush(context, SCPI_ERROR_ILLEGAL_PARAMETER_VALUE);
        return SCPI_RES_ERR;
    }
    slotIndex--;

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

    if (size == 1) {
        uint8_t value;
        if (!bp3c::eeprom::read(slotIndex, &value, 1, (uint16_t)address)) {
            SCPI_ErrorPush(context, SCPI_ERROR_EXECUTION_ERROR);
            return SCPI_RES_ERR;
        }
        SCPI_ResultUInt8(context, value);
    } else if (size == 2) {
        uint16_t value;
        if (!bp3c::eeprom::read(slotIndex, (uint8_t *)&value, 2, (uint16_t)address)) {
            SCPI_ErrorPush(context, SCPI_ERROR_EXECUTION_ERROR);
            return SCPI_RES_ERR;
        }
        SCPI_ResultUInt16(context, value);
    } else {
        uint32_t value;
        if (!bp3c::eeprom::read(slotIndex, (uint8_t *)&value, 4, (uint16_t)address)) {
            SCPI_ErrorPush(context, SCPI_ERROR_EXECUTION_ERROR);
            return SCPI_RES_ERR;
        }
        SCPI_ResultUInt32(context, value);
    }

    return SCPI_RES_OK;
}

void dumpCatalog(scpi_t *context, bool dumpIndex) {
    bool catalogEmpty = true;

    if (CH_NUM > 0) {
        for (int channelIndex = 0; channelIndex < CH_NUM; channelIndex++) {
            char channelStr[10];
            snprintf(channelStr, sizeof(channelStr), "CH%d", channelIndex + 1);
            SCPI_ResultText(context, channelStr);
            if (dumpIndex) {
                SCPI_ResultInt(context, channelIndex + 1);
            }
            catalogEmpty = false;
        }
    }

    for (int slotIndex = 0; slotIndex < NUM_SLOTS; slotIndex++) {
        Module *module = g_slots[slotIndex];
        for (int relativeChannelIndex = module->numPowerChannels; relativeChannelIndex < module->numPowerChannels + module->numOtherChannels; relativeChannelIndex++) {
            int subchannelIndex = module->getSubchannelIndexFromRelativeChannelIndex(relativeChannelIndex);

            char channelStr[10];
            snprintf(channelStr, sizeof(channelStr), "(@%d%02d)", slotIndex + 1, subchannelIndex + 1);
            SCPI_ResultText(context, channelStr);
            if (dumpIndex) {
                SCPI_ResultInt(context, 0);
            }
            catalogEmpty = false;
        }
    }

    if (catalogEmpty) {
        SCPI_ResultText(context, "");
    }

}

scpi_result_t scpi_cmd_instrumentCatalogQ(scpi_t *context) {
    dumpCatalog(context, false);
    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_instrumentCatalogFullQ(scpi_t *context) {
    dumpCatalog(context, true);
    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_instrumentCoupleTrigger(scpi_t *context) {
    SCPI_ErrorPush(context, SCPI_ERROR_EXECUTION_ERROR);
    return SCPI_RES_ERR;
}

scpi_result_t scpi_cmd_instrumentCoupleTriggerQ(scpi_t *context) {
    SCPI_ErrorPush(context, SCPI_ERROR_EXECUTION_ERROR);
    return SCPI_RES_ERR;
}

} // namespace scpi
} // namespace psu
} // namespace eez
