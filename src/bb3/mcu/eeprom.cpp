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

#include <eez/eeprom.h>
#include <eez/util.h>

#include <bb3/system.h>
#include <bb3/mcu/eeprom.h>

namespace eez {
namespace eeprom {

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
} // namespace eez
