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

#include <assert.h>
#include <cstddef>
#include <limits.h>

#include <eez/system.h>

#include <eez/gui/gui.h>

using namespace eez::mcu;

namespace eez {
namespace gui {

////////////////////////////////////////////////////////////////////////////////

static int g_findWidgetAtX;
static int g_findWidgetAtY;
static WidgetCursor g_foundWidget;
static int g_distanceToFoundWidget;
static bool g_clicked;

bool g_isActiveWidget;

////////////////////////////////////////////////////////////////////////////////

FixPointersFunctionType NONE_fixPointers = nullptr;
EnumFunctionType NONE_enum = nullptr;
DrawFunctionType NONE_draw = nullptr;
OnTouchFunctionType NONE_onTouch = nullptr;
OnKeyboardFunctionType NONE_onKeyboard = nullptr;

FixPointersFunctionType RESERVED_fixPointers = nullptr;
EnumFunctionType RESERVED_enum = nullptr;
DrawFunctionType RESERVED_draw = nullptr;
OnTouchFunctionType RESERVED_onTouch = nullptr;
OnKeyboardFunctionType RESERVED_onKeyboard = nullptr;

////////////////////////////////////////////////////////////////////////////////

#define WIDGET_TYPE(NAME, ID) extern EnumFunctionType NAME##_enum;
WIDGET_TYPES
#undef WIDGET_TYPE
#define WIDGET_TYPE(NAME, ID) &NAME##_enum,
static EnumFunctionType *g_enumWidgetFunctions[] = {
    WIDGET_TYPES
};
#undef WIDGET_TYPE

#define WIDGET_TYPE(NAME, ID) extern DrawFunctionType NAME##_draw;
WIDGET_TYPES
#undef WIDGET_TYPE
#define WIDGET_TYPE(NAME, ID) &NAME##_draw,
static DrawFunctionType *g_drawWidgetFunctions[] = {
    WIDGET_TYPES
};
#undef WIDGET_TYPE

#define WIDGET_TYPE(NAME, ID) extern OnTouchFunctionType NAME##_onTouch;
WIDGET_TYPES
#undef WIDGET_TYPE
#define WIDGET_TYPE(NAME, ID) &NAME##_onTouch,
OnTouchFunctionType *g_onTouchWidgetFunctions[] = {
    WIDGET_TYPES
};
#undef WIDGET_TYPE

#define WIDGET_TYPE(NAME, ID) extern OnKeyboardFunctionType NAME##_onKeyboard;
WIDGET_TYPES
#undef WIDGET_TYPE
#define WIDGET_TYPE(NAME, ID) &NAME##_onKeyboard,
OnKeyboardFunctionType *g_onKeyboardWidgetFunctions[] = {
    WIDGET_TYPES
};
#undef WIDGET_TYPE

////////////////////////////////////////////////////////////////////////////////

void defaultWidgetDraw(const WidgetCursor &widgetCursor) {
    widgetCursor.currentState->size = sizeof(WidgetState);

    const Widget *widget = widgetCursor.widget;

    bool refresh =
        !widgetCursor.previousState ||
        widgetCursor.previousState->flags.active != widgetCursor.currentState->flags.active;

    if (refresh) {
        drawRectangle(widgetCursor.x, widgetCursor.y, (int)widget->w, (int)widget->h, getStyle(widget->style), widgetCursor.currentState->flags.active, false, true);
    }
}

void drawWidgetCallback(const WidgetCursor &widgetCursor_) {
    WidgetCursor widgetCursor = widgetCursor_;

    uint16_t stateSize = getCurrentStateBufferSize(widgetCursor);
    assert(stateSize <= CONF_MAX_STATE_SIZE);

    widgetCursor.currentState->flags.active = g_isActiveWidget;

    const Widget *widget = widgetCursor.widget;
    if (*g_drawWidgetFunctions[widget->type]) {
        (*g_drawWidgetFunctions[widget->type])(widgetCursor);
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

    bool savedIsActiveWidget = g_isActiveWidget;
    g_isActiveWidget = g_isActiveWidget || isActiveWidget(widgetCursor);

    callback(widgetCursor);

    if (*g_enumWidgetFunctions[widgetCursor.widget->type]) {
       (*g_enumWidgetFunctions[widgetCursor.widget->type])(widgetCursor, callback);
    }

    g_isActiveWidget = savedIsActiveWidget;

	widgetCursor.x = xSaved;
	widgetCursor.y = ySaved;
}

void enumWidgets(WidgetCursor &widgetCursor, EnumWidgetsCallback callback) {
    auto appContext = widgetCursor.appContext;

    if (appContext->isActivePageInternal()) {
    	if (callback != findWidgetStep) {
    		return;
    	}

        auto internalPage = ((InternalPage *)appContext->getActivePage());

		const WidgetCursor &foundWidget = internalPage->findWidget(g_findWidgetAtX, g_findWidgetAtY, g_clicked);
		if (foundWidget) {
            g_foundWidget = foundWidget;
            return;
		}

        if (!g_foundWidget || g_foundWidget.appContext != appContext) {
            return;
        }

        bool passThrough = internalPage->canClickPassThrough();

        if (g_clicked) {
            // clicked outside internal page, close internal page
            appContext->popPage();
        }

        if (!passThrough) {
            return;
        }
    }

    if (appContext->getActivePageId() == PAGE_ID_NONE) {
        return;
    }

    auto savedWidget = widgetCursor.widget;
    widgetCursor.widget = getPageWidget(appContext->getActivePageId());
    enumWidget(widgetCursor, callback);
    widgetCursor.widget = savedWidget;
}

void enumWidgets(AppContext* appContext, EnumWidgetsCallback callback) {
	if (appContext->getActivePageId() == PAGE_ID_NONE || appContext->isActivePageInternal()) {
		return;
	}
	WidgetCursor widgetCursor;
	widgetCursor.appContext = appContext;
	widgetCursor.widget = getPageWidget(appContext->getActivePageId());
	enumWidget(widgetCursor, callback);
}

////////////////////////////////////////////////////////////////////////////////

void findWidgetStep(const WidgetCursor &widgetCursor) {
    const Widget *widget = widgetCursor.widget;

    Overlay *overlay = getOverlay(widgetCursor);

    static const int MIN_SIZE = 50;
        
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

    if (inside && (widget->type == WIDGET_TYPE_APP_VIEW || getWidgetTouchFunction(widgetCursor))) {
        int dx = g_findWidgetAtX - (x + w / 2);
        int dy = g_findWidgetAtY - (y + h / 2);
        int distance = dx * dx + dy * dy;

        if (widget->action == ACTION_ID_DRAG_OVERLAY) {
            if (overlay && !overlay->state) {
                return;
            }
            g_foundWidget = widgetCursor;
            g_distanceToFoundWidget = INT_MAX;
        } else {
            if (
				!g_foundWidget || 
				distance <= g_distanceToFoundWidget || 
				g_foundWidget.widget->type == WIDGET_TYPE_APP_VIEW ||
				g_foundWidget.widget->type == WIDGET_TYPE_LIST ||
				g_foundWidget.widget->type == WIDGET_TYPE_GRID
			) {
                g_foundWidget = widgetCursor;
                g_distanceToFoundWidget = distance;

                // if found widget is AppView, make sure we set right AppContext
                if (widget->type == WIDGET_TYPE_APP_VIEW) {
                    Value appContextValue;
                    DATA_OPERATION_FUNCTION(widget->data, DATA_OPERATION_GET, widgetCursor.cursor, appContextValue);
                    g_foundWidget.appContext = appContextValue.getAppContext();
                }
            }
        }
    }
}

WidgetCursor findWidget(AppContext* appContext, int16_t x, int16_t y, bool clicked) {
    g_foundWidget = 0;
    g_clicked = clicked;

    if (appContext->isActivePageInternal()) {
        auto internalPage = ((InternalPage *)appContext->getActivePage());

        WidgetCursor widgetCursor = internalPage->findWidget(x, y, clicked);

        if (!widgetCursor) {
        	bool passThrough = internalPage->canClickPassThrough();

            if (clicked) {
                // clicked outside internal page, close internal page
                appContext->popPage();
            }

            if (!passThrough) {
    			return g_foundWidget;
    		}
        } else {
            return widgetCursor;
        }
    }


    g_findWidgetAtX = x;
    g_findWidgetAtY = y;

    enumWidgets(appContext, findWidgetStep);

    return g_foundWidget;
}

} // namespace gui
} // namespace eez

#endif
