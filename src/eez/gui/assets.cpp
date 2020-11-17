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

#include <eez/libs/lz4/lz4.h>

#include <eez/gui/gui.h>
#include <eez/gui/widget.h>

#include <eez/libs/sd_fat/sd_fat.h>

#include <scpi/scpi.h>

namespace eez {
namespace gui {

bool g_assetsLoaded;
static Assets g_mainAssets;

static Assets g_externalAssets;

static Assets *g_fixPointersAssets;

void StyleList_fixPointers() {
    g_fixPointersAssets->styles->first = (const Style *)((uint8_t *)g_fixPointersAssets->styles + (uint32_t)g_fixPointersAssets->styles->first);
}

void WidgetList_fixPointers(WidgetList &widgetList) {
    widgetList.first = (Widget *)((uint8_t *)g_fixPointersAssets->document + (uint32_t)widgetList.first);
    for (uint32_t i = 0; i < widgetList.count; ++i) {
        Widget_fixPointers((Widget *)&widgetList.first[i]);
    }
}

void ColorList_fixPointers(ColorList &colorList) {
    colorList.first = (uint16_t *)((uint8_t *)g_fixPointersAssets->colorsData + (uint32_t)colorList.first);
}

void Theme_fixPointers(Theme *theme) {
    theme->name = (const char *)((uint8_t *)g_fixPointersAssets->colorsData + (uint32_t)theme->name);
    ColorList_fixPointers(theme->colors);
}

void ThemeList_fixPointers(ThemeList &themeList) {
    themeList.first = (Theme *)((uint8_t *)g_fixPointersAssets->colorsData + (uint32_t)themeList.first);
    for (uint32_t i = 0; i < themeList.count; ++i) {
        Theme_fixPointers(const_cast<Theme*>(&themeList.first[i]));
    }
}

void ColorsData_fixPointers() {
    ThemeList_fixPointers(g_fixPointersAssets->colorsData->themes);
    ColorList_fixPointers(g_fixPointersAssets->colorsData->colors);
}

void NameList_fixPointers(NameList *nameList) {
    if (nameList) {
        nameList->first = (void *)((uint8_t *)nameList + (uint32_t)nameList->first);
        for (uint32_t i = 0; i < nameList->count; i++) {
            nameList->first[i] = (const char *)((uint8_t *)nameList + (uint32_t)nameList->first[i]);
        }
    }
}

void fixPointers(Assets &assets) { 
    g_fixPointersAssets = &assets;

    WidgetList_fixPointers(g_fixPointersAssets->document->pages);
    StyleList_fixPointers();
    ColorsData_fixPointers();
    NameList_fixPointers(g_fixPointersAssets->actionNames);
    NameList_fixPointers(g_fixPointersAssets->dataItemNames);
}

#define WIDGET_TYPE(NAME, ID) extern FixPointersFunctionType NAME##_fixPointers;
WIDGET_TYPES
#undef WIDGET_TYPE
#define WIDGET_TYPE(NAME, ID) &NAME##_fixPointers,
static FixPointersFunctionType *g_fixWidgetPointersFunctions[] = {
    WIDGET_TYPES
};
#undef WIDGET_TYPE

// static FixWidgetPointersFunction g_fixWidgetPointersFunctions[] = {
//     nullptr,                         // WIDGET_TYPE_NONE
//     ContainerWidget_fixPointers,     // WIDGET_TYPE_CONTAINER
//     ListWidget_fixPointers,          // WIDGET_TYPE_LIST
//     GridWidget_fixPointers,          // WIDGET_TYPE_GRID
//     SelectWidget_fixPointers,        // WIDGET_TYPE_SELECT
//     nullptr,                         // WIDGET_TYPE_DISPLAY_DATA
//     TextWidget_fixPointers,          // WIDGET_TYPE_TEXT
//     MultilineTextWidget_fixPointers, // WIDGET_TYPE_MULTILINE_TEXT
//     nullptr,                         // WIDGET_TYPE_RECTANGLE
//     nullptr,                         // WIDGET_TYPE_BITMAP
//     ButtonWidget_fixPointers,        // WIDGET_TYPE_BUTTON
//     ToggleButtonWidget_fixPointers,  // WIDGET_TYPE_TOGGLE_BUTTON
//     nullptr,                         // WIDGET_TYPE_BUTTON_GROUP
//     nullptr,                         // WIDGET_TYPE_RESERVED
//     nullptr,                         // WIDGET_TYPE_BAR_GRAPH
//     nullptr,                         // WIDGET_TYPE_LAYOUT_VIEW
//     nullptr,                         // WIDGET_TYPE_YT_GRAPH
//     UpDownWidget_fixPointers,        // WIDGET_TYPE_UP_DOWN
//     nullptr,                         // WIDGET_TYPE_LIST_GRAPH
//     nullptr,                         // WIDGET_TYPE_APP_VIEW
//     ScrollBarWidget_fixPointers,     // WIDGET_TYPE_SCROLL_BAR
//     nullptr,                         // WIDGET_TYPE_SCROLL_BAR
// };

void Widget_fixPointers(Widget *widget) {
    widget->specific = (void *)((uint8_t *)g_fixPointersAssets->document + (uint32_t)widget->specific);
    if (*g_fixWidgetPointersFunctions[widget->type]) {
        (*g_fixWidgetPointersFunctions[widget->type])(widget, g_fixPointersAssets);
    }
}

void initAssets(Assets &assets, bool external, uint8_t *decompressedAssets) {
    assets.document = (Document *)(decompressedAssets + ((uint32_t *)decompressedAssets)[0]);
    assets.styles = (StyleList *)(decompressedAssets + ((uint32_t *)decompressedAssets)[1]);
    assets.fontsData = decompressedAssets + ((uint32_t *)decompressedAssets)[2];
    assets.bitmapsData = decompressedAssets + ((uint32_t *)decompressedAssets)[3];
    assets.colorsData = (Colors *)(decompressedAssets + ((uint32_t *)decompressedAssets)[4]);
    if (external) {
        assets.actionNames = (NameList *)(decompressedAssets + ((uint32_t *)decompressedAssets)[5]);
        assets.dataItemNames = (NameList *)(decompressedAssets + ((uint32_t *)decompressedAssets)[6]);
    } else {
        assets.actionNames = nullptr;
        assets.dataItemNames = nullptr;
    }
}

void decompressAssets() {
    uint8_t *decompressedAssets;

    int compressedSize = sizeof(assets) - 4;

    // first 4 bytes (uint32_t) are decompressed size
    uint32_t decompressedSize = ((uint32_t *)assets)[0];
    assert(decompressedSize <= DECOMPRESSED_ASSETS_SIZE);
    decompressedAssets = DECOMPRESSED_ASSETS_START_ADDRESS;

    int result = LZ4_decompress_safe((const char *)assets + 4, (char *)decompressedAssets, compressedSize, (int)decompressedSize);
    assert(result == (int)decompressedSize);

    initAssets(g_mainAssets, false, decompressedAssets);

    fixPointers(g_mainAssets);

    g_assetsLoaded = true;
}

const Style *getStyle(int styleID) {
    return g_mainAssets.styles->first + styleID - 1;
}

const Widget* getPageWidget(int pageId) {
    if (pageId > 0) {
        return g_mainAssets.document->pages.first + (pageId - 1);
    } else if (pageId < 0) {
        return g_externalAssets.document->pages.first + (-pageId - 1);
    }
    return nullptr;
}

const uint8_t *getFontData(int fontID) {
    if (fontID == 0) {
        return 0;
    }
    return g_mainAssets.fontsData + ((uint32_t *)g_mainAssets.fontsData)[fontID - 1];
}

const Bitmap *getBitmap(int bitmapID) {
    if (bitmapID > 0) {
        return (const Bitmap *)(g_mainAssets.bitmapsData + ((uint32_t *)g_mainAssets.bitmapsData)[bitmapID - 1]);
    } else if (bitmapID < 0) {
        return (const Bitmap *)(g_externalAssets.bitmapsData + ((uint32_t *)g_externalAssets.bitmapsData)[-bitmapID - 1]);
    }
    return nullptr;
}

int getThemesCount() {
    return (int)g_mainAssets.colorsData->themes.count;
}

const Theme *getTheme(int i) {
    return g_mainAssets.colorsData->themes.first + i;
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
    return g_mainAssets.colorsData->colors.first;
}

const char *getActionName(int16_t actionId) {
    if (actionId == 0) {
        return nullptr;
    }
    if (actionId < 0) {
        actionId = -actionId;
    }
    actionId--;
    return g_externalAssets.actionNames->first[actionId];
}

int16_t getDataIdFromName(const char *name) {
    for (uint32_t i = 0; i < g_externalAssets.dataItemNames->count; i++) {
        if (strcmp(g_externalAssets.dataItemNames->first[i], name) == 0) {
            return -((int16_t)i+1);
        }
    }
    return 0;
}

int getExternalAssetsFirstPageId() {
    return -1;
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

    if (fileSize > FILE_VIEW_BUFFER_SIZE) {
        if (err) {
            *err = SCPI_ERROR_OUT_OF_DEVICE_MEMORY;
        }
        return false;
    }

    uint8_t *fileData = FILE_VIEW_BUFFER + FILE_VIEW_BUFFER_SIZE - ((fileSize + 3) / 4) * 4;
    uint32_t bytesRead = file.read(fileData, fileSize);
    file.close();

    if (bytesRead != fileSize) {
        if (err) {
            *err = SCPI_ERROR_MASS_STORAGE_ERROR;
        }
        return false;
    }

    int compressedSize = fileSize - 4;

    // first 4 bytes (uint32_t) are decompressed size
    uint32_t decompressedSize = ((uint32_t *)fileData)[0];

    if (decompressedSize > (uint32_t)(fileData - FILE_VIEW_BUFFER)) {
        if (err) {
            *err = SCPI_ERROR_OUT_OF_DEVICE_MEMORY;
        }
        return false;
    }

    uint8_t *decompressedAssets = FILE_VIEW_BUFFER;

    int result = LZ4_decompress_safe((const char *)fileData + 4, (char *)decompressedAssets, compressedSize, (int)decompressedSize);
    if (result != (int)decompressedSize) {
        if (err) {
            *err = SCPI_ERROR_INVALID_BLOCK_DATA;
        }
        return false;
    }

    initAssets(g_externalAssets, true, decompressedAssets);

    fixPointers(g_externalAssets);

    return true;
}

} // namespace gui
} // namespace eez

#endif
