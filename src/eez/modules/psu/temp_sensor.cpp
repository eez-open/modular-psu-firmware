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

#include <eez/index.h>
#include <eez/scpi/regs.h>
#include <eez/modules/psu/temp_sensor.h>

#if defined(EEZ_PLATFORM_STM32)
#include <eez/drivers/tmp1075.h>
#include <eez/drivers/tc77.h>
#include <eez/modules/dcp405/channel.h>
#include <eez/modules/dcm220/channel.h>
#endif

#if OPTION_AUX_TEMP_SENSOR
#include <eez/modules/aux_ps/fan.h>
#endif

namespace eez {
namespace psu {

using namespace scpi;

namespace temp_sensor {

#define TEMP_SENSOR(NAME, QUES_REG_BIT, SCPI_ERROR)            \
    TempSensor(NAME, #NAME, QUES_REG_BIT, SCPI_ERROR)

TempSensor sensors[NUM_TEMP_SENSORS] = { TEMP_SENSORS };

#undef TEMP_SENSOR

////////////////////////////////////////////////////////////////////////////////

TempSensor::TempSensor(Type type_, const char *name_, int ques_bit_, int scpi_error_)
    : type(type_), name(name_), ques_bit(ques_bit_), scpi_error(scpi_error_) {
}

bool TempSensor::isInstalled() {
    if (type == AUX) {
#if OPTION_AUX_TEMP_SENSOR  
        return true;
#else
        return false;
#endif
    } else if (type >= CH1 && type <= CH6) {
        int channelIndex = type - CH1;
        return channelIndex < CH_NUM;
    } else {
        return false;
    }
}

Channel *TempSensor::getChannel() {
    if (type >= CH1 && type <= CH6) {
        return &Channel::get(type - CH1);
    }
    return NULL;
}

void TempSensor::init() {
}

float TempSensor::doRead() {
    if (!isInstalled()) {
        return NAN;
    }

#if defined(EEZ_PLATFORM_SIMULATOR)
    return simulator::getTemperature(type);
#endif

#if defined(EEZ_PLATFORM_STM32)
#if OPTION_AUX_TEMP_SENSOR
    if (type == AUX) {
        return aux_ps::fan::readTemperature();
    }
#endif

	if (type >= CH1 && type <= CH6) {
		int channelIndex = type - CH1;
		int slotIndex = Channel::get(channelIndex).slotIndex;
		auto &slot = *g_slots[slotIndex];

		if (slot.moduleInfo->moduleType == MODULE_TYPE_DCP405) {
            if (slot.moduleRevision >= MODULE_REVISION_DCP405_R1B1) {
			    return drivers::tc77::readTemperature(slotIndex);
            } else {
                return drivers::tmp1075::readTemperature(slotIndex);
            }
		} else if (slot.moduleInfo->moduleType == MODULE_TYPE_DCM220 || slot.moduleInfo->moduleType == MODULE_TYPE_DCM224) {
			return dcm220::readTemperature(channelIndex);
		}
	}
#endif

    return NAN;
}

bool TempSensor::test() {
    if (isInstalled()) {
        float temperature = doRead();
        g_testResult = 
            isNaN(temperature) || 
            temperature < TEMP_SENSOR_MIN_VALID_TEMPERATURE || 
            temperature > TEMP_SENSOR_MAX_VALID_TEMPERATURE ? TEST_FAILED : TEST_OK;
    } else {
        g_testResult = TEST_SKIPPED;
    }

    Channel *channel = getChannel();

    if (g_testResult == TEST_FAILED) {
        if (channel) {
            // set channel current max. limit to ERR_MAX_CURRENT if sensor is faulty
            channel->limitMaxCurrent(MAX_CURRENT_LIMIT_CAUSE_TEMPERATURE);
        }

        generateError(scpi_error);
    } else {
        if (channel) {
            channel->unlimitMaxCurrent();
        }
    }

    return g_testResult != TEST_FAILED;
}

float TempSensor::read() {
    if (!isInstalled()) {
        return NAN;
    }

    float value = doRead();

    if (isNaN(value) || value < TEMP_SENSOR_MIN_VALID_TEMPERATURE || value > TEMP_SENSOR_MAX_VALID_TEMPERATURE) {
        g_testResult = TEST_FAILED;

        Channel *channel = getChannel();
        if (channel) {
            // set channel current max. limit to ERR_MAX_CURRENT if sensor is faulty
            channel->limitMaxCurrent(MAX_CURRENT_LIMIT_CAUSE_TEMPERATURE);
        }

        generateError(scpi_error);
    }

    return value;
}

} // namespace temp_sensor
} // namespace psu
} // namespace eez
