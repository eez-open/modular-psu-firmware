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

	const T* ptr(Assets *assets) const {
		//if (offset == 0) {
		//	return nullptr;
		//}
		return (const T *)((uint8_t *)assets + 4 + offset); // 4 is offset of Assets::pages
	}
	
	void operator=(T* ptr) {
        offset = (uint8_t *)g_mainAssets - (uint8_t *)ptr;
    }

private:
    uint32_t offset;
};

template<typename T>
struct ListOfAssetsPtr {
    T* item(Assets *assets, int i) {
        return (T *)items.ptr(assets)[i].ptr(assets);
    }

	const T* item(Assets *assets, int i) const {
		return (const T *)items.ptr(assets)[i].ptr(assets);
	}

	uint32_t count;
private:
    AssetsPtr<AssetsPtr<uint32_t>> items;
};

template<typename T>
struct ListOfFundamentalType {
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
	uint16_t type;
	int16_t data;
	int16_t action;
	int16_t x;
	int16_t y;
	int16_t w;
	int16_t h;
	uint16_t style;
};

struct ContainerWidget : public Widget {
	ListOfAssetsPtr<Widget> widgets;
	uint16_t flags;
	int16_t overlay;
};

typedef ContainerWidget PageAsset;

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
	AssetsPtr<const GlyphData> glyphs[1];
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
	ListOfFundamentalType<uint16_t> colors;
};

struct Colors {
	ListOfAssetsPtr<Theme> themes;
	ListOfFundamentalType<uint16_t> colors;
};

////////////////////////////////////////////////////////////////////////////////

static const uint16_t EXPR_EVAL_INSTRUCTION_TYPE_MASK  = (7 << 13);
static const uint16_t EXPR_EVAL_INSTRUCTION_PARAM_MASK = ~EXPR_EVAL_INSTRUCTION_TYPE_MASK;

static const uint16_t EXPR_EVAL_INSTRUCTION_TYPE_PUSH_CONSTANT   = (0 << 13);
static const uint16_t EXPR_EVAL_INSTRUCTION_TYPE_PUSH_INPUT      = (1 << 13);
static const uint16_t EXPR_EVAL_INSTRUCTION_TYPE_PUSH_LOCAL_VAR  = (2 << 13);
static const uint16_t EXPR_EVAL_INSTRUCTION_TYPE_PUSH_GLOBAL_VAR = (3 << 13);
static const uint16_t EXPR_EVAL_INSTRUCTION_TYPE_OPERATION       = (4 << 13);
static const uint16_t EXPR_EVAL_INSTRUCTION_TYPE_END             = (5 << 13);

struct PropertyValue {
	uint16_t evalInstructions[1];
};

struct Connection {
	uint16_t targetComponentIndex;
	uint8_t targetInputIndex;
};

struct ComponentOutput {
	ListOfAssetsPtr<Connection> connections;
};

struct Component {
    uint16_t type;
    uint16_t reserved;
	ListOfFundamentalType<uint16_t> inputs;
	ListOfAssetsPtr<PropertyValue> propertyValues;
	ListOfAssetsPtr<ComponentOutput> outputs;
};

struct Flow {
	ListOfAssetsPtr<Component> components;
	ListOfAssetsPtr<Value> localVariables;
	ListOfAssetsPtr<PropertyValue> widgetDataItems;
	ListOfAssetsPtr<ComponentOutput> widgetActions;
	uint16_t nInputValues;
};

struct FlowDefinition {
	ListOfAssetsPtr<Flow> flows;
	ListOfAssetsPtr<Value> constants;
	ListOfAssetsPtr<Value> globalVariables;
};

////////////////////////////////////////////////////////////////////////////////

struct Assets {
    uint8_t projectVersion;
    uint8_t external;
    uint16_t reserved;

	ListOfAssetsPtr<PageAsset> pages;
	ListOfAssetsPtr<Style> styles;
	ListOfAssetsPtr<FontData> fonts;
	ListOfAssetsPtr<Bitmap> bitmaps;
	AssetsPtr<Colors> colorsDefinition;
	ListOfAssetsPtr<const char> actionNames;
	ListOfAssetsPtr<const char> variableNames;
	AssetsPtr<FlowDefinition> flowDefinition;
};

////////////////////////////////////////////////////////////////////////////////

void loadMainAssets();
bool loadExternalAssets(const char *filePath, int *err);
void unloadExternalAssets();

////////////////////////////////////////////////////////////////////////////////

const PageAsset *getPageAsset(int pageId);
const PageAsset* getPageAsset(int pageId, WidgetCursor& widgetCursor);

const Style *getStyle(int styleID);
const FontData *getFontData(int fontID);
const Bitmap *getBitmap(int bitmapID);

int getThemesCount();
const char *getThemeName(int i);
const uint32_t getThemeColorsCount(int themeIndex);
const uint16_t *getThemeColors(int themeIndex);
const uint16_t *getColors();

int getExternalAssetsFirstPageId();

const char *getActionName(const WidgetCursor &widgetCursor, int16_t actionId);
int16_t getDataIdFromName(const WidgetCursor &widgetCursor, const char *name);

////////////////////////////////////////////////////////////////////////////////

} // namespace gui
} // namespace eez
