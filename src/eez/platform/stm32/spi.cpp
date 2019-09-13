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

#include <spi.h>
#include <cmsis_os.h>

#include <eez/platform/stm32/spi.h>

#include <eez/index.h>

namespace eez {
namespace psu {
namespace spi {

static SPI_HandleTypeDef *spiHandle[] = { &hspi2, &hspi4, &hspi5 };

static GPIO_TypeDef *SPI_CSA_GPIO_Port[] = { SPI2_CSA_GPIO_Port, SPI4_CSA_GPIO_Port, SPI5_CSA_GPIO_Port };
static GPIO_TypeDef *SPI_CSB_GPIO_Port[] = { SPI2_CSB_GPIO_Port, SPI4_CSB_GPIO_Port, SPI5_CSB_GPIO_Port };

static const uint16_t SPI_CSA_Pin[] = { SPI2_CSA_Pin, SPI4_CSA_Pin, SPI5_CSA_Pin };
static const uint16_t SPI_CSB_Pin[] = { SPI2_CSB_Pin, SPI4_CSB_Pin, SPI5_CSB_Pin };

void init(uint8_t slotIndex, int chip) {
    // if (chip == CHIP_IOEXP ) {
    // 	spiHandle[slotIndex]->Init.Direction = SPI_DIRECTION_2LINES;
    //     WRITE_REG(spiHandle[slotIndex]->Instance->CR1, SPI_MODE_MASTER | SPI_DIRECTION_2LINES | SPI_POLARITY_LOW | SPI_PHASE_1EDGE | (SPI_NSS_SOFT & SPI_CR1_SSM) | SPI_BAUDRATEPRESCALER_16 | SPI_FIRSTBIT_MSB | SPI_CRCCALCULATION_DISABLE);

	// 	spiHandle[slotIndex]->Init.DataSize = SPI_DATASIZE_8BIT;
  	// 	WRITE_REG(spiHandle[slotIndex]->Instance->CR2, (((SPI_NSS_SOFT >> 16U) & SPI_CR2_SSOE) | SPI_TIMODE_DISABLE | SPI_NSS_PULSE_ENABLE | SPI_DATASIZE_8BIT) | SPI_RXFIFO_THRESHOLD_QF);
    // } else if (chip == CHIP_TEMP_SENSOR) {
    // 	spiHandle[slotIndex]->Init.Direction = SPI_DIRECTION_1LINE;
    //     WRITE_REG(spiHandle[slotIndex]->Instance->CR1, SPI_MODE_MASTER | SPI_DIRECTION_1LINE  | SPI_POLARITY_LOW | SPI_PHASE_1EDGE | (SPI_NSS_SOFT & SPI_CR1_SSM) | SPI_BAUDRATEPRESCALER_32 | SPI_FIRSTBIT_MSB | SPI_CRCCALCULATION_DISABLE);

	// 	spiHandle[slotIndex]->Init.DataSize = SPI_DATASIZE_16BIT;
  	// 	WRITE_REG(spiHandle[slotIndex]->Instance->CR2, (((SPI_NSS_SOFT >> 16U) & SPI_CR2_SSOE) | SPI_TIMODE_DISABLE | SPI_NSS_PULSE_ENABLE | SPI_DATASIZE_16BIT) | SPI_RXFIFO_THRESHOLD_HF);
	// } else {
	// 	// ADC & DAC
	// 	spiHandle[slotIndex]->Init.Direction = SPI_DIRECTION_2LINES;
    //     WRITE_REG(spiHandle[slotIndex]->Instance->CR1, SPI_MODE_MASTER | SPI_DIRECTION_2LINES | SPI_POLARITY_LOW | SPI_PHASE_2EDGE | (SPI_NSS_SOFT & SPI_CR1_SSM) | SPI_BAUDRATEPRESCALER_16 | SPI_FIRSTBIT_MSB | SPI_CRCCALCULATION_DISABLE);

	// 	spiHandle[slotIndex]->Init.DataSize = SPI_DATASIZE_8BIT;
  	// 	WRITE_REG(spiHandle[slotIndex]->Instance->CR2, (((SPI_NSS_SOFT >> 16U) & SPI_CR2_SSOE) | SPI_TIMODE_DISABLE | SPI_NSS_PULSE_ENABLE | SPI_DATASIZE_8BIT) | SPI_RXFIFO_THRESHOLD_QF);
    // }

    if (chip == CHIP_DCM220) {
    	WRITE_REG(spiHandle[slotIndex]->Instance->CR1, SPI_MODE_MASTER | SPI_DIRECTION_2LINES | SPI_POLARITY_LOW | SPI_PHASE_1EDGE | (SPI_NSS_SOFT & SPI_CR1_SSM) | SPI_BAUDRATEPRESCALER_8 | SPI_FIRSTBIT_MSB | SPI_CRCCALCULATION_DISABLE);
    } else if (chip == CHIP_IOEXP || chip == CHIP_TEMP_SENSOR) {
        WRITE_REG(spiHandle[slotIndex]->Instance->CR1, SPI_MODE_MASTER | SPI_DIRECTION_2LINES | SPI_POLARITY_LOW | SPI_PHASE_1EDGE | (SPI_NSS_SOFT & SPI_CR1_SSM) | SPI_BAUDRATEPRESCALER_16 | SPI_FIRSTBIT_MSB | SPI_CRCCALCULATION_DISABLE);
    } else {
        WRITE_REG(spiHandle[slotIndex]->Instance->CR1, SPI_MODE_MASTER | SPI_DIRECTION_2LINES | SPI_POLARITY_LOW | SPI_PHASE_2EDGE | (SPI_NSS_SOFT & SPI_CR1_SSM) | SPI_BAUDRATEPRESCALER_16 | SPI_FIRSTBIT_MSB | SPI_CRCCALCULATION_DISABLE);
    }
}

void selectA(uint8_t slotIndex) {
	HAL_GPIO_WritePin(SPI_CSA_GPIO_Port[slotIndex], SPI_CSA_Pin[slotIndex], GPIO_PIN_RESET);
}

void deselectA(uint8_t slotIndex) {
	HAL_GPIO_WritePin(SPI_CSA_GPIO_Port[slotIndex], SPI_CSA_Pin[slotIndex], GPIO_PIN_SET);
}

void selectB(uint8_t slotIndex) {
	HAL_GPIO_WritePin(SPI_CSB_GPIO_Port[slotIndex], SPI_CSB_Pin[slotIndex], GPIO_PIN_RESET);
}

void deselectB(uint8_t slotIndex) {
	HAL_GPIO_WritePin(SPI_CSB_GPIO_Port[slotIndex], SPI_CSB_Pin[slotIndex], GPIO_PIN_SET);
}

void select(uint8_t slotIndex, int chip) {
	taskENTER_CRITICAL();

	if (g_slots[slotIndex].moduleType == MODULE_TYPE_DCM220) {
		selectA(slotIndex);
		return;
	}

	__HAL_SPI_DISABLE(spiHandle[slotIndex]);

	init(slotIndex, chip);

	// __HAL_SPI_ENABLE(spiHandle[slotIndex]);

    if (g_slots[slotIndex].moduleType == MODULE_TYPE_DCP405) {
    	if (chip == CHIP_DAC) {
			// 00
    		selectA(slotIndex);
    		selectB(slotIndex);
		} else if (chip == CHIP_ADC) {
    		// 01
    		deselectA(slotIndex);
    		selectB(slotIndex);
    	} else if (chip == CHIP_IOEXP) {
    		// 10
    		selectA(slotIndex);
    		deselectB(slotIndex);
    	} else {
    		// TEMP_SENSOR
    		// 11
    		deselectA(slotIndex);
    		deselectB(slotIndex);
    	}
    } else {
    	if (chip == CHIP_DAC) {
			// 00
    		selectA(slotIndex);
    		selectB(slotIndex);
		} else if (chip == CHIP_ADC) {
    		// 01
    		deselectA(slotIndex);
    		selectB(slotIndex);
    	} else {
    		// IOEXP
    		// 11
    		deselectA(slotIndex);
    		deselectB(slotIndex);
    	}
    }
}

void deselect(uint8_t slotIndex) {
	if (g_slots[slotIndex].moduleType == MODULE_TYPE_DCM220) {
		deselectA(slotIndex);
	} else if (g_slots[slotIndex].moduleType == MODULE_TYPE_DCP405) {
		// 01 ADC
		deselectA(slotIndex);
		selectB(slotIndex);
	} else {
		// 10
		selectA(slotIndex);
		deselectB(slotIndex);
	}

	taskEXIT_CRITICAL();
}

void transfer(uint8_t slotIndex, uint8_t *input, uint8_t *output, uint16_t size) {
    HAL_SPI_TransmitReceive(spiHandle[slotIndex], input, output, size, 10);
}

void transmit(uint8_t slotIndex, uint8_t *input, uint16_t size) {
    HAL_SPI_Transmit(spiHandle[slotIndex], input, size, 10);
}

void receive(uint8_t slotIndex, uint8_t *output, uint16_t size) {
    HAL_SPI_Receive(spiHandle[slotIndex], output, size, 10);
}

} // namespace spi
} // namespace psu
} // namespace eez
