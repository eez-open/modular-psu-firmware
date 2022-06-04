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

/* This template is used (on 64-bit systems) to pack asset pointers into 32-bit values.
 * All pointers are relative to MEMORY_BEGIN.
 * This way, the assets created by Studio can be used without having to fix all
 * the sizes - Studio creates 32-bit pointers that are relative to the
 * beginning of the assets, which the firmware rewrites to global pointers
 * during initialization. On a 32-bit system this works just fine, but for a
 * 64-bit system the pointers have different sizes and this breaks. By
 * inserting a 'middleman' structure that stores the pointers as a 32-bit
 * offset to MEMORY_BEGIN, we can keep the pointer sizes and initialization
 * code the same.
 */
template<typename T>
struct AssetsPtrImpl {
    /* Conversion to a T pointer */
    operator T*() { return ptr(); }
    operator const T*() const { return ptr(); }
    /* Dereferencing operators */
          T* operator->()       { return ptr(); }
    const T* operator->() const { return ptr(); }

	void operator=(T* ptr) {
		if (ptr != nullptr) {
            offset = (uint8_t *)ptr - MEMORY_BEGIN;
		} else {
			offset = 0;
		}
    }

    uint32_t offset = 0;

private:
    T* ptr() {
        return offset ? (T *)(MEMORY_BEGIN + offset) : nullptr;
    }

	const T* ptr() const {
		return offset ? (const T *)(MEMORY_BEGIN + offset) : nullptr;
	}
};

/* This struct chooses the type used for AssetsPtr<T> - by default it uses an AssetsPtrImpl<> */
template<typename T, uint32_t ptrSize>
struct AssetsPtrChooser
{
    using type = AssetsPtrImpl<T>;
};

/* On 32-bit systems, we can just use raw pointers */
template<typename T>
struct AssetsPtrChooser<T, 4>
{
    using type = T*;
};

/* Utility typedef that delegates to AssetsPtrChooser */
template<typename T>
using AssetsPtr = typename AssetsPtrChooser<T, sizeof(void*)>::type;

////////////////////////////////////////////////////////////////////////////////

template<typename T>
struct ListOfAssetsPtr {
    /* Array access */
    T*       operator[](uint32_t i)       { return item(i); }
    const T* operator[](uint32_t i) const { return item(i); }

	uint32_t count = 0;
    AssetsPtr<AssetsPtr<T>> items;

private:
    T* item(int i) {
        return static_cast<T *>(static_cast<AssetsPtr<T> *>(items)[i]);
    }

    const T* item(int i) const {
        return static_cast<const T *>(static_cast<const     AssetsPtr<T> *>(items)[i]);
    }
};

template<typename T>
struct ListOfFundamentalType {
    /* Array access */
    T&       operator[](uint32_t i)       { return ptr()[i]; }
    const T& operator[](uint32_t i) const { return ptr()[i]; }

	uint32_t count;
    AssetsPtr<T> items;

private:
    T *ptr() {
        return static_cast<T *>(items);
    }
};

////////////////////////////////////////////////////////////////////////////////

struct Settings {
    uint16_t displayWidth;
    uint16_t displayHeight;
};

////////////////////////////////////////////////////////////////////////////////

#define WIDGET_FLAG_PIN_TO_LEFT (1 << 0)
#define WIDGET_FLAG_PIN_TO_RIGHT (1 << 1)
#define WIDGET_FLAG_PIN_TO_TOP (1 << 2)
#define WIDGET_FLAG_PIN_TO_BOTTOM (1 << 3)

#define WIDGET_FLAG_FIX_WIDTH (1 << 4)
#define WIDGET_FLAG_FIX_HEIGHT (1 << 5)

#define WIDGET_TIMELINE_PROPERTY_X (1 << 0)
#define WIDGET_TIMELINE_PROPERTY_Y (1 << 1)
#define WIDGET_TIMELINE_PROPERTY_WIDTH (1 << 2)
#define WIDGET_TIMELINE_PROPERTY_HEIGHT (1 << 3)
#define WIDGET_TIMELINE_PROPERTY_OPACITY (1 << 4)

struct TimelineKeyframe {
    float start;
    float end;
    uint32_t enabledProperties;
	int16_t x;
	int16_t y;
	int16_t width;
	int16_t height;
    float opacity;
};

struct Widget {
	uint16_t type;
	int16_t data;
    int16_t visible;
	int16_t action;
	int16_t x;
	int16_t y;
	int16_t width;
	int16_t height;
	int16_t style;
    uint16_t flags;
    ListOfAssetsPtr<TimelineKeyframe> timeline;
};

#define SHADOW_FLAG (1 << 0)
#define CLOSE_PAGE_IF_TOUCHED_OUTSIDE_FLAG (1 << 1)
#define PAGE_IS_USED_AS_CUSTOM_WIDGET (1 << 2)
#define PAGE_CONTAINER (1 << 3)
#define PAGE_SCALE_TO_FIT (1 << 4)

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
    uint8_t reserved1;
    uint8_t reserved2;
    uint8_t reserved3;
	uint8_t pixels[1];
};

struct GlyphsGroup {
    uint32_t encoding;
    uint32_t glyphIndex;
    uint32_t length;
};

struct FontData {
	uint8_t ascent;
	uint8_t descent;
    uint8_t reserved1;
    uint8_t reserved2;
	uint32_t encodingStart;
	uint32_t encodingEnd;
    ListOfAssetsPtr<GlyphsGroup> groups;
    ListOfAssetsPtr<GlyphData> glyphs;
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

struct Property {
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

	ListOfAssetsPtr<Property> properties;
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

struct Language {
    AssetsPtr<const char> languageID;
    ListOfAssetsPtr<const char> translations;
};

////////////////////////////////////////////////////////////////////////////////

struct Assets {
    uint8_t projectMajorVersion;
	uint8_t projectMinorVersion;
    uint8_t reserved;
    uint8_t external;

    AssetsPtr<Settings> settings;
	ListOfAssetsPtr<PageAsset> pages;
	ListOfAssetsPtr<Style> styles;
	ListOfAssetsPtr<FontData> fonts;
	ListOfAssetsPtr<Bitmap> bitmaps;
	AssetsPtr<Colors> colorsDefinition;
	ListOfAssetsPtr<const char> actionNames;
	ListOfAssetsPtr<const char> variableNames;
	AssetsPtr<FlowDefinition> flowDefinition;
    ListOfAssetsPtr<Language> languages;
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
