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

#include <eez/gui/data.h>
#include <eez/gui/widget.h>

namespace eez {
namespace gui {

static const uint32_t HEADER_TAG = 0x7A65657E;

static const uint8_t PROJECT_VERSION_V2 = 2;
static const uint8_t PROJECT_VERSION_V3 = 3;

struct Header {
	uint32_t tag; // HEADER_TAG
	uint8_t projectMajorVersion;
	uint8_t projectMinorVersion;
	uint16_t assetsType;
	uint32_t decompressedSize;
};

extern bool g_isMainAssetsLoaded;
extern Assets *g_mainAssets;
extern Assets *g_externalAssets;

////////////////////////////////////////////////////////////////////////////////

template<typename T>
struct AssetsPtr {
    T* ptr() {
        return offset ? (T *)(MEMORY_BEGIN + offset) : nullptr;
    }

	const T* ptr() const {
		return offset ? (const T *)(MEMORY_BEGIN + offset) : nullptr;
	}
	
	void operator=(T* ptr) {
		if (ptr != nullptr) {
            offset = (uint8_t *)ptr - MEMORY_BEGIN;
		} else {
			offset = 0;
		}
    }

    explicit operator bool() const {
        return offset != 0;
    }

	void fixOffset(Assets *assets) {
        if (offset) {
            offset += (uint8_t *)assets + 4 - MEMORY_BEGIN;
        }
	}

    uint32_t offset = 0;
};

template<typename T>
struct ListOfAssetsPtr {
    T* item(int i) {
        return (T *)items.ptr()[i].ptr();
    }

	const T* item(int i) const {
        return (const T *)items.ptr()[i].ptr();
	}

	auto itemsPtr() const {
		return items.ptr();
	}

	void fixOffset(Assets *assets) {
		items.fixOffset(assets);
        for (uint32_t i = 0; i < count; i++) {
            items.ptr()[i].fixOffset(assets);
        }
	}

	uint32_t count = 0;
    AssetsPtr<AssetsPtr<T>> items;
};

template<typename T>
struct ListOfFundamentalType {
    T *ptr() {
        return items.ptr();
    }

	void fixOffset(Assets *assets) {
		items.fixOffset(assets);
	}

	uint32_t count;
    AssetsPtr<T> items;
};

////////////////////////////////////////////////////////////////////////////////

#define SHADOW_FLAG (1 << 0)
#define CLOSE_PAGE_IF_TOUCHED_OUTSIDE_FLAG (1 << 1)
#define PAGE_IS_USED_AS_CUSTOM_WIDGET (1 << 2)
#define PAGE_CONTAINER (1 << 3)

struct Widget {
	uint16_t type;
	int16_t data;
	int16_t action;
	int16_t x;
	int16_t y;
	int16_t w;
	int16_t h;
	int16_t style;
};

struct PageAsset : public Widget {
	ListOfAssetsPtr<Widget> widgets;
	uint16_t flags;
	int16_t overlay;
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

    uint16_t backgroundColor;
    uint16_t color;

    uint16_t activeBackgroundColor;
    uint16_t activeColor;

    uint16_t focusBackgroundColor;
    uint16_t focusColor;

    uint8_t borderSizeTop;
    uint8_t borderSizeRight;
    uint8_t borderSizeBottom;
    uint8_t borderSizeLeft;

    uint16_t borderColor;

    uint8_t borderRadiusTLX;
	uint8_t borderRadiusTLY;
    uint8_t borderRadiusTRX;
	uint8_t borderRadiusTRY;
    uint8_t borderRadiusBLX;
	uint8_t borderRadiusBLY;
    uint8_t borderRadiusBRX;
	uint8_t borderRadiusBRY;

    uint8_t font;
    uint8_t opacity; // 0 - 255

    uint8_t paddingTop;
    uint8_t paddingRight;
    uint8_t paddingBottom;
    uint8_t paddingLeft;

