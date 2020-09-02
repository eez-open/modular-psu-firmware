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
#include <eez/modules/psu/io_pins.h>
#include <eez/modules/psu/profile.h>
#include <eez/modules/psu/scpi/psu.h>
#include <eez/modules/psu/trigger.h>
#include <eez/modules/psu/dlog_record.h>

namespace eez {
namespace psu {
namespace scpi {

////////////////////////////////////////////////////////////////////////////////

static scpi_choice_def_t sourceChoice[] = { { "BUS", trigger::SOURCE_BUS },
                                            { "IMMediate", trigger::SOURCE_IMMEDIATE },
                                            { "MANual", trigger::SOURCE_MANUAL },
                                            { "PIN1", trigger::SOURCE_PIN1 },
                                            { "PIN2", trigger::SOURCE_PIN2 },
                                            SCPI_CHOICE_LIST_END };

////////////////////////////////////////////////////////////////////////////////

scpi_result_t scpi_cmd_triggerSequenceImmediate(scpi_t *context) {
    int result = trigger::generateTrigger(trigger::SOURCE_IMMEDIATE);
    if (result != SCPI_RES_OK) {
        SCPI_ErrorPush(context, result);
        return SCPI_RES_ERR;
    }

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_triggerSequenceDelay(scpi_t *context) {
    float delay;
    if (!get_duration_param(context, delay, trigger::DELAY_MIN, trigger::DELAY_MAX,
                            trigger::DELAY_DEFAULT)) {
        return SCPI_RES_ERR;
    }

    trigger::setDelay(delay);

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_triggerSequenceDelayQ(scpi_t *context) {
    SCPI_ResultFloat(context, trigger::g_triggerDelay);
    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_triggerSequenceSource(scpi_t *context) {
    int32_t source;
    if (!SCPI_ParamChoice(context, sourceChoice, &source, true)) {
        return SCPI_RES_ERR;
    }

    trigger::setSource((trigger::Source)source);

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_triggerSequenceSourceQ(scpi_t *context) {
    resultChoiceName(context, sourceChoice, trigger::g_triggerSource);
    return SCPI_RES_OK;
}

static scpi_choice_def_t triggerOnListStopChoice[] = {
    { "OFF", TRIGGER_ON_LIST_STOP_OUTPUT_OFF },
    { "FIRSt", TRIGGER_ON_LIST_STOP_SET_TO_FIRST_STEP },
    { "LAST", TRIGGER_ON_LIST_STOP_SET_TO_LAST_STEP },
    { "STANdbay", TRIGGER_ON_LIST_STOP_STANDBY },
    SCPI_CHOICE_LIST_END /* termination of option list */
};

scpi_result_t scpi_cmd_triggerSequenceExitCondition(scpi_t *context) {
    int32_t triggerOnListStop;
    if (!SCPI_ParamChoice(context, triggerOnListStopChoice, &triggerOnListStop, true)) {
        return SCPI_RES_ERR;
    }

    Channel *channel = getPowerChannelFromParam(context);
    if (!channel) {
        return SCPI_RES_ERR;
    }

    if (!trigger::isIdle()) {
        SCPI_ErrorPush(context, SCPI_ERROR_CANNOT_CHANGE_TRANSIENT_TRIGGER);
        return SCPI_RES_ERR;
    }

    channel_dispatcher::setTriggerOnListStop(*channel, (TriggerOnListStop)triggerOnListStop);

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_triggerSequenceExitConditionQ(scpi_t *context) {
    Channel *channel = getPowerChannelFromParam(context);
    if (!channel) {
        return SCPI_RES_ERR;
    }

    resultChoiceName(context, triggerOnListStopChoice,
                     channel_dispatcher::getTriggerOnListStop(*channel));

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_initiateImmediate(scpi_t *context) {
    int result = trigger::initiate();
    if (result != SCPI_RES_OK) {
        SCPI_ErrorPush(context, result);
        return SCPI_RES_ERR;
    }

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_initiateContinuous(scpi_t *context) {
    bool enable;
    if (!SCPI_ParamBool(context, &enable, TRUE)) {
        return SCPI_RES_ERR;
    }

    int result = trigger::enableInitiateContinuous(enable);

    if (result != SCPI_RES_OK) {
        SCPI_ErrorPush(context, result);
        return SCPI_RES_ERR;
    }

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_initiateContinuousQ(scpi_t *context) {
    SCPI_ResultBool(context, trigger::g_triggerContinuousInitializationEnabled);
    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_abort(scpi_t *context) {
    trigger::abort();

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_coreTrg(scpi_t *context) {
    int result = trigger::generateTrigger(trigger::SOURCE_BUS);
    if (result != SCPI_RES_OK) {
        SCPI_ErrorPush(context, result);
        return SCPI_RES_ERR;
    }
    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_triggerDlogImmediate(scpi_t *context) {
    int result = dlog_record::startImmediately();
    if (result != SCPI_RES_OK) {
        SCPI_ErrorPush(context, result);
        return SCPI_RES_ERR;
    }
    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_triggerDlogSource(scpi_t *context) {
    int32_t source;
    if (!SCPI_ParamChoice(context, sourceChoice, &source, true)) {
        return SCPI_RES_ERR;
    }

    dlog_record::setTriggerSource((trigger::Source)source);

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_triggerDlogSourceQ(scpi_t *context) {
    resultChoiceName(context, sourceChoice, dlog_record::g_parameters.triggerSource);
    return SCPI_RES_OK;
}

} // namespace scpi
} // namespace psu
} // namespace eez