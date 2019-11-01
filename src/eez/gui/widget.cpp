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

#if OPTION_DISPLAY

#include <eez/gui/widget.h>

#include <assert.h>
#include <cstddef>

#include <eez/debug.h>
#include <eez/system.h>

#include <eez/modules/mcu/display.h>

#include <eez/gui/app_context.h>
#include <eez/gui/assets.h>
#include <eez/gui/draw.h>
#include <eez/gui/gui.h>
#include <eez/gui/touch.h>
#include <eez/gui/overlay.h>
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
#include <eez/gui/widgets/scroll_bar.h>
#include <eez/gui/widgets/select.h>
#include <eez/gui/widgets/text.h>
#include <eez/gui/widgets/toggle_button.h>
#include <eez/gui/widgets/up_down.h>
#include <eez/gui/widgets/yt_graph.h>

using namespace eez::mcu;

namespace eez {
namespace gui {

////////////////////////////////////////////////////////////////////////////////

static int g_findWidgetAtX;
static int g_findWidgetAtY;
static WidgetCursor g_foundWidget;
static int g_distanceToFoundWidget;

bool g_isActiveWidget;

////////////////////////////////////////////////////////////////////////////////

#if OPTION_SDRAM
typedef void (*FixWidgetPointersFunction)(Widget *widget);
static FixWidgetPointersFunction g_fixWidgetPointersFunctions[] = {
    nullptr,                         // WIDGET_TYPE_NONE
    ContainerWidget_fixPointers,     // WIDGET_TYPE_CONTAINER
    ListWidget_fixPointers,          // WIDGET_TYPE_LIST
    GridWidget_fixPointers,          // WIDGET_TYPE_GRID
    SelectWidget_fixPointers,        // WIDGET_TYPE_SELECT
    nullptr,                         // WIDGET_TYPE_DISPLAY_DATA
    TextWidget_fixPointers,          // WIDGET_TYPE_TEXT
    MultilineTextWidget_fixPointers, // WIDGET_TYPE_MULTILINE_TEXT
    nullptr,                         // WIDGET_TYPE_RECTANGLE
    nullptr,                         // WIDGET_TYPE_BITMAP
    ButtonWidget_fixPointers,        // WIDGET_TYPE_BUTTON
    ToggleButtonWidget_fixPointers,  // WIDGET_TYPE_TOGGLE_BUTTON
    nullptr,                         // WIDGET_TYPE_BUTTON_GROUP
    nullptr,                         // WIDGET_TYPE_SCALE
    nullptr,                         // WIDGET_TYPE_BAR_GRAPH
    nullptr,                         // WIDGET_TYPE_LAYOUT_VIEW
    nullptr,                         // WIDGET_TYPE_YT_GRAPH
    UpDownWidget_fixPointers,        // WIDGET_TYPE_UP_DOWN
    nullptr,                         // WIDGET_TYPE_LIST_GRAPH
    nullptr,                         // WIDGET_TYPE_APP_VIEW
    ScrollBarWidget_fixPointers,     // WIDGET_TYPE_SCROLL_BAR
};
#endif

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
    nullptr,               // WIDGET_TYPE_SCROLL_BAR
};

typedef void (*DrawFunctionType)(const WidgetCursor &widgetCursor);
static DrawFunctionType g_drawWidgetFunctions[] = {
    nullptr,                  // WIDGET_TYPE_NONE
    ContainerWidget_draw,     // WIDGET_TYPE_CONTAINER
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
    LayoutViewWidget_draw,    // WIDGET_TYPE_LAYOUT_VIEW
    YTGraphWidget_draw,       // WIDGET_TYPE_YT_GRAPH
    UpDownWidget_draw,        // WIDGET_TYPE_UP_DOWN
    ListGraphWidget_draw,     // WIDGET_TYPE_LIST_GRAPH
    AppViewWidget_draw,       // WIDGET_TYPE_APP_VIEW
    ScrollBarWidget_draw,     // WIDGET_TYPE_SCROLL_BAR
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
    ScrollBarWidget_onTouch,   // WIDGET_TYPE_SCROLL_BAR
};

////////////////////////////////////////////////////////////////////////////////

#if OPTION_SDRAM
void Widget_fixPointers(Widget *widget) {
    widget->specific = (void *)((uint8_t *)g_document + (uint32_t)widget->specific);
    if (g_fixWidgetPointersFunctions[widget->type]) {
        g_fixWidgetPointersFunctions[widget->type](widget);
    }
}
#endif

////////////////////////////////////////////////////////////////////////////////

void defaultWidgetDraw(const WidgetCursor &widgetCursor) {
    widgetCursor.currentState->size = sizeof(WidgetState);

    const Widget *widget = widgetCursor.widget;

    bool refresh =
        !widgetCursor.previousState ||
        widgetCursor.previousState->flags.active != widgetCursor.currentState->flags.active;

    if (refresh) {
        const Style* style = getStyle(widget->style);
		const Style* activeStyle = getStyle(widget->activeStyle);

        if (activeStyle) {
            drawRectangle(widgetCursor.x, widgetCursor.y, (int)widget->w, (int)widget->h,
                          widgetCursor.currentState->flags.active ? activeStyle : style, nullptr,
                          true, false);

        } else {
            drawRectangle(widgetCursor.x, widgetCursor.y, (int)widget->w, (int)widget->h, style,
                          nullptr, !widgetCursor.currentState->flags.active, false);
        }
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

void WidgetCursor::nextState() {
    if (previousState) {
        previousState = nextWidgetState(previousState);
    }
    if (currentState) {
        currentState = nextWidgetState(currentState);
    }
}

////////////////////////////////////////////////////////////////////////////////

void enumWidget(WidgetCursor &widgetCursor, EnumWidgetsCallback callback) {
    auto xSaved = widgetCursor.x;
    auto ySaved = widgetCursor.y;
    
    widgetCursor.x += widgetCursor.widget->x;
    widgetCursor.y += widgetCursor.widget->y;

    overlayEnumWidgetHook(widgetCursor, callback);

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

	widgetCursor.x = xSaved;
	widgetCursor.y = ySaved;
}

void enumWidgets(WidgetCursor &widgetCursor, EnumWidgetsCallback callback) {
    if (g_appContext->isActivePageInternal()) {
    	if (callback != findWidgetStep) {
    		return;
    	}

		const WidgetCursor &foundWidget = ((InternalPage *)g_appContext->getActivePage())->findWidget(g_findWidgetAtX, g_findWidgetAtY);
		if (foundWidget) {
            g_foundWidget = foundWidget;
			return;
		}

		// pass click through if active page is toast page and clicked outside
		bool passThrough = g_appContext->getActivePageId() == INTERNAL_PAGE_ID_TOAST_MESSAGE;

		// clicked outside internal page, close internal page
		popPage();

		if (!passThrough) {
			return;
		}
    }

    if (g_appContext->getActivePageId() == INTERNAL_PAGE_ID_NONE) {
        return;
    }

    auto savedWidget = widgetCursor.widget;
    widgetCursor.widget = getPageWidget(g_appContext->getActivePageId());
    enumWidget(widgetCursor, callback);
    widgetCursor.widget = savedWidget;
}

void enumWidgets(EnumWidgetsCallback callback) {
	if (g_appContext->getActivePageId() <= INTERNAL_PAGE_ID_NONE) {
		return;
	}
	WidgetCursor widgetCursor;
	widgetCursor.appContext = g_appContext;
	widgetCursor.widget = getPageWidget(g_appContext->getActivePageId());
	enumWidget(widgetCursor, callback);
}

////////////////////////////////////////////////////////////////////////////////

void findWidgetStep(const WidgetCursor &widgetCursor) {
    if (g_foundWidget && g_foundWidget.widget->action == ACTION_ID_DRAG_OVERLAY) {
        return;
    }

    const Widget *widget = widgetCursor.widget;

    Overlay *overlay = getOverlay(widgetCursor);

    static const int MIN_SIZE = 40;
        
    int x = widgetCursor.x;
    int w = overlay ? overlay->width : widget->w;
    if (w < MIN_SIZE) {
        x = x - (MIN_SIZE - w) / 2;
        w = MIN_SIZE;
    }

    int y = widgetCursor.y;
    int h = overlay ? overlay->height : widget->h;
    if (h < MIN_SIZE) {
        y = y - (MIN_SIZE - h) / 2;
        h = MIN_SIZE;
    }

    bool inside = 
        g_findWidgetAtX >= x && g_findWidgetAtX < x + w && 
        g_findWidgetAtY >= y && g_findWidgetAtY < y + h;

    if (inside && (widget->type == WIDGET_TYPE_APP_VIEW || getTouchFunction(widgetCursor))) {
        if (widget->action == ACTION_ID_DRAG_OVERLAY) {
            if (overlay && !overlay->state) {
                return;
            }
            g_foundWidget = widgetCursor;
            g_distanceToFoundWidget = 0;
        } else {
            int dx = g_findWidgetAtX - (x + w / 2);
            int dy = g_findWidgetAtY - (y + h / 2);
            int distance = dx * dx + dy * dy;

            if (!g_foundWidget || distance <= g_distanceToFoundWidget || g_foundWidget.widget->type == WIDGET_TYPE_APP_VIEW) {
                g_foundWidget = widgetCursor;
                g_distanceToFoundWidget = distance;

                // if found widget is AppView, make sure we set right AppContext
                if (widget->type == WIDGET_TYPE_APP_VIEW) {
                    Value appContextValue;
                    g_dataOperationsFunctions[widget->data](data::DATA_OPERATION_GET, (Cursor &)widgetCursor.cursor, appContextValue);
                    g_foundWidget.appContext = appContextValue.getAppContext();
                }
            }
        }
    }
}

WidgetCursor findWidget(int16_t x, int16_t y) {
    if (g_appContext->isActivePageInternal()) {
        WidgetCursor widgetCursor = ((InternalPage *)g_appContext->getActivePage())->findWidget(x, y);

        if (!widgetCursor) {
            // clicked outside internal page, close internal page
            popPage();
        } else {
            return widgetCursor;
        }
    }

    g_foundWidget = 0;

    g_findWidgetAtX = x;
    g_findWidgetAtY = y;

    enumWidgets(findWidgetStep);

    return g_foundWidget;
}

} // namespace gui
} // namespace eez

#endif