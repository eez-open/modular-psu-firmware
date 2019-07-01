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

#include <scpi/scpi.h>

#include <eez/system.h>

#include <eez/apps/psu/psu.h>

#include <eez/apps/psu/eeprom.h>

#include <i2c.h>

// EEPROM AT24C256C
// I2C-Compatible (2-Wire) Serial EEPROM
// 256-Kbit (32,768 x 8)
// http://ww1.microchip.com/downloads/en/devicedoc/atmel-8568-seeprom-at24c256c-datasheet.pdf

namespace eez {
namespace psu {
namespace eeprom {

static const uint16_t EEPROM_ADDRESS = 0b10100000;

TestResult g_testResult = TEST_FAILED;

////////////////////////////////////////////////////////////////////////////////

const int MAX_READ_CHUNK_SIZE = 16;

bool read(uint8_t *buffer, uint16_t bufferSize, uint16_t address) {
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

const int MAX_WRITE_CHUNK_SIZE = 16;

bool write(const uint8_t *buffer, uint16_t bufferSize, uint16_t address) {
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
        generateError(SCPI_ERROR_EXT_EEPROM_TEST_FAILED);
    }

    return g_testResult != TEST_FAILED;
}

} // namespace eeprom
} // namespace psu
} // namespace eez
