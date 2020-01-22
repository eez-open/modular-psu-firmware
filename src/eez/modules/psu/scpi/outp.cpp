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
#include <eez/modules/psu/profile.h>
#include <eez/modules/psu/scpi/psu.h>
#include <eez/modules/psu/trigger.h>

namespace eez {
namespace psu {
namespace scpi {

////////////////////////////////////////////////////////////////////////////////

scpi_result_t scpi_cmd_outputModeQ(scpi_t *context) {
    // TODO migrate to generic firmware
    Channel *channel = param_channel(context);
    if (!channel) {
        return SCPI_RES_ERR;
    }

    SCPI_ResultText(context, channel->getModeStr());

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_outputProtectionClear(scpi_t *context) {
    // TODO migrate to generic firmware
    Channel *channel = param_channel(context);
    if (!channel) {
        return SCPI_RES_ERR;
    }

    channel_dispatcher::clearProtection(*channel);

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_outputState(scpi_t *context) {
    // TODO migrate to generic firmware
    bool enable;
    if (!SCPI_ParamBool(context, &enable, TRUE)) {
        return SCPI_RES_ERR;
    }

    Channel *channel = param_channel(context);
    if (!channel) {
        return SCPI_RES_ERR;
    }

    int err;
    if (!channel_dispatcher::outputEnable(*channel, enable, &err)) {
        SCPI_ErrorPush(context, err);
        return SCPI_RES_ERR;
    }

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_outputStateQ(scpi_t *context) {
    // TODO migrate to generic firmware
    Channel *channel = param_channel(context);
    if (!channel) {
        return SCPI_RES_ERR;
    }

    SCPI_ResultBool(context, channel->isOutputEnabled());

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_outputTrackState(scpi_t *context) {
    // TODO migrate to generic firmware
    if (channel_dispatcher::getCouplingType() != channel_dispatcher::COUPLING_TYPE_NONE) {
        SCPI_ErrorPush(context, SCPI_ERROR_EXECUTE_ERROR_CHANNELS_ARE_COUPLED);
        return SCPI_RES_ERR;
    }

    bool enable;
    if (!SCPI_ParamBool(context, &enable, TRUE)) {
        return SCPI_RES_ERR;
    }

    Channel *channel = param_channel(context);
    if (!channel) {
        return SCPI_RES_ERR;
    }

    int err;
    if (!channel_dispatcher::isTrackingAllowed(*channel, &err)) {
        SCPI_ErrorPush(context, err);
        return SCPI_RES_ERR;
    }

    if (enable != channel->flags.trackingEnabled) {
        channel->flags.trackingEnabled = enable;
        profile::save();
    }

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_outputTrackStateQ(scpi_t *context) {
    // TODO migrate to generic firmware
    Channel *channel = param_channel(context);
    if (!channel) {
        return SCPI_RES_ERR;
    }

    SCPI_ResultBool(context, channel->flags.trackingEnabled ? true : false);

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_outputProtectionCouple(scpi_t *context) {
    // TODO migrate to generic firmware
    bool enable;
    if (!SCPI_ParamBool(context, &enable, TRUE)) {
        return SCPI_RES_ERR;
    }

    persist_conf::enableOutputProtectionCouple(enable);

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_outputProtectionCoupleQ(scpi_t *context) {
    // TODO migrate to generic firmware
    SCPI_ResultBool(context, persist_conf::isOutputProtectionCoupleEnabled());

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_outputStateTriggered(scpi_t *context) {
    // TODO migrate to generic firmware
    bool enable;
    if (!SCPI_ParamBool(context, &enable, TRUE)) {
        return SCPI_RES_ERR;
    }

    Channel *channel = param_channel(context);
    if (!channel) {
        return SCPI_RES_ERR;
    }

    channel_dispatcher::setTriggerOutputState(*channel, enable);

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_outputStateTriggeredQ(scpi_t *context) {
    // TODO migrate to generic firmware
    Channel *channel = param_channel(context);
    if (!channel) {
        return SCPI_RES_ERR;
    }

    SCPI_ResultBool(context, channel_dispatcher::getTriggerOutputState(*channel));

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_outputDprog(scpi_t *context) {
    bool enable;
    if (!SCPI_ParamBool(context, &enable, TRUE)) {
        return SCPI_RES_ERR;
    }

    Channel *channel = param_channel(context);
    if (!channel) {
        return SCPI_RES_ERR;
    }

    if (channel->params.features & CH_FEATURE_DPROG) {
        channel->setDprogState((DprogState)enable);
    } else {
        SCPI_ErrorPush(context, SCPI_ERROR_HARDWARE_MISSING);
        return SCPI_RES_ERR;
    }

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_outputDprogQ(scpi_t *context) {
    Channel *channel = param_channel(context);
    if (!channel) {
        return SCPI_RES_ERR;
    }

    if (channel->params.features & CH_FEATURE_DPROG) {
        SCPI_ResultBool(context, channel->flags.dprogState);
    } else {
        SCPI_ErrorPush(context, SCPI_ERROR_HARDWARE_MISSING);
        return SCPI_RES_ERR;
    }

    return SCPI_RES_OK;
}

} // namespace scpi
} // namespace psu
} // namespace eez
