/* / system.h
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

#include <eez/gui/widget.h>

#include <assert.h>
#include <cstddef>

#include <eez/debug.h>
#include <eez/gui/app_context.h>
#include <eez/gui/assets.h>
#include <eez/gui/draw.h>
#include <eez/gui/gui.h>
#include <eez/gui/touch.h>
#include <eez/gui/widgets/app_view.h>
#include <eez/gui/widgets/bar_graph.h>
#include <eez/gui/widgets/bitmap.h>
#include <eez/gui/widgets/button.h>
#include <eez/gui/widgets/button_group.h>
#include <eez/gui/widgets/container.h>
#include <eez/gui/widgets/display_data.h>
#include <eez/gui/widgets/grid.h>
#include <eez/gui/widgets/layout_view.h>
#include <eez/gui/widgets/list.h>
#include <eez/gui/widgets/list_graph.h>
#include <eez/gui/widgets/multiline_text.h>
#include <eez/gui/widgets/rectangle.h>
#include <eez/gui/widgets/scale.h>
#include <eez/gui/widgets/select.h>
#include <eez/gui/widgets/text.h>
#include <eez/gui/widgets/toggle_button.h>
#include <eez/gui/widgets/up_down.h>
#include <eez/gui/widgets/yt_graph.h>
#include <eez/modules/mcu/display.h>
#include <eez/system.h>

using namespace eez::mcu;

namespace eez {
namespace gui {

////////////////////////////////////////////////////////////////////////////////

static int g_findWidgetAtX;
static int g_findWidgetAtY;
static WidgetCursor g_foundWidget;

bool g_painted = true;
bool g_isActiveWidget;

////////////////////////////////////////////////////////////////////////////////

typedef void (*EnumFunctionType)(WidgetCursor &widgetCursor, EnumWidgetsCallback callback);
static EnumFunctionType g_enumWidgetFunctions[] = {
    nullptr,               // WIDGET_TYPE_NONE
    ContainerWidget_enum,  // WIDGET_TYPE_CONTAINER
    ListWidget_enum,       // WIDGET_TYPE_LIST
    GridWidget_enum,       // WIDGET_TYPE_GRID
    SelectWidget_enum,     // WIDGET_TYPE_SELECT
    nullptr,               // WIDGET_TYPE_DISPLAY_DATA
    nullptr,               // WIDGET_TYPE_TEXT
    nullptr,               // WIDGET_TYPE_MULTILINE_TEXT
    nullptr,               // WIDGET_TYPE_RECTANGLE
    nullptr,               // WIDGET_TYPE_BITMAP
    nullptr,               // WIDGET_TYPE_BUTTON
    nullptr,               // WIDGET_TYPE_TOGGLE_BUTTON
    nullptr,               // WIDGET_TYPE_BUTTON_GROUP
    nullptr,               // WIDGET_TYPE_SCALE
    nullptr,               // WIDGET_TYPE_BAR_GRAPH
    LayoutViewWidget_enum, // WIDGET_TYPE_LAYOUT_VIEW
    nullptr,               // WIDGET_TYPE_YT_GRAPH
    nullptr,               // WIDGET_TYPE_UP_DOWN
    nullptr,               // WIDGET_TYPE_LIST_GRAPH
    AppViewWidget_enum,    // WIDGET_TYPE_APP_VIEW
};

typedef void (*DrawFunctionType)(const WidgetCursor &widgetCursor);
static DrawFunctionType g_drawWidgetFunctions[] = {
    nullptr,                  // WIDGET_TYPE_NONE
    nullptr,                  // WIDGET_TYPE_CONTAINER
    nullptr,                  // WIDGET_TYPE_LIST
    nullptr,                  // WIDGET_TYPE_GRID
    nullptr,                  // WIDGET_TYPE_SELECT
    DisplayDataWidget_draw,   // WIDGET_TYPE_DISPLAY_DATA
    TextWidget_draw,          // WIDGET_TYPE_TEXT
    MultilineTextWidget_draw, // WIDGET_TYPE_MULTILINE_TEXT
    RectangleWidget_draw,     // WIDGET_TYPE_RECTANGLE
    BitmapWidget_draw,        // WIDGET_TYPE_BITMAP
    ButtonWidget_draw,        // WIDGET_TYPE_BUTTON
    ToggleButtonWidget_draw,  // WIDGET_TYPE_TOGGLE_BUTTON
    ButtonGroupWidget_draw,   // WIDGET_TYPE_BUTTON_GROUP
    ScaleWidget_draw,         // WIDGET_TYPE_SCALE
    BarGraphWidget_draw,      // WIDGET_TYPE_BAR_GRAPH
    nullptr,                  // WIDGET_TYPE_LAYOUT_VIEW
    YTGraphWidget_draw,       // WIDGET_TYPE_YT_GRAPH
    UpDownWidget_draw,        // WIDGET_TYPE_UP_DOWN
    ListGraphWidget_draw,     // WIDGET_TYPE_LIST_GRAPH
    AppViewWidget_draw,       // WIDGET_TYPE_APP_VIEW
};

OnTouchFunctionType g_onTouchFunctions[] = {
    nullptr,                   // WIDGET_TYPE_NONE
    nullptr,                   // WIDGET_TYPE_CONTAINER
    nullptr,                   // WIDGET_TYPE_LIST
    nullptr,                   // WIDGET_TYPE_GRID
    nullptr,                   // WIDGET_TYPE_SELECT
    nullptr,                   // WIDGET_TYPE_DISPLAY_DATA
    nullptr,                   // WIDGET_TYPE_TEXT
    nullptr,                   // WIDGET_TYPE_MULTILINE_TEXT
    nullptr,                   // WIDGET_TYPE_RECTANGLE
    nullptr,                   // WIDGET_TYPE_BITMAP
    nullptr,                   // WIDGET_TYPE_BUTTON
    nullptr,                   // WIDGET_TYPE_TOGGLE_BUTTON
    ButtonGroupWidget_onTouch, // WIDGET_TYPE_BUTTON_GROUP
    nullptr,                   // WIDGET_TYPE_SCALE
    nullptr,                   // WIDGET_TYPE_BAR_GRAPH
    nullptr,                   // WIDGET_TYPE_LAYOUT_VIEW
    nullptr,                   // WIDGET_TYPE_YT_GRAPH
    UpDownWidget_onTouch,      // WIDGET_TYPE_UP_DOWN
    ListGraphWidget_onTouch,   // WIDGET_TYPE_LIST_GRAPH
    nullptr,                   // WIDGET_TYPE_APP_VIEW
};

////////////////////////////////////////////////////////////////////////////////

void defaultWidgetDraw(const WidgetCursor &widgetCursor) {
    widgetCursor.currentState->size = sizeof(WidgetState);

    const Widget *widget = widgetCursor.widget;

    widgetCursor.currentState->data =
        widget->data ? data::get(widgetCursor.cursor, widget->data) : Value();

    bool refresh =
        !widgetCursor.previousState ||
        widgetCursor.previousState->flags.active != widgetCursor.currentState->flags.active ||
        widgetCursor.previousState->data != widgetCursor.currentState->data;

    if (refresh) {
        DECL_WIDGET_STYLE(style, widget);
        DECL_STYLE(activeStyle, widget->activeStyle);

        if (activeStyle) {
            drawRectangle(widgetCursor.x, widgetCursor.y, (int)widget->w, (int)widget->h,
                          widgetCursor.currentState->flags.active ? activeStyle : style, nullptr,
                          true, false);

        } else {
            drawRectangle(widgetCursor.x, widgetCursor.y, (int)widget->w, (int)widget->h, style,
                          nullptr, !widgetCursor.currentState->flags.active, false);
        }

        g_painted = true;
    }
}

void drawWidgetCallback(const WidgetCursor &widgetCursor_) {
    WidgetCursor widgetCursor = widgetCursor_;

    uint16_t stateSize = getCurrentStateBufferSize(widgetCursor);
    assert(stateSize <= CONF_MAX_STATE_SIZE);

    widgetCursor.currentState->flags.active = g_isActiveWidget;

    const Widget *widget = widgetCursor.widget;
    if (g_drawWidgetFunctions[widget->type]) {
        g_drawWidgetFunctions[widget->type](widgetCursor);
    } else {
        defaultWidgetDraw(widgetCursor);
    }
}

////////////////////////////////////////////////////////////////////////////////

WidgetState *nextWidgetState(WidgetState *p) {
    return (WidgetState *)(((uint8_t *)p) + p->size);
}

void enumWidget(WidgetCursor &widgetCursor, EnumWidgetsCallback callback) {
	widgetCursor.x += widgetCursor.widget->x;
	widgetCursor.y += widgetCursor.widget->y;

    bool savedIsActiveWidget;
    if (callback == drawWidgetCallback) {
        savedIsActiveWidget = g_isActiveWidget;
        g_isActiveWidget = g_isActiveWidget || isActiveWidget(widgetCursor);
    }

    callback(widgetCursor);

    if (g_enumWidgetFunctions[widgetCursor.widget->type]) {
        g_enumWidgetFunctions[widgetCursor.widget->type](widgetCursor, callback);
    }

    if (callback == drawWidgetCallback) {
        g_isActiveWidget = savedIsActiveWidget;
    }

	widgetCursor.x -= widgetCursor.widget->x;
	widgetCursor.y -= widgetCursor.widget->y;
}

void findWidgetStep(const WidgetCursor &widgetCursor);

void enumWidgets(WidgetCursor &widgetCursor, EnumWidgetsCallback callback) 
{
    if (g_appContext->isActivePageInternal()) {
    	if (callback == findWidgetStep) {
    		g_foundWidget = ((InternalPage *)g_appContext->getActivePage())->findWidget(g_findWidgetAtX, g_findWidgetAtY);
    		g_foundWidget.appContext = g_appContext;
    	}
    } else {
		auto savedWidgetOffset = widgetCursor.widgetOffset;
		auto savedWidget = widgetCursor.widget;

		widgetCursor.widgetOffset = getPageOffset(g_appContext->getActivePageId());

		// TODO optimize this
		DECL_WIDGET(widget, widgetCursor.widgetOffset);
		widgetCursor.widget = widget;
        
		enumWidget(widgetCursor, callback);

		widgetCursor.widgetOffset = savedWidgetOffset;
		widgetCursor.widget = savedWidget;
    }
}

void enumWidgets(EnumWidgetsCallback callback) {
	WidgetCursor widgetCursor;

	widgetCursor.appContext = g_appContext;

	widgetCursor.widgetOffset = getPageOffset(g_appContext->getActivePageId());

	// TODO optimize this
	DECL_WIDGET(widget, widgetCursor.widgetOffset);
	widgetCursor.widget = widget;

	enumWidget(widgetCursor, callback);
}

////////////////////////////////////////////////////////////////////////////////

void findWidgetStep(const WidgetCursor &widgetCursor) {
    const Widget *widget = widgetCursor.widget;

    // if (g_enumWidgetFunctions[widget->type]) {
    //     return;
    // }

    bool inside = g_findWidgetAtX >= widgetCursor.x &&
                  g_findWidgetAtX < widgetCursor.x + widget->w &&
                  g_findWidgetAtY >= widgetCursor.y && g_findWidgetAtY < widgetCursor.y + widget->h;

    if (inside && (widget->type == WIDGET_TYPE_APP_VIEW || getTouchFunction(widgetCursor))) {
        g_foundWidget = widgetCursor;

        // if found widget is AppView, make sure we set right AppContext
        if (widget->type == WIDGET_TYPE_APP_VIEW) {
            Value appContextValue;
            g_dataOperationsFunctions[widget->data](data::DATA_OPERATION_GET,
                                                    (Cursor &)widgetCursor.cursor, appContextValue);
            g_foundWidget.appContext = appContextValue.getAppContext();
        }
    }
}

WidgetCursor findWidget(int16_t x, int16_t y) {
    if (g_appContext->isActivePageInternal()) {
        return ((InternalPage *)g_appContext->getActivePage())->findWidget(x, y);
    } else {
        g_foundWidget = 0;

        g_findWidgetAtX = x;
        g_findWidgetAtY = y;

        enumWidgets(findWidgetStep);

        return g_foundWidget;
    }
}

} // namespace gui
} // namespace eez
