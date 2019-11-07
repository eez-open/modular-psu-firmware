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
#include <eez/modules/psu/temperature.h>

namespace eez {
namespace psu {
namespace scpi {

////////////////////////////////////////////////////////////////////////////////

scpi_result_t scpi_cmd_measureScalarCurrentDcQ(scpi_t *context) {
    // TODO migrate to generic firmware
    Channel *channel = param_channel(context);
    if (!channel) {
        return SCPI_RES_ERR;
    }

    char buffer[256] = { 0 };
    strcatCurrent(buffer, channel_dispatcher::getIMon(*channel));
    SCPI_ResultCharacters(context, buffer, strlen(buffer));

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_measureScalarPowerDcQ(scpi_t *context) {
    // TODO migrate to generic firmware
    Channel *channel = param_channel(context);
    if (!channel) {
        return SCPI_RES_ERR;
    }

    char buffer[256] = { 0 };
    strcatFloat(buffer, channel_dispatcher::getUMon(*channel) * channel_dispatcher::getIMon(*channel));
    SCPI_ResultCharacters(context, buffer, strlen(buffer));

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_measureScalarVoltageDcQ(scpi_t *context) {
    // TODO migrate to generic firmware
    Channel *channel = param_channel(context);
    if (!channel) {
        return SCPI_RES_ERR;
    }

    char buffer[256] = { 0 };
    strcatFloat(buffer, channel_dispatcher::getUMon(*channel));
    SCPI_ResultCharacters(context, buffer, strlen(buffer));

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_measureScalarTemperatureThermistorDcQ(scpi_t *context) {
    // TODO migrate to generic firmware
    int32_t sensor;
    if (!param_temp_sensor(context, sensor)) {
        return SCPI_RES_ERR;
    }

    char buffer[256] = { 0 };
    strcatFloat(buffer, temperature::sensors[sensor].measure());
    SCPI_ResultCharacters(context, buffer, strlen(buffer));

    return SCPI_RES_OK;
}

} // namespace scpi
} // namespace psu
} // namespace eez