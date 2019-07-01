/*
 * EEZ Generic Firmware
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
#include <eez/gui/event.h>
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
#define WIDGET_TYPE_SCALE 13
#define WIDGET_TYPE_BAR_GRAPH 14
#define WIDGET_TYPE_LAYOUT_VIEW 15
#define WIDGET_TYPE_YT_GRAPH 16
#define WIDGET_TYPE_UP_DOWN 17
#define WIDGET_TYPE_LIST_GRAPH 18
#define WIDGET_TYPE_APP_VIEW 19

typedef uint32_t OBJ_OFFSET;

////////////////////////////////////////////////////////////////////////////////

struct Bitmap {
    int16_t w;
    int16_t h;
    int16_t bpp;
    int16_t reserved;
    const uint8_t pixels[1];
};

struct List {
    uint32_t count;
    OBJ_OFFSET first;
};

struct Style {
    uint16_t flags;
    uint16_t background_color;
    uint16_t color;
    uint16_t border_size;
    uint16_t border_radius;
    uint16_t border_color;
    uint8_t font;
    uint8_t opacity; // 0 - 255
    uint8_t padding_horizontal;
    uint8_t padding_vertical;
};

typedef List Styles;

struct Widget {
    uint8_t type;
    uint16_t data;
    uint16_t action;
    int16_t x;
    int16_t y;
    int16_t w;
    int16_t h;
    uint16_t style;
    uint16_t activeStyle;
    OBJ_OFFSET specific;
};

struct PageWidget {
    List widgets;
    uint8_t closePageIfTouchedOutside;
};

struct Document {
    List pages;
};

struct Theme {
    OBJ_OFFSET name;
    List colors;
};

struct Colors {
    List themes;
    List colors;
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
    uint16_t backgroundColor;
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

    WidgetCursor() : widget(nullptr) {
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
        return appContext != rhs.appContext || widget != rhs.widget || x != rhs.x || y != rhs.y ||
               cursor != rhs.cursor;
    }

    bool operator==(const WidgetCursor &rhs) const {
        return !(*this != rhs);
    }

    operator bool() const {
        return widget != nullptr;
    }
};

////////////////////////////////////////////////////////////////////////////////

typedef void (*EnumWidgetsCallback)(const WidgetCursor &widgetCursor);
void enumWidgets(EnumWidgetsCallback callback);
void enumWidgets(WidgetState *previousState, WidgetState *currentState,
                 EnumWidgetsCallback callback);
void enumWidgets(int16_t x, int16_t y, data::Cursor &cursor,
                WidgetState *previousState, WidgetState *currentState,
                EnumWidgetsCallback callback);                

WidgetCursor findWidget(int16_t x, int16_t y);

typedef void (*OnTouchFunctionType)(const WidgetCursor &widgetCursor, Event &touchEvent);

extern OnTouchFunctionType g_onTouchFunctions[];

WidgetState *nextWidgetState(WidgetState *p);
void enumWidget(OBJ_OFFSET widgetOffset, int16_t x, int16_t y, data::Cursor &cursor,
                WidgetState *previousState, WidgetState *currentState,
                EnumWidgetsCallback callback);

extern bool g_painted;
extern bool g_isActiveWidget;
void drawWidgetCallback(const WidgetCursor &widgetCursor);

OnTouchFunctionType getTouchFunction(const WidgetCursor &widgetCursor);

} // namespace gui
} // namespace eez
