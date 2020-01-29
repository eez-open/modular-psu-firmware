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

#define STYLE_FLAGS_HORZ_ALIGN 6
#define STYLE_FLAGS_HORZ_ALIGN_LEFT 0
#define STYLE_FLAGS_HORZ_ALIGN_RIGHT 2
#define STYLE_FLAGS_HORZ_ALIGN_CENTER 4
#define STYLE_FLAGS_VERT_ALIGN 24
#define STYLE_FLAGS_VERT_ALIGN_TOP 0
#define STYLE_FLAGS_VERT_ALIGN_BOTTOM 8
#define STYLE_FLAGS_VERT_ALIGN_CENTER 16
#define STYLE_FLAGS_BLINK 32

#define WIDGET_TYPE_NONE 0
#define WIDGET_TYPE_CONTAINER 1
#define WIDGET_TYPE_LIST 2
#define WIDGET_TYPE_GRID 3
#define WIDGET_TYPE_SELECT 4
#define WIDGET_TYPE_DISPLAY_DATA 5
#define WIDGET_TYPE_TEXT 6
#define WIDGET_TYPE_MULTILINE_TEXT 7
#define WIDGET_TYPE_RECTANGLE 8
#define WIDGET_TYPE_BITMAP 9
#define WIDGET_TYPE_BUTTON 10
#define WIDGET_TYPE_TOGGLE_BUTTON 11
#define WIDGET_TYPE_BUTTON_GROUP 12
#define WIDGET_TYPE_RESERVED 13  // RESERVED
#define WIDGET_TYPE_BAR_GRAPH 14
#define WIDGET_TYPE_LAYOUT_VIEW 15
#define WIDGET_TYPE_YT_GRAPH 16
#define WIDGET_TYPE_UP_DOWN 17
#define WIDGET_TYPE_LIST_GRAPH 18
#define WIDGET_TYPE_APP_VIEW 19
#define WIDGET_TYPE_SCROLL_BAR 20

////////////////////////////////////////////////////////////////////////////////

struct Bitmap {
    int16_t w;
    int16_t h;
    int16_t bpp;
    int16_t reserved;
    const uint8_t pixels[1];
};

struct Style {
    uint16_t flags;
    uint16_t background_color;
    uint16_t color;
    uint16_t active_background_color;
    uint16_t active_color;
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
#if OPTION_SDRAM
    const Style *first;
#else
    uint32_t firstOffset;
#endif
};

void StyleList_fixPointers(StyleList &styleList);

struct Widget {
    uint8_t type;
    uint16_t data;
    uint16_t action;
    int16_t x;
    int16_t y;
    int16_t w;
    int16_t h;
    uint16_t style;
#if OPTION_SDRAM
    const void *specific;
#else
    uint32_t specificOffset;
#endif
};

#if OPTION_SDRAM
void Widget_fixPointers(Widget *widget);
#endif

struct WidgetList {
    uint32_t count;
#if OPTION_SDRAM
    const Widget *first;
#else
    uint32_t firstOffset;
#endif
};

void WidgetList_fixPointers(WidgetList &widgetList);

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

struct ColorList {
    uint32_t count;
#if OPTION_SDRAM
    const uint16_t *first;
#else
    uint32_t firstOffset;
#endif
};

void ColorList_fixPointers(ColorList &colorList);

struct Theme {
#if OPTION_SDRAM
    const char *name;
#else
    uint32_t nameOffset;
#endif
    ColorList colors;
};

void Theme_fixPointers(Theme *theme);

struct ThemeList {
    uint32_t count;
#if OPTION_SDRAM
    const Theme *first;
#else
    uint32_t firstOffset;
#endif
};

void ThemeList_fixPointers(ThemeList &themeList);

struct Colors {
    ThemeList themes;
    ColorList colors;
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
    data::Value data;
};

////////////////////////////////////////////////////////////////////////////////

class AppContext;

struct WidgetCursor {
    AppContext *appContext;
    const Widget *widget;
    int16_t x;
    int16_t y;
    data::Cursor cursor;
    WidgetState *previousState;
    WidgetState *currentState;

    WidgetCursor() 
		: appContext(nullptr), widget(nullptr), x(0), y(0),
		previousState(nullptr), currentState(nullptr)
	{
    }

    WidgetCursor(AppContext *appContext_, const Widget *widget_, int x_, int y_,
                 const data::Cursor &cursor_, WidgetState *previousState_,
                 WidgetState *currentState_)
        : appContext(appContext_), widget(widget_), x(x_), y(y_), cursor(cursor_),
          previousState(previousState_), currentState(currentState_) {
    }

    WidgetCursor &operator=(int) {
        widget = nullptr;
        cursor.i = -1;
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

typedef void (*EnumWidgetsCallback)(const WidgetCursor &widgetCursor);
void enumWidgets(EnumWidgetsCallback callback);
void enumWidgets(WidgetCursor &widgetCursor, EnumWidgetsCallback callback);

void findWidgetStep(const WidgetCursor &widgetCursor);
WidgetCursor findWidget(int16_t x, int16_t y);

typedef void (*OnTouchFunctionType)(const WidgetCursor &widgetCursor, Event &touchEvent);

extern OnTouchFunctionType g_onTouchFunctions[];

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
