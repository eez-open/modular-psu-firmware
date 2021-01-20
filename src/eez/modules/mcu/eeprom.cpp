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

#include <memory.h>

#include <eez/system.h>
#include <eez/modules/psu/psu.h>
#include <eez/modules/mcu/eeprom.h>

#define USE_EEPROM (defined(EEZ_PLATFORM_STM32) && !CONF_SURVIVE_MODE)

#if defined(EEZ_PLATFORM_STM32)
#include <i2c.h>
#endif

#if !USE_EEPROM
#include <eez/libs/sd_fat/sd_fat.h>
#endif

#include <scpi/scpi.h>

namespace eez {
namespace mcu {
namespace eeprom {

#if defined(EEZ_PLATFORM_STM32)
// EEPROM AT24C256C
// I2C-Compatible (2-Wire) Serial EEPROM
// 256-Kbit (32,768 x 8)
// http://ww1.microchip.com/downloads/en/devicedoc/atmel-8568-seeprom-at24c256c-datasheet.pdf
static const uint16_t EEPROM_ADDRESS = 0b10100000;
#endif

TestResult g_testResult = TEST_FAILED;

////////////////////////////////////////////////////////////////////////////////

#if defined(EEZ_PLATFORM_STM32)

const int MAX_READ_CHUNK_SIZE = 16;
const int MAX_WRITE_CHUNK_SIZE = 16;

bool readFromEEPROM(uint8_t *buffer, uint16_t bufferSize, uint16_t address) {
    for (uint16_t i = 0; i < bufferSize; i += MAX_READ_CHUNK_SIZE) {
        uint16_t chunkAddress = address + i;

        uint16_t chunkSize = MIN(MAX_READ_CHUNK_SIZE, bufferSize - i);

        uint8_t data[2] = {
                I2C_MEM_ADD_MSB(chunkAddress),
                I2C_MEM_ADD_LSB(chunkAddress)
        };

        HAL_StatusTypeDef returnValue;

        taskENTER_CRITICAL();
        returnValue = HAL_I2C_Master_Transmit(&hi2c1, EEPROM_ADDRESS, data, 2, HAL_MAX_DELAY);
        if (returnValue != HAL_OK) {
            taskEXIT_CRITICAL();
            return false;
        }
        returnValue = HAL_I2C_Master_Receive(&hi2c1, EEPROM_ADDRESS, buffer + i, chunkSize, HAL_MAX_DELAY);
        taskEXIT_CRITICAL();
        if (returnValue != HAL_OK) {
            return false;
        }
    }

    return true;
}

bool writeToEEPROM(const uint8_t *buffer, uint16_t bufferSize, uint16_t address) {
    for (uint16_t i = 0; i < bufferSize; i += MAX_WRITE_CHUNK_SIZE) {
        uint16_t chunkAddress = address + i;

        uint16_t chunkSize = MIN(MAX_WRITE_CHUNK_SIZE, bufferSize - i);

        HAL_StatusTypeDef returnValue;

        taskENTER_CRITICAL();
        returnValue = HAL_I2C_Mem_Write(&hi2c1, EEPROM_ADDRESS, chunkAddress, I2C_MEMADD_SIZE_16BIT, (uint8_t *)buffer + i, chunkSize, HAL_MAX_DELAY);
        taskEXIT_CRITICAL();

        if (returnValue != HAL_OK) {
            return false;
        }

        delay(5);

        // verify
        uint8_t verify[MAX_WRITE_CHUNK_SIZE];

        uint8_t data[2] = {
                I2C_MEM_ADD_MSB(chunkAddress),
                I2C_MEM_ADD_LSB(chunkAddress)
        };

        taskENTER_CRITICAL();
        returnValue = HAL_I2C_Master_Transmit(&hi2c1, EEPROM_ADDRESS, data, 2, HAL_MAX_DELAY);
        if (returnValue != HAL_OK) {
            taskEXIT_CRITICAL();
            return false;
        }
        returnValue = HAL_I2C_Master_Receive(&hi2c1, EEPROM_ADDRESS, verify, chunkSize, HAL_MAX_DELAY);
        taskEXIT_CRITICAL();
        if (returnValue != HAL_OK) {
            return false;
        }

        for (int j = 0; j < chunkSize; ++j) {
            if (buffer[i+j] != verify[j]) {
                return false;
            }
        }
    }

    return true;
}

#endif

#if !USE_EEPROM
const char *EEPROM_FILE_PATH = "/EEPROM.state";
#endif

bool read(uint8_t *buffer, uint16_t bufferSize, uint16_t address) {
#if USE_EEPROM
    return readFromEEPROM(buffer, bufferSize, address);
#else
    File file;
    if (!file.open(EEPROM_FILE_PATH, FILE_READ)) {
        return false;
    }
    file.seek(address);
    size_t readBytes = file.read(buffer, bufferSize);
    if (readBytes < bufferSize) {
        memset(buffer + readBytes, 0xFF, bufferSize - readBytes);
    }
    file.close();
    return true;
#endif
}

bool write(const uint8_t *buffer, uint16_t bufferSize, uint16_t address) {
#if USE_EEPROM
    return writeToEEPROM(buffer, bufferSize, address);
#else
    File file;
    if (!file.open(EEPROM_FILE_PATH, FILE_OPEN_ALWAYS | FILE_WRITE)) {
        return false;
    }
    file.seek(address);
    file.write(buffer, bufferSize);
    file.close();
    return true;
#endif
}

void init() {
#if !USE_EEPROM
    File file;
    if (!file.open(EEPROM_FILE_PATH, FILE_READ)) {
        if (file.open(EEPROM_FILE_PATH, FILE_CREATE_ALWAYS | FILE_WRITE)) {
#if defined(EEZ_PLATFORM_STM32)
			uint8_t bufferTemp[MAX_WRITE_CHUNK_SIZE];
			for (size_t address = 0; address < EEPROM_SIZE; address += MAX_WRITE_CHUNK_SIZE) {
				readFromEEPROM(bufferTemp, MAX_WRITE_CHUNK_SIZE, address);
				file.write(bufferTemp, MAX_WRITE_CHUNK_SIZE);
				WATCHDOG_RESET(WATCHDOG_LONG_OPERATION);
			}
#endif
        	file.close();
        }
    }
#endif

	g_testResult = TEST_OK;
}

bool test() {
    // TODO add test
    return g_testResult == TEST_OK;
}

void resetAllExceptOnTimeCounters() {
    uint8_t buffer[64];
    
    memset(buffer, 0xFF, 64);

    uint32_t address;

    for (address = 0; address < EEPROM_ONTIME_START_ADDRESS; address += 64) {
        WATCHDOG_RESET(WATCHDOG_LONG_OPERATION);
        write(buffer, MIN(EEPROM_ONTIME_START_ADDRESS - address, 64), (uint16_t)address);
    }

    for (address = EEPROM_ONTIME_START_ADDRESS + 6 * sizeof(uint32_t); address < EEPROM_SIZE; address += 64) {
        WATCHDOG_RESET(WATCHDOG_LONG_OPERATION);
        write(buffer, MIN(EEPROM_SIZE - address, 64), (uint16_t)address);
    }
}

} // namespace eeprom
} // namespace mcu
} // namespace eez
