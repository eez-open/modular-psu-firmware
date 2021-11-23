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

#ifdef EEZ_PLATFORM_STM32
#include <i2c.h>
#include <cmsis_os.h>
#endif

#include <bb3/system.h>

// TODO
#include <bb3/psu/psu.h> // TestResult

#include <bb3/psu/channel_dispatcher.h>

#include <bb3/bp3c/io_exp.h>

#include <bb3/psu/psu.h>
#include <bb3/psu/channel.h>
#include <bb3/psu/io_pins.h>

#include <scpi/scpi.h>

#ifdef EEZ_PLATFORM_STM32

#if EEZ_BP3C_REVISION_R1B1
// http://www.ti.com/lit/ds/symlink/pca9536.pdf
#define IOEXP_ADDRESS 0x82 // 1 0 0 0 0 0 1 R/W'
#endif

#if EEZ_BP3C_REVISION_R3B1
// https://www.ti.com/lit/ds/symlink/tca9534.pdf
#define IOEXP_ADDRESS1 0x70
#define IOEXP_ADDRESS2 0x40 // 0 1 0 0 A2 A1 A0 R/W'
static uint16_t IOEXP_ADDRESS = IOEXP_ADDRESS1;
#endif

#define REG_INPUT_PORT         0x00
#define REG_OUTPUT_PORT        0x01
#define REG_POLARITY_INVERSION 0x02
#define REG_CONFIGURATION      0x03

#define COUPLING_MODE_NONE          0b10000000
#define COUPLING_MODE_COMMON_GROUND 0b10000001
#define COUPLING_MODE_PARALLEL      0b10000010
#define COUPLING_MODE_SERIES        0b10000100
#define COUPLING_MODE_SPLIT_RAIL    0b10001000

#endif

namespace eez {
namespace bp3c {
namespace io_exp {

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

#if defined(EEZ_PLATFORM_STM32)
bool doTest() {
#if EEZ_BP3C_REVISION_R1B1
	// all 4 pins are output (bit 0, 1, 2 and 3 should be set to 0)
	uint8_t configValue = 0b11110000;
#endif

#if EEZ_BP3C_REVISION_R3B1
	// all 8 pins are output (should be set to 0)
	uint8_t configValue = 0b00000000;
#endif

	if (write(REG_CONFIGURATION, configValue) == HAL_OK) {
		// check if configuration register is properly set
		uint8_t value;
		if (read(REG_CONFIGURATION, &value) == HAL_OK && value == configValue) {
			// all good
			g_testResult = TEST_OK;
			hardResetModules();
			return true;
		}
	}

	return false;
}
#endif

void init() {
#if defined(EEZ_PLATFORM_STM32)
	g_testResult = TEST_FAILED;

	if (!doTest()) {
#if EEZ_BP3C_REVISION_R3B1
		IOEXP_ADDRESS = IOEXP_ADDRESS2;
		doTest();
#endif
	}

	if (g_testResult == TEST_FAILED) {
		generateError(SCPI_ERROR_BACKPLANE_IOEXP_TEST_FAILED);
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
	if (channelCoupling == psu::channel_dispatcher::COUPLING_TYPE_PARALLEL) {
		write(REG_OUTPUT_PORT, COUPLING_MODE_PARALLEL);
	} else if (channelCoupling == psu::channel_dispatcher::COUPLING_TYPE_SERIES) {
		write(REG_OUTPUT_PORT, COUPLING_MODE_SERIES);
	} else if (channelCoupling == psu::channel_dispatcher::COUPLING_TYPE_SPLIT_RAILS) {
		write(REG_OUTPUT_PORT, COUPLING_MODE_SPLIT_RAIL);
	} else if (channelCoupling == psu::channel_dispatcher::COUPLING_TYPE_COMMON_GND) {
		write(REG_OUTPUT_PORT, COUPLING_MODE_COMMON_GROUND);
	} else {
		write(REG_OUTPUT_PORT, COUPLING_MODE_NONE);
	}
#endif
}

void hardResetModules() {
#ifdef EEZ_PLATFORM_STM32
    io_exp::writeToOutputPort(0b00000000);
    delay(5);
    io_exp::writeToOutputPort(0b10000000);
	// give some time to the modules to initialize
	osDelay(100);
#endif
}

void writeToOutputPort(uint8_t value) {
#ifdef EEZ_PLATFORM_STM32
    write(REG_OUTPUT_PORT, value);
#endif
}

} // namespace io_exp
} // namespace bp3c
} // namespace eez
