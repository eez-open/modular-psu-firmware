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

#pragma once

#include <stdint.h>

namespace eez {
namespace spi {

extern SPI_HandleTypeDef *handle[];

extern GPIO_TypeDef *IRQ_GPIO_Port[];
extern const uint16_t IRQ_Pin[];

static const int CHIP_DAC = 0;
static const int CHIP_ADC = 1;
static const int CHIP_IOEXP = 2;
static const int CHIP_TEMP_SENSOR = 3;
static const int CHIP_SLAVE_MCU = 4;
static const int CHIP_SLAVE_MCU_NO_CRC = 5;

void init(uint8_t slotIndex, int chip);

void select(uint8_t slotIndex, int chip);
void deselect(uint8_t slotIndex);

HAL_StatusTypeDef transfer1(uint8_t slotIndex, uint8_t *input, uint8_t *output);
HAL_StatusTypeDef transfer2(uint8_t slotIndex, uint8_t *input, uint8_t *output);
HAL_StatusTypeDef transfer3(uint8_t slotIndex, uint8_t *input, uint8_t *output);
HAL_StatusTypeDef transfer4(uint8_t slotIndex, uint8_t *input, uint8_t *output);
HAL_StatusTypeDef transfer5(uint8_t slotIndex, uint8_t *input, uint8_t *output);

HAL_StatusTypeDef transfer(uint8_t slotIndex, uint8_t *input, uint8_t *output, uint16_t size);

HAL_StatusTypeDef transmit(uint8_t slotIndex, uint8_t *input, uint16_t size);

HAL_StatusTypeDef transferDMA(uint8_t slotIndex, uint8_t *input, uint8_t *output, uint16_t size);

} // namespace spi
} // namespace eez
