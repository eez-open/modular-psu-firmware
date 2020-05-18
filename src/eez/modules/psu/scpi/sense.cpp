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
#include <eez/modules/psu/profile.h>
#include <eez/modules/psu/scpi/psu.h>

namespace eez {
namespace psu {
namespace scpi {

////////////////////////////////////////////////////////////////////////////////

scpi_result_t scpi_cmd_senseCurrentDcRangeUpper(scpi_t *context) {
    CurrentRangeSelectionMode mode;

    scpi_number_t param;
    if (!SCPI_ParamNumber(context, scpi_special_numbers_def, &param, true)) {
        return SCPI_RES_ERR;
    }
    if (param.special) {
        if (param.content.tag == SCPI_NUM_MIN) {
            mode = CURRENT_RANGE_SELECTION_ALWAYS_LOW;
        } else if (param.content.tag == SCPI_NUM_MAX) {
            mode = CURRENT_RANGE_SELECTION_ALWAYS_HIGH;
        } else if (param.content.tag == SCPI_NUM_DEF) {
            mode = CURRENT_RANGE_SELECTION_USE_BOTH;
        } else {
            SCPI_ErrorPush(context, SCPI_ERROR_ILLEGAL_PARAMETER_VALUE);
            return SCPI_RES_ERR;
        }
    } else {
        if (param.unit != SCPI_UNIT_NONE && param.unit != SCPI_UNIT_AMPER) {
            SCPI_ErrorPush(context, SCPI_ERROR_INVALID_SUFFIX);
            return SCPI_RES_ERR;
        }

        float value = (float)param.content.value;
        if (value == 0.05f) {
            mode = CURRENT_RANGE_SELECTION_ALWAYS_LOW;
        } else if (value == 5.0f) {
            mode = CURRENT_RANGE_SELECTION_ALWAYS_HIGH;
        } else {
            SCPI_ErrorPush(context, SCPI_ERROR_ILLEGAL_PARAMETER_VALUE);
            return SCPI_RES_ERR;
        }
    }

    Channel *channel = param_channel(context);
    if (!channel) {
        return SCPI_RES_ERR;
    }

    if (!channel->hasSupportForCurrentDualRange()) {
        SCPI_ErrorPush(context, SCPI_ERROR_HARDWARE_MISSING);
        return SCPI_RES_ERR;
    }

    if (channel->flags.trackingEnabled) {
        SCPI_ErrorPush(context, SCPI_ERROR_EXECUTION_ERROR);
        return SCPI_RES_ERR;
    }

    channel_dispatcher::setCurrentRangeSelectionMode(*channel, mode);

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_senseCurrentDcRangeUpperQ(scpi_t *context) {
    Channel *channel = param_channel(context);
    if (!channel) {
        return SCPI_RES_ERR;
    }

    if (!channel->hasSupportForCurrentDualRange()) {
        SCPI_ErrorPush(context, SCPI_ERROR_HARDWARE_MISSING);
        return SCPI_RES_ERR;
    }

    if (channel->flags.trackingEnabled) {
        SCPI_ErrorPush(context, SCPI_ERROR_EXECUTION_ERROR);
        return SCPI_RES_ERR;
    }

    CurrentRangeSelectionMode mode = channel->getCurrentRangeSelectionMode();

    if (mode == CURRENT_RANGE_SELECTION_ALWAYS_LOW) {
        SCPI_ResultFloat(context, 0.05f);
    } else if (mode == CURRENT_RANGE_SELECTION_ALWAYS_HIGH) {
        SCPI_ResultFloat(context, 5);
    } else {
        SCPI_ResultText(context, "Default");
    }

    return SCPI_RES_OK;
}

} // namespace scpi
} // namespace psu
} // namespace eez
