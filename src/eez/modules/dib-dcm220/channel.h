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

static const uint16_t MODULE_REVISION_DCM220_R2B4  = 0x0204;
static const uint16_t MODULE_REVISION_DCM224_R1B1  = 0x0101;

namespace eez {
namespace dcm220 {

extern ModuleInfo *g_dcm220ModuleInfo;
extern ModuleInfo *g_dcm224ModuleInfo;

} // namespace dcm220
} // namespace eez
