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

#include <eez/gui/geometry.h>

namespace eez {
namespace gui {

#define WIDGET_TYPES \
    WIDGET_TYPE(NONE, 0) \
    WIDGET_TYPE(CONTAINER, 1) \
    WIDGET_TYPE(LIST, 2) \
    WIDGET_TYPE(GRID, 3) \
    WIDGET_TYPE(SELECT, 4) \
    WIDGET_TYPE(DISPLAY_DATA, 5) \
    WIDGET_TYPE(TEXT, 6) \
    WIDGET_TYPE(MULTILINE_TEXT, 7) \
    WIDGET_TYPE(RECTANGLE, 8) \
    WIDGET_TYPE(BITMAP, 9) \
    WIDGET_TYPE(BUTTON, 10) \
    WIDGET_TYPE(TOGGLE_BUTTON, 11) \
    WIDGET_TYPE(BUTTON_GROUP, 12) \
    WIDGET_TYPE(RESERVED, 13) \
    WIDGET_TYPE(BAR_GRAPH, 14) \
    WIDGET_TYPE(LAYOUT_VIEW, 15) \
    WIDGET_TYPE(YT_GRAPH, 16) \
    WIDGET_TYPE(UP_DOWN, 17) \
    WIDGET_TYPE(LIST_GRAPH, 18) \
    WIDGET_TYPE(APP_VIEW, 19) \
    WIDGET_TYPE(SCROLL_BAR, 20) \
    WIDGET_TYPE(PROGRESS, 21)

#define WIDGET_TYPE(NAME, ID) WIDGET_TYPE_##NAME = ID,
enum ValueTypes {
	WIDGET_TYPES
};
#undef WIDGET_TYPE

typedef void (*EnumWidgetsCallback)(const WidgetCursor &widgetCursor);

struct Widget;
struct Assets;

typedef void (*FixPointersFunctionType)(Widget *widget, Assets *assets);
typedef void (*EnumFunctionType)(WidgetCursor &widgetCursor, EnumWidgetsCallback callback);
typedef void (*DrawFunctionType)(const WidgetCursor &widgetCursor);
typedef void (*OnTouchFunctionType)(const WidgetCursor &widgetCursor, Event &touchEvent);

////////////////////////////////////////////////////////////////////////////////

struct Bitmap {
    int16_t w;
    int16_t h;
    int16_t bpp;
    int16_t reserved;
    const uint8_t pixels[1];
};

////////////////////////////////////////////////////////////////////////////////

#define STYLE_FLAGS_HORZ_ALIGN_MASK 0x7
#define STYLE_FLAGS_HORZ_ALIGN_LEFT 0
#define STYLE_FLAGS_HORZ_ALIGN_RIGHT 1
#define STYLE_FLAGS_HORZ_ALIGN_CENTER 2
#define STYLE_FLAGS_HORZ_ALIGN_LEFT_RIGHT 3 // align left, but if it doesn't fit then align right

#define STYLE_FLAGS_VERT_ALIGN_MASK (0x7 << 3)
#define STYLE_FLAGS_VERT_ALIGN_TOP (0 << 3)
#define STYLE_FLAGS_VERT_ALIGN_BOTTOM (1 << 3)
#define STYLE_FLAGS_VERT_ALIGN_CENTER (2 << 3)

#define STYLE_FLAGS_BLINK (1 << 6)

struct Style {
    uint16_t flags;
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
    const Style *first;
};

void StyleList_fixPointers(StyleList &styleList);

////////////////////////////////////////////////////////////////////////////////

struct Widget {
    uint8_t type;
    int16_t data;
    int16_t action;
    int16_t x;
    int16_t y;
    int16_t w;
    int16_t h;
    uint16_t style;
    const void *specific;
};

void Widget_fixPointers(Widget *widget);

struct WidgetList {
    uint32_t count;
    const Widget *first;
};

void WidgetList_fixPointers(WidgetList &widgetList);

////////////////////////////////////////////////////////////////////////////////

#define SHADOW_FLAG 1
#define CLOSE_PAGE_IF_TOUCHED_OUTSIDE_FLAG 2

struct PageWidget {
    WidgetList widgets;
    uint16_t overlay;
    uint8_t flags;
};

struct Document {
    WidgetList pages;
};

////////////////////////////////////////////////////////////////////////////////

struct ColorList {
    uint32_t count;
    const uint16_t *first;
};

void ColorList_fixPointers(ColorList &colorList);

struct Theme {
    const char *name;
    ColorList colors;
};

void Theme_fixPointers(Theme *theme);

struct ThemeList {
    uint32_t count;
    const Theme *first;
};

void ThemeList_fixPointers(ThemeList &themeList);

struct Colors {
    ThemeList themes;
    ColorList colors;
};

////////////////////////////////////////////////////////////////////////////////

struct NameList {
    uint32_t count;
    const char **first;
};

struct Assets {
    Document *document;
    StyleList *styles;
    uint8_t *fontsData;
    uint8_t *bitmapsData;
    Colors *colorsData;
    NameList *actionNames;
    NameList *dataItemNames;
};

////////////////////////////////////////////////////////////////////////////////

struct WidgetStateFlags {
    unsigned active : 1;
    unsigned focused : 1;
    unsigned blinking : 1;
    unsigned enabled : 1;
};

struct WidgetState {
    uint16_t size;
    WidgetStateFlags flags;
    Value data;
};

class AppContext;

struct WidgetCursor {
    AppContext *appContext;
    const Widget *widget;
    int16_t x;
    int16_t y;
    Cursor cursor;
    WidgetState *previousState;
    WidgetState *currentState;

