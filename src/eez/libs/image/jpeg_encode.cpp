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

#include <stdlib.h>
#include <stdint.h>
#include <assert.h>

#include "toojpeg.h"

#include <eez/system.h>

#if defined(EEZ_PLATFORM_STM32)
#include <eez/platform/stm32/defines.h>
static const size_t MAX_IMAGE_DATA_SIZE = VRAM_SCREENSHOOT_JPEG_OUT_BUFFER_SIZE;
static uint8_t *g_imageData = (uint8_t *)VRAM_SCREENSHOOT_JPEG_OUT_BUFFER;
#endif

#if defined(EEZ_PLATFORM_SIMULATOR)
static const size_t MAX_IMAGE_DATA_SIZE = 256 * 1024;
static uint8_t g_imageData[MAX_IMAGE_DATA_SIZE];
#endif

static size_t g_imageDataSize;

void WRITE_ONE_BYTE(unsigned char byte) {
	g_imageData[g_imageDataSize++] = byte;
}

int jpegEncode(const uint8_t *screenshotPixels, unsigned char **imageData, size_t *imageDataSize) {
	g_imageDataSize = 0;
	TooJpeg::writeJpeg(WRITE_ONE_BYTE, screenshotPixels, 480, 272);
    *imageData = g_imageData;
    *imageDataSize = g_imageDataSize;
    return 0;
}
