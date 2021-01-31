/*
 * EEZ Modular Firmware
 * Copyright (C) 2020-present, Envox d.o.o.
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

scpi_result_t scpi_cmd_routeOpen(scpi_t *context) {
    ChannelList channelList;
    param_channels(context, channelList);
    if (channelList.numChannels == 0) {
        return SCPI_RES_ERR;
    }

    int err;
    if (!channel_dispatcher::routeOpen(channelList, &err)) {
        SCPI_ErrorPush(context, err);
        return SCPI_RES_ERR;
    }

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_routeOpenQ(scpi_t *context) {
    ChannelList channelList;
    param_channels(context, channelList);
    if (channelList.numChannels == 0) {
        return SCPI_RES_ERR;
    }

    int err;

    for (int i = 0; i < channelList.numChannels; i++) {
        bool isRouteOpen;

        if (!g_slots[channelList.channels[i].slotIndex]->isRouteOpen(channelList.channels[i].subchannelIndex, isRouteOpen, &err)) {
            SCPI_ErrorPush(context, err);
            return SCPI_RES_ERR;
        }

        SCPI_ResultBool(context, isRouteOpen);
    }

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_routeClose(scpi_t *context) {
    ChannelList channelList;
    param_channels(context, channelList);
    if (channelList.numChannels == 0) {
        return SCPI_RES_ERR;
    }

    int err;
    if (!channel_dispatcher::routeClose(channelList, &err)) {
        SCPI_ErrorPush(context, err);
        return SCPI_RES_ERR;
    }

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_routeCloseQ(scpi_t *context) {
    ChannelList channelList;
    param_channels(context, channelList);
    if (channelList.numChannels == 0) {
        return SCPI_RES_ERR;
    }

    int err;

    for (int i = 0; i < channelList.numChannels; i++) {
        bool isRouteOpen;

        if (!g_slots[channelList.channels[i].slotIndex]->isRouteOpen(channelList.channels[i].subchannelIndex, isRouteOpen, &err)) {
            SCPI_ErrorPush(context, err);
            return SCPI_RES_ERR;
        }

        SCPI_ResultBool(context, !isRouteOpen);
    }

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_routeLabelRow(scpi_t *context) {
    int32_t slotIndex;
    if (!SCPI_ParamInt(context, &slotIndex, TRUE)) {
        return SCPI_RES_ERR;
    }
    if (slotIndex < 1 || slotIndex > NUM_SLOTS) {
        SCPI_ErrorPush(context, SCPI_ERROR_ILLEGAL_PARAMETER_VALUE);
        return SCPI_RES_ERR;
    }
    slotIndex--;

    int32_t rowIndex;
    if (!SCPI_ParamInt(context, &rowIndex, TRUE)) {
        return SCPI_RES_ERR;
    }
    int err;
    int numRows;
    if (!g_slots[slotIndex]->getSwitchMatrixNumRows(numRows, &err)) {
        SCPI_ErrorPush(context, err);
        return SCPI_RES_ERR;
    }
    if (rowIndex < 1 || rowIndex > numRows) {
        SCPI_ErrorPush(context, SCPI_ERROR_ILLEGAL_PARAMETER_VALUE);
        return SCPI_RES_ERR;
    }
    rowIndex--;

    const char *str;
    size_t len;
    if (!SCPI_ParamCharacters(context, &str, &len, true)) {
        return SCPI_RES_ERR;
    }
    if (len > Module::MAX_SWITCH_MATRIX_LABEL_LENGTH) {
        SCPI_ErrorPush(context, SCPI_ERROR_TOO_MUCH_DATA);
        return SCPI_RES_ERR;
    }
    if (len < 1) {
        SCPI_ErrorPush(context, SCPI_ERROR_INVALID_STRING_DATA);
        return SCPI_RES_ERR;
    }

    char label[Module::MAX_SWITCH_MATRIX_LABEL_LENGTH + 1];
    memcpy(label, str, len);
    label[len] = 0;

    if (!g_slots[slotIndex]->setSwitchMatrixRowLabel(rowIndex, label, &err)) {
        SCPI_ErrorPush(context, err);
        return SCPI_RES_ERR;
    }

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_routeLabelRowQ(scpi_t *context) {
    int32_t slotIndex;
    if (!SCPI_ParamInt(context, &slotIndex, TRUE)) {
        return SCPI_RES_ERR;
    }
    if (slotIndex < 1 || slotIndex > NUM_SLOTS) {
        SCPI_ErrorPush(context, SCPI_ERROR_ILLEGAL_PARAMETER_VALUE);
        return SCPI_RES_ERR;
    }
    slotIndex--;

    int32_t rowIndex;
    if (!SCPI_ParamInt(context, &rowIndex, TRUE)) {
        return SCPI_RES_ERR;
    }
    int err;
    int numRows;
    if (!g_slots[slotIndex]->getSwitchMatrixNumRows(numRows, &err)) {
        SCPI_ErrorPush(context, err);
        return SCPI_RES_ERR;
    }
    if (rowIndex < 1 || rowIndex > numRows) {
        SCPI_ErrorPush(context, SCPI_ERROR_ILLEGAL_PARAMETER_VALUE);
        return SCPI_RES_ERR;
    }
    rowIndex--;

    char label[Module::MAX_SWITCH_MATRIX_LABEL_LENGTH + 1];
    if (!g_slots[slotIndex]->getSwitchMatrixRowLabel(rowIndex, label, sizeof(label), &err)) {
        SCPI_ErrorPush(context, err);
        return SCPI_RES_ERR;
    }

    SCPI_ResultText(context, label);

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_routeLabelColumn(scpi_t *context) {
    int32_t slotIndex;
    if (!SCPI_ParamInt(context, &slotIndex, TRUE)) {
        return SCPI_RES_ERR;
    }
    if (slotIndex < 1 || slotIndex > NUM_SLOTS) {
        SCPI_ErrorPush(context, SCPI_ERROR_ILLEGAL_PARAMETER_VALUE);
        return SCPI_RES_ERR;
    }
    slotIndex--;

    int32_t columnIndex;
    if (!SCPI_ParamInt(context, &columnIndex, TRUE)) {
        return SCPI_RES_ERR;
    }
    int err;
    int numColumns;
    if (!g_slots[slotIndex]->getSwitchMatrixNumColumns(numColumns, &err)) {
        SCPI_ErrorPush(context, err);
        return SCPI_RES_ERR;
    }
    if (columnIndex < 1 || columnIndex > numColumns) {
        SCPI_ErrorPush(context, SCPI_ERROR_ILLEGAL_PARAMETER_VALUE);
        return SCPI_RES_ERR;
    }
    columnIndex--;

    const char *str;
    size_t len;
    if (!SCPI_ParamCharacters(context, &str, &len, true)) {
        return SCPI_RES_ERR;
    }
    if (len > Module::MAX_SWITCH_MATRIX_LABEL_LENGTH) {
        SCPI_ErrorPush(context, SCPI_ERROR_TOO_MUCH_DATA);
        return SCPI_RES_ERR;
    }
    if (len < 1) {
        SCPI_ErrorPush(context, SCPI_ERROR_INVALID_STRING_DATA);
        return SCPI_RES_ERR;
    }

    char label[Module::MAX_SWITCH_MATRIX_LABEL_LENGTH + 1];
    memcpy(label, str, len);
    label[len] = 0;

    if (!g_slots[slotIndex]->setSwitchMatrixColumnLabel(columnIndex, label, &err)) {
        SCPI_ErrorPush(context, err);
        return SCPI_RES_ERR;
    }

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_routeLabelColumnQ(scpi_t *context) {
    int32_t slotIndex;
    if (!SCPI_ParamInt(context, &slotIndex, TRUE)) {
        return SCPI_RES_ERR;
    }
    if (slotIndex < 1 || slotIndex > NUM_SLOTS) {
        SCPI_ErrorPush(context, SCPI_ERROR_ILLEGAL_PARAMETER_VALUE);
        return SCPI_RES_ERR;
    }
    slotIndex--;

    int32_t columnIndex;
    if (!SCPI_ParamInt(context, &columnIndex, TRUE)) {
        return SCPI_RES_ERR;
    }
    int err;
    int numColumns;
    if (!g_slots[slotIndex]->getSwitchMatrixNumColumns(numColumns, &err)) {
        SCPI_ErrorPush(context, err);
        return SCPI_RES_ERR;
    }
    if (columnIndex < 1 || columnIndex > numColumns) {
        SCPI_ErrorPush(context, SCPI_ERROR_ILLEGAL_PARAMETER_VALUE);
        return SCPI_RES_ERR;
    }
    columnIndex--;

    char label[Module::MAX_SWITCH_MATRIX_LABEL_LENGTH + 1];
    if (!g_slots[slotIndex]->getSwitchMatrixColumnLabel(columnIndex, label, sizeof(label), &err)) {
        SCPI_ErrorPush(context, err);
        return SCPI_RES_ERR;
    }

    SCPI_ResultText(context, label);

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_routeLabelChannel(scpi_t *context) {
    SCPI_ErrorPush(context, SCPI_ERROR_HARDWARE_MISSING);
    return SCPI_RES_ERR;
}

scpi_result_t scpi_cmd_routeLabelChannelQ(scpi_t *context) {
    SCPI_ErrorPush(context, SCPI_ERROR_HARDWARE_MISSING);
    return SCPI_RES_ERR;
}

} // namespace scpi
} // namespace psu
} // namespace eez
