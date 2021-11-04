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

#include <stdint.h>

#include <eez/memory.h>
#include <eez/libs/sd_fat/sd_fat.h>
#include <eez/libs/image/bitmap.h>

    // const uint8_t buffer[] = {
    //     // BMP Header (14 bytes)

    //     // ID field (42h, 4Dh): BM
    //     0x42, 0x4D,
    //     // size of BMP file
    //     0x36, 0xFA, 0x05, 0x00, // 14 + 40 + (480 * 272 * 3) = 391734‬ = 0x5FA36
    //     // unused
    //     0x00, 0x00,
    //     // unused
    //     0x00, 0x00,
    //     // Offset where the pixel array (bitmap data) can be found
    //     0x36, 0x00, 0x00, 0x00, // 53 = 0x36

    //     // DIB Header (40 bytes)

    //     // Number of bytes in the DIB header (from this point)
    //     0x28, 0x00, 0x00, 0x00, // 40 = 0x28
    //     // Width of the bitmap in pixels
    //     0xE0, 0x01, 0x00, 0x00, // 480 = 0x1E0
    //     // Height of the bitmap in pixels. Positive for bottom to top pixel order.
    //     0x10, 0x01, 0x00, 0x00, // 272 = 0x110
    //     // Number of color planes being used
    //     0x01, 0x00,
    //     // Number of bits per pixel
    //     0x18, 0x00, // 24 = 0x18
    //     // BI_RGB, no pixel array compression used
    //     0x00, 0x00, 0x00, 0x00,
    //     // Size of the raw bitmap data (including padding)
    //     0x00, 0xFA, 0x05, 0x00, // 480 * 272 * 3 = 0x5FA00
    //     // Print resolution of the image,
    //     // 72 DPI × 39.3701 inches per metre yields 2834.6472
    //     0x13, 0x0B, 0x00, 0x00, // 2835 pixels/metre horizontal
    //     0x13, 0x0B, 0x00, 0x00, // 2835 pixels/metre vertical
    //     // Number of colors in the palette
    //     0x00, 0x00, 0x00, 0x00,
    //     // 0 means all colors are important
    //     0x00, 0x00, 0x00, 0x00,
    // };

uint32_t readUint32(const uint8_t *bytes) {
    return bytes[0] | (bytes[1] << 8) | (bytes[2] << 16) | (bytes[3] << 24);
}

uint16_t readUint16(const uint8_t *bytes) {
    return bytes[0] | (bytes[1] << 8);
}

ImageDecodeResult bitmapDecode(const char *filePath, Image *image) {
    eez::File file;
    if (!file.open(filePath, FILE_OPEN_EXISTING | FILE_READ)) {
        return IMAGE_DECODE_ERR_FILE_NOT_FOUND;
    }

    uint32_t bytesRead;

    uint8_t bmpHeader[14];

    bytesRead = file.read(bmpHeader, sizeof(bmpHeader));
    if (bytesRead != sizeof(bmpHeader)) {
        file.close();
        return IMAGE_DECODE_ERR_FILE_READ;
    }

    uint8_t dibHeader[40];

    bytesRead = file.read(dibHeader, sizeof(dibHeader));
    if (bytesRead != sizeof(dibHeader)) {
        file.close();
        return IMAGE_DECODE_ERR_FILE_READ;
    }

    uint32_t offset = readUint32(bmpHeader + 10);

    uint32_t width = readUint32(dibHeader + 4);
    if (width > 480) {
        file.close();
        return IMAGE_DECODE_ERR_FILE_READ;
    }

    uint32_t height = readUint32(dibHeader + 8);
    if (height > 272) {
        file.close();
        return IMAGE_DECODE_ERR_FILE_READ;
    }

    uint16_t numColorPlanes = readUint16(dibHeader + 12);
    if (numColorPlanes != 1) {
        file.close();
        return IMAGE_DECODE_ERR_FILE_READ;
    }

    uint16_t bitsPerPixel = readUint16(dibHeader + 14);
    if (bitsPerPixel != 24) {
        file.close();
        return IMAGE_DECODE_ERR_FILE_READ;
    }

    uint32_t lineBytes = width * 3;

    offset += height * lineBytes;

    auto lineBuffer = FILE_VIEW_BUFFER;

    for (uint32_t line = 0; line < height; line++) {
        offset -= lineBytes;
        if (!file.seek(offset)) {
            file.close();
            return IMAGE_DECODE_ERR_FILE_READ;
        }

        bytesRead = file.read(lineBuffer, lineBytes);
        if (bytesRead != lineBytes) {
            file.close();
            return IMAGE_DECODE_ERR_FILE_READ;
        }

        for (uint32_t i = 0; i < lineBytes; i += 3) {
            auto temp = lineBuffer[i];
            lineBuffer[i] = lineBuffer[i + 2];
            lineBuffer[i + 2] = temp;
        }

        lineBuffer += lineBytes;
    }

    file.close();

    image->width = width;
    image->height = height;
    image->bpp = 24;
    image->lineOffset = 0;
    image->pixels = FILE_VIEW_BUFFER;

    return IMAGE_DECODE_OK;
}
