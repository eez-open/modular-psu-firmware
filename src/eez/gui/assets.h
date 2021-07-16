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

template <typename T>
struct List {
    uint32_t count;
    AssetsPtr<T> first;
};

////////////////////////////////////////////////////////////////////////////////

#define SHADOW_FLAG 1
#define CLOSE_PAGE_IF_TOUCHED_OUTSIDE_FLAG 2

struct PageWidget {
    List<const Widget> widgets;
    uint16_t overlay;
    uint8_t flags;
};

struct Widget {
    uint8_t type;
    int16_t data;
    int16_t action;
    int16_t x;
    int16_t y;
    int16_t w;
    int16_t h;
    uint16_t style;
    AssetsPtr<const void> specific;
};

struct Document {
	List<const Widget> pages;
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

struct StyleList {
    uint32_t count;
    AssetsPtr<const Style> first;
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

struct ColorList {
    uint32_t count;
    AssetsPtr<const uint16_t> first;
};

struct Theme {
    AssetsPtr<const char> name;
    ColorList colors;
};

struct ThemeList {
    uint32_t count;
    AssetsPtr<const Theme> first;
};

struct Colors {
    ThemeList themes;
    ColorList colors;
};

////////////////////////////////////////////////////////////////////////////////

struct NameList {
    uint32_t count;
    AssetsPtr<AssetsPtr<const char>> first;
};

////////////////////////////////////////////////////////////////////////////////

struct ComponentInputValue {
    uint16_t valueIndex;
};

struct ComponentInput {
    List<ComponentInputValue> values;
};

struct Connection {
    uint16_t targetComponentIndex;
    uint8_t targetInputIndex;
};

struct ComponentOutput {
    List<Connection> connections;
};

struct Component {
    uint16_t type;
    List<ComponentInput> inputs;
    List<ComponentOutput> outputs;
    AssetsPtr<const void> specific;
};

struct Flow {
    List<const Component> components;
};

struct FlowDefinition {
    List<const Flow> flows;
    List<gui::Value> flowValues;
    List<AssetsPtr<ComponentInput>> widgetDataItems;
    List<AssetsPtr<ComponentOutput>> widgetActions;
};

////////////////////////////////////////////////////////////////////////////////

struct Assets {
    uint16_t projectVersion;
    bool external;

    AssetsPtr<Document> document;
    AssetsPtr<StyleList> styles;
    AssetsPtr<uint8_t> fontsData;
    AssetsPtr<uint8_t> bitmapsData;
    AssetsPtr<Colors> colorsData;

    AssetsPtr<NameList> actionNames;
	AssetsPtr<NameList> dataItemNames;

    AssetsPtr<FlowDefinition> flowDefinition;
};

////////////////////////////////////////////////////////////////////////////////

void WidgetList_fixPointers(List<const Widget> &list, Assets *assets);
void Widget_fixPointers(Widget *widget, Assets *assets);

template <typename T>
void Widget_fixPointers(AssetsPtr<const Widget> T::*widgetProperty, Widget *widget, Assets *assets) {
    auto specific = (T *)widget->specific;
    specific->*widgetProperty = (Widget *)((uint8_t *)(void *)assets->document + (uint32_t)(specific->*widgetProperty));
	Widget_fixPointers((Widget *)&*(specific->*widgetProperty), assets);
}

template <typename T>
void Text_fixPointer(AssetsPtr<const char> T::*textProperty, Widget *widget, Assets *assets) {
	auto specific = (T *)widget->specific;
	specific->*textProperty = (const char *)((uint8_t *)(void *)assets->document + (uint32_t)(specific->*textProperty));
}

////////////////////////////////////////////////////////////////////////////////

void loadMainAssets();
bool loadExternalAssets(const char *filePath, int *err);

extern bool g_isMainAssetsLoaded;
extern Assets *g_mainAssets;
extern Assets *g_externalAssets;

const Widget *getPageWidget(int pageId);
const Style *getStyle(int styleID);
const uint8_t *getFontData(int fontID);
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
#define GET_WIDGET_LIST_ELEMENT(list, index) &((list).first[index])

////////////////////////////////////////////////////////////////////////////////

} // namespace gui
} // namespace eez
