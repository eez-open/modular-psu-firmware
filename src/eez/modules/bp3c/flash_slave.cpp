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

#include <eez/firmware.h>
#include <eez/system.h>

#include <eez/modules/psu/psu.h>
#include <eez/modules/psu/channel_dispatcher.h>
#include <eez/modules/psu/io_pins.h>
#include <eez/modules/psu/datetime.h>
#include <eez/modules/psu/profile.h>
#include <eez/modules/psu/channel_dispatcher.h>
#include <eez/modules/psu/sd_card.h>
#include <eez/modules/psu/scpi/psu.h>
#include <eez/modules/psu/gui/psu.h>

#include <eez/modules/bp3c/flash_slave.h>
#include <eez/modules/bp3c/io_exp.h>
#include <eez/modules/bp3c/eeprom.h>

#include <eez/libs/sd_fat/sd_fat.h>

#ifdef EEZ_PLATFORM_STM32

#include <memory.h>
#include "main.h"
#include "usart.h"
#include <eez/platform/stm32/spi.h>

#endif

namespace eez {

void highPriorityThreadOneIter();

using namespace scpi;

namespace bp3c {
namespace flash_slave {

bool g_bootloaderMode = false;
static int g_slotIndex;
static char g_hexFilePath[MAX_PATH_LENGTH + 1];
bool g_ate;

struct HexRecord {
	uint8_t recordLength;
	uint16_t address;
	uint8_t recordType;
	uint8_t data[256];
	uint8_t checksum;
};

#ifdef EEZ_PLATFORM_STM32

static const uint8_t CMD_WRITE_MEMORY = 0x31;
static const uint8_t CMD_EXTENDED_ERASE = 0x44;
static const uint8_t ENTER_BOOTLOADER = 0x7F;
static const uint8_t CRC_MASK = 0xFF;

static const uint8_t BL_SPI_SOF = 0x5A;

static const uint8_t ACK = 0x79;
static const uint8_t NACK = 0x1F;

static const uint32_t SYNC_TIMEOUT = 10000;
static const uint32_t CMD_TIMEOUT = 100;

static const uint32_t SPI_EEPROM_ERASE_TIMEOUT = 15000;

#define phuart (g_mcuRevision >= MCU_REVISION_R3B3 ? &huart4 : &huart7)

#endif

static const uint32_t SPI_ACK_TIMEOUT = 1000;

void enterBootloaderMode(int slotIndex);
bool syncWithSlave(int slotIndex);
bool eraseAll(int slotIndex);
bool readHexRecord(psu::sd_card::BufferedFileRead &file, HexRecord &hexRecord);
bool writeMemory(int slotIndex, uint32_t address, const uint8_t *buffer, uint32_t bufferSize);

void start(int slotIndex, const char *hexFilePath, bool ate) {
	g_slotIndex = slotIndex;
	
	stringCopy(g_hexFilePath, MAX_PATH_LENGTH, hexFilePath);
	
	g_ate = ate;

	if (isPsuThread()) {
		doStart();
	} else {
		sendMessageToPsu(PSU_MESSAGE_FLASH_SLAVE_START);
	}
}

void doStart() {
#if OPTION_DISPLAY
	psu::gui::showAsyncOperationInProgress("Preparing...");
#endif
	enterBootloaderMode(g_slotIndex);
	sendMessageToLowPriorityThread(THREAD_MESSAGE_FLASH_SLAVE_UPLOAD_HEX_FILE);
}

void enterBootloaderMode(int slotIndex) {
    psu::profile::saveToLocation(10);

    g_bootloaderMode = true;

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

	if (g_mcuRevision >= MCU_REVISION_R3B3) {
    	MX_UART4_Init();
	} else {
    	MX_UART7_Init();
	}

#endif // EEZ_PLATFORM_STM32

	using namespace psu;
	if (g_ate && g_slotIndex == 2 && g_slots[0]->moduleType == MODULE_TYPE_DCP405) {
	    WATCHDOG_RESET(WATCHDOG_LONG_OPERATION);

	    g_slots[0]->initChannels();
		auto &channel = Channel::get(0);

		channel_dispatcher::setVoltage(channel, 40);
		channel_dispatcher::setCurrent(channel, 3.5);
		channel_dispatcher::outputEnable(channel, true);
		
		osDelay(100);
	}
}

void uploadHexFile() {
	bool success = false;
	bool dowloadStarted = false;
	if (syncWithSlave(g_slotIndex)) {
		bool eofReached = false;
	    File file;
	    psu::sd_card::BufferedFileRead bufferedFile(file);
	    size_t totalSize = 0;
		HexRecord hexRecord;
		uint32_t addressUpperBits = 0;

#if OPTION_DISPLAY
		psu::gui::showAsyncOperationInProgress("Erasing..");
#endif

		for (int ntry = 1; !eraseAll(g_slotIndex); ntry++) {
			DebugTrace("Failed to erase all!\n");
			if (ntry == 5) {
				goto Exit;
			}
			osDelay(10);
		}

	    if (!file.open(g_hexFilePath, FILE_OPEN_EXISTING | FILE_READ)) {
			DebugTrace("Can't open firmware hex file!\n");
			goto Exit;
	    }

		dowloadStarted = true;

#if OPTION_DISPLAY
		psu::gui::hideAsyncOperationInProgress();
    	psu::gui::showProgressPageWithoutAbort("Writing...");
		psu::gui::updateProgressPage(0, 0);

	    totalSize = file.size();
#endif

		while (!eofReached && readHexRecord(bufferedFile, hexRecord)) {
			size_t currentPosition = file.tell();

	#if OPTION_DISPLAY
			psu::gui::updateProgressPage(currentPosition, totalSize);
	#endif

			if (hexRecord.recordType == 0x04) {
				addressUpperBits = ((hexRecord.data[0] << 8) + hexRecord.data[1]) << 16;
			} else if (hexRecord.recordType == 0x00) {
				uint32_t address = addressUpperBits | hexRecord.address;
				if (!writeMemory(g_slotIndex, address, hexRecord.data, hexRecord.recordLength)) {
					DebugTrace("Failed to write memory at address %08x\n", address);
					break;
				}
			} else if (hexRecord.recordType == 0x01) {
				eofReached = true;
			}
		}

		file.close();

Exit:
#if OPTION_DISPLAY
		osDelay(100);
		if (dowloadStarted) {
			psu::gui::hideProgressPage();
		} else {
			psu::gui::hideAsyncOperationInProgress();
		}
		osDelay(100);
#endif

		if (eofReached) {
			uint16_t value = 0xA5A5;
			bp3c::eeprom::write(g_slotIndex, (const uint8_t *)&value, 2, 4);
			g_slots[g_slotIndex]->firmwareInstalled = true;

			success = true;
		} else {
#if OPTION_DISPLAY
			psu::gui::errorMessage("Downloading failed!");
#endif
		}
	} else {
		DebugTrace("Failed to sync with slave\n");

#if OPTION_DISPLAY
		osDelay(100);
		psu::gui::hideAsyncOperationInProgress();
		osDelay(100);
		psu::gui::errorMessage("Failed to start update!");
#endif
	}

	auto value = Value(g_slotIndex, VALUE_TYPE_SLOT_INFO);
	char slotInfo[64];
	value.toText(slotInfo, sizeof(slotInfo));
	if (success) {
		InfoTrace("%s firmware updated\n", slotInfo);
	} else {
		ErrorTrace("%s firmware update failed\n", slotInfo);
	}

	sendMessageToPsu(PSU_MESSAGE_FLASH_SLAVE_LEAVE_BOOTLOADER_MODE);
}

void leaveBootloaderMode() {
    g_bootloaderMode = false;

#if defined(EEZ_PLATFORM_STM32)
    // disable BOOT0 flag
    io_exp::writeToOutputPort(0b10000000);
    osDelay(5);
	io_exp::hardResetModules();

    psu::initChannels();
    psu::testChannels();
#endif

    psu::profile::recallFromLocation(10);

#if defined(EEZ_PLATFORM_STM32)
    HAL_UART_DeInit(phuart);
    psu::io_pins::refresh();
#endif

    g_ate = 0;
}

#if defined(EEZ_PLATFORM_STM32)

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

#endif

bool waitForAck(int slotIndex, uint32_t timeout = SPI_ACK_TIMEOUT) {
#if defined(EEZ_PLATFORM_STM32)
	uint32_t startTime = HAL_GetTick();
  	do {
		uint8_t txData = 0;
		uint8_t rxData;

    	taskENTER_CRITICAL();
		spi::select(slotIndex, spi::CHIP_SLAVE_MCU_NO_CRC);
		spi::transmit(slotIndex, &txData, 1);
		spi::deselect(slotIndex);

		spi::select(slotIndex, spi::CHIP_SLAVE_MCU_NO_CRC);
		spi::transfer(slotIndex, &txData, &rxData, 1);
		spi::deselect(slotIndex);
		taskEXIT_CRITICAL();

    	if (rxData == ACK) {
			// received ACK
			txData = ACK;

			taskENTER_CRITICAL();
			spi::select(slotIndex, spi::CHIP_SLAVE_MCU_NO_CRC);
			spi::transmit(slotIndex, &txData, 1);
			spi::deselect(slotIndex);
			taskEXIT_CRITICAL();

      		return true;
    	}
		
		if (rxData == NACK) {
      		// Received NACK
      		return false;
    	}
	} while (HAL_GetTick() - startTime < timeout);
#endif
    return false;
}

bool syncWithSlave(int slotIndex) {
#if defined(EEZ_PLATFORM_STM32)
	if (g_slots[slotIndex]->flashMethod == FLASH_METHOD_STM32_BOOTLOADER_SPI) {
		uint8_t txData;

		txData = BL_SPI_SOF;

		for (int ntry = 0; ntry < 5; ntry++) {
			taskENTER_CRITICAL();
			spi::select(slotIndex, spi::CHIP_SLAVE_MCU_NO_CRC);
			spi::transmit(slotIndex, &txData, 1);
			spi::deselect(slotIndex);
			taskEXIT_CRITICAL();

			if (waitForAck(slotIndex)) {
				return true;
			}
		}

		return false;
	} else {
		uint32_t startTime = HAL_GetTick();
		do {
			taskENTER_CRITICAL();
			sendDataNoCRC(ENTER_BOOTLOADER);
			uint8_t rxData[1];
			HAL_StatusTypeDef result = HAL_UART_Receive(phuart, rxData, 1, 100);
			taskEXIT_CRITICAL();
			if (result == HAL_OK && rxData[0] == ACK) {
				return true;
			}
		} while (HAL_GetTick() - startTime < SYNC_TIMEOUT);
		return false;
	}
#endif

#if defined(EEZ_PLATFORM_SIMULATOR)
    return true;
#endif
}

bool eraseAll(int slotIndex) {
#if defined(EEZ_PLATFORM_STM32)
	static uint8_t buffer[3] = { 0xFF, 0xFF, 0x00 };

	if (g_slots[slotIndex]->flashMethod == FLASH_METHOD_STM32_BOOTLOADER_SPI) {
		uint8_t txData[3];

		txData[0] = BL_SPI_SOF;
		txData[1] = CMD_EXTENDED_ERASE;
		txData[2] = CRC_MASK ^ CMD_EXTENDED_ERASE;

		taskENTER_CRITICAL();
		spi::select(slotIndex, spi::CHIP_SLAVE_MCU_NO_CRC);
		spi::transmit(slotIndex, txData, 3);
		spi::deselect(slotIndex);
		taskEXIT_CRITICAL();

		if (!waitForAck(slotIndex)) {
			return false;
		}

#if OPTION_DISPLAY
		psu::gui::showAsyncOperationInProgress("Erasing...");
#endif

		taskENTER_CRITICAL();
		spi::select(slotIndex, spi::CHIP_SLAVE_MCU_NO_CRC);
		spi::transmit(slotIndex, buffer, 3);
		spi::deselect(slotIndex);
		taskEXIT_CRITICAL();

		return waitForAck(slotIndex, SPI_EEPROM_ERASE_TIMEOUT);
	} else {
		taskENTER_CRITICAL();

		sendDataAndCRC(CMD_EXTENDED_ERASE);

		uint8_t rxData[1];
		HAL_StatusTypeDef result = HAL_UART_Receive(phuart, rxData, 1, CMD_TIMEOUT);
		if (result != HAL_OK || rxData[0] != ACK) {
			taskEXIT_CRITICAL();
			return false;
		}

		HAL_UART_Transmit(phuart, buffer, 3, 20);

		result = HAL_UART_Receive(phuart, rxData, 1, CMD_TIMEOUT);
		if (result != HAL_OK || rxData[0] != ACK) {
			taskEXIT_CRITICAL();
			return false;
		}

		taskEXIT_CRITICAL();
		return true;
	}
#endif

#if defined(EEZ_PLATFORM_SIMULATOR)
    return true;
#endif
}

bool writeMemory(int slotIndex, uint32_t address, const uint8_t *buffer, uint32_t bufferSize) {
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

	if (g_slots[slotIndex]->flashMethod == FLASH_METHOD_STM32_BOOTLOADER_SPI) {
		uint8_t txData[3];

		txData[0] = BL_SPI_SOF;
		txData[1] = CMD_WRITE_MEMORY;
		txData[2] = CRC_MASK ^ CMD_WRITE_MEMORY;

		taskENTER_CRITICAL();
		spi::select(slotIndex, spi::CHIP_SLAVE_MCU_NO_CRC);
		spi::transmit(slotIndex, txData, 3);
		spi::deselect(slotIndex);
		taskEXIT_CRITICAL();

		if (!waitForAck(slotIndex)) {
			return false;
		}

		taskENTER_CRITICAL();
		spi::select(slotIndex, spi::CHIP_SLAVE_MCU_NO_CRC);
		spi::transmit(slotIndex, addressAndCrc, 5);
		spi::deselect(slotIndex);
		taskEXIT_CRITICAL();

		if (!waitForAck(slotIndex)) {
			return false;
		}

		taskENTER_CRITICAL();
		spi::select(slotIndex, spi::CHIP_SLAVE_MCU_NO_CRC);
		spi::transmit(slotIndex, &numBytes, 1);
		spi::transmit(slotIndex, (uint8_t *)buffer, bufferSize);
		spi::transmit(slotIndex, &crc, 1);
		spi::deselect(slotIndex);
		taskEXIT_CRITICAL();

		return waitForAck(slotIndex);
	} else {
		taskENTER_CRITICAL();

		sendDataAndCRC(CMD_WRITE_MEMORY);

		uint8_t rxData[1];
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
	}
#endif

#if defined(EEZ_PLATFORM_SIMULATOR)
    osDelay(1);
    return true;
#endif
}

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
		osDelay(1);
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

} // namespace flash_slave
} // namespace bp3c
} // namespace eez
