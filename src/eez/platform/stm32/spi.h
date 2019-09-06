/*
 * EEZ Generic Firmware
 * Copyright (C) 2019-present, Envox d.o.o.
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
namespace psu {
namespace spi {

static const int CHIP_DAC = 0;
static const int CHIP_ADC = 1;
static const int CHIP_IOEXP = 2;
static const int CHIP_TEMP_SENSOR = 3;
static const int CHIP_DCM220 = 4;

void select(uint8_t slotIndex, int chip);
void deselect(uint8_t slotIndex);

void transfer(uint8_t slotIndex, uint8_t *input, uint8_t *output, uint16_t size);
void transmit(uint8_t slotIndex, uint8_t *input, uint16_t size);
void receive(uint8_t slotIndex, uint8_t *output, uint16_t size);

} // namespace spi
} // namespace psu
} // namespace eez
