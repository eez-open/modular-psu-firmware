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

#include <eez/index.h>

static const uint16_t MODULE_REVISION_DCP405_R1B1  = 0x0101;
static const uint16_t MODULE_REVISION_DCP405_R2B5  = 0x0205;
static const uint16_t MODULE_REVISION_DCP405_R2B7  = 0x0207;
static const uint16_t MODULE_REVISION_DCP405_R2B11 = 0x020B;
static const uint16_t MODULE_REVISION_DCP405_R3B1  = 0x0301;
static const uint16_t MODULE_REVISION_DCP405_R3B3  = 0x0303;

namespace eez {
namespace dcp405 {

extern Module *g_module;

bool isDacRampActive();
void tickDacRamp(uint32_t tickCount);

} // namespace dcp405
} // namespace eez
