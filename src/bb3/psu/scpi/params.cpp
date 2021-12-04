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

#include <bb3/psu/psu.h>

#include <bb3/psu/channel_dispatcher.h>
#include <bb3/psu/scpi/psu.h>
#include <bb3/psu/temp_sensor.h>
#include <bb3/psu/ontime.h>

namespace eez {
namespace psu {
namespace scpi {

////////////////////////////////////////////////////////////////////////////////

scpi_choice_def_t channelChoice[] = {
    { "CH1", 1 }, 
    { "CH2", 2 }, 
    { "CH3", 3 }, 
    { "CH4", 4 }, 
    { "CH5", 5 }, 
    { "CH6", 6 },
    SCPI_CHOICE_LIST_END /* termination of option list */
};

scpi_choice_def_t channelChoiceWithAll[] = {
    { "ALL", 0 },
    { "CH1", 1 }, 
    { "CH2", 2 }, 
    { "CH3", 3 }, 
    { "CH4", 4 }, 
    { "CH5", 5 }, 
    { "CH6", 6 },
    SCPI_CHOICE_LIST_END /* termination of option list */
};

#define TEMP_SENSOR(NAME, QUES_REG_BIT, SCPI_ERROR) { #NAME, temp_sensor::NAME }
scpi_choice_def_t temp_sensor_choice[] = {
    TEMP_SENSORS, SCPI_CHOICE_LIST_END /* termination of option list */
};
#undef TEMP_SENSOR

scpi_choice_def_t internal_external_choice[] = {
    { "INTernal", 0 }, { "EXTernal", 1 }, SCPI_CHOICE_LIST_END /* termination of option list */
};

////////////////////////////////////////////////////////////////////////////////

// returns selected channel if number of selected channels is 1
SlotAndSubchannelIndex *getSelectedChannel(scpi_t *context) {
    scpi_psu_t *psu_context = (scpi_psu_t *)context->user_context;
    if (psu_context->selectedChannels.numChannels == 1) {
        return &psu_context->selectedChannels.channels[0];
    }
    return nullptr;
}

void param_channels(scpi_t *context, ChannelList &channelList, scpi_bool_t mandatory, scpi_bool_t skip_channel_check) {
    scpi_parameter_t parameter;
    if (!SCPI_Parameter(context, &parameter, mandatory)) {
        channelList.numChannels = 0;
        if (mandatory || SCPI_ParamErrorOccurred(context)) {
            return;
        }
        auto selectedChannel = getSelectedChannel(context);
        if (!selectedChannel) {
            return;
        }
        if (!skip_channel_check) {
            auto channel = Channel::getBySlotIndex(selectedChannel->slotIndex, selectedChannel->subchannelIndex);
            if (channel) {
                if (!checkPowerChannel(context, channel->channelIndex)) {
                    return;
                }
            }
        }
        channelList.numChannels = 1;
        channelList.channels[0].slotIndex = selectedChannel->slotIndex;
        channelList.channels[0].subchannelIndex = selectedChannel->subchannelIndex;
    } else {
        param_channels(context, &parameter, channelList, skip_channel_check);
    }
}

int getNumSlotChannels(int slotIndex) {
    return g_slots[slotIndex]->getNumSubchannels();
}

bool isValidSlotAndSubchannelIndex(int slotIndex, int subchannelIndex) {
    return g_slots[slotIndex]->isValidSubchannelIndex(subchannelIndex);
}

bool absoluteChannelIndexToSlotAndSubchannelIndex(int absoluteChannelIndex, SlotAndSubchannelIndex &slotAndSubchannelIndex) {
    if (absoluteChannelIndex >= 101) {
        int slotIndex = absoluteChannelIndex / 100;
        if (slotIndex > NUM_SLOTS) {
            return false;
        }
        slotIndex--;

        int subchannelIndex = absoluteChannelIndex % 100 - 1;

        if (!isValidSlotAndSubchannelIndex(slotIndex, subchannelIndex)) {
            return false;
        }

        slotAndSubchannelIndex.slotIndex = slotIndex;
        slotAndSubchannelIndex.subchannelIndex = subchannelIndex;

        return true;
    }
        
    if (absoluteChannelIndex < CH_NUM) {
        Channel &channel = Channel::get(absoluteChannelIndex);
        slotAndSubchannelIndex.slotIndex = channel.slotIndex;
        slotAndSubchannelIndex.subchannelIndex = channel.subchannelIndex;
        return true;
    }

    return false;
}

void param_channels(scpi_t *context, scpi_parameter_t *parameter, ChannelList &channelList, scpi_bool_t skip_channel_check) {
    channelList.numChannels = 0;

    if (parameter->type == SCPI_TOKEN_PROGRAM_EXPRESSION) {
        bool isRange;
        int32_t valueFrom;
        int32_t valueTo;
        for (int i = 0; ; i++) {
            scpi_expr_result_t result = SCPI_ExprNumericListEntryInt(context, parameter, i, &isRange, &valueFrom, &valueTo);
            if (result == SCPI_EXPR_OK) {
                if (isRange) {
                    SlotAndSubchannelIndex slotAndSubchannelIndex;

                    // from must be valid channel number
                    if (!absoluteChannelIndexToSlotAndSubchannelIndex(valueFrom, slotAndSubchannelIndex)) {
                        channelList.numChannels = 0;
                        return;
                    }

                    // to must be valid channel number
                    if (!absoluteChannelIndexToSlotAndSubchannelIndex(valueTo, slotAndSubchannelIndex)) {
                        channelList.numChannels = 0;
                        return;
                    }

                    for (int value = valueFrom; value <= valueTo; value++) {
                        if (absoluteChannelIndexToSlotAndSubchannelIndex(value, slotAndSubchannelIndex)) {
                            if (channelList.numChannels == MAX_NUM_CH_IN_CH_LIST) {
                                channelList.numChannels = 0;
                                return;
                            }
                            channelList.channels[channelList.numChannels] = slotAndSubchannelIndex;
                            channelList.numChannels++;
                        }
                    }
                } else {
                    SlotAndSubchannelIndex slotAndSubchannelIndex;
                    if (!absoluteChannelIndexToSlotAndSubchannelIndex(valueFrom, slotAndSubchannelIndex)) {
                        channelList.numChannels = 0;
                        return;
                    }

                    if (channelList.numChannels == MAX_NUM_CH_IN_CH_LIST) {
                        channelList.numChannels = 0;
                        return;
                    }

                    channelList.channels[channelList.numChannels] = slotAndSubchannelIndex;
                    channelList.numChannels++;
                }
            } else if (result == SCPI_EXPR_NO_MORE) {
                break;
            } else {
                channelList.numChannels = 0;
                return;
            }
        }
    } else {
        int32_t absoluteChannelIndex;
        if (!SCPI_ParamToChoice(context, parameter, channelChoiceWithAll, &absoluteChannelIndex)) {
            channelList.numChannels = 0;
            return;
        }

        if (absoluteChannelIndex > 0) {
            absoluteChannelIndex--;

            SlotAndSubchannelIndex slotAndSubchannelIndex;
            if (!absoluteChannelIndexToSlotAndSubchannelIndex(absoluteChannelIndex, slotAndSubchannelIndex)) {
                channelList.numChannels = 0;
                return;
            }

            channelList.channels[0] = slotAndSubchannelIndex;
            channelList.numChannels = 1;

        } else {
            // all power channels
            for (int channelIndex = 0; channelIndex < CH_NUM; channelIndex++) {
                auto &channel = Channel::get(channelIndex);
                channelList.channels[channelIndex].slotIndex = channel.slotIndex;
                channelList.channels[channelIndex].subchannelIndex = channel.subchannelIndex;
            }
            channelList.numChannels = CH_NUM;
        }
    }

    if (!skip_channel_check) {
        for (int i = 0; i < channelList.numChannels; i++) {
            auto channel = Channel::getBySlotIndex(channelList.channels[i].slotIndex, channelList.channels[i].subchannelIndex);
            if (channel) {
                if (!checkPowerChannel(context, channel->channelIndex)) {
                    channelList.numChannels = 0;
                    return;
                }
            }
        }
    }
}

bool getChannelFromParam(scpi_t *context, SlotAndSubchannelIndex &slotAndSubchannelIndex, scpi_bool_t mandatory) {
    ChannelList channelList;
    param_channels(context, channelList, mandatory, FALSE);
    if (channelList.numChannels == 1) {
        slotAndSubchannelIndex = channelList.channels[0];
        return true;
    }
    SCPI_ErrorPush(context, SCPI_ERROR_HARDWARE_MISSING);
    return false;
}

bool getChannelFromCommandNumber(scpi_t *context, SlotAndSubchannelIndex &slotAndSubchannelIndex) {
    int32_t absoluteChannelIndex;
    SCPI_CommandNumbers(context, &absoluteChannelIndex, 1, 0);
    if (absoluteChannelIndex != 0) {
        absoluteChannelIndex--;
        if (!absoluteChannelIndexToSlotAndSubchannelIndex(absoluteChannelIndex, slotAndSubchannelIndex)) {
            SCPI_ErrorPush(context, SCPI_ERROR_HEADER_SUFFIX_OUTOFRANGE);
            return false;
        }
        return true;
    } else {
        auto selectedChannel = getSelectedChannel(context);
        if (selectedChannel) {
            slotAndSubchannelIndex = *selectedChannel;
            return true;
        }
    }

    return false;
}

Channel *getSelectedPowerChannel(scpi_t *context) {
    auto selectedChannel = getSelectedChannel(context);
    if (selectedChannel) {
        return Channel::getBySlotIndex(selectedChannel->slotIndex, selectedChannel->subchannelIndex);
    }
    return nullptr;
}

Channel *getPowerChannelFromParam(scpi_t *context, scpi_bool_t mandatory, scpi_bool_t skip_channel_check) {
    ChannelList channelList;
    param_channels(context, channelList, mandatory, skip_channel_check);
    if (channelList.numChannels == 1) {
        return Channel::getBySlotIndex(channelList.channels[0].slotIndex, channelList.channels[0].subchannelIndex);
    }
    return nullptr;
}

Channel *getPowerChannelFromCommandNumber(scpi_t *context) {
    Channel *channel;

    int32_t absoluteChannelIndex;
    SCPI_CommandNumbers(context, &absoluteChannelIndex, 1, 0);
    if (absoluteChannelIndex != 0) {
        absoluteChannelIndex--;
        SlotAndSubchannelIndex slotAndSubchannelIndex;
        if (!absoluteChannelIndexToSlotAndSubchannelIndex(absoluteChannelIndex, slotAndSubchannelIndex)) {
            SCPI_ErrorPush(context, SCPI_ERROR_HEADER_SUFFIX_OUTOFRANGE);
            return nullptr;
        }
        channel = Channel::getBySlotIndex(slotAndSubchannelIndex.slotIndex, slotAndSubchannelIndex.subchannelIndex);
    } else {
        channel = getSelectedPowerChannel(context);
    }

    if (!channel) {
        SCPI_ErrorPush(context, SCPI_ERROR_HEADER_SUFFIX_OUTOFRANGE);
        return nullptr;
    }

    if (!checkPowerChannel(context, channel->channelIndex)) {
        return nullptr;
    }

    return channel;
}

bool checkPowerChannel(scpi_t *context, int channelIndex) {
    if (channelIndex < 0 || channelIndex > CH_NUM - 1) {
        SCPI_ErrorPush(context, SCPI_ERROR_CHANNEL_NOT_FOUND);
        return false;
    }

    if (!g_powerIsUp) {
        SCPI_ErrorPush(context, SCPI_ERROR_EXECUTION_ERROR);
        return false;
    }

    Channel &channel = Channel::get(channelIndex);

    if (channel.isTestFailed()) {
        SCPI_ErrorPush(context, SCPI_ERROR_HARDWARE_ERROR);
        return false;
    }

    if (!channel.flags.powerOk) {
        if (channelIndex < 6) {
            SCPI_ErrorPush(context, SCPI_ERROR_CH1_FAULT_DETECTED - channelIndex);
        } else {
            // TODO !!??
            SCPI_ErrorPush(context, SCPI_ERROR_CH1_FAULT_DETECTED);
        }
        return false;
    }

    if (!channel.isOk()) {
        SCPI_ErrorPush(context, SCPI_ERROR_EXECUTION_ERROR);
        return false;
    }

    return true;
}

bool param_temp_sensor(scpi_t *context, int32_t &sensor) {
    if (!SCPI_ParamChoice(context, temp_sensor_choice, &sensor, FALSE)) {
#if OPTION_AUX_TEMP_SENSOR
        if (SCPI_ParamErrorOccurred(context)) {
            return false;
        }
        sensor = temp_sensor::AUX;
#else
        SCPI_ErrorPush(context, SCPI_ERROR_HARDWARE_MISSING);
        return false;
#endif
    }

    if (!temp_sensor::sensors[sensor].isInstalled()) {
        SCPI_ErrorPush(context, SCPI_ERROR_HARDWARE_MISSING);
        return false;
    }

    return true;
}

bool get_voltage_param(scpi_t *context, float &value, const Channel *channel, const Channel::Value *cv) {
    scpi_number_t param;
    if (!SCPI_ParamNumber(context, scpi_special_numbers_def, &param, true)) {
        return false;
    }

    return get_voltage_from_param(context, param, value, channel->slotIndex, channel->subchannelIndex, cv);
}

bool get_voltage_param(scpi_t *context, float &value, int slotIndex, int subchannelIndex, const Channel::Value *cv) {
    scpi_number_t param;
    if (!SCPI_ParamNumber(context, scpi_special_numbers_def, &param, true)) {
        return false;
    }

    return get_voltage_from_param(context, param, value, slotIndex, subchannelIndex, cv);
}

bool get_voltage_protection_level_param(scpi_t *context, float &value, float min, float max, float def) {
    scpi_number_t param;
    if (!SCPI_ParamNumber(context, scpi_special_numbers_def, &param, true)) {
        return false;
    }

    return get_voltage_protection_level_from_param(context, param, value, min, max, def);
}

bool get_current_param(scpi_t *context, float &value, const Channel *channel, const Channel::Value *cv) {
    scpi_number_t param;
    if (!SCPI_ParamNumber(context, scpi_special_numbers_def, &param, true)) {
        return false;
    }

    return get_current_from_param(context, param, value, channel->slotIndex, channel->subchannelIndex, cv);
}

bool get_current_param(scpi_t *context, float &value, int slotIndex, int subchannelIndex, const Channel::Value *cv) {
    scpi_number_t param;
    if (!SCPI_ParamNumber(context, scpi_special_numbers_def, &param, true)) {
        return false;
    }

    return get_current_from_param(context, param, value, slotIndex, subchannelIndex, cv);
}

bool get_power_param(scpi_t *context, float &value, float min, float max, float def) {
    scpi_number_t param;
    if (!SCPI_ParamNumber(context, scpi_special_numbers_def, &param, true)) {
        return false;
    }

    return get_power_from_param(context, param, value, min, max, def);
}

bool get_temperature_param(scpi_t *context, float &value, float min, float max, float def) {
    scpi_number_t param;
    if (!SCPI_ParamNumber(context, scpi_special_numbers_def, &param, true)) {
        return false;
    }

    return get_temperature_from_param(context, param, value, min, max, def);
}

bool get_duration_param(scpi_t *context, float &value, float min, float max, float def) {
    scpi_number_t param;
    if (!SCPI_ParamNumber(context, scpi_special_numbers_def, &param, true)) {
        return false;
    }

    return get_duration_from_param(context, param, value, min, max, def);
}

bool get_voltage_from_param(scpi_t *context, const scpi_number_t &param, float &value, int slotIndex, int subchannelIndex, const Channel::Value *cv) {
    if (param.special) {
        if (param.content.tag == SCPI_NUM_MAX) {
            value = channel_dispatcher::getUMax(slotIndex, subchannelIndex);
        } else if (param.content.tag == SCPI_NUM_MIN) {
            value = channel_dispatcher::getUMin(slotIndex, subchannelIndex);
        } else if (param.content.tag == SCPI_NUM_DEF) {
            value = channel_dispatcher::getUDef(slotIndex, subchannelIndex);
        } else if (param.content.tag == SCPI_NUM_UP && cv) {
            value = channel_dispatcher::getUSet(slotIndex, subchannelIndex) + cv->step;
            if (value > channel_dispatcher::getUMax(slotIndex, subchannelIndex))
                value = channel_dispatcher::getUMax(slotIndex, subchannelIndex);
        } else if (param.content.tag == SCPI_NUM_DOWN && cv) {
            value = channel_dispatcher::getUSet(slotIndex, subchannelIndex) - cv->step;
            if (value < channel_dispatcher::getUMin(slotIndex, subchannelIndex))
                value = channel_dispatcher::getUMin(slotIndex, subchannelIndex);
        } else {
            SCPI_ErrorPush(context, SCPI_ERROR_ILLEGAL_PARAMETER_VALUE);
            return false;
        }
    } else {
        if (param.unit != SCPI_UNIT_NONE && param.unit != SCPI_UNIT_VOLT) {
            SCPI_ErrorPush(context, SCPI_ERROR_INVALID_SUFFIX);
            return false;
        }

        value = (float)param.content.value;

        if (value < channel_dispatcher::getUMin(slotIndex, subchannelIndex) || value > channel_dispatcher::getUMax(slotIndex, subchannelIndex)) {
            SCPI_ErrorPush(context, SCPI_ERROR_DATA_OUT_OF_RANGE);
            return false;
        }
    }

    return true;
}

bool get_voltage_protection_level_from_param(scpi_t *context, const scpi_number_t &param,
                                             float &value, float min, float max, float def) {
    if (param.special) {
        if (param.content.tag == SCPI_NUM_MAX) {
            value = max;
        } else if (param.content.tag == SCPI_NUM_MIN) {
            value = min;
        } else if (param.content.tag == SCPI_NUM_DEF) {
            value = def;
        } else {
            SCPI_ErrorPush(context, SCPI_ERROR_ILLEGAL_PARAMETER_VALUE);
            return false;
        }
    } else {
        if (param.unit != SCPI_UNIT_NONE && param.unit != SCPI_UNIT_VOLT) {
            SCPI_ErrorPush(context, SCPI_ERROR_INVALID_SUFFIX);
            return false;
        }

        value = (float)param.content.value;
        if (value < min || value > max) {
            SCPI_ErrorPush(context, SCPI_ERROR_DATA_OUT_OF_RANGE);
            return false;
        }
    }
    return true;
}

bool get_current_from_param(scpi_t *context, const scpi_number_t &param, float &value, int slotIndex, int subchannelIndex, const Channel::Value *cv) {
    if (param.special) {
        if (param.content.tag == SCPI_NUM_MAX) {
            value = channel_dispatcher::getIMax(slotIndex, subchannelIndex);
        } else if (param.content.tag == SCPI_NUM_MIN) {
            value = channel_dispatcher::getIMin(slotIndex, subchannelIndex);
        } else if (param.content.tag == SCPI_NUM_DEF) {
            value = channel_dispatcher::getIDef(slotIndex, subchannelIndex);
        } else if (param.content.tag == SCPI_NUM_UP && cv) {
            value = channel_dispatcher::getISet(slotIndex, subchannelIndex) + cv->step;
            if (value > channel_dispatcher::getIMax(slotIndex, subchannelIndex))
                value = channel_dispatcher::getIMax(slotIndex, subchannelIndex);
        } else if (param.content.tag == SCPI_NUM_DOWN && cv) {
            value = channel_dispatcher::getISet(slotIndex, subchannelIndex) - cv->step;
            if (value < channel_dispatcher::getIMin(slotIndex, subchannelIndex))
                value = channel_dispatcher::getIMin(slotIndex, subchannelIndex);
        } else {
            SCPI_ErrorPush(context, SCPI_ERROR_ILLEGAL_PARAMETER_VALUE);
            return false;
        }
    } else {
        if (param.unit != SCPI_UNIT_NONE && param.unit != SCPI_UNIT_AMPER) {
            SCPI_ErrorPush(context, SCPI_ERROR_INVALID_SUFFIX);
            return false;
        }

        value = (float)param.content.value;
        if (value < channel_dispatcher::getIMin(slotIndex, subchannelIndex) ||
            value > channel_dispatcher::getIMax(slotIndex, subchannelIndex)) {
            SCPI_ErrorPush(context, SCPI_ERROR_DATA_OUT_OF_RANGE);
            return false;
        }
    }

    return true;
}

bool get_power_from_param(scpi_t *context, const scpi_number_t &param, float &value, float min,
                          float max, float def) {
    if (param.special) {
        if (param.content.tag == SCPI_NUM_MAX) {
            value = max;
        } else if (param.content.tag == SCPI_NUM_MIN) {
            value = min;
        } else if (param.content.tag == SCPI_NUM_DEF) {
            value = def;
        } else {
            SCPI_ErrorPush(context, SCPI_ERROR_ILLEGAL_PARAMETER_VALUE);
            return false;
        }
    } else {
        if (param.unit != SCPI_UNIT_NONE) {
            SCPI_ErrorPush(context, SCPI_ERROR_INVALID_SUFFIX);
            return false;
        }

        value = (float)param.content.value;
        if (value < min || value > max) {
            SCPI_ErrorPush(context, SCPI_ERROR_DATA_OUT_OF_RANGE);
            return false;
        }
    }
    return true;
}

bool get_temperature_from_param(scpi_t *context, const scpi_number_t &param, float &value,
                                float min, float max, float def) {
    if (param.special) {
        if (param.content.tag == SCPI_NUM_MAX) {
            value = max;
        } else if (param.content.tag == SCPI_NUM_MIN) {
            value = min;
        } else if (param.content.tag == SCPI_NUM_DEF) {
            value = def;
        } else {
            SCPI_ErrorPush(context, SCPI_ERROR_ILLEGAL_PARAMETER_VALUE);
            return false;
        }
    } else {
        if (param.unit != SCPI_UNIT_NONE && param.unit != SCPI_UNIT_CELSIUS) {
            SCPI_ErrorPush(context, SCPI_ERROR_INVALID_SUFFIX);
            return false;
        }

        value = (float)param.content.value;
        if (value < min || value > max) {
            SCPI_ErrorPush(context, SCPI_ERROR_DATA_OUT_OF_RANGE);
            return false;
        }
    }

    return true;
}

bool get_duration_from_param(scpi_t *context, const scpi_number_t &param, float &value, float min,
                             float max, float def) {
    if (param.special) {
        if (param.content.tag == SCPI_NUM_MAX) {
            value = max;
        } else if (param.content.tag == SCPI_NUM_MIN) {
            value = min;
        } else if (param.content.tag == SCPI_NUM_DEF) {
            value = def;
        } else {
            SCPI_ErrorPush(context, SCPI_ERROR_ILLEGAL_PARAMETER_VALUE);
            return false;
        }
    } else {
        if (param.unit != SCPI_UNIT_NONE && param.unit != SCPI_UNIT_SECOND) {
            SCPI_ErrorPush(context, SCPI_ERROR_INVALID_SUFFIX);
            return false;
        }

        value = (float)param.content.value;
        if (value < min || value > max) {
            SCPI_ErrorPush(context, SCPI_ERROR_DATA_OUT_OF_RANGE);
            return false;
        }
    }

    return true;
}

bool get_voltage_limit_param(scpi_t *context, float &value, const Channel *channel,
                             const Channel::Value *cv) {
    scpi_number_t param;
    if (!SCPI_ParamNumber(context, scpi_special_numbers_def, &param, true)) {
        return false;
    }

    return get_voltage_limit_from_param(context, param, value, channel, cv);
}

bool get_current_limit_param(scpi_t *context, float &value, const Channel *channel,
                             const Channel::Value *cv) {
    scpi_number_t param;
    if (!SCPI_ParamNumber(context, scpi_special_numbers_def, &param, true)) {
        return false;
    }

    return get_current_limit_from_param(context, param, value, channel, cv);
}

bool get_power_limit_param(scpi_t *context, float &value, const Channel *channel,
                           const Channel::Value *cv) {
    scpi_number_t param;
    if (!SCPI_ParamNumber(context, scpi_special_numbers_def, &param, true)) {
        return false;
    }

    return get_power_limit_from_param(context, param, value, channel, cv);
}

bool get_voltage_limit_from_param(scpi_t *context, const scpi_number_t &param, float &value,
                                  const Channel *channel, const Channel::Value *cv) {
    if (context == NULL || channel == NULL) {
        SCPI_ErrorPush(context, SCPI_ERROR_ILLEGAL_PARAMETER_VALUE);
        return false;
    }
    if (param.special) {
        if (param.content.tag == SCPI_NUM_MAX) {
            value = channel_dispatcher::getUMaxLimit(*channel);
        } else if (param.content.tag == SCPI_NUM_MIN) {
            value = channel_dispatcher::getUMin(*channel);
        } else if (param.content.tag == SCPI_NUM_DEF) {
            value = channel_dispatcher::getUMaxLimit(*channel);
        } else {
            SCPI_ErrorPush(context, SCPI_ERROR_ILLEGAL_PARAMETER_VALUE);
            return false;
        }
    } else {
        if (param.unit != SCPI_UNIT_NONE && param.unit != SCPI_UNIT_VOLT) {
            SCPI_ErrorPush(context, SCPI_ERROR_INVALID_SUFFIX);
            return false;
        }

        value = (float)param.content.value;

        if (value < channel_dispatcher::getUMin(*channel) ||
            value > channel_dispatcher::getUMaxLimit(*channel)) {
            SCPI_ErrorPush(context, SCPI_ERROR_DATA_OUT_OF_RANGE);
            return false;
        }
    }

    return true;
}

bool get_current_limit_from_param(scpi_t *context, const scpi_number_t &param, float &value,
                                  const Channel *channel, const Channel::Value *cv) {
    if (context == NULL || channel == NULL) {
        SCPI_ErrorPush(context, SCPI_ERROR_ILLEGAL_PARAMETER_VALUE);
        return false;
    }
    if (param.special) {
        if (param.content.tag == SCPI_NUM_MAX) {
            value = channel_dispatcher::getIMaxLimit(*channel);
        } else if (param.content.tag == SCPI_NUM_MIN) {
            value = channel_dispatcher::getIMin(*channel);
        } else if (param.content.tag == SCPI_NUM_DEF) {
            value = channel_dispatcher::getIMaxLimit(*channel);
        } else {
            SCPI_ErrorPush(context, SCPI_ERROR_ILLEGAL_PARAMETER_VALUE);
            return false;
        }
    } else {
        if (param.unit != SCPI_UNIT_NONE && param.unit != SCPI_UNIT_AMPER) {
            SCPI_ErrorPush(context, SCPI_ERROR_INVALID_SUFFIX);
            return false;
        }

        value = (float)param.content.value;

        if (value < channel_dispatcher::getIMin(*channel) ||
            value > channel_dispatcher::getIMaxLimit(*channel)) {
            SCPI_ErrorPush(context, SCPI_ERROR_DATA_OUT_OF_RANGE);
            return false;
        }
    }

    return true;
}

bool get_power_limit_from_param(scpi_t *context, const scpi_number_t &param, float &value,
                                const Channel *channel, const Channel::Value *cv) {
    if (context == NULL || channel == NULL) {
        SCPI_ErrorPush(context, SCPI_ERROR_ILLEGAL_PARAMETER_VALUE);
        return false;
    }
    if (param.special) {
        if (param.content.tag == SCPI_NUM_MAX) {
            value = channel_dispatcher::getPowerMaxLimit(*channel);
        } else if (param.content.tag == SCPI_NUM_MIN) {
            value = channel_dispatcher::getPowerMinLimit(*channel);
        } else if (param.content.tag == SCPI_NUM_DEF) {
            value = channel_dispatcher::getPowerMaxLimit(*channel);
        }
    } else {
        if (param.unit != SCPI_UNIT_NONE && param.unit != SCPI_UNIT_WATT) {
            SCPI_ErrorPush(context, SCPI_ERROR_INVALID_SUFFIX);
            return false;
        }

        value = (float)param.content.value;

        if (value < channel_dispatcher::getPowerMinLimit(*channel) ||
            value > channel_dispatcher::getPowerMaxLimit(*channel)) {
            SCPI_ErrorPush(context, SCPI_ERROR_DATA_OUT_OF_RANGE);
            return false;
        }
    }

    return true;
}

scpi_result_t result_float(scpi_t *context, float value, Unit unit) {
    char buffer[32] = { 0 };
    stringAppendFloat(buffer, sizeof(buffer), value);
    SCPI_ResultCharacters(context, buffer, strlen(buffer));
    return SCPI_RES_OK;
}

bool get_profile_location_param(scpi_t *context, int &location, bool all_locations) {
    int32_t param;
    if (!SCPI_ParamInt(context, &param, true)) {
        return false;
    }

    if (param < (all_locations ? 0 : 1) || param > NUM_PROFILE_LOCATIONS - 1) {
        SCPI_ErrorPush(context, SCPI_ERROR_DATA_OUT_OF_RANGE);
        return false;
    }

    location = (int)param;

    return true;
}

void outputOnTime(scpi_t *context, uint32_t time) {
    char str[128];
    ontime::counterToString(str, sizeof(str), time);
    SCPI_ResultText(context, str);
}

bool checkPassword(scpi_t *context, const char *againstPassword) {
    const char *password;
    size_t len;

    if (!SCPI_ParamCharacters(context, &password, &len, true)) {
        return false;
    }

    size_t nPassword = strlen(againstPassword);
    if (nPassword != len || strncmp(password, againstPassword, len) != 0) {
        SCPI_ErrorPush(context, SCPI_ERROR_INVALID_PASSWORD);
        return false;
    }

    return true;
}

void cleanupPath(char *filePath) {
    char *q = filePath;

    for (char *p = filePath; *p; ++p) {
        if (*p == '\\') {
            *p = '/';
        }

        if (*p == '/' && (p > filePath && *(p - 1) == '/')) {
            // '//' -> '/'
            continue;
        } else if (*p == '.') {
            if (!*(p + 1)) {
                // '<...>/.' -> '<...>'
                break;
            } else if (*(p + 1) == '/') {
                // '<...>/./<...>' -> '<...>/<...>'
                ++p;
                continue;
            } else if (*(p + 1) == '.') {
                // '<...>/something/..<...>' -> '<...>/<...>'
                q -= 2;
                while (true) {
                    if (q < filePath) {
                        q = filePath;
                        break;
                    }
                    if (*q == '/') {
                        break;
                    }
                    --q;
                }
                ++p;
                continue;
            }
        }

        *q++ = *p;
    }

    // remove trailing '/' unless it is ':/'
    if (q > filePath && *(q - 1) == '/' && !(q - 1 > filePath && *(q - 2) == ':')) {
        --q;
    }

    // if empty then make it '/'
    if (q == filePath) {
        *q++ = '/';
    }

    *q = 0;
}

bool getFilePath(scpi_t *context, char *filePath, bool mandatory, bool *isParameterSpecified) {
    scpi_psu_t *psuContext = (scpi_psu_t *)context->user_context;

    const char *filePathParam;
    size_t filePathParamLen;
    if (SCPI_ParamCharacters(context, &filePathParam, &filePathParamLen, mandatory)) {
        if (filePathParamLen > MAX_PATH_LENGTH) {
            SCPI_ErrorPush(context, SCPI_ERROR_FILE_NAME_ERROR);
            return false;
        }

        // is it absolute file path?
        if (filePathParam[0] == '/' || filePathParam[0] == '\\' || filePathParam[1] == ':') {
            // yes
            memcpy(filePath, filePathParam, filePathParamLen);
            filePath[filePathParamLen] = 0;
        } else {
            // no, combine with current directory to get absolute path
            size_t currentDirectoryLen = strlen(psuContext->currentDirectory);
            size_t filePathLen = currentDirectoryLen + 1 + filePathParamLen;
            if (filePathLen > MAX_PATH_LENGTH) {
                SCPI_ErrorPush(context, SCPI_ERROR_FILE_NAME_ERROR);
                return false;
            }
            memcpy(filePath, psuContext->currentDirectory, currentDirectoryLen);
            filePath[currentDirectoryLen] = '/';
            memcpy(filePath + currentDirectoryLen + 1, filePathParam, filePathParamLen);
            filePath[filePathLen] = 0;
        }
        if (isParameterSpecified) {
            *isParameterSpecified = true;
        }
    } else {
        if (SCPI_ParamErrorOccurred(context)) {
            return false;
        }
        stringCopy(filePath, sizeof(filePath), psuContext->currentDirectory);
        if (isParameterSpecified) {
            *isParameterSpecified = false;
        }
    }

    cleanupPath(filePath);

    return true;
}

} // namespace scpi
} // namespace psu
} // namespace eez
