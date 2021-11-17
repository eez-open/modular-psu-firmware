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
 
#pragma once

#include <bb3/psu/psu.h>

namespace eez {
namespace psu {
namespace scpi {

extern scpi_choice_def_t temp_sensor_choice[];
extern scpi_choice_def_t internal_external_choice[];
extern scpi_choice_def_t unitChoice[];

void param_channels(scpi_t *context, ChannelList &channelList, scpi_bool_t mandatory = FALSE, scpi_bool_t skip_channel_check = FALSE);
void param_channels(scpi_t *context, scpi_parameter_t *parameter, ChannelList &channelList, scpi_bool_t skip_channel_check = FALSE);

bool getChannelFromParam(scpi_t *context, SlotAndSubchannelIndex &slotAndSubchannelIndex, scpi_bool_t mandatory = FALSE);
bool getChannelFromCommandNumber(scpi_t *context, SlotAndSubchannelIndex &slotAndSubchannelIndex);

bool absoluteChannelIndexToSlotAndSubchannelIndex(int absoluteChannelIndex, SlotAndSubchannelIndex &slotAndSubchannelIndex);

SlotAndSubchannelIndex *getSelectedChannel(scpi_t *context);

Channel *getSelectedPowerChannel(scpi_t *context);
Channel *getPowerChannelFromParam(scpi_t *context, scpi_bool_t mandatory = FALSE, scpi_bool_t skip_channel_check = FALSE);
Channel *getPowerChannelFromCommandNumber(scpi_t *context);
bool checkPowerChannel(scpi_t *context, int channelIndex);

bool param_temp_sensor(scpi_t *context, int32_t &sensor);

bool get_voltage_param(scpi_t *context, float &value, const Channel *channel, const Channel::Value *cv);
bool get_voltage_param(scpi_t *context, float &value, int slotIndex, int subchannelIndex, const Channel::Value *cv);
bool get_voltage_protection_level_param(scpi_t *context, float &value, float min, float max, float def);
bool get_current_param(scpi_t *context, float &value, const Channel *channel, const Channel::Value *cv);
bool get_current_param(scpi_t *context, float &value, int slotIndex, int subchannelIndex, const Channel::Value *cv);
bool get_power_param(scpi_t *context, float &value, float min, float max, float def);
bool get_temperature_param(scpi_t *context, float &value, float min, float max, float def);
bool get_duration_param(scpi_t *context, float &value, float min, float max, float def);

bool get_voltage_from_param(scpi_t *context, const scpi_number_t &param, float &value, int slotIndex, int subchannelIndex, const Channel::Value *cv);
bool get_voltage_protection_level_from_param(scpi_t *context, const scpi_number_t &param, float &value, float min, float max, float def);
bool get_current_from_param(scpi_t *context, const scpi_number_t &param, float &value, int slotIndex, int subchannelIndex, const Channel::Value *cv);
bool get_power_from_param(scpi_t *context, const scpi_number_t &param, float &value, float min, float max, float def);
bool get_temperature_from_param(scpi_t *context, const scpi_number_t &param, float &value, float min, float max, float def);
bool get_duration_from_param(scpi_t *context, const scpi_number_t &param, float &value, float min, float max, float def);

bool get_voltage_limit_param(scpi_t *context, float &value, const Channel *channel, const Channel::Value *cv);
bool get_current_limit_param(scpi_t *context, float &value, const Channel *channel, const Channel::Value *cv);
bool get_power_limit_param(scpi_t *context, float &value, const Channel *channel, const Channel::Value *cv);
bool get_voltage_limit_from_param(scpi_t *context, const scpi_number_t &param, float &value, const Channel *channel, const Channel::Value *cv);
bool get_current_limit_from_param(scpi_t *context, const scpi_number_t &param, float &value, const Channel *channel, const Channel::Value *cv);
bool get_power_limit_from_param(scpi_t *context, const scpi_number_t &param, float &value, const Channel *channel, const Channel::Value *cv);

scpi_result_t get_source_value(scpi_t *context, Unit unit, float value, float min, float max, float def);

scpi_result_t result_float(scpi_t *context, float value, Unit unit);
bool get_profile_location_param(scpi_t *context, int &location, bool all_locations = false);

void outputOnTime(scpi_t* context, uint32_t time);
bool checkPassword(scpi_t *context, const char *againstPassword);

// returns current directory if parameter is not specified
bool getFilePath(scpi_t *context, char *filePath, bool mandatory, bool *isParameterSpecified = nullptr);

}
}
} // namespace eez::psu::scpi
