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

} // namespace scpi
} // namespace psu
} // namespace eez
