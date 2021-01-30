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

#include <eez/modules/psu/channel_dispatcher.h>
#include <eez/modules/psu/scpi/psu.h>

namespace eez {
namespace psu {
namespace scpi {

////////////////////////////////////////////////////////////////////////////////

scpi_result_t scpi_cmd_measureScalarCurrentDcQ(scpi_t *context) {
    SlotAndSubchannelIndex slotAndSubchannelIndex;
    if (!getChannelFromParam(context, slotAndSubchannelIndex)) {
        return SCPI_RES_ERR;
    }

    int err;
    float value;
    if (!channel_dispatcher::getMeasuredCurrent(slotAndSubchannelIndex.slotIndex, slotAndSubchannelIndex.subchannelIndex, value, &err)) {
        SCPI_ErrorPush(context, err);
        return SCPI_RES_ERR;
    }

    char buffer[256] = { 0 };
    stringAppendFloat(buffer, sizeof(buffer), value);
    SCPI_ResultCharacters(context, buffer, strlen(buffer));

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_measureScalarPowerDcQ(scpi_t *context) {
    SlotAndSubchannelIndex slotAndSubchannelIndex;
    if (!getChannelFromParam(context, slotAndSubchannelIndex)) {
        return SCPI_RES_ERR;
    }

    int err;
    float voltage;
    if (!channel_dispatcher::getMeasuredVoltage(slotAndSubchannelIndex.slotIndex, slotAndSubchannelIndex.subchannelIndex, voltage, &err)) {
        SCPI_ErrorPush(context, err);
        return SCPI_RES_ERR;
    }

    float current;
    if (!channel_dispatcher::getMeasuredCurrent(slotAndSubchannelIndex.slotIndex, slotAndSubchannelIndex.subchannelIndex, current, &err)) {
        SCPI_ErrorPush(context, err);
        return SCPI_RES_ERR;
    }

    char buffer[256] = { 0 };
    stringAppendFloat(buffer, sizeof(buffer), voltage * current);
    SCPI_ResultCharacters(context, buffer, strlen(buffer));

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_measureScalarVoltageDcQ(scpi_t *context) {
    SlotAndSubchannelIndex slotAndSubchannelIndex;
    if (!getChannelFromParam(context, slotAndSubchannelIndex)) {
        return SCPI_RES_ERR;
    }

    int err;
    float value;
    if (!channel_dispatcher::getMeasuredVoltage(slotAndSubchannelIndex.slotIndex, slotAndSubchannelIndex.subchannelIndex, value, &err)) {
        SCPI_ErrorPush(context, err);
        return SCPI_RES_ERR;
    }

    char buffer[256] = { 0 };
    stringAppendFloat(buffer, sizeof(buffer), value);
    SCPI_ResultCharacters(context, buffer, strlen(buffer));

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_measureDigitalByteQ(scpi_t *context) {
    SlotAndSubchannelIndex slotAndSubchannelIndex;
    if (!getChannelFromParam(context, slotAndSubchannelIndex)) {
        return SCPI_RES_ERR;
    }

    uint8_t data;
    int err;
    if (!channel_dispatcher::getDigitalInputData(slotAndSubchannelIndex.slotIndex, slotAndSubchannelIndex.subchannelIndex, data, &err)) {
        SCPI_ErrorPush(context, err);
        return SCPI_RES_ERR;
    }

    SCPI_ResultUInt8(context, data);
    return SCPI_RES_OK;
}

} // namespace scpi
} // namespace psu
} // namespace eez