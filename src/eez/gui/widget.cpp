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
#include <eez/gui/assets.h>

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

bool g_isActiveWidget;

////////////////////////////////////////////////////////////////////////////////

struct NoneWidgetState : public WidgetState {
    NoneWidgetState(const WidgetCursor &widgetCursor) : WidgetState(widgetCursor) {
    }
};

struct ReservedWidgetState : public WidgetState {
    ReservedWidgetState(const WidgetCursor &widgetCursor) : WidgetState(widgetCursor) {
    }
};

#define WIDGET_TYPE(NAME_PASCAL_CASE, NAME, ID) \
void NAME##placementNew(void *ptr, const WidgetCursor& widgetCursor) { new (ptr) NAME_PASCAL_CASE##WidgetState(widgetCursor); }
WIDGET_TYPES
#undef WIDGET_TYPE

typedef void (*WidgetStatePlacementNewFunctionType)(void *ptr, const WidgetCursor& widgetCursor);

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

WidgetCursor WidgetState::getFirstChildWidgetCursor(WidgetCursor &widgetCursor, WidgetState *&currentState, WidgetState *&previousState) {
    auto widgetStateSize = g_widgetStateSize[widgetCursor.widget->type];

    if (previousState) {
        previousState = (WidgetState *)((uint8_t *)previousState + widgetStateSize);
    }

    currentState = (WidgetState *)((uint8_t *)currentState + widgetStateSize);

    //
    uint32_t stateSize = (uint8_t *)currentState - (uint8_t *)g_currentState;
    assert(stateSize <= CONF_MAX_STATE_SIZE);

	return widgetCursor;
}

void WidgetState::draw(WidgetState *previousState) {
    const Widget *widget = widgetCursor.widget;

    bool refresh = !previousState || previousState->flags.active != flags.active;
	if (refresh) {
        drawRectangle(widgetCursor.x, widgetCursor.y, (int)widget->w, (int)widget->h, getStyle(widget->style), flags.active, false, true);
    }
}

bool WidgetState::hasOnTouch() {
    return false;
}

void WidgetState::onTouch(Event &touchEvent) {
}

bool WidgetState::hasOnKeyboard() {
    return false;
}

bool WidgetState::onKeyboard(uint8_t key, uint8_t mod) { 
    return false; 
}

////////////////////////////////////////////////////////////////////////////////

bool WidgetCursor::isPage() const {
    return widget->type == WIDGET_TYPE_CONTAINER && ((((ContainerWidget *)widget)->flags & PAGE_CONTAINER) != 0);
}

////////////////////////////////////////////////////////////////////////////////

void enumWidget(WidgetCursor &widgetCursor, WidgetState *currentState, WidgetState *previousState) {
    const Widget *widget = widgetCursor.widget;

    bool savedIsActiveWidget = g_isActiveWidget;
    g_isActiveWidget = g_isActiveWidget || isActiveWidget(widgetCursor);

    auto widgetState = currentState;

	g_widgetStatePlacementNewFunctions[widget->type](widgetState, widgetCursor);

    widgetState->widgetStateSize = g_widgetStateSize[widget->type];
    widgetState->widgetCursor.x += widget->x;
    widgetState->widgetCursor.y += widget->y;

    widgetState->draw(previousState);

    g_isActiveWidget = savedIsActiveWidget;
}

void enumNoneWidget(WidgetCursor &widgetCursor, WidgetState *currentState, WidgetState *previousState) {
	static Widget g_noneWidget = { WIDGET_TYPE_NONE };
	const Widget *widget = &g_noneWidget;

	auto widgetState = currentState;

	g_widgetStatePlacementNewFunctions[widget->type](widgetState, widgetCursor);

	widgetState->widgetCursor = widgetCursor;
	widgetState->widgetCursor.widget = widget;
	widgetState->widgetStateSize = g_widgetStateSize[widget->type];
}

