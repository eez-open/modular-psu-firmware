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

#include <eez/os.h>

namespace eez {
namespace eeprom {

// opcodes
static const uint8_t WREN = 6;
static const uint8_t WRDI = 4;
static const uint8_t RDSR = 5;
static const uint8_t WRSR = 1;
static const uint8_t READ = 3;
static const uint8_t WRITE = 2;

static const uint16_t EEPROM_ONTIME_START_ADDRESS = 64;

static const uint16_t EEPROM_SIZE = 32768;

void init();
bool test();

extern TestResult g_testResult;

bool read(uint8_t *buffer, uint16_t buffer_size, uint16_t address);
bool write(const uint8_t *buffer, uint16_t buffer_size, uint16_t address);

} // namespace eeprom
} // namespace eez
