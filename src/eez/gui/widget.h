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
#include <eez/memory.h>

namespace eez {

namespace flow {
	struct FlowState;
}

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
    WIDGET_TYPE(PROGRESS, 21) \
    WIDGET_TYPE(CANVAS, 22) \
    WIDGET_TYPE(GAUGE, 23) \
    WIDGET_TYPE(INPUT, 24) \

#define WIDGET_TYPE(NAME, ID) WIDGET_TYPE_##NAME = ID,
enum WidgetTypes {
	WIDGET_TYPES
};
#undef WIDGET_TYPE

typedef void (*EnumWidgetsCallback)(const WidgetCursor &widgetCursor);

struct Widget;
struct Assets;

typedef void (*EnumFunctionType)(WidgetCursor &widgetCursor, EnumWidgetsCallback callback);
typedef void (*DrawFunctionType)(const WidgetCursor &widgetCursor);
typedef void (*OnTouchFunctionType)(const WidgetCursor &widgetCursor, Event &touchEvent);
typedef bool (*OnKeyboardFunctionType)(const WidgetCursor &widgetCursor, uint8_t key, uint8_t mod);

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

typedef int Cursor;

static const size_t MAX_ITERATORS = 4;

struct WidgetCursor {
	Assets *assets;
	AppContext *appContext;
    const Widget *widget;
    Cursor cursor;
	int32_t iterators[MAX_ITERATORS];
    flow::FlowState *flowState;
    int16_t x;
    int16_t y;

    WidgetCursor() 
		: assets(nullptr)
		, appContext(nullptr)
        , widget(nullptr)
        , cursor(-1)
        , flowState(0)
        , x(0)
        , y(0)
	{
		for (size_t i = 0; i < MAX_ITERATORS; i++) {
			iterators[i] = -1;
		}
    }

    WidgetCursor(
		Assets *assets_,
        AppContext *appContext_,
        const Widget *widget_,
        const Cursor cursor_,
		int32_t *iterators_,
        WidgetState *previousState_,
        WidgetState *currentState_,
		flow::FlowState *flowState,
		int16_t x_,
		int16_t y_
    )
        : assets(assets_)
		, appContext(appContext_)
        , widget(widget_)
		, cursor(cursor_)
		, flowState(flowState)
		, x(x_)
		, y(y_)
    {
		if (iterators_) {
			for (size_t i = 0; i < MAX_ITERATORS; i++) {
				iterators[i] = iterators_[i];
			}
		} else {
			for (size_t i = 0; i < MAX_ITERATORS; i++) {
				iterators[i] = -1;
			}
		}
	}

	WidgetCursor(Cursor cursor_)
		: assets(nullptr)
		, appContext(nullptr)
		, widget(nullptr)
		, cursor(cursor_)
		, flowState(0)
		, x(0)
		, y(0) 
	{
		for (size_t i = 0; i < MAX_ITERATORS; i++) {
			iterators[i] = -1;
		}
	}

    bool operator!=(const WidgetCursor &rhs) const {
		if (widget != rhs.widget || cursor != rhs.cursor) {
			return true;
		}
		for (size_t i = 0; i < MAX_ITERATORS; i++) {
			if (iterators[i] != rhs.iterators[i]) {
				return true;
			}
		}
		return false;
	}

    bool operator==(const WidgetCursor &rhs) const {
        return !(*this != rhs);
    }

    explicit operator bool() const {
        return widget != nullptr;
    }

	void pushIterator(int32_t it) {
		for (size_t i = MAX_ITERATORS - 1; i > 0; i--) {
			iterators[i] = iterators[i - 1];
		}
		iterators[0] = it;
	}

	void popIterator() {
		for (size_t i = 1; i < MAX_ITERATORS; i++) {
			iterators[i - 1] = iterators[i];
		}
		iterators[MAX_ITERATORS - 1] = -1;
	}
};

////////////////////////////////////////////////////////////////////////////////

void enumWidgets(AppContext* appContext, EnumWidgetsCallback callback);
void enumWidgets(WidgetCursor &widgetCursor, EnumWidgetsCallback callback);

void findWidgetStep(const WidgetCursor &widgetCursor);
WidgetCursor findWidget(AppContext* appContext, int16_t x, int16_t y, bool clicked = true);

extern OnTouchFunctionType *g_onTouchWidgetFunctions[];
extern OnKeyboardFunctionType *g_onKeyboardWidgetFunctions[];

WidgetState *nextWidgetState(WidgetState *p);
void enumWidget(WidgetCursor &widgetCursor, EnumWidgetsCallback callback);

extern bool g_isActiveWidget;
void drawWidgetCallback(const WidgetCursor &widgetCursor);

OnTouchFunctionType getWidgetTouchFunction(const WidgetCursor &widgetCursor);

int overrideStyleHook(const WidgetCursor &widgetCursor, int styleId);
uint16_t overrideStyleColorHook(const WidgetCursor &widgetCursor, const Style *style);
uint16_t overrideActiveStyleColorHook(const WidgetCursor &widgetCursor, const Style *style);

} // namespace gui
} // namespace eez