////////////////////////////////////////////////////////////////////////////////

void forEachWidget(AppContext* appContext, EnumWidgetsCallback callback) {
    if (!g_currentState) {
        return;
    }

    WidgetState *widgetState = g_currentState;
	WidgetState *widgetStateEnd = nextWidgetState(g_currentState);
    while (widgetState != widgetStateEnd) {
        if (!callback(widgetState)) {
            break;
        }
        widgetState = (WidgetState *)((uint8_t*)widgetState + g_widgetStateSize[widgetState->widgetCursor.widget->type]);
    }
}

WidgetState *getWidgetState(const WidgetCursor &widgetCursor) {
    if (!g_currentState) {
        return nullptr;
    }

    WidgetState *widgetState = g_currentState;
	WidgetState *widgetStateEnd = nextWidgetState(g_currentState);
    while (widgetState != widgetStateEnd) {
        if (widgetState->widgetCursor == widgetCursor) {
            return widgetState;
        }
        widgetState = (WidgetState *)((uint8_t*)widgetState + g_widgetStateSize[widgetState->widgetCursor.widget->type]);
    }    

	return nullptr;
}

void freeWidgetStates(WidgetState *topWidgetState) {
    WidgetState *widgetState = topWidgetState;
	WidgetState *widgetStateEnd = nextWidgetState(topWidgetState);
    while (widgetState != widgetStateEnd) {
        auto nextWidgetState = (WidgetState *)((uint8_t*)widgetState + g_widgetStateSize[widgetState->widgetCursor.widget->type]);
        widgetState->~WidgetState();
        widgetState = nextWidgetState;
    }
}

////////////////////////////////////////////////////////////////////////////////

static int g_findWidgetAtX;
static int g_findWidgetAtY;
static WidgetCursor g_foundWidget;
static int g_distanceToFoundWidget;
static bool g_clicked;

static int g_xOverlayOffset = 0;
static int g_yOverlayOffset = 0;
static WidgetState *g_widgetStateAfterOverlay = nullptr;

static bool findWidgetStep(WidgetState *widgetState) {
    const WidgetCursor &widgetCursor = widgetState->widgetCursor;

	if (widgetCursor.appContext->isActivePageInternal()) {
		auto internalPage = ((InternalPage *)widgetCursor.appContext->getActivePage());

		WidgetCursor foundWidget = internalPage->findWidgetInternalPage(g_findWidgetAtX, g_findWidgetAtY, g_clicked);
		if (foundWidget) {
			g_foundWidget = foundWidget;
			return false;
        }

        if (g_clicked) {
            // clicked outside internal page, close internal page
            widgetCursor.appContext->popPage();
        }

        bool passThrough = internalPage->canClickPassThrough();
        if (!passThrough) {
			g_foundWidget = 0;
            return false;
        }
	}

    if (widgetCursor.isPage()) {
        g_foundWidget = widgetCursor;
        g_distanceToFoundWidget = INT_MAX;
    }

    const Widget *widget = widgetCursor.widget;

    Overlay *overlay = getOverlay(widgetCursor);

    if (widgetState == g_widgetStateAfterOverlay) {
        g_xOverlayOffset = 0;
        g_yOverlayOffset = 0;
        g_widgetStateAfterOverlay = nullptr;
    }

    if (overlay) {
        getOverlayOffset(widgetCursor, g_xOverlayOffset, g_yOverlayOffset);
        g_widgetStateAfterOverlay = nextWidgetState(widgetState);
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
                    if (widget->data != DATA_ID_NONE) {
                        Value appContextValue;
                        DATA_OPERATION_FUNCTION(widget->data, DATA_OPERATION_GET, widgetCursor, appContextValue);
                        g_foundWidget.appContext = appContextValue.getAppContext();
                    }
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

    forEachWidget(appContext, findWidgetStep);

    return g_foundWidget;
}

} // namespace gui
} // namespace eez
