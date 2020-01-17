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

#include "eez/modules/bp3c/flash_slave.h"
#include "eez/system.h"
#include "eez/modules/psu/psu.h"
#include "eez/modules/psu/io_pins.h"
#include "eez/modules/bp3c/io_exp.h"

#ifdef EEZ_PLATFORM_STM32

#include <memory.h>

#include "main.h"
#include "usart.h"

#endif

namespace eez {
namespace bp3c {
namespace flash_slave {

bool g_bootloaderMode = false;

#ifdef EEZ_PLATFORM_STM32

enum FlashStatus
{
	STAND_BY,
	SEND_START,
	WAIT_FOR_RESPONSE,
	GOT_ACK,
	GOT_NACK,
	RESPONSE_TIMEOUT,
	SEND_GET,
	DATA_READY,
	WAIT_FOR_RESPONSE_1,
	WAIT_FOR_RESPONSE_2,
	WAIT_FOR_RESPONSE_3,
	WAIT_FOR_RESPONSE_4,
	WAIT_FOR_RESPONSE_5,
	WAIT_FOR_RESPONSE_6,
	IDLE
};

static const uint8_t CMD_GET = 0x00;
static const uint8_t CMD_GET_VERSION = 0x01;
static const uint8_t CMD_ID = 0x02;
static const uint8_t CMD_READ_MEMORY = 0x11;
static const uint8_t CMD_GO = 0x21;
static const uint8_t CMD_WRITE_MEMORY = 0x31;
static const uint8_t CMD_ERASE = 0x43;
static const uint8_t CMD_EXTENDED_ERASE = 0x44;
static const uint8_t CMD_WRITE_PROTECT = 0x63;
static const uint8_t CMD_WRITE_UNPROTECT = 0x73;
static const uint8_t CMD_READOUT_PROTECT = 0x82;
static const uint8_t CMD_READOUT_UNPROTECT = 0x92;
static const uint8_t ENTER_BOOTLOADER = 0x7F;
static const uint8_t CRC_MASK = 0xFF;

static const uint8_t ACK = 0x79;
static const uint8_t NACK = 0x1F;

static const uint32_t MAX_TIMEOUT = 10000;

UART_HandleTypeDef *phuart = &huart7;

static uint8_t rxData[128];

static FlashStatus flashStatus = STAND_BY;

void sendDataAndCRC(uint8_t data) {
	uint8_t sendData[1];
	sendData[0] = data;
	HAL_UART_Transmit(phuart, sendData, 1, 20);
	sendData[0] = CRC_MASK ^ data;
	HAL_UART_Transmit(phuart, sendData, 1, 20);
}

void sendDataNoCRC(uint8_t data) {
	uint8_t sendData[1];
	sendData[0] = data;
	HAL_UART_Transmit(phuart, sendData, 1, 20);
}

void resetSlave(void) {
	// not implemented -- I do manual reset
}

bool getByte(bool start = false) {
	uint32_t startTime = HAL_GetTick();
	do {
		if (HAL_UART_Receive(phuart, rxData, 1, 100) == HAL_OK) {
			return true;
		}
		if (start) {
			sendDataNoCRC(ENTER_BOOTLOADER);
		}
	} while (HAL_GetTick() - startTime < MAX_TIMEOUT);
	flashStatus = RESPONSE_TIMEOUT;
	return false;
}

uint8_t flashSlaveFSM() {
	static uint8_t numBytes = 0;

	switch (flashStatus) {
	case STAND_BY:
		// statements
		resetSlave();
		flashStatus = SEND_START;
		break;
	case WAIT_FOR_RESPONSE:
		if (getByte(true)) {
			DebugTrace("Received: 0x%02x\n", (int)rxData[0]);

			if (rxData[0] == ACK) {
				flashStatus = GOT_ACK;
			} else if (rxData[0] == NACK) {
				flashStatus = GOT_NACK;
			}
		}
		break;
	case SEND_START:
		sendDataNoCRC(ENTER_BOOTLOADER);
		flashStatus = WAIT_FOR_RESPONSE;
		break;
	case GOT_ACK:
		flashStatus = DATA_READY;
		break;
	case GOT_NACK:
		flashStatus = DATA_READY;
		break;
	case RESPONSE_TIMEOUT:
		flashStatus = STAND_BY;
		break;
	case DATA_READY:
		flashStatus = SEND_GET;
		break;
	case SEND_GET:
		sendDataAndCRC(CMD_GET);
		flashStatus = WAIT_FOR_RESPONSE_1;
		break;
	case WAIT_FOR_RESPONSE_1:
		if (getByte()) {
			DebugTrace("Received: 0x%02x\n", (int)rxData[0]);

			if (rxData[0] == ACK) {
				flashStatus = WAIT_FOR_RESPONSE_2;
			} else if (rxData[0] == NACK) {
				flashStatus = GOT_NACK;
			}
		}
		break;
	case WAIT_FOR_RESPONSE_2:
		if (getByte()) {
			DebugTrace("Received: 0x%02x\n", (int)rxData[0]);

			numBytes = rxData[0];
			//numBytes += 2;
			DebugTrace("Num bytes: %d\n", (int)numBytes);

			flashStatus = WAIT_FOR_RESPONSE_3;
		}
		break;
	case WAIT_FOR_RESPONSE_3:
		if (getByte()) {
			DebugTrace("Received: 0x%02x\n", (int)rxData[0]);

			numBytes--;
			if (numBytes == 0) {
				flashStatus = IDLE;
			}
		}
		break;
	default:
		flashStatus = STAND_BY;
	}
	return flashStatus;
}

void toggleBootloader(int slotIndex) {
	if (!g_bootloaderMode) {
		psu::reset();

		g_bootloaderMode = true;

		// power down channels

		for (int i = 0; i < psu::CH_NUM; ++i) {
			psu::Channel::get(i).onPowerDown();
		}
		osDelay(25);

		// enable BOOT0 flag for selected slot and reset modules

		if (slotIndex == 0) {
			io_exp::writeToOutputPort(0b10010000);
		} else if (slotIndex == 1) {
			io_exp::writeToOutputPort(0b10100000);
		} else if (slotIndex == 2) {
			io_exp::writeToOutputPort(0b11000000);
		}

		osDelay(5);

		if (slotIndex == 0) {
			io_exp::writeToOutputPort(0b00010000);
		} else if (slotIndex == 1) {
			io_exp::writeToOutputPort(0b00100000);
		} else if (slotIndex == 2) {
			io_exp::writeToOutputPort(0b01000000);
		}

		osDelay(25);

		if (slotIndex == 0) {
			io_exp::writeToOutputPort(0b10010000);
		} else if (slotIndex == 1) {
			io_exp::writeToOutputPort(0b10100000);
		} else if (slotIndex == 2) {
			io_exp::writeToOutputPort(0b11000000);
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

		DebugTrace("start slave FSM\n");

		MX_UART7_Init();

		int wasFlashStatus = -1;
		flashStatus = STAND_BY;
		while (true) {
			flashSlaveFSM();

			if (flashStatus != wasFlashStatus) {
				DebugTrace("flash status: %d\n", flashStatus);
				wasFlashStatus = flashStatus;
			}

			if (flashStatus == IDLE || flashStatus == RESPONSE_TIMEOUT) {
				break;
			}
		}

		DebugTrace("end slave FSM\n");
	} else {
		// disable BOOT0 flag for selected slot and reset modules
		io_exp::writeToOutputPort(0b10000000);
		delay(5);
		io_exp::writeToOutputPort(0b00000000);
		delay(25);
		io_exp::writeToOutputPort(0b10000000);

		// init channels
		for (int i = 0; i < psu::CH_NUM; ++i) {
			psu::Channel::get(i).init();
		}

		// test channels
		for (int i = 0; i < psu::CH_NUM; ++i) {
			psu::Channel::get(i).test();
		}

		HAL_UART_DeInit(phuart);

		g_bootloaderMode = false;

		psu::io_pins::refresh();
	}
}

#endif // EEZ_PLATFORM_STM32

} // namespace flash_slave
} // namespace bp3c
} // namespace eez

#ifdef EEZ_PLATFORM_STM32
void byteFromSlave() {
	// TODO
}
#endif
