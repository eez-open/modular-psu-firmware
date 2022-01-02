/*
 * EEZ Modular Firmware
 * Copyright (C) 2020-present, Envox d.o.o.
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

#if defined(EEZ_PLATFORM_STM32)
#include <main.h>
#include <crc.h>
#include <bb3/platform/stm32/spi.h>
#include <stdlib.h>
#endif

#include <eez/core/debug.h>
#include <bb3/index.h>
#include <bb3/system.h>
#include <bb3/tasks.h>

#include <bb3/bp3c/comm.h>

#include "scpi/scpi.h"

#define SPI_SLAVE_SYNBYTE         0x53
#define SPI_MASTER_SYNBYTE        0xAC

#define CONF_MASTER_SYNC_TIMEOUT_MS 1000

namespace eez {
namespace bp3c {
namespace comm {

bool masterSynchro(int slotIndex) {
    auto &slot = *g_slots[slotIndex];

#if defined(EEZ_PLATFORM_STM32)
    uint32_t start = millis();

    uint8_t txBuffer[15] = { SPI_MASTER_SYNBYTE, (uint8_t)(slot.moduleRevision >> 8), (uint8_t)(slot.moduleRevision & 0xFF) };
    uint8_t rxBuffer[15];

    while (true) {
        WATCHDOG_RESET(WATCHDOG_LONG_OPERATION);

        if (HAL_GPIO_ReadPin(spi::IRQ_GPIO_Port[slotIndex], spi::IRQ_Pin[slotIndex]) == GPIO_PIN_RESET) {
            spi::select(slotIndex, spi::CHIP_SLAVE_MCU);
            auto result = spi::transfer(slotIndex, txBuffer, rxBuffer, sizeof(rxBuffer));
            spi::deselect(slotIndex);

            if (result == HAL_OK && rxBuffer[0] == SPI_SLAVE_SYNBYTE) {
				slot.firmwareVersionAcquired = true;
				slot.firmwareMajorVersion = rxBuffer[1];
				slot.firmwareMinorVersion = rxBuffer[2];
				slot.idw0 = (rxBuffer[3] << 24) | (rxBuffer[4] << 16) | (rxBuffer[5] << 8) | rxBuffer[6];
				slot.idw1 = (rxBuffer[7] << 24) | (rxBuffer[8] << 16) | (rxBuffer[9] << 8) | rxBuffer[10];
				slot.idw2 = (rxBuffer[11] << 24) | (rxBuffer[12] << 16) | (rxBuffer[13] << 8) | rxBuffer[14];
				return true;
            }
        }

        int32_t diff = millis() - start;
        if (diff > CONF_MASTER_SYNC_TIMEOUT_MS) {
            break;
        }

        osDelay(1);
    }

    slot.firmwareVersionAcquired = true;
    slot.firmwareMajorVersion = 0;
    slot.firmwareMinorVersion = 0;
    slot.idw0 = 0;
    slot.idw1 = 0;
    slot.idw2 = 0;
    return false;
#endif

#if defined(EEZ_PLATFORM_SIMULATOR)
    slot.firmwareVersionAcquired = true;
    slot.firmwareMajorVersion = 1;
    slot.firmwareMinorVersion = 0;
    slot.idw0 = 0;
    slot.idw1 = 0;
    slot.idw2 = 0;
    return true;
#endif
}

TransferResult transfer(int slotIndex, uint8_t *output, uint8_t *input, uint32_t bufferSize) {
#if defined(EEZ_PLATFORM_STM32)
    spi::handle[slotIndex]->ErrorCode = 0;

    spi::select(slotIndex, spi::CHIP_SLAVE_MCU);
    auto result = spi::transfer(slotIndex, output, input, bufferSize);
    spi::deselect(slotIndex);

    if (g_slots[slotIndex]->spiCrcCalculationEnable) {
        if (spi::handle[slotIndex]->ErrorCode == HAL_SPI_ERROR_CRC) {
            return TRANSFER_STATUS_CRC_ERROR;
        } else {
            return (TransferResult)result;
        }
    } else {
        if (result == HAL_OK) {
            uint32_t crc = HAL_CRC_Calculate(&hcrc, (uint32_t *)input, bufferSize - 4);
            return crc == *((uint32_t *)(input + bufferSize - 4)) ? TRANSFER_STATUS_OK : TRANSFER_STATUS_CRC_ERROR;
        } else {
            return (TransferResult)result;
        }
    }
#endif

#if defined(EEZ_PLATFORM_SIMULATOR)
    return TRANSFER_STATUS_OK;
#endif
}

TransferResult transferDMA(int slotIndex, uint8_t *output, uint8_t *input, uint32_t bufferSize) {
#if defined(EEZ_PLATFORM_STM32)
    spi::handle[slotIndex]->ErrorCode = 0;

    spi::select(slotIndex, spi::CHIP_SLAVE_MCU);
    auto result = spi::transferDMA(slotIndex, output, input, bufferSize);
    return (TransferResult)result;
#endif

#if defined(EEZ_PLATFORM_SIMULATOR)
    return TRANSFER_STATUS_OK;
#endif
}

void abortTransfer(int slotIndex) {
#if defined(EEZ_PLATFORM_STM32)
	spi::abortTransfer(slotIndex);
    spi::deselect(slotIndex);
#endif
}

void updateParamsStart() {
    if (isLowPriorityThread()) {
#if defined(EEZ_PLATFORM_STM32)        
        taskENTER_CRITICAL();
#endif
    }
}

bool updateParamsFinish(const void *updateParams, const void *lastTransferedParams, size_t paramsSize, uint32_t timeout, int *err) {
    if (isLowPriorityThread()) {
#if defined(EEZ_PLATFORM_STM32)        
        taskEXIT_CRITICAL();
#endif

        uint32_t start = millis();
        while (true) {
            if (memcmp(updateParams, lastTransferedParams, paramsSize) == 0) {
                return true;
            }
            if (millis() - start > timeout) {
                if (err) {
                    *err = SCPI_ERROR_TIME_OUT;
                }
                return false;
            }
        }
    }

    return true;
}

} // namespace comm
} // namespace bp3c
} // namespace eez
