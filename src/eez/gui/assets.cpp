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

static const uint32_t HEADER_TAG = 0x7A65657E;

struct Header {
	uint32_t tag; // HEADER_TAG
	uint16_t projectVersion;
	uint16_t assetsType;
	uint32_t decompressedSize;
};

////////////////////////////////////////////////////////////////////////////////

#define WIDGET_TYPE(NAME, ID) extern FixPointersFunctionType NAME##_fixPointers;
WIDGET_TYPES
#undef WIDGET_TYPE
#define WIDGET_TYPE(NAME, ID) &NAME##_fixPointers,
static FixPointersFunctionType *g_fixWidgetPointersFunctions[] = {
    WIDGET_TYPES
};
#undef WIDGET_TYPE

////////////////////////////////////////////////////////////////////////////////

void Widget_fixPointers(Widget *widget, Assets *assets) {
    widget->specific = (void *)((uint8_t *)assets->document + (uint32_t)widget->specific);

    if (*g_fixWidgetPointersFunctions[widget->type]) {
        (*g_fixWidgetPointersFunctions[widget->type])(widget, assets);
    }
}

void WidgetList_fixPointers(List<const Widget> &widgetList, Assets *assets) {
    widgetList.first = (Widget *)((uint8_t *)assets->document + (uint32_t)widgetList.first);

    for (uint32_t i = 0; i < widgetList.count; ++i) {
        Widget_fixPointers((Widget *)&widgetList.first[i], assets);
    }
}

////////////////////////////////////////

void StyleList_fixPointers(Assets *assets) {
	assets->styles->first = (const Style *)((uint8_t *)assets->styles + (uint32_t)assets->styles->first);
}

////////////////////////////////////////

void ColorList_fixPointers(Assets *assets, ColorList &colorList) {
    colorList.first = (uint16_t *)((uint8_t *)assets->colorsData + (uint32_t)colorList.first);
}

void Theme_fixPointers(Assets *assets, Theme *theme) {
    theme->name = (const char *)((uint8_t *)assets->colorsData + (uint32_t)theme->name);
    ColorList_fixPointers(assets, theme->colors);
}

void ThemeList_fixPointers(Assets *assets) {
	ThemeList &themeList = assets->colorsData->themes;

    themeList.first = (Theme *)((uint8_t *)assets->colorsData + (uint32_t)themeList.first);
    for (uint32_t i = 0; i < themeList.count; ++i) {
        Theme_fixPointers(assets, const_cast<Theme*>(&themeList.first[i]));
    }
}

void ColorsData_fixPointers(Assets *assets) {
    ThemeList_fixPointers(assets);
    ColorList_fixPointers(assets, assets->colorsData->colors);
}

////////////////////////////////////////

void NameList_fixPointers(NameList *nameList) {
    if (nameList) {
        nameList->first = (const char **)((uint8_t *)nameList + (uint32_t)nameList->first);
        for (uint32_t i = 0; i < nameList->count; i++) {
            nameList->first[i] = (const char *)((uint8_t *)nameList + (uint32_t)nameList->first[i]);
        }
    }
}

////////////////////////////////////////

void Component_fixPointers(Component *component, Assets *assets) {
	component->inputs.first = (ComponentInput *)((uint8_t *)assets->flowDefinition + (uint32_t)component->inputs.first);
    for (uint32_t i = 0; i < component->inputs.count; ++i) {
        component->inputs.first[i].values.first =
            (ComponentInputValue *)((uint8_t *)assets->flowDefinition + (uint32_t)component->inputs.first[i].values.first);
    }

	component->outputs.first = (ComponentOutput *)((uint8_t *)assets->flowDefinition + (uint32_t)component->outputs.first);
    for (uint32_t i = 0; i < component->outputs.count; ++i) {
        component->outputs.first[i].connections.first = 
            (Connection *)((uint8_t *)assets->flowDefinition + (uint32_t)component->outputs.first[i].connections.first);
    }

	component->specific = (void *)((uint8_t *)assets->flowDefinition + (uint32_t)component->specific);
}

void Flow_fixPointers(Flow *flow, Assets *assets) {
    // fix components pointers
    flow->components.first = (const Component *)((uint8_t *)assets->flowDefinition + (uint32_t)flow->components.first);
    for (uint32_t i = 0; i < flow->components.count; ++i) {
        Component_fixPointers((Component *)&flow->components.first[i], assets);
    }
}

