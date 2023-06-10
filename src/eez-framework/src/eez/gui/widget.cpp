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

#include <eez/conf-internal.h>

#if EEZ_OPTION_GUI

#include <assert.h>
#include <cstddef>
#include <limits.h>

#include <eez/core/debug.h>
#include <eez/core/os.h>
#include <eez/core/util.h>

#include <eez/gui/gui.h>
#include <eez/core/assets.h>

#include <eez/gui/draw.h>

#include <eez/gui/widgets/containers/app_view.h>
#include <eez/gui/widgets/containers/container.h>
#include <eez/gui/widgets/containers/grid.h>
#include <eez/gui/widgets/containers/user_widget.h>
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
#include <eez/gui/widgets/line_chart.h>
#include <eez/gui/widgets/qr_code.h>

namespace eez {
namespace gui {

////////////////////////////////////////////////////////////////////////////////

bool g_isActiveWidget;
EnumWidgetsCallback g_findCallback;
bool g_foundWidgetAtDownInvalid;

bool g_isRTL = false;

////////////////////////////////////////////////////////////////////////////////

struct NoneWidgetState : public WidgetState {
    bool updateState() {
        WIDGET_STATE_START(Widget);
        WIDGET_STATE_END()
    }
};

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

#define RENDER_WIDGET() \
    if ((!widget->visible || widgetState->isVisible.toBool()) && widgetCursor.opacity > 0) { \
        auto savedOpacity = display::setOpacity(widgetCursor.opacity); \
        widgetState->render(); \
        display::setOpacity(savedOpacity); \
    } else { \
        int x1 = g_widgetCursor.x; \
        int y1 = g_widgetCursor.y; \
        int x2 = g_widgetCursor.x + g_widgetCursor.w - 1; \
        int y2 = g_widgetCursor.y + g_widgetCursor.h - 1; \
        drawBorderAndBackground(x1, y1, x2, y2, nullptr, TRANSPARENT_COLOR_INDEX); \
    } \

void enumWidget() {
    WidgetCursor &widgetCursor = g_widgetCursor;
    const Widget *widget = widgetCursor.widget;

	auto widgetState = widgetCursor.currentState;

	bool savedIsActiveWidget = g_isActiveWidget;
	g_isActiveWidget = g_isActiveWidget || widgetCursor == g_activeWidget;

	if (g_findCallback) {
        if (!widget->visible || widgetState->isVisible.toBool()) {
    		g_findCallback();
        }
	} else {
		if (widgetCursor.hasPreviousState && widget->type == widgetState->type) {
            // reuse existing widget state
            bool refresh = widgetState->updateState();
            if (refresh || widgetCursor.refreshed) {
                RENDER_WIDGET();
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

            RENDER_WIDGET();

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
	if (stateSize > GUI_STATE_BUFFER_SIZE) {
        return;
    }

	widgetState->enumChildren();

	g_isActiveWidget = savedIsActiveWidget;
}

void enumNoneWidget() {
    WidgetCursor &widgetCursor = g_widgetCursor;
    auto savedWidget = widgetCursor.widget;
    widgetCursor.widget = &g_noneWidget;
    auto savedX = g_widgetCursor.x;
    auto savedY = g_widgetCursor.y;
    g_widgetCursor.x += g_noneWidget.x;
    g_widgetCursor.y += g_noneWidget.y;
    g_widgetCursor.w = g_noneWidget.width;
    g_widgetCursor.h = g_noneWidget.height;
	enumWidget();
    g_widgetCursor.x = savedX;
    g_widgetCursor.y = savedY;
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
                        g_foundWidget.appContext = (AppContext *)appContextValue.getVoidPointer();
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

////////////////////////////////////////////////////////////////////////////////

void resizeWidget(
    const Widget *widget,
    Rect &widgetRect,
    int containerOriginalWidth,
    int containerOriginalHeight,
    int containerWidth,
    int containerHeight
) {
    auto flags = widget->flags;

    auto pinToLeft = flags & WIDGET_FLAG_PIN_TO_LEFT;
    auto pinToRight = flags & WIDGET_FLAG_PIN_TO_RIGHT;
    auto pinToTop = flags & WIDGET_FLAG_PIN_TO_TOP;
    auto pinToBottom = flags & WIDGET_FLAG_PIN_TO_BOTTOM;

    auto fixWidth = flags & WIDGET_FLAG_FIX_WIDTH;
    auto fixHeight = flags & WIDGET_FLAG_FIX_HEIGHT;

    auto left = widgetRect.x;
    auto right = widgetRect.x + widgetRect.w;

    if (pinToLeft) {
        // left = left;
    } else {
        if (!fixWidth) {
            left =
                (widgetRect.x * containerWidth) /
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
            right = left + widgetRect.w;
        } else if (pinToRight && !pinToLeft) {
            left = right - widgetRect.w;
        } else if (!pinToLeft && !pinToRight) {
            auto center =
                ((widgetRect.x + widgetRect.w / 2) *
                    containerWidth) /
                containerOriginalWidth;
            left = center - widgetRect.w / 2;
            right = left + widgetRect.w;
        }
    }

    auto top = widgetRect.y;
    auto bottom = widgetRect.y + widgetRect.h;

    if (pinToTop) {
        //top = top;
    } else {
        if (!fixHeight) {
            top =
                (widgetRect.y * containerHeight) /
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
            bottom = top + widgetRect.h;
        } else if (pinToBottom && !pinToTop) {
            top = bottom - widgetRect.h;
        } else if (!pinToTop && !pinToBottom) {
            auto center =
                ((widgetRect.y + widgetRect.h / 2) *
                    containerHeight) /
                containerOriginalHeight;
            top = center - widgetRect.h / 2;
            bottom = top + widgetRect.h;
        }
    }


    widgetRect.x = left;
    widgetRect.y = top;
    widgetRect.w = right - left;
    widgetRect.h = bottom - top;
}

void applyTimeline(WidgetCursor& widgetCursor, Rect &widgetRect) {
    if (widgetCursor.widget->timeline.count > 0) {
        auto x = widgetCursor.widget->x;
        auto y = widgetCursor.widget->y;
        auto w = widgetCursor.widget->width;
        auto h = widgetCursor.widget->height;
        float opacity = 1.0f;

        auto timelinePosition = widgetCursor.flowState->timelinePosition;
        for (uint32_t i = 0; i < widgetCursor.widget->timeline.count; i++) {
            auto keyframe = widgetCursor.widget->timeline[i];

            if (timelinePosition < keyframe->start) {
                continue;
            }

            if (
                timelinePosition >= keyframe->start &&
                timelinePosition <= keyframe->end
            ) {
                auto t =
                    keyframe->start == keyframe->end
                        ? 1
                        : (timelinePosition - keyframe->start) /
                        (keyframe->end - keyframe->start);

                if (keyframe->enabledProperties & WIDGET_TIMELINE_PROPERTY_X) {
                    auto t2 = g_easingFuncs[keyframe->xEasingFunc](t);

                    if (keyframe->enabledProperties & WIDGET_TIMELINE_PROPERTY_CP2) {
                        auto p1 = x;
                        auto p2 = keyframe->cp1x;
                        auto p3 = keyframe->cp2x;
                        auto p4 = keyframe->x;
                        x =
                            (1 - t2) * (1 - t2) * (1 - t2) * p1 +
                            3 * (1 - t2) * (1 - t2) * t2 * p2 +
                            3 * (1 - t2) * t2 * t2 * p3 +
                            t2 * t2 * t2 * p4;
                    } else if (keyframe->enabledProperties & WIDGET_TIMELINE_PROPERTY_CP1) {
                        auto p1 = x;
                        auto p2 = keyframe->cp1x;
                        auto p3 = keyframe->x;
                        x =
                            (1 - t2) * (1 - t2) * p1 +
                            2 * (1 - t2) * t2 * p2 +
                            t2 * t2 * p3;
                    } else {
                        auto p1 = x;
                        auto p2 = keyframe->x;
                        x = (1 - t2) * p1 + t2 * p2;
                    }
                }

                if (keyframe->enabledProperties & WIDGET_TIMELINE_PROPERTY_WIDTH) {
                    w += g_easingFuncs[keyframe->widthEasingFunc](t) * (keyframe->width - w);
                }

                if (keyframe->enabledProperties & WIDGET_TIMELINE_PROPERTY_Y) {
                    auto t2 = g_easingFuncs[keyframe->yEasingFunc](t);

                    if (keyframe->enabledProperties & WIDGET_TIMELINE_PROPERTY_CP2) {
                        auto p1 = y;
                        auto p2 = keyframe->cp1y;
                        auto p3 = keyframe->cp2y;
                        auto p4 = keyframe->y;
                        y =
                            (1 - t2) * (1 - t2) * (1 - t2) * p1 +
                            3 * (1 - t2) * (1 - t2) * t2 * p2 +
                            3 * (1 - t2) * t2 * t2 * p3 +
                            t2 * t2 * t2 * p4;
                    } else if (keyframe->enabledProperties & WIDGET_TIMELINE_PROPERTY_CP1) {
                        auto p1 = y;
                        auto p2 = keyframe->cp1y;
                        auto p3 = keyframe->y;
                        y =
                            (1 - t2) * (1 - t2) * p1 +
                            2 * (1 - t2) * t2 * p2 +
                            t2 * t2 * p3;
                    } else {
                        auto p1 = y;
                        auto p2 = keyframe->y;
                        y = (1 - t2) * p1 + t2 * p2;
                    }
                }

                if (keyframe->enabledProperties & WIDGET_TIMELINE_PROPERTY_HEIGHT) {
                    h += g_easingFuncs[keyframe->heightEasingFunc](t) * (keyframe->height - h);
                }

                if (keyframe->enabledProperties & WIDGET_TIMELINE_PROPERTY_OPACITY) {
                    opacity += g_easingFuncs[keyframe->opacityEasingFunc](t) * (keyframe->opacity - opacity);
                }

                break;
            }

            if (keyframe->enabledProperties & WIDGET_TIMELINE_PROPERTY_X) {
                x = keyframe->x;
            }
            if (keyframe->enabledProperties & WIDGET_TIMELINE_PROPERTY_Y) {
                y = keyframe->y;
            }
            if (keyframe->enabledProperties & WIDGET_TIMELINE_PROPERTY_WIDTH) {
                w = keyframe->width;
            }
            if (keyframe->enabledProperties & WIDGET_TIMELINE_PROPERTY_HEIGHT) {
                h = keyframe->height;
            }

            if (keyframe->enabledProperties & WIDGET_TIMELINE_PROPERTY_OPACITY) {
                opacity = keyframe->opacity;
            }
        }

        widgetRect.x = x;
        widgetRect.y = y;

        widgetRect.w = w;
        widgetRect.h = h;

        widgetCursor.opacity = (uint8_t)roundf(255.0f * opacity);
    } else {
        widgetRect.x = widgetCursor.widget->x;
        widgetRect.y = widgetCursor.widget->y;

        widgetRect.w = widgetCursor.widget->width;
        widgetRect.h = widgetCursor.widget->height;
    }
}

void doStaticLayout(
    WidgetCursor& widgetCursor,
    const ListOfAssetsPtr<Widget> &widgets,
    int containerOriginalWidth,
    int containerOriginalHeight,
    int containerWidth,
    int containerHeight
) {
    bool callResizeWidget = containerOriginalWidth != containerWidth || containerOriginalHeight != containerHeight;

    for (uint32_t index = 0; index < widgets.count; ++index) {
        widgetCursor.widget = widgets[index];

        auto savedX = widgetCursor.x;
        auto savedY = widgetCursor.y;
        auto savedOpacity = widgetCursor.opacity;

        Rect widgetRect;

        applyTimeline(widgetCursor, widgetRect);

        if (callResizeWidget) {
            resizeWidget(widgetCursor.widget, widgetRect, containerOriginalWidth, containerOriginalHeight, containerWidth, containerHeight);
        }

        widgetCursor.x += widgetRect.x;
        widgetCursor.y += widgetRect.y;

        widgetCursor.w = widgetRect.w;
        widgetCursor.h = widgetRect.h;

        if (g_isRTL) {
            widgetCursor.x = savedX + containerWidth - ((widgetCursor.x - savedX) + widgetCursor.w);
        }

        enumWidget();

        widgetCursor.x = savedX;
        widgetCursor.y = savedY;
        widgetCursor.opacity = savedOpacity;
    }
}

} // namespace gui
} // namespace eez

#endif // EEZ_OPTION_GUI