	int16_t backgroundImage;
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

static const uint16_t EXPR_EVAL_INSTRUCTION_TYPE_MASK  = 0x0007 << 13;
static const uint16_t EXPR_EVAL_INSTRUCTION_PARAM_MASK = 0xFFFF >> 3;

static const uint16_t EXPR_EVAL_INSTRUCTION_TYPE_PUSH_CONSTANT   = (0 << 13);
static const uint16_t EXPR_EVAL_INSTRUCTION_TYPE_PUSH_INPUT      = (1 << 13);
static const uint16_t EXPR_EVAL_INSTRUCTION_TYPE_PUSH_LOCAL_VAR  = (2 << 13);
static const uint16_t EXPR_EVAL_INSTRUCTION_TYPE_PUSH_GLOBAL_VAR = (3 << 13);
static const uint16_t EXPR_EVAL_INSTRUCTION_TYPE_PUSH_OUTPUT     = (4 << 13);
static const uint16_t EXPR_EVAL_INSTRUCTION_ARRAY_ELEMENT        = (5 << 13);
static const uint16_t EXPR_EVAL_INSTRUCTION_TYPE_OPERATION       = (6 << 13);
static const uint16_t EXPR_EVAL_INSTRUCTION_TYPE_END             = (7 << 13);

struct PropertyValue {
	uint8_t evalInstructions[1];
};

struct Connection {
	uint16_t targetComponentIndex;
	uint16_t targetInputIndex;
};

struct ComponentOutput {
	ListOfAssetsPtr<Connection> connections;
	uint32_t isSeqOut;
};

static const uint16_t BREAKPOINT_ENABLED = 1;
static const uint16_t BREAKPOINT_DISABLED = 2;

struct Component {
    uint16_t type;
    uint16_t breakpoint;

	// These are indexes to Flow::componentInputs.
	// We use this to check if component is ready to run (i.e. all mandatory inputs have a value).
	ListOfFundamentalType<uint16_t> inputs;

	ListOfAssetsPtr<PropertyValue> propertyValues;
	ListOfAssetsPtr<ComponentOutput> outputs;
	int16_t errorCatchOutput;
	uint16_t reserved;
};

struct WidgetDataItem {
	int16_t componentIndex;
	int16_t propertyValueIndex;
};

struct WidgetActionItem {
	int16_t componentIndex;
	int16_t componentOutputIndex;
};

#define COMPONENT_INPUT_FLAG_IS_SEQ_INPUT   (1 << 0)
#define COMPONENT_INPUT_FLAG_IS_OPTIONAL (1 << 1)
typedef uint8_t ComponentInput;

struct Flow {
	ListOfAssetsPtr<Component> components;
	ListOfAssetsPtr<Value> localVariables;

	// List of all component inputs of all components in this flow
	// When flow state is created we reserve this many Value's in memory
	// to keep the latest value of component input.
	ListOfFundamentalType<ComponentInput> componentInputs;

	ListOfAssetsPtr<WidgetDataItem> widgetDataItems;
	ListOfAssetsPtr<WidgetActionItem> widgetActions;
};

struct FlowDefinition {
	ListOfAssetsPtr<Flow> flows;
	ListOfAssetsPtr<Value> constants;
	ListOfAssetsPtr<Value> globalVariables;
};

////////////////////////////////////////////////////////////////////////////////

struct Assets {
    uint8_t projectMajorVersion;
	uint8_t projectMinorVersion;
    uint8_t reserved;
    uint8_t external;

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

bool decompressAssetsData(const uint8_t *assetsData, uint32_t assetsDataSize, Assets *decompressedAssets, uint32_t maxDecompressedAssetsSize, int *err);

////////////////////////////////////////////////////////////////////////////////

void loadMainAssets(const uint8_t *assets, uint32_t assetsSize);
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

int getExternalAssetsMainPageId();

const char *getActionName(const WidgetCursor &widgetCursor, int16_t actionId);
int16_t getDataIdFromName(const WidgetCursor &widgetCursor, const char *name);

////////////////////////////////////////////////////////////////////////////////

} // namespace gui
} // namespace eez
