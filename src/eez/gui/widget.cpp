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

#include <assert.h>
#include <cstddef>
#include <limits.h>

#include <eez/debug.h>
#include <eez/os.h>

#include <eez/gui_conf.h>
#include <eez/gui/gui.h>

#include <eez/gui/widgets/app_view.h>
#include <eez/gui/widgets/bar_graph.h>
#include <eez/gui/widgets/bitmap.h>
#include <eez/gui/widgets/button_group.h>
#include <eez/gui/widgets/button.h>
#include <eez/gui/widgets/canvas.h>
#include <eez/gui/widgets/container.h>
#include <eez/gui/widgets/display_data.h>
#include <eez/gui/widgets/gauge.h>
#include <eez/gui/widgets/grid.h>
#include <eez/gui/widgets/input.h>
#include <eez/gui/widgets/layout_view.h>
#include <eez/gui/widgets/list_graph.h>
#include <eez/gui/widgets/list.h>
#include <eez/gui/widgets/multiline_text.h>
#include <eez/gui/widgets/progress.h>
#include <eez/gui/widgets/rectangle.h>
#include <eez/gui/widgets/scroll_bar.h>
#include <eez/gui/widgets/select.h>
#include <eez/gui/widgets/text.h>
#include <eez/gui/widgets/toggle_button.h>
#include <eez/gui/widgets/up_down.h>
#include <eez/gui/widgets/yt_graph.h>

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

EnumFunctionType NONE_enum = nullptr;
DrawFunctionType NONE_draw = nullptr;
OnTouchFunctionType NONE_onTouch = nullptr;
OnKeyboardFunctionType NONE_onKeyboard = nullptr;

EnumFunctionType RESERVED_enum = nullptr;
DrawFunctionType RESERVED_draw = nullptr;
OnTouchFunctionType RESERVED_onTouch = nullptr;
OnKeyboardFunctionType RESERVED_onKeyboard = nullptr;

////////////////////////////////////////////////////////////////////////////////

struct NoneWidgetState : public WidgetState {};
struct ReservedWidgetState : public WidgetState {};

#define WIDGET_TYPE(NAME_PASCAL_CASE, NAME, ID) \
void NAME##placementNew(void *ptr) { new (ptr) NAME_PASCAL_CASE##WidgetState; }
WIDGET_TYPES
#undef WIDGET_TYPE

typedef void (*WidgetStatePlacementNewFunctionType)(void *ptr);

#define WIDGET_TYPE(NAME_PASCAL_CASE, NAME, ID) NAME##placementNew,
static WidgetStatePlacementNewFunctionType g_widgetStatePlacementNewFunctions[] = {
    WIDGET_TYPES
};
#undef WIDGET_TYPE

#define WIDGET_TYPE(NAME_PASCAL_CASE, NAME, ID) sizeof(NAME_PASCAL_CASE##WidgetState),
static size_t g_widgetStateSize[] = {
    WIDGET_TYPES
};
#undef WIDGET_TYPE

////////////////////////////////////////////////////////////////////////////////

#define WIDGET_TYPE(NAME_PASCAL_CASE, NAME, ID) extern EnumFunctionType NAME##_enum;
WIDGET_TYPES
#undef WIDGET_TYPE
#define WIDGET_TYPE(NAME_PASCAL_CASE, NAME, ID) &NAME##_enum,
static EnumFunctionType *g_enumWidgetFunctions[] = {
    WIDGET_TYPES
};
#undef WIDGET_TYPE

#define WIDGET_TYPE(NAME_PASCAL_CASE, NAME, ID) extern DrawFunctionType NAME##_draw;
WIDGET_TYPES
#undef WIDGET_TYPE
#define WIDGET_TYPE(NAME_PASCAL_CASE, NAME, ID) &NAME##_draw,
static DrawFunctionType *g_drawWidgetFunctions[] = {
    WIDGET_TYPES
};
#undef WIDGET_TYPE

#define WIDGET_TYPE(NAME_PASCAL_CASE, NAME, ID) extern OnTouchFunctionType NAME##_onTouch;
WIDGET_TYPES
#undef WIDGET_TYPE
#define WIDGET_TYPE(NAME_PASCAL_CASE, NAME, ID) &NAME##_onTouch,
OnTouchFunctionType *g_onTouchWidgetFunctions[] = {
    WIDGET_TYPES
};
#undef WIDGET_TYPE

#define WIDGET_TYPE(NAME_PASCAL_CASE, NAME, ID) extern OnKeyboardFunctionType NAME##_onKeyboard;
WIDGET_TYPES
#undef WIDGET_TYPE
#define WIDGET_TYPE(NAME_PASCAL_CASE, NAME, ID) &NAME##_onKeyboard,
OnKeyboardFunctionType *g_onKeyboardWidgetFunctions[] = {
    WIDGET_TYPES
};
#undef WIDGET_TYPE

////////////////////////////////////////////////////////////////////////////////

