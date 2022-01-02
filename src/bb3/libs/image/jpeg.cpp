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

#if defined(EEZ_PLATFORM_STM32)
#include <jpeg.h>
extern "C" {
#include <jpeg_utils.h>
#define YCBCR_420_BLOCK_SIZE       384     /* YCbCr 4:2:0 MCU : 4 8x8 blocks of Y + 1 8x8 block of Cb + 1 8x8 block of Cr   */
#define YCBCR_422_BLOCK_SIZE       256     /* YCbCr 4:2:2 MCU : 2 8x8 blocks of Y + 1 8x8 block of Cb + 1 8x8 block of Cr   */
#define YCBCR_444_BLOCK_SIZE       192     /* YCbCr 4:4:4 MCU : 1 8x8 block of Y + 1 8x8 block of Cb + 1 8x8 block of Cr   */
}
#endif

#include "toojpeg.h"

#include <bb3/system.h>
#include <eez/core/debug.h>
#include <eez/core/memory.h>
#include <eez/core/util.h>
#include <eez/fs/fs.h>
#include <bb3/libs/image/jpeg.h>

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

uint8_t *g_fileData;

#if defined(EEZ_PLATFORM_STM32)

static bool g_jpegInitialized;

#else

extern "C" void * const g_jpegDecodeContext = (void *)FILE_VIEW_BUFFER;

#define NJ_USE_LIBC 0
#define NJ_USE_WIN32 0
extern "C" {
#include "nanojpeg.c"
}

static uint8_t *g_decodeBuffer = FILE_VIEW_BUFFER + sizeof(nj_context_t);

uint8_t *g_decodeDynamicMemory;

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

#endif

ImageDecodeResult jpegDecode(const char *filePath, Image *image) {
    eez::File file;
    if (!file.open(filePath, FILE_OPEN_EXISTING | FILE_READ)) {
        return IMAGE_DECODE_ERR_FILE_NOT_FOUND;
    }

    uint32_t fileSize = file.size();

    if (fileSize == 0) {
        file.close();
        return IMAGE_DECODE_ERR_FILE_READ;
    }

    if (fileSize > FILE_VIEW_BUFFER_SIZE) {
        file.close();
        return IMAGE_DECODE_ERR_NOT_ENOUGH_MEMORY;
    }    

    g_fileData = FILE_VIEW_BUFFER + FILE_VIEW_BUFFER_SIZE - ((fileSize + 3) / 4) * 4;
    uint32_t bytesRead = file.read(g_fileData, fileSize);
    file.close();

    if (bytesRead != fileSize) {
        return IMAGE_DECODE_ERR_FILE_READ;
    }

#if defined(EEZ_PLATFORM_STM32)
    if (!g_jpegInitialized) {
    	JPEG_InitColorTables();

    	hjpeg.Instance = JPEG;
    	HAL_JPEG_Init(&hjpeg);

    	g_jpegInitialized = true;
    }

    uint32_t availableMemoryForDecode = g_fileData - FILE_VIEW_BUFFER;

    if (HAL_JPEG_Decode(&hjpeg, g_fileData, fileSize, FILE_VIEW_BUFFER, availableMemoryForDecode, 5000) != HAL_OK) {
        return IMAGE_DECODE_ERR_DECODE;
    }

    JPEG_ConfTypeDef jpegInfo;
	if (HAL_JPEG_GetInfo(&hjpeg, &jpegInfo) != HAL_OK) {
        return IMAGE_DECODE_ERR_DECODE;
	}

    uint32_t width = jpegInfo.ImageWidth;
    uint32_t height = jpegInfo.ImageHeight;
    if (width > 480 || height > 272) {
        return IMAGE_DECODE_ERR_IMAGE_SIZE_UNSUPPORTED;
    }

    uint32_t blockSize;
    uint32_t lineOffset = 0;

    if (jpegInfo.ChromaSubsampling == JPEG_420_SUBSAMPLING) {
        blockSize = YCBCR_420_BLOCK_SIZE;
        if ((width % 16) != 0) {
            lineOffset = 16 - (width % 16);
        }
        if ((height % 16) != 0) {
            jpegInfo.ImageHeight += 16 - (height % 16);
        }
    } else if (jpegInfo.ChromaSubsampling == JPEG_422_SUBSAMPLING) {
        blockSize = YCBCR_422_BLOCK_SIZE;
        if ((width % 16) != 0) {
            lineOffset = 16 - (width % 16);
        }
        if ((height % 8) != 0) {
            jpegInfo.ImageHeight += 8 - (height % 8);
        }
    } else {
    	// JPEG_444_SUBSAMPLING
    	blockSize = YCBCR_444_BLOCK_SIZE;
        if ((width % 8) != 0) {
            lineOffset = 8 - (width % 8);
        }
        if ((height % 8) != 0) {
            jpegInfo.ImageHeight += 8 - (height % 8);
        }
    }

    jpegInfo.ImageWidth += lineOffset;

    JPEG_YCbCrToRGB_Convert_Function convertFunction;
    uint32_t numMCUs;
    if (JPEG_GetDecodeColorConvertFunc(&jpegInfo, &convertFunction, &numMCUs) != HAL_OK) {
        return IMAGE_DECODE_ERR_DECODE;
    }

    uint32_t inputBufferSize = numMCUs * blockSize;
    uint8_t *outputBuffer = FILE_VIEW_BUFFER + inputBufferSize;

    if (inputBufferSize + width * height * 2 > availableMemoryForDecode) {
        // not enough memory to decode JPEG
        return IMAGE_DECODE_ERR_NOT_ENOUGH_MEMORY;
    }

    uint32_t convertedDataCount;
    convertFunction(FILE_VIEW_BUFFER, outputBuffer, 0, inputBufferSize, &convertedDataCount);

    image->width = width;
    image->height = height;
    image->bpp = 16;
    image->lineOffset = lineOffset;
    image->pixels = outputBuffer;

    return IMAGE_DECODE_OK;

#else

    g_decodeDynamicMemory = g_decodeBuffer;

    njInit();

    if (njDecode(g_fileData, fileSize) != NJ_OK) {
        return IMAGE_DECODE_ERR_DECODE;
    }

    if (njGetWidth() > 480 || njGetHeight() > 272 || !njIsColor() || njGetImageSize() > 480 * 272 * 3) {
        return IMAGE_DECODE_ERR_IMAGE_SIZE_UNSUPPORTED;
    }

    image->width = njGetWidth();
    image->height = njGetHeight();
    image->bpp = 24;
    image->lineOffset = 0;
    image->pixels = njGetImage();

    return IMAGE_DECODE_OK;

#endif
}
