/*
 * EEZ Generic Firmware
 * Copyright (C) 2018-present, Envox d.o.o.
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

#define SDRAM_START_ADDRESS 0xc0000000u

#define DISPLAY_WIDTH 480
#define DISPLAY_HEIGHT 272

#define DISPLAY_BPP 16

#define DISPLAY_BYTESPP (DISPLAY_BPP / 8)

#define VRAM_BUFFER_SIZE (DISPLAY_WIDTH * DISPLAY_HEIGHT * DISPLAY_BYTESPP)

#define VRAM_BUFFER1_START_ADDRESS (SDRAM_START_ADDRESS + 1 * 512 * 1024)
#define VRAM_BUFFER2_START_ADDRESS (SDRAM_START_ADDRESS + 2 * 512 * 1024)
#define VRAM_BUFFER3_START_ADDRESS (SDRAM_START_ADDRESS + 3 * 512 * 1024)
#define VRAM_BUFFER4_START_ADDRESS (SDRAM_START_ADDRESS + 4 * 512 * 1024)

#define DECOMPRESSED_ASSETS_START_ADDRESS SDRAM_START_ADDRESS