void defaultWidgetDraw(const WidgetCursor &widgetCursor) {
    const Widget *widget = widgetCursor.widget;

    bool refresh =
        !widgetCursor.previousState ||
        widgetCursor.previousState->flags.active != widgetCursor.currentState->flags.active;

    if (refresh) {
        drawRectangle(widgetCursor.x, widgetCursor.y, (int)widget->w, (int)widget->h, getStyle(widget->style), widgetCursor.currentState->flags.active, false, true);
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
    currentState = nextWidgetState(currentState);
}

////////////////////////////////////////////////////////////////////////////////

void enumWidget(WidgetCursor &widgetCursor) {
    auto xSaved = widgetCursor.x;
    auto ySaved = widgetCursor.y;
    
    widgetCursor.x += widgetCursor.widget->x;
    widgetCursor.y += widgetCursor.widget->y;

    if (isOverlay(widgetCursor)) {
        // update overlay data
        auto containerWidget = (const ContainerWidget *)widgetCursor.widget;
        Value widgetCursorValue((void *)&widgetCursor, VALUE_TYPE_POINTER);
        DATA_OPERATION_FUNCTION(containerWidget->overlay, DATA_OPERATION_UPDATE_OVERLAY_DATA, widgetCursor, widgetCursorValue);
    }

    bool savedIsActiveWidget = g_isActiveWidget;
    g_isActiveWidget = g_isActiveWidget || isActiveWidget(widgetCursor);

    auto stateSize = getCurrentStateBufferSize(widgetCursor);
	static size_t maxStateSize = 0;
	if (stateSize > maxStateSize) {
		maxStateSize = stateSize;
		DebugTrace("max state size: %d\n", maxStateSize);
	}
    assert(stateSize <= CONF_MAX_STATE_SIZE);

    const Widget *widget = widgetCursor.widget;
    widgetCursor.currentState->widgetCursor = widgetCursor;
	widgetCursor.currentState->size = g_widgetStateSize[widget->type];
    widgetCursor.currentState->flags.active = g_isActiveWidget;

    if (*g_drawWidgetFunctions[widget->type]) {
        (*g_drawWidgetFunctions[widget->type])(widgetCursor);
    } else {
        defaultWidgetDraw(widgetCursor);
    }

    if (*g_enumWidgetFunctions[widgetCursor.widget->type]) {
       (*g_enumWidgetFunctions[widgetCursor.widget->type])(widgetCursor);
    }

    g_isActiveWidget = savedIsActiveWidget;

	widgetCursor.x = xSaved;
	widgetCursor.y = ySaved;
}

void forEachWidgetInAppContext(AppContext* appContext, EnumWidgetsCallback callback) {
	if (appContext->getActivePageId() == PAGE_ID_NONE || appContext->isActivePageInternal()) {
		return;
	}

    if (!g_currentState) {
        return;
    }

    WidgetState *currentState = g_currentState;
    while (currentState != g_currentStateEnd) {
        if (!callback(currentState->widgetCursor)) {
            break;
        }
        currentState = (WidgetState *)((uint8_t*)currentState + g_widgetStateSize[currentState->widgetCursor.widget->type]);
    }
}

////////////////////////////////////////////////////////////////////////////////

static int g_xOverlayOffset = 0;
static int g_yOverlayOffset = 0;
static WidgetState *g_widgetStateAfterOverlay = nullptr;

static bool findWidgetStep(const WidgetCursor &widgetCursor) {
	if (widgetCursor.appContext->isActivePageInternal()) {
		auto internalPage = ((InternalPage *)widgetCursor.appContext->getActivePage());

		WidgetCursor widgetCursor2 = internalPage->findWidget(g_findWidgetAtX, g_findWidgetAtY, g_clicked);

		if (!widgetCursor2) {
			bool passThrough = internalPage->canClickPassThrough();

			if (g_clicked) {
				// clicked outside internal page, close internal page
				widgetCursor.appContext->popPage();
			}

			if (!passThrough) {
				return false;
			}
		}
		else {
			g_foundWidget = widgetCursor2;
			return false;
		}
	}


    const Widget *widget = widgetCursor.widget;

    Overlay *overlay = getOverlay(widgetCursor);

    if (widgetCursor.currentState == g_widgetStateAfterOverlay) {
        g_xOverlayOffset = 0;
        g_yOverlayOffset = 0;
        g_widgetStateAfterOverlay = nullptr;
    }

    if (overlay) {
        getOverlayOffset(widgetCursor, g_xOverlayOffset, g_yOverlayOffset);
        g_widgetStateAfterOverlay = nextWidgetState(widgetCursor.currentState);
    }

    static const int MIN_SIZE = 50;
        
    int x = widgetCursor.x + g_xOverlayOffset;
    int w = overlay ? overlay->width : widget->w;
    if (w < MIN_SIZE) {
        x = x - (MIN_SIZE - w) / 2;
        w = MIN_SIZE;
    }

    int y = widgetCursor.y + g_yOverlayOffset;
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

        auto action = getWidgetAction(widgetCursor);        
        if (action == EEZ_CONF_ACTION_ID_DRAG_OVERLAY) {
            if (overlay && !overlay->state) {
                return true;
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
                    DATA_OPERATION_FUNCTION(widget->data, DATA_OPERATION_GET, widgetCursor, appContextValue);
                    g_foundWidget.appContext = appContextValue.getAppContext();
                }
            }
        }
    }

	return true;
}

WidgetCursor findWidget(AppContext* appContext, int16_t x, int16_t y, bool clicked) {
    g_foundWidget = 0;
    g_clicked = clicked;

    g_findWidgetAtX = x;
    g_findWidgetAtY = y;

    g_xOverlayOffset = 0;
    g_yOverlayOffset = 0;
    g_widgetStateAfterOverlay = nullptr;

    forEachWidgetInAppContext(appContext, findWidgetStep);

    return g_foundWidget;
}

} // namespace gui
} // namespace eez
