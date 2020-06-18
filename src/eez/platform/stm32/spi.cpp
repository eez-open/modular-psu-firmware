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

#if defined(EEZ_PLATFORM_STM32)

#include <spi.h>
#include <cmsis_os.h>

#include <eez/platform/stm32/spi.h>

#include <eez/index.h>

namespace eez {
namespace spi {

SPI_HandleTypeDef *handle[] = { &hspi2, &hspi4, &hspi5 };

static GPIO_TypeDef *SPI_CSA_GPIO_Port[] = { SPI2_CSA_GPIO_Port, SPI4_CSA_GPIO_Port, SPI5_CSA_GPIO_Port };
static GPIO_TypeDef *SPI_CSB_GPIO_Port[] = { SPI2_CSB_GPIO_Port, SPI4_CSB_GPIO_Port, SPI5_CSB_GPIO_Port };

static const uint16_t SPI_CSA_Pin[] = { SPI2_CSA_Pin, SPI4_CSA_Pin, SPI5_CSA_Pin };
static const uint16_t SPI_CSB_Pin[] = { SPI2_CSB_Pin, SPI4_CSB_Pin, SPI5_CSB_Pin };

static int g_chip[] = { -1, -1, -1 };

GPIO_TypeDef *IRQ_GPIO_Port[] = { SPI2_IRQ_GPIO_Port, SPI4_IRQ_GPIO_Port, SPI5_IRQ_GPIO_Port };
const uint16_t IRQ_Pin[] = { SPI2_IRQ_Pin, SPI4_IRQ_Pin, SPI5_IRQ_Pin };

void select(uint8_t slotIndex, int chip) {
	taskENTER_CRITICAL();

	auto &slot = *g_slots[slotIndex];

	if (g_chip[slotIndex] != chip) {
		__HAL_SPI_DISABLE(handle[slotIndex]);

		if (chip == CHIP_SLAVE_MCU) {
			uint32_t crcCalculation = (slot.moduleInfo->spiCrcCalculationEnable ? SPI_CRCCALCULATION_ENABLE : SPI_CRCCALCULATION_DISABLE);
			WRITE_REG(handle[slotIndex]->Instance->CR1, SPI_MODE_MASTER | SPI_DIRECTION_2LINES | SPI_POLARITY_LOW | SPI_PHASE_1EDGE | (SPI_NSS_SOFT & SPI_CR1_SSM) | slot.moduleInfo->spiBaudRatePrescaler | SPI_FIRSTBIT_MSB | crcCalculation);
			handle[slotIndex]->Init.CRCCalculation = crcCalculation;
		} else if (chip == CHIP_SLAVE_MCU_BOOTLOADER) {
			// CRC calculation should be always disabled for bootloader
			WRITE_REG(handle[slotIndex]->Instance->CR1, SPI_MODE_MASTER | SPI_DIRECTION_2LINES | SPI_POLARITY_LOW | SPI_PHASE_1EDGE | (SPI_NSS_SOFT & SPI_CR1_SSM) | slot.moduleInfo->spiBaudRatePrescaler | SPI_FIRSTBIT_MSB | SPI_CRCCALCULATION_DISABLE);
			handle[slotIndex]->Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
		} else if (chip == CHIP_IOEXP || chip == CHIP_TEMP_SENSOR) {
			WRITE_REG(handle[slotIndex]->Instance->CR1, SPI_MODE_MASTER | SPI_DIRECTION_2LINES | SPI_POLARITY_LOW | SPI_PHASE_1EDGE | (SPI_NSS_SOFT & SPI_CR1_SSM) | SPI_BAUDRATEPRESCALER_16 | SPI_FIRSTBIT_MSB | SPI_CRCCALCULATION_DISABLE);
			handle[slotIndex]->Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
		} else {
			WRITE_REG(handle[slotIndex]->Instance->CR1, SPI_MODE_MASTER | SPI_DIRECTION_2LINES | SPI_POLARITY_LOW | SPI_PHASE_2EDGE | (SPI_NSS_SOFT & SPI_CR1_SSM) | SPI_BAUDRATEPRESCALER_16 | SPI_FIRSTBIT_MSB | SPI_CRCCALCULATION_DISABLE);
			handle[slotIndex]->Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
		}

		g_chip[slotIndex] = chip;

		// __HAL_SPI_ENABLE(handle[slotIndex]);
	}

    if (slot.moduleInfo->moduleType == MODULE_TYPE_DCP405) {
    	if (chip == CHIP_DAC) {
			// 00
    		HAL_GPIO_WritePin(SPI_CSA_GPIO_Port[slotIndex], SPI_CSA_Pin[slotIndex], GPIO_PIN_RESET);
    		HAL_GPIO_WritePin(SPI_CSB_GPIO_Port[slotIndex], SPI_CSB_Pin[slotIndex], GPIO_PIN_RESET);
		} else if (chip == CHIP_ADC) {
    		// 01
    		HAL_GPIO_WritePin(SPI_CSA_GPIO_Port[slotIndex], SPI_CSA_Pin[slotIndex], GPIO_PIN_SET);
    		HAL_GPIO_WritePin(SPI_CSB_GPIO_Port[slotIndex], SPI_CSB_Pin[slotIndex], GPIO_PIN_RESET);
    	} else if (chip == CHIP_IOEXP) {
    		// 10
    		HAL_GPIO_WritePin(SPI_CSA_GPIO_Port[slotIndex], SPI_CSA_Pin[slotIndex], GPIO_PIN_RESET);
    		HAL_GPIO_WritePin(SPI_CSB_GPIO_Port[slotIndex], SPI_CSB_Pin[slotIndex], GPIO_PIN_SET);
    	} else {
    		// TEMP_SENSOR
    		// 11
    		HAL_GPIO_WritePin(SPI_CSA_GPIO_Port[slotIndex], SPI_CSA_Pin[slotIndex], GPIO_PIN_SET);
    		HAL_GPIO_WritePin(SPI_CSB_GPIO_Port[slotIndex], SPI_CSB_Pin[slotIndex], GPIO_PIN_SET);
    	}
    } else {
		HAL_GPIO_WritePin(SPI_CSA_GPIO_Port[slotIndex], SPI_CSA_Pin[slotIndex], GPIO_PIN_RESET);
	}
}

void deselect(uint8_t slotIndex) {
	auto &slot = *g_slots[slotIndex];

	if (slot.moduleInfo->moduleType == MODULE_TYPE_DCP405) {
		// 01 ADC
		HAL_GPIO_WritePin(SPI_CSA_GPIO_Port[slotIndex], SPI_CSA_Pin[slotIndex], GPIO_PIN_SET);
		HAL_GPIO_WritePin(SPI_CSB_GPIO_Port[slotIndex], SPI_CSB_Pin[slotIndex], GPIO_PIN_RESET);
	} else {
		HAL_GPIO_WritePin(SPI_CSA_GPIO_Port[slotIndex], SPI_CSA_Pin[slotIndex], GPIO_PIN_SET);
	}

	taskEXIT_CRITICAL();
}

HAL_StatusTypeDef transfer(uint8_t slotIndex, uint8_t *input, uint8_t *output, uint16_t size) {
    return HAL_SPI_TransmitReceive(handle[slotIndex], input, output, size, 100);
}

HAL_StatusTypeDef transmit(uint8_t slotIndex, uint8_t *input, uint16_t size) {
    return HAL_SPI_Transmit(handle[slotIndex], input, size, 100);
}

HAL_StatusTypeDef receive(uint8_t slotIndex, uint8_t *output, uint16_t size) {
    return HAL_SPI_Receive(handle[slotIndex], output, size, 100);
}

} // namespace spi
} // namespace eez

#endif // EEZ_PLATFORM_STM32