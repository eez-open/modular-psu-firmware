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

static scpi_choice_def_t g_senseFunctionChoice[] = {
    { "CURRent", MEASURE_MODE_CURRENT },
    { "VOLTage", MEASURE_MODE_VOLTAGE },
    SCPI_CHOICE_LIST_END /* termination of option list */
};

scpi_result_t scpi_cmd_senseFunctionOn(scpi_t *context) {
    int32_t mode;
    if (!SCPI_ParamChoice(context, g_senseFunctionChoice, &mode, true)) {
        return SCPI_RES_ERR;
    }

    SlotAndSubchannelIndex slotAndSubchannelIndex;
    if (!getChannelFromParam(context, slotAndSubchannelIndex)) {
        return SCPI_RES_ERR;
    }

    int err;
    if (!channel_dispatcher::setMeasureMode(slotAndSubchannelIndex.slotIndex, slotAndSubchannelIndex.subchannelIndex, (MeasureMode)mode, &err)) {
        SCPI_ErrorPush(context, err);
        return SCPI_RES_ERR;
    }

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_senseFunctionOnQ(scpi_t *context) {
    SlotAndSubchannelIndex slotAndSubchannelIndex;
    if (!getChannelFromParam(context, slotAndSubchannelIndex)) {
        return SCPI_RES_ERR;
    }

    MeasureMode mode;
    int err;
    if (!channel_dispatcher::getMeasureMode(slotAndSubchannelIndex.slotIndex, slotAndSubchannelIndex.subchannelIndex, mode, &err)) {
        SCPI_ErrorPush(context, err);
        return SCPI_RES_ERR;
    }

    resultChoiceName(context, g_senseFunctionChoice, mode);

    return SCPI_RES_OK;
}

static scpi_choice_def_t currentRangeSelection[] = {
    { "LOW",  CURRENT_RANGE_SELECTION_ALWAYS_LOW },
    { "HIGH", CURRENT_RANGE_SELECTION_ALWAYS_HIGH },
    { "BEST", CURRENT_RANGE_SELECTION_USE_BOTH },
    SCPI_CHOICE_LIST_END /* termination of option list */
};

scpi_result_t scpi_cmd_senseCurrentDcRange(scpi_t *context) {
    CurrentRangeSelectionMode mode;

    scpi_parameter_t parameter;
    if (!SCPI_Parameter(context, &parameter, true)) {
        return SCPI_RES_ERR;
    }

    SlotAndSubchannelIndex slotAndSubchannelIndex;
    if (!getChannelFromParam(context, slotAndSubchannelIndex)) {
        return SCPI_RES_ERR;
    }

    Channel *channel = Channel::getBySlotIndex(slotAndSubchannelIndex.slotIndex, slotAndSubchannelIndex.subchannelIndex);
    if (channel) {
        if (SCPI_ParamIsNumber(&parameter, true)) {
            float value;
            if (!SCPI_ParamToFloat(context, &parameter, &value)) {
                return SCPI_RES_ERR;
            }
            if (value == 0.05f) {
                mode = CURRENT_RANGE_SELECTION_ALWAYS_LOW;
            } else if (value == 5.0f) {
                mode = CURRENT_RANGE_SELECTION_ALWAYS_HIGH;
            } else {
                SCPI_ErrorPush(context, SCPI_ERROR_ILLEGAL_PARAMETER_VALUE);
                return SCPI_RES_ERR;
            }
        } else {
            int32_t modeInt;
            if (!SCPI_ParamToChoice(context, &parameter, currentRangeSelection, &modeInt)) {
                return SCPI_RES_ERR;
            }
            mode = (CurrentRangeSelectionMode)modeInt;
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
    } else {
        int32_t range;
        if (!SCPI_ParamToInt32(context, &parameter, &range)) {
			SCPI_ErrorPush(context, SCPI_ERROR_ILLEGAL_PARAMETER_VALUE);
            return SCPI_RES_ERR;
        }

        if (range < 0 || range > 255) {
            SCPI_ErrorPush(context, SCPI_ERROR_ILLEGAL_PARAMETER_VALUE);
            return SCPI_RES_ERR;
        }

        int err;
        if (!channel_dispatcher::setMeasureCurrentRange(slotAndSubchannelIndex.slotIndex, slotAndSubchannelIndex.subchannelIndex, range, &err)) {
            SCPI_ErrorPush(context, err);
            return SCPI_RES_ERR;
        }
    }

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_senseCurrentDcRangeQ(scpi_t *context) {
    SlotAndSubchannelIndex slotAndSubchannelIndex;
    if (!getChannelFromParam(context, slotAndSubchannelIndex)) {
        return SCPI_RES_ERR;
    }

    Channel *channel = Channel::getBySlotIndex(slotAndSubchannelIndex.slotIndex, slotAndSubchannelIndex.subchannelIndex);
    if (channel) {
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
            SCPI_ResultText(context, "BEST");
        }
    } else {
        uint8_t range;
        int err;
        if (!channel_dispatcher::getMeasureCurrentRange(slotAndSubchannelIndex.slotIndex, slotAndSubchannelIndex.subchannelIndex, range, &err)) {
            SCPI_ErrorPush(context, err);
            return SCPI_RES_ERR;
        }

        SCPI_ResultUInt8(context, range);
    }

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_senseVoltageDcRange(scpi_t *context) {
    int32_t range;
    if (!SCPI_ParamInt32(context, &range, true)) {
        return SCPI_RES_ERR;
    }

    if (range < 0 || range > 255) {
        SCPI_ErrorPush(context, SCPI_ERROR_ILLEGAL_PARAMETER_VALUE);
        return SCPI_RES_ERR;
    }

    SlotAndSubchannelIndex slotAndSubchannelIndex;
    if (!getChannelFromParam(context, slotAndSubchannelIndex)) {
        return SCPI_RES_ERR;
    }

    int err;
    if (!channel_dispatcher::setMeasureVoltageRange(slotAndSubchannelIndex.slotIndex, slotAndSubchannelIndex.subchannelIndex, range, &err)) {
        SCPI_ErrorPush(context, err);
        return SCPI_RES_ERR;
    }

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_senseVoltageDcRangeQ(scpi_t *context) {
    SlotAndSubchannelIndex slotAndSubchannelIndex;
    if (!getChannelFromParam(context, slotAndSubchannelIndex)) {
        return SCPI_RES_ERR;
    }

    uint8_t range;
    int err;
    if (!channel_dispatcher::getMeasureVoltageRange(slotAndSubchannelIndex.slotIndex, slotAndSubchannelIndex.subchannelIndex, range, &err)) {
        SCPI_ErrorPush(context, err);
        return SCPI_RES_ERR;
    }

    SCPI_ResultUInt8(context, range);

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_senseCurrentDcNplcycles(scpi_t *context) {
    int32_t numPowerLineCycles;
    if (!SCPI_ParamInt32(context, &numPowerLineCycles, true)) {
        return SCPI_RES_ERR;
    }

    if (numPowerLineCycles < 0) {
        SCPI_ErrorPush(context, SCPI_ERROR_ILLEGAL_PARAMETER_VALUE);
        return SCPI_RES_ERR;
    }

    SlotAndSubchannelIndex slotAndSubchannelIndex;
    if (!getChannelFromParam(context, slotAndSubchannelIndex)) {
        return SCPI_RES_ERR;
    }

    int err;
    if (!channel_dispatcher::setMeasureCurrentNumPowerLineCylces(slotAndSubchannelIndex.slotIndex, slotAndSubchannelIndex.subchannelIndex, numPowerLineCycles, &err)) {
        SCPI_ErrorPush(context, err);
        return SCPI_RES_ERR;
    }

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_senseCurrentDcNplcyclesQ(scpi_t *context) {
    SlotAndSubchannelIndex slotAndSubchannelIndex;
    if (!getChannelFromParam(context, slotAndSubchannelIndex)) {
        return SCPI_RES_ERR;
    }

    uint8_t numPowerLineCycles;
    int err;
    if (!channel_dispatcher::getMeasureCurrentNumPowerLineCycles(slotAndSubchannelIndex.slotIndex, slotAndSubchannelIndex.subchannelIndex, numPowerLineCycles, &err)) {
        SCPI_ErrorPush(context, err);
        return SCPI_RES_ERR;
    }

    SCPI_ResultUInt8(context, numPowerLineCycles);

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_senseCurrentDcApertureQ(scpi_t *context) {
    SlotAndSubchannelIndex slotAndSubchannelIndex;
    if (!getChannelFromParam(context, slotAndSubchannelIndex)) {
        return SCPI_RES_ERR;
    }

    uint8_t numPowerLineCycles;
    int err;
    if (!channel_dispatcher::getMeasureCurrentNumPowerLineCycles(slotAndSubchannelIndex.slotIndex, slotAndSubchannelIndex.subchannelIndex, numPowerLineCycles, &err)) {
        SCPI_ErrorPush(context, err);
        return SCPI_RES_ERR;
    }

    SCPI_ResultFloat(context, numPowerLineCycles / persist_conf::getPowerLineFrequency());

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_senseVoltageDcNplcycles(scpi_t *context) {
    int32_t numPowerLineCycles;
    if (!SCPI_ParamInt32(context, &numPowerLineCycles, true)) {
        return SCPI_RES_ERR;
    }

    if (numPowerLineCycles < 0) {
        SCPI_ErrorPush(context, SCPI_ERROR_ILLEGAL_PARAMETER_VALUE);
        return SCPI_RES_ERR;
    }

    SlotAndSubchannelIndex slotAndSubchannelIndex;
    if (!getChannelFromParam(context, slotAndSubchannelIndex)) {
        return SCPI_RES_ERR;
    }

    int err;
    if (!channel_dispatcher::setMeasureVoltageNumPowerLineCylces(slotAndSubchannelIndex.slotIndex, slotAndSubchannelIndex.subchannelIndex, numPowerLineCycles, &err)) {
        SCPI_ErrorPush(context, err);
        return SCPI_RES_ERR;
    }

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_senseVoltageDcNplcyclesQ(scpi_t *context) {
    SlotAndSubchannelIndex slotAndSubchannelIndex;
    if (!getChannelFromParam(context, slotAndSubchannelIndex)) {
        return SCPI_RES_ERR;
    }

    uint8_t numPowerLineCycles;
    int err;
    if (!channel_dispatcher::getMeasureVoltageNumPowerLineCycles(slotAndSubchannelIndex.slotIndex, slotAndSubchannelIndex.subchannelIndex, numPowerLineCycles, &err)) {
        SCPI_ErrorPush(context, err);
        return SCPI_RES_ERR;
    }

    SCPI_ResultUInt8(context, numPowerLineCycles);

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_senseVoltageDcApertureQ(scpi_t *context) {
    SlotAndSubchannelIndex slotAndSubchannelIndex;
    if (!getChannelFromParam(context, slotAndSubchannelIndex)) {
        return SCPI_RES_ERR;
    }

    uint8_t numPowerLineCycles;
    int err;
    if (!channel_dispatcher::getMeasureVoltageNumPowerLineCycles(slotAndSubchannelIndex.slotIndex, slotAndSubchannelIndex.subchannelIndex, numPowerLineCycles, &err)) {
        SCPI_ErrorPush(context, err);
        return SCPI_RES_ERR;
    }

    SCPI_ResultFloat(context, numPowerLineCycles / persist_conf::getPowerLineFrequency());

    return SCPI_RES_OK;
}

} // namespace scpi
} // namespace psu
} // namespace eez
