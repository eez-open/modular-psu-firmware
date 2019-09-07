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

#include <memory.h>

#ifdef EEZ_PLATFORM_STM32
#include <i2c.h>
#endif

#if defined(EEZ_PLATFORM_SIMULATOR)
#include <stdio.h>
#endif

#include <scpi/scpi.h>

#include <eez/system.h>
#include <eez/index.h>

#include <eez/apps/psu/psu.h>

#include <eez/modules/dcpX05/eeprom.h>

// DCP405:
//   EEPROM M24C32
//   32-Kbit serial I²C bus EEPROM
//   https://www.st.com/resource/en/datasheet/cd00001012.pdf

// DCP505:
//   EEPROM AT24C04D
//   4-Kbit (512 x 8) I²C-Compatible (2-wire) Serial EEPROM
//   https://www.st.com/resource/en/datasheet/cd00001012.pdf

namespace eez {
namespace dcpX05 {
namespace eeprom {

#ifdef EEZ_PLATFORM_STM32
static const uint16_t EEPROM_ADDRESS[] = { 
    0b10100010, 
    0b10100100, 
    0b10100110 
};
#endif

TestResult g_testResult = TEST_FAILED;

////////////////////////////////////////////////////////////////////////////////

#ifdef EEZ_PLATFORM_STM32
const int MAX_READ_CHUNK_SIZE = 16;
#endif

bool read(uint8_t slotIndex, uint8_t *buffer, uint16_t bufferSize, uint16_t address) {

#ifdef EEZ_PLATFORM_STM32
    for (uint16_t i = 0; i < bufferSize; i += MAX_READ_CHUNK_SIZE) {
        uint16_t chunkAddress = address + i;

        uint16_t chunkSize = MIN(MAX_READ_CHUNK_SIZE, bufferSize - i);

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
        fp = fopen(filePath, "w+b");
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

#ifdef EEZ_PLATFORM_STM32
const int MAX_WRITE_CHUNK_SIZE = 16;
#endif

bool write(uint8_t slotIndex, const uint8_t *buffer, uint16_t bufferSize, uint16_t address) {

#ifdef EEZ_PLATFORM_STM32
    for (uint16_t i = 0; i < bufferSize; i += MAX_WRITE_CHUNK_SIZE) {
        uint16_t chunkAddress = address + i;

        uint16_t chunkSize = MIN(MAX_WRITE_CHUNK_SIZE, bufferSize - i);

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
        uint8_t verify[MAX_WRITE_CHUNK_SIZE];

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

    if (g_testResult == TEST_FAILED) {
        psu::generateError(SCPI_ERROR_EXT_EEPROM_TEST_FAILED);
    }

    return g_testResult != TEST_FAILED;
}

} // namespace eeprom
} // namespace dcpX05
} // namespace eez
