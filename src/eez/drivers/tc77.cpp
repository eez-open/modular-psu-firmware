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

#include <eez/drivers/tc77.h>

#include <main.h>
#include <eez/platform/stm32/spi.h>

#include <eez/modules/psu/psu.h>
#include <eez/modules/dcpX05/ioexp.h>

// TC77 Thermal Sensor with SPI Interface
// http://www.ti.com/lit/ds/symlink/tmp1075.pdf

namespace eez {

using namespace psu;

namespace drivers {
namespace tc77 {

float readTemperature(uint8_t slotIndex) {
    uint8_t output[2] = { 0, 0 };
    uint8_t result[2];

    spi::select(slotIndex, spi::CHIP_TEMP_SENSOR);
    spi::transfer(slotIndex, output, result, 2);
    spi::deselect(slotIndex);

    uint16_t adc = (((uint16_t)result[0]) << 8) | result[1];

    adc >>= 3;
    if (adc & 0x1000) {
        adc |= 0xE000;
    }

    float temperature = roundPrec((int16_t)adc * 0.0625f, 1);

    return temperature;
}

} // namespace tc77
} // namespace drivers
} // namespace eez