void FlowValue_fixPointers(gui::Value &flowValue, Assets *assets) {
    if (flowValue.type_ == VALUE_TYPE_STRING) {
        flowValue.str_ = (const char *)((uint8_t *)(void *)assets->flowDefinition + (uint32_t)(flowValue.str_));
    }
}

void FlowDefinition_fixPointers(Assets *assets) {
    // fix flows pointers
    assets->flowDefinition->flows.first = (Flow *)((uint8_t *)assets->flowDefinition + (uint32_t)assets->flowDefinition->flows.first);
    for (uint32_t i = 0; i < assets->flowDefinition->flows.count; ++i) {
        Flow_fixPointers((Flow *)&assets->flowDefinition->flows.first[i], assets);
    }

    // fix flowValues pointers
    assets->flowDefinition->flowValues.first = (gui::Value *)((uint8_t *)assets->flowDefinition + (uint32_t)assets->flowDefinition->flowValues.first);
	for (uint32_t i = 0; i < assets->flowDefinition->flowValues.count; ++i) {
		FlowValue_fixPointers(assets->flowDefinition->flowValues.first[i], assets);
	}

    // fix widgetDataItems pointers
    assets->flowDefinition->widgetDataItems.first = (AssetsPtr<ComponentInput> *)((uint8_t *)assets->flowDefinition + (uint32_t)assets->flowDefinition->widgetDataItems.first);
	for (uint32_t i = 0; i < assets->flowDefinition->widgetDataItems.count; ++i) {
		assets->flowDefinition->widgetDataItems.first[i] = (ComponentInput *)((uint8_t *)assets->flowDefinition + (uint32_t)assets->flowDefinition->widgetDataItems.first[i]);
	}

    // fix widgetActions pointers
    assets->flowDefinition->widgetActions.first = (AssetsPtr<ComponentOutput> *)((uint8_t *)assets->flowDefinition + (uint32_t)assets->flowDefinition->widgetActions.first);
	for (uint32_t i = 0; i < assets->flowDefinition->widgetActions.count; ++i) {
		assets->flowDefinition->widgetActions.first[i] = (ComponentOutput *)((uint8_t *)assets->flowDefinition + (uint32_t)assets->flowDefinition->widgetActions.first[i]);
	}
}

////////////////////////////////////////

void fixPointers(Assets *assets) {
	auto offsetOrigin = (uint8_t *)&assets->document;

	assets->document = (Document *)(offsetOrigin + (uint32_t)assets->document);
	WidgetList_fixPointers(assets->document->pages, assets);

	assets->styles = (StyleList *)(offsetOrigin + (uint32_t)assets->styles);
	StyleList_fixPointers(assets);

	assets->fontsData = offsetOrigin + (uint32_t)assets->fontsData;
	assets->bitmapsData = offsetOrigin + (uint32_t)assets->bitmapsData;

	assets->colorsData = (Colors *)(offsetOrigin + (uint32_t)assets->colorsData);
	ColorsData_fixPointers(assets);

	if (assets->external || assets->projectVersion >= 3) {
		assets->actionNames = (NameList *)(offsetOrigin + (uint32_t)assets->actionNames);
		NameList_fixPointers(assets->actionNames);

		assets->dataItemNames = (NameList *)(offsetOrigin + (uint32_t)assets->dataItemNames);
		NameList_fixPointers(assets->dataItemNames);
	}

	if (assets->projectVersion >= 3) {
		assets->flowDefinition = (FlowDefinition *)(offsetOrigin + (uint32_t)assets->flowDefinition);
        FlowDefinition_fixPointers(assets);
	}
}

////////////////////////////////////////////////////////////////////////////////

