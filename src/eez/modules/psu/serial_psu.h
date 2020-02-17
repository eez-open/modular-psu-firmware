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

#include <eez/modules/psu/scpi/psu.h>

namespace eez {
namespace psu {
namespace serial {

extern TestResult g_testResult;
extern scpi_t g_scpiContext;

void init();

#define SERIAL_INPUT_AVAILABLE 1
#define SERIAL_LINE_STATE_CHANGED 2

void onQueueMessage(uint32_t type, uint32_t param);

bool isConnected();

void update();

}
}
} // namespace eez::psu::serial
