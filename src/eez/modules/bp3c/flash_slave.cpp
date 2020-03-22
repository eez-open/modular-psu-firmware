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

#include <assert.h>

#include "eez/firmware.h"
#include "eez/system.h"

#include "eez/modules/psu/psu.h"
#include "eez/modules/psu/io_pins.h"
#include "eez/modules/psu/datetime.h"
#include "eez/modules/psu/profile.h"
#include "eez/modules/psu/channel_dispatcher.h"
#include "eez/modules/psu/sd_card.h"
#include <eez/modules/psu/scpi/psu.h>
#include <eez/modules/psu/gui/psu.h>

#include "eez/modules/bp3c/flash_slave.h"
#include <eez/modules/bp3c/io_exp.h>

#include <eez/libs/sd_fat/sd_fat.h>

#include <eez/scpi/scpi.h>

#ifdef EEZ_PLATFORM_STM32

#include <memory.h>

#include "main.h"
#include "usart.h"

#endif

namespace eez {

using namespace scpi;

namespace bp3c {
namespace flash_slave {

bool g_bootloaderMode = false;
static int g_slotIndex;
static char g_hexFilePath[MAX_PATH_LENGTH + 1];

#ifdef EEZ_PLATFORM_STM32

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

static const uint32_t SYNC_TIMEOUT = 3000;
static const uint32_t CMD_TIMEOUT = 100;

static UART_HandleTypeDef *phuart = &huart7;

static uint8_t rxData[128];

struct BootloaderInfo {
	uint8_t bootloaderVersion;

