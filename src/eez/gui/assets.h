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

#pragma once

#include <eez/gui/document.h>

namespace eez {
namespace gui {

extern bool g_isMainAssetsLoaded;
extern Assets *g_mainAssets;
extern Assets *g_externalAssets;

////////////////////////////////////////////////////////////////////////////////

template<typename T>
struct AssetsPtr {
    T* ptr(Assets *assets) {
		//if (offset == 0) {
		//	return nullptr;
		//}
        return (T *)((uint8_t *)assets + 4 + offset); // 4 is offset of Assets::pages
    }

    void operator=(T* ptr) {
        offset = (uint8_t *)g_mainAssets - (uint8_t *)ptr;
    }

private:
    uint32_t offset;
};

template<typename T>
struct AssetsPtrList {
    T* item(Assets *assets, int i) {
        return (T *)items.ptr(assets)[i].ptr(assets);
    }

	uint32_t count;

private:
    AssetsPtr<AssetsPtr<uint32_t>> items;
};

template<typename T>
struct SimpleList {
    T *ptr(Assets *assets) {
        return items.ptr(assets);
    }

	uint32_t count;

private:
    AssetsPtr<T> items;
};

////////////////////////////////////////////////////////////////////////////////

#define SHADOW_FLAG 1
#define CLOSE_PAGE_IF_TOUCHED_OUTSIDE_FLAG 2

struct Widget {
	uint8_t type;
	uint8_t reserved1;
	int16_t data;
	int16_t action;
	int16_t x;
	int16_t y;
	int16_t w;
	int16_t h;
	uint16_t style;
};

struct PageWidget : public Widget {
	AssetsPtrList<Widget> widgets;
    uint16_t overlay;
    uint16_t flags;
};

////////////////////////////////////////////////////////////////////////////////

#define STYLE_FLAGS_HORZ_ALIGN_MASK 0x7
#define STYLE_FLAGS_HORZ_ALIGN_LEFT 0
#define STYLE_FLAGS_HORZ_ALIGN_RIGHT 1
#define STYLE_FLAGS_HORZ_ALIGN_CENTER 2

#define STYLE_FLAGS_VERT_ALIGN_MASK (0x7 << 3)
#define STYLE_FLAGS_VERT_ALIGN_TOP (0 << 3)
#define STYLE_FLAGS_VERT_ALIGN_BOTTOM (1 << 3)
#define STYLE_FLAGS_VERT_ALIGN_CENTER (2 << 3)

#define STYLE_FLAGS_BLINK (1 << 6)

struct Style {
    uint16_t flags; // STYLE_FLAGS_...
    uint16_t background_color;
    uint16_t color;
    uint16_t active_background_color;
    uint16_t active_color;
    uint16_t focus_background_color;
    uint16_t focus_color;
    uint8_t border_size_top;
    uint8_t border_size_right;
    uint8_t border_size_bottom;
    uint8_t border_size_left;
    uint16_t border_radius;
    uint16_t border_color;
    uint8_t font;
    uint8_t opacity; // 0 - 255
    uint8_t padding_top;
    uint8_t padding_right;
    uint8_t padding_bottom;
    uint8_t padding_left;
    uint8_t margin_top;
    uint8_t margin_right;
    uint8_t margin_bottom;
    uint8_t margin_left;
};

////////////////////////////////////////////////////////////////////////////////

struct GlyphData {
	int8_t dx;         // DWIDTH (-128 indicated empty glyph)
	uint8_t width;     // BBX width
	uint8_t height;    // BBX height
	int8_t x;          // BBX xoffset
	int8_t y;          // BBX yoffset
    int8_t reserved1;
    int8_t reserved2;
    int8_t reserved3;
	uint8_t pixels[1];
};

struct FontData {
	uint8_t ascent;
	uint8_t descent;
	uint8_t encodingStart;
	uint8_t encodingEnd;
	AssetsPtr<GlyphData> glyphs[1];
};

////////////////////////////////////////////////////////////////////////////////

struct Bitmap {
    int16_t w;
    int16_t h;
    int16_t bpp;
    int16_t reserved;
    const uint8_t pixels[1];
};

////////////////////////////////////////////////////////////////////////////////

struct Theme {
	AssetsPtr<const char> name;
	SimpleList<uint16_t> colors;
};

struct Colors {
	AssetsPtrList<Theme> themes;
	SimpleList<uint16_t> colors;
};

////////////////////////////////////////////////////////////////////////////////

struct ComponentInput {
    uint16_t valueIndex;
};

struct Connection {
    uint16_t targetComponentIndex;
    uint8_t targetInputIndex;
};

struct ComponentOutput {
	AssetsPtrList<Connection> connections;
};

struct Component {
    uint16_t type;
    uint16_t reserved;
	AssetsPtrList<ComponentInput> inputs;
	AssetsPtrList<ComponentOutput> outputs;
};

struct Flow {
	AssetsPtrList<Component> components;
};

struct FlowDefinition {
	AssetsPtrList<Flow> flows;
	AssetsPtrList<Value> flowValues;
	AssetsPtrList<ComponentInput> widgetDataItems;
	AssetsPtrList<ComponentOutput> widgetActions;
};

////////////////////////////////////////////////////////////////////////////////

struct Assets {
    uint8_t projectVersion;
    uint8_t external;
    uint16_t reserved;

	AssetsPtrList<PageWidget> pages;
	AssetsPtrList<Style> styles;
	AssetsPtrList<FontData> fonts;
	AssetsPtrList<Bitmap> bitmaps;
	AssetsPtr<Colors> colorsDefinition;
	AssetsPtrList<const char> actionNames;
	AssetsPtrList<const char> variableNames;
	AssetsPtr<FlowDefinition> flowDefinition;
};

////////////////////////////////////////////////////////////////////////////////

void loadMainAssets();
bool loadExternalAssets(const char *filePath, int *err);

const Widget *getPageWidget(int pageId);
const Style *getStyle(int styleID);
const FontData *getFontData(int fontID);
const Bitmap *getBitmap(int bitmapID);

int getThemesCount();
const char *getThemeName(int i);
const uint16_t *getThemeColors(int themeIndex);
const uint32_t getThemeColorsCount(int themeIndex);
const uint16_t *getColors();

int getExternalAssetsFirstPageId();

const char *getActionName(int16_t actionId);
int16_t getDataIdFromName(const char *name);

#define GET_WIDGET_PROPERTY(widget, propertyName, type) ((type)widget->propertyName)

////////////////////////////////////////////////////////////////////////////////

} // namespace gui
} // namespace eez
