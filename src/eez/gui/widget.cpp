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

#include <eez/gui/widgets/containers/app_view.h>
#include <eez/gui/widgets/containers/container.h>
#include <eez/gui/widgets/containers/grid.h>
#include <eez/gui/widgets/containers/layout_view.h>
#include <eez/gui/widgets/containers/list.h>
#include <eez/gui/widgets/containers/select.h>

#include <eez/gui/widgets/bar_graph.h>
#include <eez/gui/widgets/bitmap.h>
#include <eez/gui/widgets/button_group.h>
#include <eez/gui/widgets/button.h>
#include <eez/gui/widgets/canvas.h>
#include <eez/gui/widgets/display_data.h>
#include <eez/gui/widgets/gauge.h>
#include <eez/gui/widgets/input.h>
#include <eez/gui/widgets/list_graph.h>
#include <eez/gui/widgets/multiline_text.h>
#include <eez/gui/widgets/progress.h>
#include <eez/gui/widgets/rectangle.h>
#include <eez/gui/widgets/scroll_bar.h>
#include <eez/gui/widgets/text.h>
#include <eez/gui/widgets/toggle_button.h>
#include <eez/gui/widgets/up_down.h>
#include <eez/gui/widgets/yt_graph.h>

namespace eez {
namespace gui {

////////////////////////////////////////////////////////////////////////////////

bool g_isActiveWidget;
EnumWidgetsCallback g_findCallback;

////////////////////////////////////////////////////////////////////////////////

struct NoneWidgetState : public WidgetState {
	void render(WidgetCursor &widgetCursor) {}
};

static Widget g_noneWidget = { WIDGET_TYPE_NONE };

struct ReservedWidgetState : public WidgetState {
	void render(WidgetCursor &widgetCursor) {}
};

// widget placementNew functions
#define WIDGET_TYPE(NAME_PASCAL_CASE, NAME, ID) \
void NAME##_placementNew(void *ptr) { new (ptr) NAME_PASCAL_CASE##WidgetState(); }
WIDGET_TYPES
#undef WIDGET_TYPE

typedef void (*WidgetStatePlacementNewFunctionType)(void *ptr);

#define WIDGET_TYPE(NAME_PASCAL_CASE, NAME, ID) NAME##_placementNew,
static WidgetStatePlacementNewFunctionType g_widgetStatePlacementNewFunctions[] = {
    WIDGET_TYPES
};
#undef WIDGET_TYPE

// widget state sizes
#define WIDGET_TYPE(NAME_PASCAL_CASE, NAME, ID) sizeof(NAME_PASCAL_CASE##WidgetState),
static size_t g_widgetStateSizes[] = {
    WIDGET_TYPES
};
#undef WIDGET_TYPE

////////////////////////////////////////////////////////////////////////////////

bool WidgetState::updateState(const WidgetCursor &widgetCursor) {
	return false;
    }

void WidgetState::render(WidgetCursor &widgetCursor) {
}

void WidgetState::enumChildren(WidgetCursor &widgetCursor) {
}

bool WidgetState::hasOnTouch() {
    return false;
}

void WidgetState::onTouch(const WidgetCursor &widgetCursor, Event &touchEvent) {
}

bool WidgetState::hasOnKeyboard() {
    return false;
}

bool WidgetState::onKeyboard(const WidgetCursor &widgetCursor, uint8_t key, uint8_t mod) { 
    return false; 
}

////////////////////////////////////////////////////////////////////////////////

bool WidgetCursor::isPage() const {
    return widget->type == WIDGET_TYPE_CONTAINER && ((((ContainerWidget *)widget)->flags & PAGE_CONTAINER) != 0);
}

////////////////////////////////////////////////////////////////////////////////

void enumWidget(WidgetCursor &widgetCursor) {
    const Widget *widget = widgetCursor.widget;

    auto savedX = widgetCursor.x;
	auto savedY = widgetCursor.y;

    widgetCursor.x += widget->x;
    widgetCursor.y += widget->y;

	auto widgetState = widgetCursor.currentState;

	bool savedIsActiveWidget = g_isActiveWidget;
	g_isActiveWidget = g_isActiveWidget || isActiveWidget(widgetCursor);

	if (g_findCallback) {
		g_findCallback(widgetCursor);
	} else {
		if (widgetCursor.hasPreviousState && widget->type == widgetState->type) {
			if (widgetState->updateState(widgetCursor)) {
				widgetState->render(widgetCursor);
			}
		}
		else {
			if (widgetCursor.hasPreviousState) {
				freeWidgetStates(widgetState);
				widgetCursor.hasPreviousState = false;
			}
			g_widgetStatePlacementNewFunctions[widget->type](widgetState);
			widgetState->type = widget->type;
			
			widgetState->updateState(widgetCursor);
			widgetState->render(widgetCursor);
		}
	}

	widgetCursor.currentState = (WidgetState *)((uint8_t *)widgetCursor.currentState + g_widgetStateSizes[widget->type]);

	uint32_t stateSize = (uint8_t *)widgetCursor.currentState - (uint8_t *)g_widgetStateStart;
	assert(stateSize <= CONF_MAX_STATE_SIZE);

	widgetState->enumChildren(widgetCursor);

	g_isActiveWidget = savedIsActiveWidget;

    widgetCursor.x = savedX;
    widgetCursor.y = savedY;
}

void enumNoneWidget(WidgetCursor &widgetCursor) {
    auto savedWidget = widgetCursor.widget;
    widgetCursor.widget = &g_noneWidget;
	enumWidget(widgetCursor);
    widgetCursor.widget = savedWidget;
}    

////////////////////////////////////////////////////////////////////////////////

void freeWidgetStates(WidgetState *widgetStateStart) {
    WidgetState *widgetState = widgetStateStart;
    while (widgetState < g_widgetStateEnd) {
        auto nextWidgetState = (WidgetState *)((uint8_t*)widgetState + g_widgetStateSizes[widgetState->type]);
        widgetState->~WidgetState();
        widgetState = nextWidgetState;
    }
}

////////////////////////////////////////////////////////////////////////////////

void forEachWidget(EnumWidgetsCallback callback) {
	g_findCallback = callback;
    enumRootWidget();
	g_findCallback = nullptr;
}

////////////////////////////////////////////////////////////////////////////////

static int g_findWidgetAtX;
static int g_findWidgetAtY;
static bool g_clicked;

static bool g_found;
static WidgetCursor g_foundWidget;
static int g_distanceToFoundWidget;

int g_xOverlayOffset = 0;
int g_yOverlayOffset = 0;

static AppContext *g_popPageAppContext;

static void findWidgetStep(const WidgetCursor &widgetCursor) {
	if (g_found) {
		return;
	}

	if (widgetCursor.appContext->isActivePageInternal()) {
		auto internalPage = ((InternalPage *)widgetCursor.appContext->getActivePage());

		WidgetCursor foundWidget = internalPage->findWidgetInternalPage(widgetCursor, g_findWidgetAtX, g_findWidgetAtY, g_clicked);
		if (foundWidget) {
			g_foundWidget = foundWidget;
			g_found = true;
			return;
        }

        if (g_clicked) {
            // clicked outside internal page, close internal page
            g_popPageAppContext = widgetCursor.appContext;
        }

        bool passThrough = internalPage->canClickPassThrough();
        if (!passThrough) {
			g_foundWidget = 0;
			g_found = true;
			return;
		}
	}

    if (widgetCursor.isPage()) {
		if (g_foundWidget && g_foundWidget.appContext == widgetCursor.appContext) {
			g_foundWidget = widgetCursor;
			g_distanceToFoundWidget = INT_MAX;
		}
    }

    const Widget *widget = widgetCursor.widget;

    Overlay *overlay = getOverlay(widgetCursor);
	if (overlay) {
		getOverlayOffset(widgetCursor, g_xOverlayOffset, g_yOverlayOffset);
	}

	int x = widgetCursor.x + g_xOverlayOffset;
	int y = widgetCursor.y + g_yOverlayOffset;

	if (overlay) {
		g_xOverlayOffset = 0;
		g_yOverlayOffset = 0;
	}

    static const int MIN_SIZE = 50;
        
    int w = overlay ? overlay->width : widget->w;
    if (w < MIN_SIZE) {
        x = x - (MIN_SIZE - w) / 2;
        w = MIN_SIZE;
    }

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
                    if (widget->data != DATA_ID_NONE) {
                        Value appContextValue;
                        DATA_OPERATION_FUNCTION(widget->data, DATA_OPERATION_GET, widgetCursor, appContextValue);
                        g_foundWidget.appContext = appContextValue.getAppContext();
                    }
                }
            }
        }
    }
}

WidgetCursor findWidget(int16_t x, int16_t y, bool clicked) {
	g_found = false;
    g_foundWidget = 0;
    g_clicked = clicked;

    g_findWidgetAtX = x;
    g_findWidgetAtY = y;

    g_xOverlayOffset = 0;
    g_yOverlayOffset = 0;

    g_popPageAppContext = nullptr;

    forEachWidget(findWidgetStep);

    if (g_popPageAppContext) {
        g_popPageAppContext->popPage();
        g_popPageAppContext = nullptr;
    }

    return g_foundWidget;
}

} // namespace gui
} // namespace eez
