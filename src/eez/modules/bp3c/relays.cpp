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

#include <eez/system.h>

// TODO
#include <eez/modules/psu/psu.h> // TestResult

#include <eez/modules/psu/channel_dispatcher.h>

#include <eez/modules/bp3c/relays.h>

#include <eez/modules/psu/psu.h>
#include <eez/modules/psu/channel.h>
#include <eez/modules/psu/io_pins.h>

#ifdef EEZ_PLATFORM_STM32

#if EEZ_BP3C_REVISION_R1B1
// http://www.ti.com/lit/ds/symlink/pca9536.pdf
#define IOEXP_ADDRESS 0x82 // 1 0 0 0 0 0 1 R/W'
#endif

#if EEZ_BP3C_REVISION_R3B1
// https://www.ti.com/lit/ds/symlink/tca9534.pdf
#define IOEXP_ADDRESS 0x40 // 0 1 0 0 A2 A1 A0 R/W'
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

#include <eez/modules/bp3c/flashSlave.h>

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

#ifdef EEZ_PLATFORM_STM32

bool g_bootloaderMode = false;

void hardResetModules() {
	write(REG_OUTPUT_PORT, 0);	
}

void toggleBootloader(int slotIndex) {
	if(!g_bootloaderMode) {
		psu::reset();

		g_bootloaderMode = true;

		// init channels
		for (int i = 0; i < psu::CH_NUM; ++i) {
			psu::Channel::get(i).onPowerDown();
		}

		osDelay(25);

		if (slotIndex == 0) {
			write(REG_OUTPUT_PORT, 0b10010000);
		} else if (slotIndex == 1) {
			write(REG_OUTPUT_PORT, 0b10100000);
		} else if (slotIndex == 2) {
			write(REG_OUTPUT_PORT, 0b11000000);
		}

		osDelay(5);

		if (slotIndex == 0) {
			write(REG_OUTPUT_PORT, 0b00010000);	
		} else if (slotIndex == 1) {
			write(REG_OUTPUT_PORT, 0b00100000);	
		} else if (slotIndex == 2) {
			write(REG_OUTPUT_PORT, 0b01000000);	
		}

		osDelay(25);

		if (slotIndex == 0) {
			write(REG_OUTPUT_PORT, 0b10010000);	
		} else if (slotIndex == 1) {
			write(REG_OUTPUT_PORT, 0b10100000);	
		} else if (slotIndex == 2) {
			write(REG_OUTPUT_PORT, 0b11000000);	
		}

		osDelay(25);

		// GPIO_InitTypeDef GPIO_InitStruct = { 0 };

		// GPIO_InitStruct.Pin = UART_RX_DIN1_Pin;
		// GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
		// GPIO_InitStruct.Pull = GPIO_NOPULL;
		// GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
		// GPIO_InitStruct.Alternate = GPIO_AF8_UART7;
		// HAL_GPIO_Init(UART_RX_DIN1_GPIO_Port, &GPIO_InitStruct);

		// GPIO_InitStruct.Pin = UART_TX_DOUT1_Pin;
		// GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
		// GPIO_InitStruct.Pull = GPIO_NOPULL;
		// GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
		// GPIO_InitStruct.Alternate = GPIO_AF12_UART7;
		// HAL_GPIO_Init(UART_TX_DOUT1_GPIO_Port, &GPIO_InitStruct);

		DebugTrace("start slave FSM");

		int wasFlashStatus = -1;
		uint32_t startTick = HAL_GetTick();
		while (true) {
			flashSlaveFSM();
			if (flashStatus != wasFlashStatus) {
				DebugTrace("flash status: %d", flashStatus);
				wasFlashStatus = flashStatus;
			}

			uint32_t currentTick = HAL_GetTick();
			if (currentTick - startTick >= 3000) {
				break;
			}
		}

		DebugTrace("end slave FSM after 3 seconds");
	} else {
		write(REG_OUTPUT_PORT, 0b10000000);
		delay(5);
		write(REG_OUTPUT_PORT, 0b00000000);
		delay(25);
		write(REG_OUTPUT_PORT, 0b10000000);	

		psu::io_pins::refresh();

		// init channels
		for (int i = 0; i < psu::CH_NUM; ++i) {
			psu::Channel::get(i).init();
		}

		// test channels
		for (int i = 0; i < psu::CH_NUM; ++i) {
			psu::Channel::get(i).test();
		}

		g_bootloaderMode = false;
	}
}

#endif

} // namespace relays
} // namespace bp3c
} // namespace eez
