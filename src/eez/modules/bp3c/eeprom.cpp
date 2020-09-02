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

#ifdef EEZ_PLATFORM_STM32
#include <i2c.h>
#endif

#if defined(EEZ_PLATFORM_SIMULATOR)
#include <stdio.h>
#endif

#include <eez/system.h>
#include <eez/index.h>
#include <eez/modules/psu/psu.h>
#include <eez/modules/bp3c/eeprom.h>

#include <scpi/scpi.h>

// DCP405:
//   EEPROM M24C32
//   32-Kbit serial I²C bus EEPROM
//   https://www.st.com/resource/en/datasheet/cd00001012.pdf

namespace eez {
namespace bp3c {
namespace eeprom {

#ifdef EEZ_PLATFORM_STM32

#if EEZ_MCU_REVISION_R1B2
static const uint16_t EEPROM_ADDRESS[] = { 
    0b10100010, 
    0b10100100, 
    0b10101110
};
#endif

#if EEZ_MCU_REVISION_R1B5
static const uint16_t EEPROM_ADDRESS[] = {
    0b10100010,
    0b10100100,
    0b10100110
};
#endif

#endif

TestResult g_testResult = TEST_FAILED;

////////////////////////////////////////////////////////////////////////////////

#ifdef EEZ_PLATFORM_STM32
const int PAGE_SIZE = 32;
#endif

bool read(uint8_t slotIndex, uint8_t *buffer, uint16_t bufferSize, uint16_t address) {

#ifdef EEZ_PLATFORM_STM32
    for (uint16_t i = 0; i < bufferSize; i += PAGE_SIZE) {
        uint16_t chunkAddress = address + i;

        uint16_t chunkSize = MIN(PAGE_SIZE, bufferSize - i);

        uint8_t data[2];
        data[0] = I2C_MEM_ADD_MSB(chunkAddress);
        data[1] = I2C_MEM_ADD_LSB(chunkAddress);

        const uint16_t eepromAddress = EEPROM_ADDRESS[slotIndex];

        taskENTER_CRITICAL();
        HAL_I2C_Master_Transmit(&hi2c1, eepromAddress, data, 2, HAL_MAX_DELAY);
        HAL_StatusTypeDef returnValue = HAL_I2C_Master_Receive(&hi2c1, eepromAddress, buffer + i, chunkSize, HAL_MAX_DELAY);
        taskEXIT_CRITICAL();

        if (returnValue != HAL_OK) {
            return false;
        }
    }

    return true;
#endif

#if defined(EEZ_PLATFORM_SIMULATOR)
    char fileName[20];
    sprintf(fileName, "EEPROM_SLOT%d.state", slotIndex + 1);
    char *filePath = getConfFilePath(fileName);
    FILE *fp = fopen(filePath, "r+b");
    if (fp == NULL) {
        writeModuleType(slotIndex, MODULE_TYPE_DCP405);
        fp = fopen(filePath, "r+b");
    }
    
    if (fp == NULL) {
        return false;
    }

    fseek(fp, address, SEEK_SET);
    size_t readBytes = fread(buffer, 1, bufferSize, fp);
    fclose(fp);

    return readBytes == bufferSize;

    return true;
#endif

}

bool write(uint8_t slotIndex, const uint8_t *buffer, uint16_t bufferSize, uint16_t address) {

#ifdef EEZ_PLATFORM_STM32
    for (uint16_t i = 0; i < bufferSize; i += PAGE_SIZE) {
        uint16_t chunkAddress = address + i;

        uint16_t chunkSize = MIN(PAGE_SIZE, bufferSize - i);

        const uint16_t eepromAddress = EEPROM_ADDRESS[slotIndex];

        taskENTER_CRITICAL();
        HAL_StatusTypeDef returnValue = HAL_I2C_Mem_Write(
            &hi2c1, eepromAddress, chunkAddress, I2C_MEMADD_SIZE_16BIT, (uint8_t *)buffer + i, chunkSize, HAL_MAX_DELAY);
        taskEXIT_CRITICAL();

        if (returnValue != HAL_OK) {
            return false;
        }

        delay(5);

        // verify
        uint8_t verify[PAGE_SIZE];

        uint8_t data[2];
		data[0] = I2C_MEM_ADD_MSB(chunkAddress);
		data[1] = I2C_MEM_ADD_LSB(chunkAddress);

        taskENTER_CRITICAL();
        returnValue = HAL_I2C_Master_Transmit(&hi2c1, eepromAddress, data, 2, HAL_MAX_DELAY);
        if (returnValue != HAL_OK) {
            taskEXIT_CRITICAL();
            return false;
        }
        returnValue = HAL_I2C_Master_Receive(
                &hi2c1, eepromAddress, verify, chunkSize, HAL_MAX_DELAY);
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
#endif

#if defined(EEZ_PLATFORM_SIMULATOR)
    char fileName[20];
    sprintf(fileName, "EEPROM_SLOT%d.state", slotIndex + 1);
    char *filePath = getConfFilePath(fileName);
    FILE *fp = fopen(filePath, "r+b");
    if (fp == NULL) {
        fp = fopen(filePath, "w+b");
    }
    if (fp == NULL) {
        return false;
    }

    fseek(fp, address, SEEK_SET);
    fwrite(buffer, 1, bufferSize, fp);
    fclose(fp);

    return true;
#endif

}

void init() {
}

bool test() {
#if OPTION_EXT_EEPROM
    // TODO add test
    g_testResult = TEST_OK;
#else
    g_testResult = TEST_SKIPPED;
#endif

    return g_testResult != TEST_FAILED;
}

void writeModuleType(uint8_t slotIndex, uint16_t moduleType) {
    uint16_t buffer[] = {
        moduleType,
        getModule(moduleType)->latestModuleRevision
    };
    write(slotIndex, (uint8_t *)buffer, 4, 0);
}

void resetAllExceptOnTimeCounters(uint8_t slotIndex) {
    uint8_t buffer[64];
    
    memset(buffer, 0xFF, 64);

    uint32_t address;

    for (address = 0; address < EEPROM_ONTIME_START_ADDRESS; address += 64) {
        WATCHDOG_RESET();
        write(slotIndex, buffer, MIN(EEPROM_ONTIME_START_ADDRESS - address, 64), (uint16_t)address);
    }

    for (address = EEPROM_ONTIME_START_ADDRESS + 6 * sizeof(uint32_t); address < EEPROM_SIZE; address += 64) {
        WATCHDOG_RESET();
        write(slotIndex, buffer, MIN(EEPROM_SIZE - address, 64), (uint16_t)address);
    }
}

} // namespace eeprom
} // namespace bp3c
} // namespace eez
