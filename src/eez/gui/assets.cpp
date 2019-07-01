/* / system.h
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

#include <eez/gui/assets.h>

#include <assert.h>
#include <stdlib.h>

#include <eez/libs/lz4/lz4.h>

#include <eez/gui/document.h>
#include <eez/system.h>

#ifdef EEZ_PLATFORM_STM32
#include <eez/platform/stm32/defines.h>
#endif

namespace eez {
namespace gui {

static uint8_t *g_decompressedAssets;

Document *g_document;
Styles *g_styles;
uint8_t *g_fontsData;
uint8_t *g_bitmapsData;
Colors *g_colorsData;

void decompressAssets() {
    int compressedSize = sizeof(assets) - 4;

    // first 4 bytes (uint32_t) are decompressed size
    int decompressedSize = (int)(((uint32_t *)assets)[0]);

#ifdef EEZ_PLATFORM_SIMULATOR
    g_decompressedAssets = (uint8_t *)malloc(decompressedSize);
    if (!g_decompressedAssets) {
        shutdown();
        return;
    }
#else
    g_decompressedAssets = (uint8_t *)DECOMPRESSED_ASSETS_START_ADDRESS;
#endif

    int result = LZ4_decompress_safe((const char *)assets + 4, (char *)g_decompressedAssets,
                                     compressedSize, decompressedSize);

    assert(result == decompressedSize);

    g_document = (Document *)(g_decompressedAssets + ((uint32_t *)g_decompressedAssets)[0]);
    g_styles = (Styles *)(g_decompressedAssets + ((uint32_t *)g_decompressedAssets)[1]);
    g_fontsData = g_decompressedAssets + ((uint32_t *)g_decompressedAssets)[2];
    g_bitmapsData = g_decompressedAssets + ((uint32_t *)g_decompressedAssets)[3];
    g_colorsData = (Colors *)(g_decompressedAssets + ((uint32_t *)g_decompressedAssets)[4]);
}

} // namespace gui
} // namespace eez
