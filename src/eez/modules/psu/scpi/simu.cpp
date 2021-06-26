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

#include <eez/modules/psu/scpi/psu.h>

#ifdef EEZ_PLATFORM_SIMULATOR

#include <eez/modules/psu/channel_dispatcher.h>
#include <eez/modules/psu/io_pins.h>

// SIMULATOR SPECIFC CONFIG
#define SIM_LOAD_MIN 0
#define SIM_LOAD_DEF 1000.0f
#define SIM_LOAD_MAX 10000000.0F

#define SIM_TEMP_MIN 0
#define SIM_TEMP_DEF 25.0f
#define SIM_TEMP_MAX 120.0f

namespace eez {
namespace psu {

using namespace simulator;

namespace scpi {

////////////////////////////////////////////////////////////////////////////////

bool get_resistance_from_param(scpi_t *context, const scpi_number_t &param, float &value) {
    if (param.special) {
        if (param.content.tag == SCPI_NUM_MAX) {
            value = SIM_LOAD_MAX;
        } else if (param.content.tag == SCPI_NUM_MIN) {
            value = SIM_LOAD_MIN;
        } else if (param.content.tag == SCPI_NUM_DEF) {
            value = SIM_LOAD_DEF;
        } else if (param.content.tag == SCPI_NUM_INF) {
            value = INFINITY;
        } else {
            SCPI_ErrorPush(context, SCPI_ERROR_ILLEGAL_PARAMETER_VALUE);
            return false;
        }
    } else {
        if (param.unit != SCPI_UNIT_NONE && param.unit != SCPI_UNIT_OHM) {
            SCPI_ErrorPush(context, SCPI_ERROR_INVALID_SUFFIX);
            return false;
        }

        value = (float)param.content.value;
        if (value < SIM_LOAD_MIN || value > SIM_LOAD_MAX) {
            SCPI_ErrorPush(context, SCPI_ERROR_DATA_OUT_OF_RANGE);
            return false;
        }
    }
    return true;
}

static bool get_resistance_param(scpi_t *context, float &value) {
    scpi_number_t param;
    if (!SCPI_ParamNumber(context, scpi_special_numbers_def, &param, true)) {
        return false;
    }

    return get_resistance_from_param(context, param, value);
}

////////////////////////////////////////////////////////////////////////////////

scpi_result_t scpi_cmd_simulatorLoadState(scpi_t *context) {
    bool state;
    if (!SCPI_ParamBool(context, &state, TRUE)) {
        return SCPI_RES_ERR;
    }

    Channel *channel = getPowerChannelFromParam(context, FALSE, TRUE);
    if (!channel) {
        return SCPI_RES_ERR;
    }

    channel_dispatcher::setLoadEnabled(*channel, state);

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_simulatorLoadStateQ(scpi_t *context) {
    Channel *channel = getPowerChannelFromParam(context, FALSE, TRUE);
    if (!channel) {
        return SCPI_RES_ERR;
    }

    SCPI_ResultBool(context, channel->simulator.getLoadEnabled());

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_simulatorLoad(scpi_t *context) {
    float value;
    if (!get_resistance_param(context, value)) {
        return SCPI_RES_ERR;
    }

    Channel *channel = getPowerChannelFromParam(context, FALSE, TRUE);
    if (!channel) {
        return SCPI_RES_ERR;
    }

    channel_dispatcher::setLoad(*channel, value);

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_simulatorLoadQ(scpi_t *context) {
    Channel *channel = getPowerChannelFromParam(context, FALSE, TRUE);
    if (!channel) {
        return SCPI_RES_ERR;
    }

    float value;

    int32_t spec;
    if (!SCPI_ParamChoice(context, scpi_special_numbers_def, &spec, false)) {
        if (SCPI_ParamErrorOccurred(context)) {
            return SCPI_RES_ERR;
        }

        value = channel->simulator.getLoad();
    } else {
        if (spec == SCPI_NUM_MIN) {
            value = SIM_LOAD_MIN;
        } else if (spec == SCPI_NUM_MAX) {
            value = SIM_LOAD_MAX;
        } else if (spec == SCPI_NUM_DEF) {
            value = SIM_LOAD_DEF;
        } else {
            SCPI_ErrorPush(context, SCPI_ERROR_ILLEGAL_PARAMETER_VALUE);
            return SCPI_RES_ERR;
        }
    }

    return result_float(context, value, UNIT_OHM);
}

scpi_result_t scpi_cmd_simulatorVoltageProgramExternal(scpi_t *context) {
    float value;
    if (!get_voltage_param(context, value, 0, 0)) {
        return SCPI_RES_ERR;
    }

    Channel *channel = getPowerChannelFromParam(context, FALSE, TRUE);
    if (!channel) {
        return SCPI_RES_ERR;
    }

    if (channel->channelIndex < 2 && channel_dispatcher::getCouplingType() != channel_dispatcher::COUPLING_TYPE_NONE) {
        SCPI_ErrorPush(context, SCPI_ERROR_EXECUTE_ERROR_CHANNELS_ARE_COUPLED);
        return SCPI_RES_ERR;
    }

    if (!(channel->params.features & CH_FEATURE_RPROG)) {
        SCPI_ErrorPush(context, SCPI_ERROR_HARDWARE_MISSING);
        return SCPI_RES_ERR;
    }

    channel->simulator.setVoltProgExt(value);

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_simulatorVoltageProgramExternalQ(scpi_t *context) {
    Channel *channel = getPowerChannelFromParam(context, FALSE, TRUE);
    if (!channel) {
        return SCPI_RES_ERR;
    }

    if (channel->channelIndex < 2 && channel_dispatcher::getCouplingType() != channel_dispatcher::COUPLING_TYPE_NONE) {
        SCPI_ErrorPush(context, SCPI_ERROR_EXECUTE_ERROR_CHANNELS_ARE_COUPLED);
        return SCPI_RES_ERR;
    }

    if (!(channel->params.features & CH_FEATURE_RPROG)) {
        SCPI_ErrorPush(context, SCPI_ERROR_HARDWARE_MISSING);
        return SCPI_RES_ERR;
    }

    return result_float(context, channel->simulator.getVoltProgExt(), UNIT_VOLT);
}

scpi_result_t scpi_cmd_simulatorPwrgood(scpi_t *context) {
    bool on;
    if (!SCPI_ParamBool(context, &on, TRUE)) {
        return SCPI_RES_ERR;
    }

    Channel *channel = getPowerChannelFromParam(context, FALSE, TRUE);
    if (!channel) {
        return SCPI_RES_ERR;
    }

    simulator::setPwrgood(channel->channelIndex, on);

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_simulatorPwrgoodQ(scpi_t *context) {
    Channel *channel = getPowerChannelFromParam(context, FALSE, TRUE);
    if (!channel) {
        return SCPI_RES_ERR;
    }

    SCPI_ResultBool(context, simulator::getPwrgood(channel->channelIndex));

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_simulatorRpol(scpi_t *context) {

    bool on;
    if (!SCPI_ParamBool(context, &on, TRUE)) {
        return SCPI_RES_ERR;
    }

    Channel *channel = getPowerChannelFromParam(context, FALSE, TRUE);
    if (!channel) {
        return SCPI_RES_ERR;
    }

    if (channel->channelIndex < 2 && channel_dispatcher::getCouplingType() == channel_dispatcher::COUPLING_TYPE_SERIES) {
        SCPI_ErrorPush(context, SCPI_ERROR_EXECUTE_ERROR_CHANNELS_ARE_COUPLED);
        return SCPI_RES_ERR;
    }

    if (channel->params.features & CH_FEATURE_RPOL) {
        SCPI_ErrorPush(context, SCPI_ERROR_HARDWARE_MISSING);
        return SCPI_RES_ERR;
    }

    simulator::setRPol(channel->channelIndex, on);

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_simulatorRpolQ(scpi_t *context) {
    Channel *channel = getPowerChannelFromParam(context, FALSE, TRUE);
    if (!channel) {
        return SCPI_RES_ERR;
    }

    if (channel->channelIndex < 2 && channel_dispatcher::getCouplingType() == channel_dispatcher::COUPLING_TYPE_SERIES) {
        SCPI_ErrorPush(context, SCPI_ERROR_EXECUTE_ERROR_CHANNELS_ARE_COUPLED);
        return SCPI_RES_ERR;
    }

    if (channel->params.features & CH_FEATURE_RPOL) {
        SCPI_ErrorPush(context, SCPI_ERROR_HARDWARE_MISSING);
        return SCPI_RES_ERR;
    }

    SCPI_ResultBool(context, simulator::getRPol(channel->channelIndex));

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_simulatorTemperature(scpi_t *context) {
    float value;
    if (!get_temperature_param(context, value, -100.0f, 200.0f, 25.0f)) {
        return SCPI_RES_ERR;
    }

    int32_t sensor;
    if (!param_temp_sensor(context, sensor)) {
        return SCPI_RES_ERR;
    }

    simulator::setTemperature(sensor, value);

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_simulatorTemperatureQ(scpi_t *context) {
    int32_t sensor;
    if (!param_temp_sensor(context, sensor)) {
        return SCPI_RES_ERR;
    }

    float value;

    int32_t spec;
    if (!SCPI_ParamChoice(context, scpi_special_numbers_def, &spec, false)) {
        if (SCPI_ParamErrorOccurred(context)) {
            return SCPI_RES_ERR;
        }

        value = simulator::getTemperature(sensor);
    } else {
        if (spec == SCPI_NUM_MIN) {
            value = SIM_TEMP_MIN;
        } else if (spec == SCPI_NUM_MAX) {
            value = SIM_TEMP_MAX;
        } else if (spec == SCPI_NUM_DEF) {
            value = SIM_TEMP_DEF;
        } else {
            SCPI_ErrorPush(context, SCPI_ERROR_ILLEGAL_PARAMETER_VALUE);
            return SCPI_RES_ERR;
        }
    }

    return result_float(context, value, UNIT_CELSIUS);
}

scpi_result_t scpi_cmd_simulatorGui(scpi_t *context) {
    SCPI_ErrorPush(context, SCPI_ERROR_EXECUTION_ERROR);
    return SCPI_RES_ERR;
}

scpi_result_t scpi_cmd_simulatorExit(scpi_t *context) {
    simulator::exit();

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_simulatorQuit(scpi_t *context) {
    return scpi_cmd_simulatorExit(context);
}

scpi_result_t scpi_cmd_simulatorPin1(scpi_t *context) {
    bool value;
    if (!SCPI_ParamBool(context, &value, TRUE)) {
        return SCPI_RES_ERR;
    }

    io_pins::ioPinWrite(EXT_TRIG1, value ? 1 : 0);

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_simulatorPin1Q(scpi_t *context) {
    SCPI_ResultBool(context, io_pins::ioPinRead(EXT_TRIG1) ? 1 : 0);
    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_simulatorPin2(scpi_t *context) {
    bool value;
    if (!SCPI_ParamBool(context, &value, TRUE)) {
        return SCPI_RES_ERR;
    }

    io_pins::ioPinWrite(EXT_TRIG2, value ? 1 : 0);

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_simulatorPin2Q(scpi_t *context) {
    SCPI_ResultBool(context, io_pins::ioPinRead(EXT_TRIG2) ? 1 : 0);
    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_simulatorDigitalDataByte(scpi_t *context) {
	SlotAndSubchannelIndex slotAndSubchannelIndex;
	if (!getChannelFromParam(context, slotAndSubchannelIndex)) {
		return SCPI_RES_ERR;
	}

	uint32_t data;
	if (!SCPI_ParamUInt32(context, &data, true)) {
		return SCPI_RES_ERR;
	}

	if (data > 255) {
		SCPI_ErrorPush(context, SCPI_ERROR_ILLEGAL_PARAMETER_VALUE);
		return SCPI_RES_ERR;
	}

	int err;
	if (!channel_dispatcher::setDigitalInputData(slotAndSubchannelIndex.slotIndex, slotAndSubchannelIndex.subchannelIndex, data, &err)) {
		SCPI_ErrorPush(context, err);
		return SCPI_RES_ERR;
	}

	return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_simulatorUart(scpi_t *context) {
	const char *text;
	size_t textLen;

	if (!SCPI_ParamCharacters(context, &text, &textLen, true)) {
		return SCPI_RES_ERR;
	}

	uart::simulatorPut(text, textLen);

	return SCPI_RES_OK;
}

} // namespace scpi
} // namespace psu
} // namespace eez

#else

namespace eez {
namespace psu {
namespace scpi {

scpi_result_t scpi_cmd_simulatorLoadState(scpi_t *context) {
    SCPI_ErrorPush(context, SCPI_ERROR_UNDEFINED_HEADER);
    return SCPI_RES_ERR;
}

scpi_result_t scpi_cmd_simulatorLoadStateQ(scpi_t *context) {
    SCPI_ErrorPush(context, SCPI_ERROR_UNDEFINED_HEADER);
    return SCPI_RES_ERR;
}

scpi_result_t scpi_cmd_simulatorLoad(scpi_t *context) {
    SCPI_ErrorPush(context, SCPI_ERROR_UNDEFINED_HEADER);
    return SCPI_RES_ERR;
}

scpi_result_t scpi_cmd_simulatorLoadQ(scpi_t *context) {
    SCPI_ErrorPush(context, SCPI_ERROR_UNDEFINED_HEADER);
    return SCPI_RES_ERR;
}

scpi_result_t scpi_cmd_simulatorVoltageProgramExternal(scpi_t *context) {
    SCPI_ErrorPush(context, SCPI_ERROR_UNDEFINED_HEADER);
    return SCPI_RES_ERR;
}

scpi_result_t scpi_cmd_simulatorVoltageProgramExternalQ(scpi_t *context) {
    SCPI_ErrorPush(context, SCPI_ERROR_UNDEFINED_HEADER);
    return SCPI_RES_ERR;
}

scpi_result_t scpi_cmd_simulatorPwrgood(scpi_t *context) {
    SCPI_ErrorPush(context, SCPI_ERROR_UNDEFINED_HEADER);
    return SCPI_RES_ERR;
}

scpi_result_t scpi_cmd_simulatorPwrgoodQ(scpi_t *context) {
    SCPI_ErrorPush(context, SCPI_ERROR_UNDEFINED_HEADER);
    return SCPI_RES_ERR;
}

scpi_result_t scpi_cmd_simulatorRpol(scpi_t *context) {
    SCPI_ErrorPush(context, SCPI_ERROR_UNDEFINED_HEADER);
    return SCPI_RES_ERR;
}

scpi_result_t scpi_cmd_simulatorRpolQ(scpi_t *context) {
    SCPI_ErrorPush(context, SCPI_ERROR_UNDEFINED_HEADER);
    return SCPI_RES_ERR;
}

scpi_result_t scpi_cmd_simulatorTemperature(scpi_t *context) {
    SCPI_ErrorPush(context, SCPI_ERROR_UNDEFINED_HEADER);
    return SCPI_RES_ERR;
}

scpi_result_t scpi_cmd_simulatorTemperatureQ(scpi_t *context) {
    SCPI_ErrorPush(context, SCPI_ERROR_UNDEFINED_HEADER);
    return SCPI_RES_ERR;
}

scpi_result_t scpi_cmd_simulatorGui(scpi_t *context) {
    SCPI_ErrorPush(context, SCPI_ERROR_UNDEFINED_HEADER);
    return SCPI_RES_ERR;
}

scpi_result_t scpi_cmd_simulatorExit(scpi_t *context) {
    SCPI_ErrorPush(context, SCPI_ERROR_UNDEFINED_HEADER);
    return SCPI_RES_ERR;
}

scpi_result_t scpi_cmd_simulatorQuit(scpi_t *context) {
    SCPI_ErrorPush(context, SCPI_ERROR_UNDEFINED_HEADER);
    return SCPI_RES_ERR;
}

scpi_result_t scpi_cmd_simulatorPin1(scpi_t *context) {
    SCPI_ErrorPush(context, SCPI_ERROR_UNDEFINED_HEADER);
    return SCPI_RES_ERR;
}

scpi_result_t scpi_cmd_simulatorPin1Q(scpi_t *context) {
    SCPI_ErrorPush(context, SCPI_ERROR_UNDEFINED_HEADER);
    return SCPI_RES_ERR;
}

scpi_result_t scpi_cmd_simulatorPin2(scpi_t *context) {
    SCPI_ErrorPush(context, SCPI_ERROR_UNDEFINED_HEADER);
    return SCPI_RES_ERR;
}

scpi_result_t scpi_cmd_simulatorPin2Q(scpi_t *context) {
    SCPI_ErrorPush(context, SCPI_ERROR_UNDEFINED_HEADER);
    return SCPI_RES_ERR;
}

scpi_result_t scpi_cmd_simulatorDigitalDataByte(scpi_t *context) {
	SCPI_ErrorPush(context, SCPI_ERROR_UNDEFINED_HEADER);
	return SCPI_RES_ERR;
}

scpi_result_t scpi_cmd_simulatorUart(scpi_t *context) {
	SCPI_ErrorPush(context, SCPI_ERROR_UNDEFINED_HEADER);
	return SCPI_RES_ERR;
}

} // namespace scpi
} // namespace psu
} // namespace eez

#endif
