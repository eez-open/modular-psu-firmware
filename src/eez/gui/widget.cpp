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

#include <eez/core/debug.h>
#include <eez/core/os.h>

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
#include <eez/gui/widgets/drop_down_list.h>
#include <eez/gui/widgets/gauge.h>
#include <eez/gui/widgets/input.h>
#include <eez/gui/widgets/list_graph.h>
#include <eez/gui/widgets/multiline_text.h>
#include <eez/gui/widgets/progress.h>
#include <eez/gui/widgets/rectangle.h>
#include <eez/gui/widgets/roller.h>
#include <eez/gui/widgets/scroll_bar.h>
#include <eez/gui/widgets/slider.h>
#include <eez/gui/widgets/switch.h>
#include <eez/gui/widgets/text.h>
#include <eez/gui/widgets/toggle_button.h>
#include <eez/gui/widgets/up_down.h>
#include <eez/gui/widgets/yt_graph.h>

namespace eez {
namespace gui {

////////////////////////////////////////////////////////////////////////////////

bool g_isActiveWidget;
EnumWidgetsCallback g_findCallback;
bool g_foundWidgetAtDownInvalid;

////////////////////////////////////////////////////////////////////////////////

struct NoneWidgetState : public WidgetState {};

static Widget g_noneWidget = { WIDGET_TYPE_NONE };

struct ReservedWidgetState : public WidgetState {};

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

bool WidgetState::updateState() {
	return false;
}

void WidgetState::render() {
}

void WidgetState::enumChildren() {
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

void WidgetCursor::pushBackground(int x, int y, const Style *style, bool active) {
    if (backgroundStyleStackPointer == BACKGROUND_STYLE_STACK_SIZE) {
        // make space: remove item at the bottom of stack
        for (size_t i = 1; i < BACKGROUND_STYLE_STACK_SIZE; i++) {
            backgroundStyleStack[i - 1] = backgroundStyleStack[i];
        }
        backgroundStyleStackPointer--;
    }

    backgroundStyleStack[backgroundStyleStackPointer].x = x;
    backgroundStyleStack[backgroundStyleStackPointer].y = y;
    backgroundStyleStack[backgroundStyleStackPointer].style = style;
    backgroundStyleStack[backgroundStyleStackPointer].active = active;
    backgroundStyleStackPointer++;
}

void WidgetCursor::popBackground() {
	if (backgroundStyleStackPointer > 0) {
		backgroundStyleStackPointer--;
	}
}

////////////////////////////////////////////////////////////////////////////////

void enumWidget() {
    WidgetCursor &widgetCursor = g_widgetCursor;

    const Widget *widget = widgetCursor.widget;

    auto savedX = widgetCursor.x;
	auto savedY = widgetCursor.y;

    widgetCursor.x += widget->x;
    widgetCursor.y += widget->y;

	auto widgetState = widgetCursor.currentState;

	bool savedIsActiveWidget = g_isActiveWidget;
	g_isActiveWidget = g_isActiveWidget || widgetCursor == g_activeWidget;

	if (g_findCallback) {
		g_findCallback();
	} else {
		if (widgetCursor.hasPreviousState && widget->type == widgetState->type) {
            // reuse existing widget state
            bool refresh = widgetState->updateState();
            if (refresh || widgetCursor.refreshed) {
                widgetState->render();
            }
		} else {
			if (widgetCursor.hasPreviousState) {
				// clear old state from current state
				freeWidgetStates(widgetState);
				widgetCursor.hasPreviousState = false;
			}

			// create new widget state
			g_widgetStatePlacementNewFunctions[widget->type](widgetState);
			widgetState->type = widget->type;

			widgetState->updateState();
			widgetState->render();

			if (g_foundWidgetAtDownInvalid) {
				// find new cursor for g_foundWidgetAtDown
				auto &foundWidgetAtDown = getFoundWidgetAtDown();
				if (foundWidgetAtDown == widgetCursor) {
					foundWidgetAtDown = widgetCursor;
					g_foundWidgetAtDownInvalid = false;
				}
			}
		}
	}

	widgetCursor.currentState = (WidgetState *)((uint8_t *)widgetCursor.currentState + g_widgetStateSizes[widget->type]);

	uint32_t stateSize = (uint8_t *)widgetCursor.currentState - (uint8_t *)g_widgetStateStart;
	assert(stateSize <= CONF_MAX_STATE_SIZE);

	widgetState->enumChildren();

	g_isActiveWidget = savedIsActiveWidget;

    widgetCursor.x = savedX;
    widgetCursor.y = savedY;
}

void enumNoneWidget() {
    WidgetCursor &widgetCursor = g_widgetCursor;
    auto savedWidget = widgetCursor.widget;
    widgetCursor.widget = &g_noneWidget;
    g_widgetCursor.w = g_noneWidget.width;
    g_widgetCursor.h = g_noneWidget.height;
	enumWidget();
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

    // invalidate g_foundWidgetAtDown if it was among freed widgets
	auto &widgetCursor = getFoundWidgetAtDown();
	if (widgetCursor.currentState >= widgetStateStart) {
		g_foundWidgetAtDownInvalid = true;
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

static void findWidgetStep() {
	if (g_found) {
		return;
	}

    WidgetCursor &widgetCursor = g_widgetCursor;

	if (widgetCursor.appContext->isActivePageInternal()) {
		auto internalPage = (InternalPage *)widgetCursor.appContext->getActivePage();

		WidgetCursor foundWidget = internalPage->findWidgetInternalPage(g_findWidgetAtX, g_findWidgetAtY, g_clicked);
		if (foundWidget) {
			g_foundWidget = foundWidget;
			g_found = true;
			return;
        }

        if (g_clicked) {
            if (internalPage->closeIfTouchedOutside()) {
                // clicked outside internal page, close internal page (if not toast)
                g_popPageAppContext = widgetCursor.appContext;
            }
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

    int w = overlay ? overlay->width : widgetCursor.w;
    if (w < MIN_SIZE) {
        x = x - (MIN_SIZE - w) / 2;
        w = MIN_SIZE;
    }

    int h = overlay ? overlay->height : widgetCursor.h;
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
        if (action == ACTION_ID_DRAG_OVERLAY) {
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
                        Value appContextValue = get(widgetCursor, widget->data);
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

void resizeWidget(
    WidgetCursor &widgetCursor,
    int containerOriginalWidth,
    int containerOriginalHeight,
    int containerWidth,
    int containerHeight
) {
    auto widget = widgetCursor.widget;
    auto flags = widget->flags;

    auto pinToLeft = flags & WIDGET_FLAG_PIN_TO_LEFT;
    auto pinToRight = flags & WIDGET_FLAG_PIN_TO_RIGHT;
    auto pinToTop = flags & WIDGET_FLAG_PIN_TO_TOP;
    auto pinToBottom = flags & WIDGET_FLAG_PIN_TO_BOTTOM;

    auto fixWidth = flags & WIDGET_FLAG_FIX_WIDTH;
    auto fixHeight = flags & WIDGET_FLAG_FIX_HEIGHT;

    auto left = widget->x;
    auto right = widget->x + widget->width;

    if (pinToLeft) {
        // left = left;
    } else {
        if (!fixWidth) {
            left =
                (widget->x * containerWidth) /
                containerOriginalWidth;
        }
    }

    if (pinToRight) {
        right = containerWidth - (containerOriginalWidth - right);
    } else {
        if (!fixWidth) {
            right = (right * containerWidth) / containerOriginalWidth;
        }
    }

    if (fixWidth) {
        if (pinToLeft && !pinToRight) {
            right = left + widget->width;
        } else if (pinToRight && !pinToLeft) {
            left = right - widget->width;
        } else if (!pinToLeft && !pinToRight) {
            auto center =
                ((widget->x + widget->width / 2) *
                    containerWidth) /
                containerOriginalWidth;
            left = center - widget->width / 2;
            right = left + widget->width;
        }
    }

    auto top = widget->y;
    auto bottom = widget->y + widget->height;

    if (pinToTop) {
        //top = top;
    } else {
        if (!fixHeight) {
            top =
                (widget->y * containerHeight) /
                containerOriginalHeight;
        }
    }

    if (pinToBottom) {
        bottom = containerHeight - (containerOriginalHeight - bottom);
    } else {
        if (!fixHeight) {
            bottom =
                (bottom * containerHeight) / containerOriginalHeight;
        }
    }

    if (fixHeight) {
        if (pinToTop && !pinToBottom) {
            bottom = top + widget->height;
        } else if (pinToBottom && !pinToTop) {
            top = bottom - widget->height;
        } else if (!pinToTop && !pinToBottom) {
            auto center =
                ((widget->y + widget->height / 2) *
                    containerHeight) /
                containerOriginalHeight;
            top = center - widget->height / 2;
            bottom = top + widget->height;
        }
    }


    widgetCursor.x += left - widget->x;
    widgetCursor.y += top - widget->y;
    widgetCursor.w = right - left;
    widgetCursor.h = bottom - top;
}

} // namespace gui
} // namespace eez
