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

#include <assert.h>
#include <stdlib.h>
#include <memory.h>

#include <eez/alloc.h>
#include <eez/system.h>
#include <eez/memory.h>
#include <eez/debug.h>
#include <eez/flow/flow.h>

#include <eez/libs/lz4/lz4.h>

#include <eez/gui/gui.h>
#include <eez/gui/widget.h>
#include <eez/gui_conf.h>

#include <scpi/scpi.h>

namespace eez {
namespace gui {

bool g_isMainAssetsLoaded;
Assets *g_mainAssets = (Assets *)DECOMPRESSED_ASSETS_START_ADDRESS;
Assets *g_externalAssets;

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

// disable warning: offsetof within non-standard-layout type ... is conditionally-supported [-Winvalid-offsetof]
#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Winvalid-offsetof"
#endif

	auto decompressedDataOffset = offsetof(Assets, pages);

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif


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
	
	if (decompressResult != (int)decompressedSize) {
		if (err) {
			*err = SCPI_ERROR_INVALID_BLOCK_DATA;
		}
		return false;
	}
	
	return true;
}

void loadMainAssets(const uint8_t *assets, uint32_t assetsSize) {
	g_mainAssets->external = false;
	auto decompressedSize = decompressAssetsData(assets, assetsSize, g_mainAssets, MAX_DECOMPRESSED_ASSETS_SIZE, nullptr);
	assert(decompressedSize);
	g_isMainAssetsLoaded = true;
}

void unloadExternalAssets() {
	if (g_externalAssets) {
		free(g_externalAssets);
		g_externalAssets = nullptr;
	}
}

////////////////////////////////////////////////////////////////////////////////

const PageAsset* getPageAsset(int pageId) {
	if (pageId > 0) {
		return g_mainAssets->pages.item(g_mainAssets, (pageId - 1));
	} else if (pageId < 0) {
		if (g_externalAssets == nullptr) {
			return nullptr;
		}
		return g_externalAssets->pages.item(g_externalAssets, (-pageId - 1));
	}
	return nullptr;
}

const PageAsset* getPageAsset(int pageId, WidgetCursor& widgetCursor) {
	if (pageId < 0) {
		widgetCursor.assets = g_externalAssets;
		widgetCursor.flowState = flow::getFlowState(pageId, widgetCursor);
	} else {
	    widgetCursor.assets = g_mainAssets;
    }
	return getPageAsset(pageId);
}

const Style *getStyle(int styleID) {
	if (styleID > 0) {
		return g_mainAssets->styles.item(g_mainAssets, styleID - 1);
	} else if (styleID < 0) {
		if (g_externalAssets == nullptr) {
			return getStyle(EEZ_CONF_STYLE_ID_DEFAULT);
		}
		return g_externalAssets->styles.item(g_externalAssets, -styleID - 1);
	}
	return getStyle(EEZ_CONF_STYLE_ID_DEFAULT);
}

const FontData *getFontData(int fontID) {
	if (fontID > 0) {
		return g_mainAssets->fonts.item(g_mainAssets, fontID - 1);
	} else if (fontID < 0) {
		if (g_externalAssets == nullptr) {
			return nullptr;
		}
		return g_externalAssets->fonts.item(g_externalAssets, -fontID - 1);
	}
	return nullptr;
}

const Bitmap *getBitmap(int bitmapID) {
	if (bitmapID > 0) {
		return g_mainAssets->bitmaps.item(g_mainAssets, bitmapID - 1);
	} else if (bitmapID < 0) {
		if (g_externalAssets == nullptr) {
			return nullptr;
		}
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

int getExternalAssetsMainPageId() {
	return -1;
}

const char *getActionName(const WidgetCursor &widgetCursor, int16_t actionId) {
	if (actionId == 0) {
		return nullptr;
	}

	if (actionId < 0) {
		actionId = -actionId;
	}
	actionId--;

	if (!widgetCursor.assets) {
		return "";
	}

	return widgetCursor.assets->actionNames.item(widgetCursor.assets, actionId);
}

int16_t getDataIdFromName(const WidgetCursor &widgetCursor, const char *name) {
	if (!widgetCursor.assets) {
		return 0;
	}

	for (uint32_t i = 0; i < widgetCursor.assets->variableNames.count; i++) {
		if (strcmp(widgetCursor.assets->variableNames.item(widgetCursor.assets, i), name) == 0) {
			return -((int16_t)i + 1);
		}
	}
	return 0;
}

} // namespace gui
} // namespace eez
