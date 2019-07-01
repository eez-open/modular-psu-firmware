/*
 * EEZ PSU Firmware
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

#ifdef EEZ_PLATFORM_STM32
#include <i2c.h>
#include <cmsis_os.h>
#endif

// TODO
#include <eez/apps/psu/psu.h> // TestResult

#include <eez/apps/psu/channel_dispatcher.h>

#include <eez/modules/bp3c/relays.h>


#ifdef EEZ_PLATFORM_STM32
#define IOEXP_ADDRESS 0x82

#define REG_INPUT_PORT         0x00
#define REG_OUTPUT_PORT        0x01
#define REG_POLARITY_INVERSION 0x02
#define REG_CONFIGURATION      0x03

#define COUPLING_MODE_NONE          0b000
#define COUPLING_MODE_COMMON_GROUND 0b0001
#define COUPLING_MODE_PARALLEL      0b0010
#define COUPLING_MODE_SERIES        0b0100
#define COUPLING_MODE_SPLIT_RAIL    0b1000
#endif

namespace eez {
namespace bp3c {
namespace relays {

TestResult g_testResult;

#ifdef EEZ_PLATFORM_STM32
HAL_StatusTypeDef write(uint8_t reg, uint8_t value) {
	taskENTER_CRITICAL();

	uint8_t data[2] = { reg, value };
    HAL_StatusTypeDef returnValue = HAL_I2C_Master_Transmit(&hi2c1, IOEXP_ADDRESS, data, 2, 5);

    taskEXIT_CRITICAL();

    return returnValue;
}

HAL_StatusTypeDef read(uint8_t reg, uint8_t *pValue) {
	taskENTER_CRITICAL();

    HAL_StatusTypeDef returnValue = HAL_I2C_Master_Transmit(&hi2c1, IOEXP_ADDRESS, &reg, 1, 5);
    if (returnValue == HAL_OK) {
    	returnValue = HAL_I2C_Master_Receive(&hi2c1, IOEXP_ADDRESS, pValue, 1, 5);
    }

    taskEXIT_CRITICAL();

    return returnValue;
}
#endif

void init() {
#if defined(EEZ_PLATFORM_STM32)
	g_testResult = TEST_FAILED;

	// all 4 pins are output (bit 0, 1, 2 and 3 should be set to 0)
	uint8_t configValue = 0b11110000;
	if (write(REG_CONFIGURATION, configValue) == HAL_OK) {
		// check if configuration register is properly set
		uint8_t value;
		if (read(REG_CONFIGURATION, &value) == HAL_OK) {
			if (value == configValue) {
				// all good, now set coupling mode to NONE
				if (write(REG_OUTPUT_PORT, COUPLING_MODE_NONE) == HAL_OK) {
					g_testResult = TEST_OK;
				}
			}
		}
	}
#else
    g_testResult = TEST_OK;
#endif
}

bool test() {
    return g_testResult == TEST_OK;
}

void switchChannelCoupling(int channelCoupling) {
#if defined(EEZ_PLATFORM_STM32)
	if (channelCoupling == psu::channel_dispatcher::TYPE_PARALLEL) {
		write(REG_OUTPUT_PORT, COUPLING_MODE_PARALLEL);
	} else if (channelCoupling == psu::channel_dispatcher::TYPE_SERIES) {
		write(REG_OUTPUT_PORT, COUPLING_MODE_SERIES);
	} else if (channelCoupling == psu::channel_dispatcher::TYPE_SPLIT_RAIL) {
		write(REG_OUTPUT_PORT, COUPLING_MODE_SPLIT_RAIL);
	} else if (channelCoupling == psu::channel_dispatcher::TYPE_COMMON_GROUND) {
		write(REG_OUTPUT_PORT, COUPLING_MODE_COMMON_GROUND);
	} else {
		write(REG_OUTPUT_PORT, COUPLING_MODE_NONE);
	}
#endif
}

} // namespace relays
} // namespace bp3c
} // namespace eez
