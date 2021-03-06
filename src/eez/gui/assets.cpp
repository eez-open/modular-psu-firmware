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

#if OPTION_DISPLAY

#include <assert.h>
#include <stdlib.h>
#include <memory.h>

#include <eez/system.h>
#include <eez/memory.h>
#include <eez/debug.h>

#include <eez/libs/lz4/lz4.h>

#include <eez/gui/gui.h>
#include <eez/gui/widget.h>

#include <eez/libs/sd_fat/sd_fat.h>

#include <scpi/scpi.h>

namespace eez {
namespace gui {

bool g_isMainAssetsLoaded;
Assets *g_mainAssets = (Assets *)DECOMPRESSED_ASSETS_START_ADDRESS;
Assets *g_externalAssets = (Assets *)EXTERNAL_ASSETS_BUFFER;

////////////////////////////////////////////////////////////////////////////////

static const uint32_t HEADER_TAG = 0x7A65657E;

struct Header {
	uint32_t tag; // HEADER_TAG
	uint16_t projectVersion;
	uint16_t assetsType;
	uint32_t decompressedSize;
};

////////////////////////////////////////////////////////////////////////////////

bool decompressAssetsData(const uint8_t *assetsData, uint32_t assetsDataSize, Assets *decompressedAssets, uint32_t maxDecompressedAssetsSize, int *err) {
	uint32_t compressedDataOffset;
	uint32_t decompressedSize;

	auto header = (Header *)assetsData;

	if (header->tag != HEADER_TAG) {
		if (err) {
			*err = SCPI_ERROR_INVALID_BLOCK_DATA;
		}
		return false;
	}

	decompressedAssets->projectVersion = header->projectVersion;

	compressedDataOffset = sizeof(Header);
	decompressedSize = header->decompressedSize;

	auto decompressedDataOffset = offsetof(Assets, pages);

	if (decompressedDataOffset + decompressedSize > maxDecompressedAssetsSize) {
		if (err) {
			*err = SCPI_ERROR_OUT_OF_DEVICE_MEMORY;
		}
		return false;
	}

	int compressedSize = assetsDataSize - compressedDataOffset;

    int decompressResult = LZ4_decompress_safe(
		(const char *)(assetsData + compressedDataOffset),
		(char *)decompressedAssets + decompressedDataOffset,
		compressedSize,
		decompressedSize
	);
	
	if (decompressResult != decompressedSize) {
		if (err) {
			*err = SCPI_ERROR_INVALID_BLOCK_DATA;
		}
		return false;
	}
	
	return true;
}

void loadMainAssets() {
	g_mainAssets->external = false;
	auto decompressResult = decompressAssetsData(assets, sizeof(assets), g_mainAssets, MAX_DECOMPRESSED_ASSETS_SIZE, nullptr);
	assert(decompressResult);
	g_isMainAssetsLoaded = true;
}

bool loadExternalAssets(const char *filePath, int *err) {
    eez::File file;
    if (!file.open(filePath, FILE_OPEN_EXISTING | FILE_READ)) {
        if (err) {
            *err = SCPI_ERROR_FILE_NAME_NOT_FOUND;
        }
        return false;
    }

    uint32_t fileSize = file.size();

    if (fileSize == 0) {
        if (err) {
            *err = SCPI_ERROR_MASS_STORAGE_ERROR;
        }
        return false;
    }

    if (fileSize > EXTERNAL_ASSETS_BUFFER_SIZE) {
        if (err) {
            *err = SCPI_ERROR_OUT_OF_DEVICE_MEMORY;
        }
        return false;
    }

    uint8_t *fileData = EXTERNAL_ASSETS_BUFFER + EXTERNAL_ASSETS_BUFFER_SIZE - ((fileSize + 3) / 4) * 4;
    uint32_t bytesRead = file.read(fileData, fileSize);
    file.close();

    if (bytesRead != fileSize) {
        if (err) {
            *err = SCPI_ERROR_MASS_STORAGE_ERROR;
        }
        return false;
    }

	g_externalAssets->external = true;

	return decompressAssetsData(fileData, fileSize, g_externalAssets, fileData - EXTERNAL_ASSETS_BUFFER, err);
}

////////////////////////////////////////////////////////////////////////////////

const PageAsset* getPageAsset(int pageId) {
	if (pageId > 0) {
		return g_mainAssets->pages.item(g_mainAssets, (pageId - 1));
	} else if (pageId < 0) {
		return g_externalAssets->pages.item(g_externalAssets, (-pageId - 1));
	}
	return nullptr;
}

const PageAsset* getPageAsset(int pageId, WidgetCursor& widgetCursor) {
	if (pageId < 0) {
		widgetCursor.assets = g_externalAssets;
	} else {
	    widgetCursor.assets = g_mainAssets;
    }
	return getPageAsset(pageId);
}

const Style *getStyle(int styleID) {
	if (styleID <= 0) {
		return nullptr;
	}
	return g_mainAssets->styles.item(g_mainAssets, styleID - 1);
}

const FontData *getFontData(int fontID) {
	if (fontID <= 0) {
		return nullptr;
	}
	return g_mainAssets->fonts.item(g_mainAssets, fontID - 1);
}

const Bitmap *getBitmap(int bitmapID) {
	if (bitmapID > 0) {
		return g_mainAssets->bitmaps.item(g_mainAssets, bitmapID - 1);
	} else if (bitmapID < 0) {
		return g_externalAssets->bitmaps.item(g_externalAssets, -bitmapID - 1);
	}
	return nullptr;
}

int getThemesCount() {
	return (int)g_mainAssets->colorsDefinition.ptr(g_mainAssets)->themes.count;
}

Theme *getTheme(int i) {
	return g_mainAssets->colorsDefinition.ptr(g_mainAssets)->themes.item(g_mainAssets, i);
}

const char *getThemeName(int i) {
	return getTheme(i)->name.ptr(g_mainAssets);
}

const uint32_t getThemeColorsCount(int themeIndex) {
	return getTheme(themeIndex)->colors.count;
}

const uint16_t *getThemeColors(int themeIndex) {
	return getTheme(themeIndex)->colors.ptr(g_mainAssets);
}

const uint16_t *getColors() {
	return g_mainAssets->colorsDefinition.ptr(g_mainAssets)->colors.ptr(g_mainAssets);
}

int getExternalAssetsFirstPageId() {
	return -1;
}

const char *getActionName(const WidgetCursor& widgetCursor, int16_t actionId) {
	// if (actionId == 0) {
	// 	return nullptr;
	// }
	// if (actionId < 0) {
	// 	actionId = -actionId;
	// }
	// actionId--;
	// return g_externalAssets->actionNames.item(g_externalAssets, actionId);
	return nullptr;
}

int16_t getDataIdFromName(const WidgetCursor& widgetCursor, const char *name) {
	// for (uint32_t i = 0; i < g_externalAssets->variableNames.count; i++) {
	// 	if (strcmp(g_externalAssets->variableNames.item(g_externalAssets, i), name) == 0) {
	// 		return -((int16_t)i + 1);
	// 	}
	// }
	return 0;
}

} // namespace gui
} // namespace eez

#endif
