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

#define SDRAM_START_ADDRESS 0xc0000000u
#define SDRAM_SIZE (8 * 1024 * 1024)

#define DISPLAY_WIDTH 480
#define DISPLAY_HEIGHT 272

#define DECOMPRESSED_ASSETS_START_ADDRESS SDRAM_START_ADDRESS

#define DLOG_BUFFER (SDRAM_START_ADDRESS + 3 * 512 * 1024)
#define DLOG_BUFFER_SIZE (1024 * 1024)

#define VRAM_SCREENSHOOT_JPEG_OUT_BUFFER (DLOG_BUFFER + DLOG_BUFFER_SIZE)
#define VRAM_SCREENSHOOT_JPEG_OUT_BUFFER_SIZE (256 * 1024)
#define VRAM_SCREENSHOOT_BUFFER_START_ADDRESS (VRAM_SCREENSHOOT_JPEG_OUT_BUFFER + VRAM_SCREENSHOOT_JPEG_OUT_BUFFER_SIZE)

#define VRAM_SCREENSHOOT_BUFFER_SIZE (DISPLAY_WIDTH * DISPLAY_HEIGHT * 3) // RGB888

#define VRAM_BUFFER1_START_ADDRESS (VRAM_SCREENSHOOT_BUFFER_START_ADDRESS + VRAM_SCREENSHOOT_BUFFER_SIZE)

#define VRAM_BUFFER_SIZE (DISPLAY_WIDTH * DISPLAY_HEIGHT * 2) // RGB565

#define VRAM_BUFFER2_START_ADDRESS (VRAM_BUFFER1_START_ADDRESS + VRAM_BUFFER_SIZE)
#define VRAM_BUFFER3_START_ADDRESS (VRAM_BUFFER2_START_ADDRESS + VRAM_BUFFER_SIZE)
#define VRAM_BUFFER4_START_ADDRESS (VRAM_BUFFER3_START_ADDRESS + VRAM_BUFFER_SIZE)
#define VRAM_BUFFER5_START_ADDRESS (VRAM_BUFFER4_START_ADDRESS + VRAM_BUFFER_SIZE)