	bool getCmdSupported;
	bool getVersionCmdSupported;
	bool getIdCmdSupported;
	bool readMemoryCmdSupported;
	bool goCmdSupported;
	bool writeMemoryCmdSupported;
	bool eraseCmdSupported;
	bool extendedEraseCmdSupported;
	bool writeProtectCmdSupported;
	bool writeUnprotectCmdSupported;
	bool readoutProtectCmdSupported;
	bool readoutUnprotectCmdSupported;
};

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

bool execGetCmd(BootloaderInfo &bootloaderInfo) {
	taskENTER_CRITICAL();
	sendDataAndCRC(CMD_GET);
	HAL_StatusTypeDef result = HAL_UART_Receive(phuart, rxData, 2, CMD_TIMEOUT);
	if (result != HAL_OK || rxData[0] != ACK) {
		taskEXIT_CRITICAL();
		return false;
	}
	result = HAL_UART_Receive(phuart, rxData + 2, rxData[1] + 2, CMD_TIMEOUT);
	taskEXIT_CRITICAL();

	if (result != HAL_OK || rxData[0] != ACK || rxData[14] != ACK) {
		return false;
	}

	bootloaderInfo.bootloaderVersion = rxData[2];

	for (uint16_t i = 0; i < rxData[1]; i++) {
		uint8_t cmd = rxData[3 + i];
		if (cmd == CMD_GET) {
			bootloaderInfo.getCmdSupported = true;
		} else if (cmd == CMD_GET_VERSION) {
			bootloaderInfo.getVersionCmdSupported = true;
		} else if (cmd == CMD_ID) {
			bootloaderInfo.getIdCmdSupported = true;
		} else if (cmd == CMD_READ_MEMORY) {
			bootloaderInfo.readMemoryCmdSupported = true;
		} else if (cmd == CMD_GO) {
			bootloaderInfo.goCmdSupported = true;
		} else if (cmd == CMD_WRITE_MEMORY) {
			bootloaderInfo.writeMemoryCmdSupported = true;
		} else if (cmd == CMD_ERASE) {
			bootloaderInfo.eraseCmdSupported = true;
		} else if (cmd == CMD_EXTENDED_ERASE) {
			bootloaderInfo.extendedEraseCmdSupported = true;
		} else if (cmd == CMD_WRITE_PROTECT) {
			bootloaderInfo.writeProtectCmdSupported = true;
		} else if (cmd == CMD_WRITE_UNPROTECT) {
			bootloaderInfo.writeUnprotectCmdSupported = true;
		} else if (cmd == CMD_READOUT_PROTECT) {
			bootloaderInfo.readoutProtectCmdSupported = true;
		} else if (cmd == CMD_READOUT_UNPROTECT) {
			bootloaderInfo.readoutUnprotectCmdSupported = true;
		}
	}

	return true;
}

bool readMemory(uint32_t address, uint8_t *buffer, uint32_t bufferSize) {
	assert(bufferSize <= 256);
	
	uint8_t addressAndCrc[5] = { 
		(uint8_t)(address >> 24),
		(uint8_t)((address >> 16) & 0xFF),
		(uint8_t)((address >> 8) & 0xFF),
		(uint8_t)(address & 0xFF)
	};
	addressAndCrc[4] = addressAndCrc[0] ^ addressAndCrc[1] ^ addressAndCrc[2] ^ addressAndCrc[3];

	uint8_t numBytesAndCrc[2] = { (uint8_t)(bufferSize - 1) };
	numBytesAndCrc[1] = ~numBytesAndCrc[0];

	taskENTER_CRITICAL();

	sendDataAndCRC(CMD_READ_MEMORY);

	HAL_StatusTypeDef result = HAL_UART_Receive(phuart, rxData, 1, CMD_TIMEOUT);
	if (result != HAL_OK || rxData[0] != ACK) {
		taskEXIT_CRITICAL();
		return false;
	}

	HAL_UART_Transmit(phuart, addressAndCrc, 5, 20);

	result = HAL_UART_Receive(phuart, rxData, 1, CMD_TIMEOUT);
	if (result != HAL_OK || rxData[0] != ACK) {
		taskEXIT_CRITICAL();
		return false;
	}

	HAL_UART_Transmit(phuart, numBytesAndCrc, 2, 20);

	result = HAL_UART_Receive(phuart, rxData, 1, CMD_TIMEOUT);
	if (result != HAL_OK || rxData[0] != ACK) {
		taskEXIT_CRITICAL();
		return false;
	}

	result = HAL_UART_Receive(phuart, buffer, bufferSize, CMD_TIMEOUT);
	if (result != HAL_OK) {
		taskEXIT_CRITICAL();
		return false;
	}

	taskEXIT_CRITICAL();
	return true;
}

#endif

bool syncWithSlave() {
#if defined(EEZ_PLATFORM_STM32)
    uint32_t startTime = HAL_GetTick();
    do {
        taskENTER_CRITICAL();
        sendDataNoCRC(ENTER_BOOTLOADER);
        HAL_StatusTypeDef result = HAL_UART_Receive(phuart, rxData, 1, 100);
        taskEXIT_CRITICAL();
        if (result == HAL_OK && rxData[0] == ACK) {
            return true;
        }
    } while (HAL_GetTick() - startTime < SYNC_TIMEOUT);
    return false;
#endif

#if defined(EEZ_PLATFORM_SIMULATOR)
    return true;
#endif
}


bool eraseAll() {
#if defined(EEZ_PLATFORM_STM32)
    taskENTER_CRITICAL();

	sendDataAndCRC(CMD_EXTENDED_ERASE);

	HAL_StatusTypeDef result = HAL_UART_Receive(phuart, rxData, 1, CMD_TIMEOUT);
	if (result != HAL_OK || rxData[0] != ACK) {
		taskEXIT_CRITICAL();
		return false;
	}

	static uint8_t buffer[3] = { 0xFF, 0xFF, 0x00 };
	HAL_UART_Transmit(phuart, buffer, 3, 20);

	result = HAL_UART_Receive(phuart, rxData, 1, CMD_TIMEOUT);
	if (result != HAL_OK || rxData[0] != ACK) {
		taskEXIT_CRITICAL();
		return false;
	}

	taskEXIT_CRITICAL();
	return true;
#endif

#if defined(EEZ_PLATFORM_SIMULATOR)
    return true;
#endif
}

bool writeMemory(uint32_t address, const uint8_t *buffer, uint32_t bufferSize) {
	assert(bufferSize <= 256);

#if defined(EEZ_PLATFORM_STM32)
    uint8_t addressAndCrc[5] = {
		(uint8_t)(address >> 24),
		(uint8_t)((address >> 16) & 0xFF),
		(uint8_t)((address >> 8) & 0xFF),
		(uint8_t)(address & 0xFF)
	};
	addressAndCrc[4] = addressAndCrc[0] ^ addressAndCrc[1] ^ addressAndCrc[2] ^ addressAndCrc[3];

	uint8_t numBytes = (uint8_t)(bufferSize - 1);

	uint8_t crc = numBytes;
	for (unsigned i = 0; i < bufferSize; i++) {
		crc ^= buffer[i];
	}

	taskENTER_CRITICAL();

	sendDataAndCRC(CMD_WRITE_MEMORY);

	HAL_StatusTypeDef result = HAL_UART_Receive(phuart, rxData, 1, CMD_TIMEOUT);
	if (result != HAL_OK || rxData[0] != ACK) {
		taskEXIT_CRITICAL();
		return false;
	}

	HAL_UART_Transmit(phuart, addressAndCrc, 5, 20);

	result = HAL_UART_Receive(phuart, rxData, 1, CMD_TIMEOUT);
	if (result != HAL_OK || rxData[0] != ACK) {
		taskEXIT_CRITICAL();
		return false;
	}

	HAL_UART_Transmit(phuart, &numBytes, 1, 20);
	HAL_UART_Transmit(phuart, (uint8_t *)buffer, bufferSize, 20);
	HAL_UART_Transmit(phuart, &crc, 1, 20);

	result = HAL_UART_Receive(phuart, rxData, 1, CMD_TIMEOUT);
	if (result != HAL_OK || rxData[0] != ACK) {
		taskEXIT_CRITICAL();
		return false;
	}

	taskEXIT_CRITICAL();
	return true;
#endif

#if defined(EEZ_PLATFORM_SIMULATOR)
    osDelay(1);
    return true;
#endif
}

void enterBootloaderMode(int slotIndex) {
    g_bootloaderMode = true;

    psu::profile::saveToLocation(10);

#if defined(EEZ_PLATFORM_STM32)

    reset();

    // power down channels
    psu::powerDownChannels();

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

    MX_UART7_Init();
#endif
}

void leaveBootloaderMode() {
#if defined(EEZ_PLATFORM_STM32)
    // disable BOOT0 flag
    io_exp::writeToOutputPort(0b10000000);
    osDelay(5);
	io_exp::hardResetModules();

    psu::initChannels();
    psu::testChannels();
#endif

    g_bootloaderMode = false;
    psu::profile::recallFromLocation(10);

#if defined(EEZ_PLATFORM_STM32)
    HAL_UART_DeInit(phuart);
    psu::io_pins::refresh();
#endif
}

struct HexRecord {
	uint8_t recordLength;
	uint16_t address;
	uint8_t recordType;
	uint8_t data[256];
	uint8_t checksum;
};

uint8_t hex(uint8_t digit) {
	if (digit < 'A') {
		return digit - '0';
	} else if (digit < 'a') {
		return 10 + (digit - 'A');
	} else {
		return 10 + (digit - 'a');
	}
}

bool readHexRecord(psu::sd_card::BufferedFileRead &file, HexRecord &hexRecord) {
	uint8_t buffer[512];

	int bytes = file.read(buffer, 9);
	if (bytes != 9) {
		return false;
	}

	if (buffer[0] != ':') {
		return false;
	}

	hexRecord.recordLength = (hex(buffer[1]) << 4) + hex(buffer[2]);
	hexRecord.address = (hex(buffer[3]) << 12) + (hex(buffer[4]) << 8) + (hex(buffer[5]) << 4) + hex(buffer[6]);
	hexRecord.recordType = (hex(buffer[7]) << 4) + hex(buffer[8]);

	if (hexRecord.recordLength > 0) {
		bytes = file.read(buffer, hexRecord.recordLength * 2);
		if (bytes != hexRecord.recordLength * 2) {
			return false;
		}

		for (unsigned i = 0; i < hexRecord.recordLength; i++) {
			hexRecord.data[i] = (hex(buffer[2 * i]) << 4) + hex(buffer[2 * i + 1]);
		}
	} else {
		delay(1);
	}

	bytes = file.read(buffer, 2);
	if (bytes != 2) {
		return false;
	}
	hexRecord.checksum = (hex(buffer[0]) << 4) + hex(buffer[1]);

	while (file.peek() != ':' && file.peek() != EOF) {
		file.read();
	}

	return true;
}

bool start(int slotIndex, const char *hexFilePath, int *err) {
	psu::channel_dispatcher::disableOutputForAllChannels();

	enterBootloaderMode(slotIndex);

	if (!syncWithSlave()) {
		DebugTrace("Failed to sync with slave\n");
		if (err) {
			*err = SCPI_ERROR_EXECUTION_ERROR;
		}
		return false;
	}

	g_slotIndex = slotIndex;
	strcpy(g_hexFilePath, hexFilePath);

	osMessagePut(g_scpiMessageQueueId, SCPI_QUEUE_MESSAGE(SCPI_QUEUE_MESSAGE_TARGET_NONE, SCPI_QUEUE_MESSAGE_FLASH_SLAVE_UPLOAD_HEX_FILE, 0), osWaitForever);

	return true;
}

void uploadHexFile() {
	// uint8_t buffer[256];
	// if (readMemory(0x08000000, buffer, sizeof(buffer))) {
	// 	for (size_t i = 0; i < sizeof(buffer) / 16; i++) {
	// 		for (size_t j = 0; j < 16; j++) {
	// 			DebugTrace("%02x", (int)buffer[i * 16 + j]);
	// 		}
	// 		DebugTrace("\n");
	// 	}
	// }

    // uint8_t hour, minute, second;
    // psu::datetime::getTime(hour, minute, second);
    // DebugTrace("[%02d:%02d:%02d] Flash started\n", hour, minute, second);

	bool eofReached = false;
    File file;
    psu::sd_card::BufferedFileRead bufferedFile(file);
    size_t totalSize = 0;
	HexRecord hexRecord;
	uint32_t addressUpperBits = 0;

#if OPTION_DISPLAY
    psu::gui::showProgressPageWithoutAbort("Downloading firmware...");
#endif

    if (!eraseAll()) {
		DebugTrace("Failed to erase all!\n");
		goto Exit;
	}

    if (!file.open(g_hexFilePath, FILE_OPEN_EXISTING | FILE_READ)) {
		DebugTrace("Can't open firmware hex file!\n");
		goto Exit;
    }

#if OPTION_DISPLAY
    totalSize = file.size();
#endif

	while (!eofReached && readHexRecord(bufferedFile, hexRecord)) {

#if OPTION_DISPLAY
        psu::gui::updateProgressPage(file.tell(), totalSize);
#endif

		if (hexRecord.recordType == 0x04) {
			addressUpperBits = ((hexRecord.data[0] << 8) + hexRecord.data[1]) << 16;
		} else if (hexRecord.recordType == 0x00) {
			uint32_t address = addressUpperBits | hexRecord.address;
			if (!writeMemory(address, hexRecord.data, hexRecord.recordLength)) {
				DebugTrace("Failed to write memory at address %08x\n", address);
				break;
			}
		} else if (hexRecord.recordType == 0x01) {
			eofReached = true;
		}
	}

	// uint8_t hour, minute, second;
	// psu::datetime::getTime(hour, minute, second);
	// DebugTrace("[%02d:%02d:%02d] Flash finished\n", hour, minute, second);

	file.close();

Exit:

#if OPTION_DISPLAY
    psu::gui::hideProgressPage();
#endif

	leaveBootloaderMode();

	if (!eofReached) {
		psu::gui::errorMessage("Downloading failed!");
	}
}

} // namespace flash_slave
} // namespace bp3c
} // namespace eez

#ifdef EEZ_PLATFORM_STM32
void byteFromSlave() {
	// TODO
}
#endif
