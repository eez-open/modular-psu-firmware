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
#include <memory.h>

#include <eez/libs/lz4/lz4.h>

#include <eez/gui/gui.h>
#include <eez/gui/document.h>
#include <eez/system.h>

#ifdef EEZ_PLATFORM_STM32
#include <eez/platform/stm32/defines.h>
#endif

namespace eez {
namespace gui {

static uint8_t *g_decompressedAssets;

Document *g_document;
StyleList *g_styles;
uint8_t *g_fontsData;
uint8_t *g_bitmapsData;
Colors *g_colorsData;

#if OPTION_SDRAM
void StyleList_fixPointers(StyleList &styleList) {
    styleList.first = (Style *)((uint8_t *)g_styles + (uint32_t)styleList.first);
}

void WidgetList_fixPointers(WidgetList &widgetList) {
    widgetList.first = (Widget *)((uint8_t *)g_document + (uint32_t)widgetList.first);
    for (uint32_t i = 0; i < widgetList.count; ++i) {
        Widget_fixPointers((Widget *)widgetList.first + i);
    }
}

void ColorList_fixPointers(ColorList &colorList) {
    colorList.first = (uint16_t *)((uint8_t *)g_colorsData + (uint32_t)colorList.first);
}

void Theme_fixPointers(Theme *theme) {
    theme->name = (const char *)((uint8_t *)g_colorsData + (uint32_t)theme->name);
    ColorList_fixPointers(theme->colors);
}

void ThemeList_fixPointers(ThemeList &themeList) {
    themeList.first = (Theme *)((uint8_t *)g_colorsData + (uint32_t)themeList.first);
    for (uint32_t i = 0; i < themeList.count; ++i) {
        Theme_fixPointers((Theme *)themeList.first + i);
    }
}

void ColorsData_fixPointers() {
    ThemeList_fixPointers(g_colorsData->themes);
    ColorList_fixPointers(g_colorsData->colors);
}

void fixPointers() {
    WidgetList_fixPointers(g_document->pages);
    StyleList_fixPointers(*g_styles);
    ColorsData_fixPointers();
}
#endif

void decompressAssets() {
#if OPTION_SDRAM
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
#else
    g_decompressedAssets = (uint8_t *)assets;
#endif

    g_document = (Document *)(g_decompressedAssets + ((uint32_t *)g_decompressedAssets)[0]);
    g_styles = (StyleList *)(g_decompressedAssets + ((uint32_t *)g_decompressedAssets)[1]);
    g_fontsData = g_decompressedAssets + ((uint32_t *)g_decompressedAssets)[2];
    g_bitmapsData = g_decompressedAssets + ((uint32_t *)g_decompressedAssets)[3];
    g_colorsData = (Colors *)(g_decompressedAssets + ((uint32_t *)g_decompressedAssets)[4]);

#if OPTION_SDRAM
    fixPointers();
#endif
}

const Style *getStyle(int styleID) {
#if OPTION_SDRAM
    return g_styles->first + styleID - 1;
#else
    return (Style *)((uint8_t *)g_styles + (uint32_t)g_styles->firstOffset) + styleID - 1;
#endif
}

const Style *getWidgetStyle(const Widget *widget) {
    return getStyle(transformStyle(widget));
}

const Widget* getPageWidget(int pageId) {
#if OPTION_SDRAM
    return g_document->pages.first + pageId;
#else
    return (const Widget *)((uint8_t *)g_document + (uint32_t)g_document->pages.firstOffset) + pageId;
#endif
}

const uint8_t *getFontData(int fontID) {
    if (fontID == 0) {
        return 0;
    }
    return g_fontsData + ((uint32_t *)g_fontsData)[fontID - 1];
}

const Bitmap *getBitmap(int bitmapID) {
    return (const Bitmap *)(g_bitmapsData + ((uint32_t *)g_bitmapsData)[bitmapID - 1]);
}

int getThemesCount() {
    return (int)g_colorsData->themes.count;
}

const Theme *getTheme(int i) {
#if OPTION_SDRAM
    return g_colorsData->themes.first + i;
#else
    return (const Theme *)((uint8_t *)g_colorsData + (uint32_t)(g_colorsData->themes).firstOffset) + i;
#endif
}

const char *getThemeName(int i) {
#if OPTION_SDRAM
    return getTheme(i)->name;
#else
    return (const char *)((uint8_t *)g_colorsData + (uint32_t)getTheme(i)->nameOffset);
#endif
}

const uint16_t *getThemeColors(int themeIndex) {
#if OPTION_SDRAM
    return getTheme(themeIndex)->colors.first;
#else
    return (const uint16_t *)((uint8_t *)g_colorsData + (uint32_t)getTheme(themeIndex)->colors.firstOffset);
#endif
}

const uint32_t getThemeColorsCount(int themeIndex) {
    return getTheme(themeIndex)->colors.count;
}

const uint16_t *getColors() {
#if OPTION_SDRAM
    return g_colorsData->colors.first;
#else
    return (const uint16_t *)((uint8_t *)g_colorsData + (uint32_t)g_colorsData->colors.firstOffset);
#endif
}

} // namespace gui
} // namespace eez
