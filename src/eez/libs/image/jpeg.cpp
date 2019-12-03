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
#include <memory.h>
#include <assert.h>

#include "toojpeg.h"

#include <eez/system.h>
#include <eez/debug.h>
#include <eez/libs/sd_fat/sd_fat.h>

#include <eez/memory.h>

static size_t g_imageDataSize;

void WRITE_ONE_BYTE(unsigned char byte) {
	VRAM_SCREENSHOOT_JPEG_OUT_BUFFER[g_imageDataSize++] = byte;
}

int jpegEncode(const uint8_t *screenshotPixels, unsigned char **imageData, size_t *imageDataSize) {
	g_imageDataSize = 0;
	TooJpeg::writeJpeg(WRITE_ONE_BYTE, screenshotPixels, 480, 272);
    *imageData = VRAM_SCREENSHOOT_JPEG_OUT_BUFFER;
    *imageDataSize = g_imageDataSize;
    return 0;
}

extern "C" void * const g_jpegDecodeContext = (void *)FILE_VIEW_BUFFER;

#define NJ_USE_LIBC 0
#define NJ_USE_WIN32 0
extern "C" {
#include "nanojpeg.c"
}

static uint8_t *g_decodeBuffer = FILE_VIEW_BUFFER + sizeof(nj_context_t);
static const size_t DECODE_BUFFER_SIZE = FILE_VIEW_BUFFER_SIZE - sizeof(nj_context_t);

uint8_t *g_decodeDynamicMemory;
uint8_t *g_fileData;

extern "C" void* njAllocMem(int size) {
    if (g_decodeDynamicMemory + size > g_fileData) {
        return nullptr;
    }
    auto temp = g_decodeDynamicMemory;
    g_decodeDynamicMemory += size;

    // DebugTrace("alloc %d %p\n", size, temp);

    return temp;
}

extern "C" void njFreeMem(void* block) {
    // DebugTrace("free %p\n", block);
}

extern "C" void njFillMem(void* block, unsigned char byte, int size) {
    memset(block, byte, size);
}

extern "C" void njCopyMem(void* dest, const void* src, int size) {
    memcpy(dest, src, size);
}

uint8_t *jpegDecode(const char *filePath) {
    // DebugTrace("alloc %d\n", sizeof(nj_context_t));

    uint32_t fileSize;
    uint32_t bytesRead;

    eez::File file;
    if (!file.open(filePath, FILE_OPEN_EXISTING | FILE_READ)) {
        goto ErrorNoClose;
    }

    fileSize = file.size();
    if (fileSize == 0 || fileSize > DECODE_BUFFER_SIZE) {
        goto Error;
    }

    g_fileData = g_decodeBuffer + DECODE_BUFFER_SIZE - fileSize;
    bytesRead = file.read(g_fileData, fileSize);
    if (bytesRead != fileSize) {
        goto Error;
    }

    file.close();

    g_decodeDynamicMemory = g_decodeBuffer;

    njInit();

    if (njDecode(g_fileData, fileSize) != NJ_OK) {
        goto Error;
    }

    if (njGetWidth() != 480 || njGetHeight() != 272 || !njIsColor() || njGetImageSize() != 480 * 272 * 3) {
        goto Error;
    }

    return njGetImage();

Error:
    file.close();

ErrorNoClose:
    return nullptr;
}