    WidgetCursor() 
		: appContext(nullptr)
        , widget(nullptr)
        , x(0)
        , y(0)
        , cursor(-1)
        , previousState(nullptr)
        , currentState(nullptr)
	{
    }

    WidgetCursor(
        AppContext *appContext_,
        const Widget *widget_,
        int x_,
        int y_,
        const Cursor cursor_,
        WidgetState *previousState_,
        WidgetState *currentState_
    )
        : appContext(appContext_)
        , widget(widget_)
        , x(x_)
        , y(y_)
        , cursor(cursor_)
        , previousState(previousState_)
        , currentState(currentState_)
    {
    }

    WidgetCursor &operator=(int) {
        widget = nullptr;
        cursor = -1;
        return *this;
    }

    bool operator!=(const WidgetCursor &rhs) const {
        return appContext != rhs.appContext || widget != rhs.widget || cursor != rhs.cursor;
    }

    bool operator==(const WidgetCursor &rhs) const {
        return !(*this != rhs);
    }

    operator bool() const {
        return widget != nullptr;
    }

    void nextState();
};

////////////////////////////////////////////////////////////////////////////////

void enumWidgets(AppContext* appContext, EnumWidgetsCallback callback);
void enumWidgets(WidgetCursor &widgetCursor, EnumWidgetsCallback callback);

void findWidgetStep(const WidgetCursor &widgetCursor);
WidgetCursor findWidget(AppContext* appContext, int16_t x, int16_t y);

extern OnTouchFunctionType *g_onTouchWidgetFunctions[];

WidgetState *nextWidgetState(WidgetState *p);
void enumWidget(WidgetCursor &widgetCursor, EnumWidgetsCallback callback);

extern bool g_isActiveWidget;
void drawWidgetCallback(const WidgetCursor &widgetCursor);

OnTouchFunctionType getTouchFunction(const WidgetCursor &widgetCursor);

uint16_t overrideStyleHook(const WidgetCursor &widgetCursor, uint16_t styleId);
uint16_t overrideStyleColorHook(const WidgetCursor &widgetCursor, const Style *style);
uint16_t overrideActiveStyleColorHook(const WidgetCursor &widgetCursor, const Style *style);

} // namespace gui
} // namespace eez