void loadMainAssets() {
    uint16_t projectVersion;

	int compressedSize;
	uint32_t decompressedSize;
	uint32_t compressedDataOffset;

	auto header = (Header *)assets;
	if (header->tag == HEADER_TAG) {
		// project version >= 3
        projectVersion = header->projectVersion;

		compressedSize = sizeof(assets) - sizeof(Header);
		decompressedSize = header->decompressedSize;
		compressedDataOffset = sizeof(Header);
	} else {
		// project version < 3,
        projectVersion = 2;

		// first 4 bytes (uint32_t) are decompressed size
		compressedSize = sizeof(assets) - 4;
		decompressedSize = ((uint32_t *)header)[0];
		compressedDataOffset = 4;
	}

    assert(decompressedSize + ((uint8_t *)(&g_mainAssets->document) - (uint8_t *)g_mainAssets) <= DECOMPRESSED_ASSETS_SIZE);

    g_mainAssets->projectVersion = projectVersion;
    g_mainAssets->external = false;

    int result = LZ4_decompress_safe((const char *)(assets + compressedDataOffset), (char *)&g_mainAssets->document, compressedSize, (int)decompressedSize);
    assert(result == (int)decompressedSize);

	fixPointers(g_mainAssets);

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

    uint16_t projectVersion;

    int compressedSize;
    uint32_t decompressedSize;
	uint32_t compressedDataOffset;

	auto header = (Header *)fileData;
	if (header->tag == HEADER_TAG) {
		// project version >= 3
        projectVersion = header->projectVersion;

		compressedSize = fileSize - sizeof(Header);
		decompressedSize = header->decompressedSize;
		compressedDataOffset = sizeof(Header);
	} else {
		// project version < 3,
        projectVersion = 2;

		// first 4 bytes (uint32_t) are decompressed size
		compressedSize = fileSize - 4;
		decompressedSize = ((uint32_t *)header)[0];
		compressedDataOffset = 4;
	}

	uint32_t availableDecompressedSize = (uint32_t)(fileData - EXTERNAL_ASSETS_BUFFER - (((uint8_t *)(&g_externalAssets->document) - (uint8_t *)g_externalAssets)));
    if (decompressedSize > availableDecompressedSize) {
        if (err) {
            *err = SCPI_ERROR_OUT_OF_DEVICE_MEMORY;
        }
        return false;
    }

    g_externalAssets->projectVersion = projectVersion;
    g_externalAssets->external = true;

    int result = LZ4_decompress_safe((const char *)fileData + compressedDataOffset, (char *)&g_externalAssets->document, compressedSize, (int)decompressedSize);
    if (result != (int)decompressedSize) {
        if (err) {
            *err = SCPI_ERROR_INVALID_BLOCK_DATA;
        }
        return false;
    }

    fixPointers(g_externalAssets);

    return true;
}

const Style *getStyle(int styleID) {
	return g_mainAssets->styles->first + styleID - 1;
}

const Widget* getPageWidget(int pageId) {
	if (pageId > 0) {
		return g_mainAssets->document->pages.first + (pageId - 1);
	} else if (pageId < 0) {
		return g_externalAssets->document->pages.first + (-pageId - 1);
	}
	return nullptr;
}

const uint8_t *getFontData(int fontID) {
	if (fontID == 0) {
		return 0;
	}
	return g_mainAssets->fontsData + ((uint32_t *)g_mainAssets->fontsData)[fontID - 1];
}

const Bitmap *getBitmap(int bitmapID) {
	if (bitmapID > 0) {
		return (const Bitmap *)(g_mainAssets->bitmapsData + ((uint32_t *)g_mainAssets->bitmapsData)[bitmapID - 1]);
	} else if (bitmapID < 0) {
		return (const Bitmap *)(g_externalAssets->bitmapsData + ((uint32_t *)g_externalAssets->bitmapsData)[-bitmapID - 1]);
	}
	return nullptr;
}

int getThemesCount() {
	return (int)g_mainAssets->colorsData->themes.count;
}

const Theme *getTheme(int i) {
	return g_mainAssets->colorsData->themes.first + i;
}

const char *getThemeName(int i) {
	return getTheme(i)->name;
}

const uint16_t *getThemeColors(int themeIndex) {
	return getTheme(themeIndex)->colors.first;
}

const uint32_t getThemeColorsCount(int themeIndex) {
	return getTheme(themeIndex)->colors.count;
}

const uint16_t *getColors() {
	return g_mainAssets->colorsData->colors.first;
}

int getExternalAssetsFirstPageId() {
	return -1;
}

const char *getActionName(int16_t actionId) {
	if (actionId == 0) {
		return nullptr;
	}
	if (actionId < 0) {
		actionId = -actionId;
	}
	actionId--;
	return g_externalAssets->actionNames->first[actionId];
}

int16_t getDataIdFromName(const char *name) {
	for (uint32_t i = 0; i < g_externalAssets->dataItemNames->count; i++) {
		if (strcmp(g_externalAssets->dataItemNames->first[i], name) == 0) {
			return -((int16_t)i + 1);
		}
	}
	return 0;
}

} // namespace gui
} // namespace eez

#endif
