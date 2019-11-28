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

#if OPTION_DISPLAY

#include <eez/gui/widgets/container.h>
#include <eez/gui/widgets/layout_view.h>

#include <eez/gui/assets.h>
#include <eez/gui/draw.h>
#include <eez/gui/app_context.h>
#include <eez/gui/overlay.h>

#include <eez/system.h>
#include <eez/debug.h>

#if OPTION_SDRAM
#include <eez/modules/mcu/display.h>
#endif

namespace eez {
namespace gui {

#if OPTION_SDRAM
void ContainerWidget_fixPointers(Widget *widget) {
    ContainerWidget *containerWidget = (ContainerWidget *)widget->specific;
    WidgetList_fixPointers(containerWidget->widgets);
}
#endif

void enumContainer(WidgetCursor &widgetCursor, EnumWidgetsCallback callback, const WidgetList &widgets) {
    auto savedCurrentState = widgetCursor.currentState;
	auto savedPreviousState = widgetCursor.previousState;

    WidgetState *endOfContainerInPreviousState = 0;
    if (widgetCursor.previousState) {
        endOfContainerInPreviousState = nextWidgetState(widgetCursor.previousState);
    }

    // move to the first child widget state
    if (widgetCursor.previousState) {
        if (widgetCursor.widget->type == WIDGET_TYPE_CONTAINER) {
            widgetCursor.previousState = (WidgetState *)(((ContainerWidgetState *)widgetCursor.previousState) + 1);
        } else if (widgetCursor.widget->type == WIDGET_TYPE_LAYOUT_VIEW) {
            widgetCursor.previousState = (WidgetState *)(((LayoutViewWidgetState *)widgetCursor.previousState) + 1);
        } else {
            ++widgetCursor.previousState;
        }
    }
    if (widgetCursor.currentState) {
        if (widgetCursor.widget->type == WIDGET_TYPE_CONTAINER) {
            widgetCursor.currentState = (WidgetState *)(((ContainerWidgetState *)widgetCursor.currentState) + 1);
        } else if (widgetCursor.widget->type == WIDGET_TYPE_LAYOUT_VIEW) {
            widgetCursor.currentState = (WidgetState *)(((LayoutViewWidgetState *)widgetCursor.currentState) + 1);
        } else {
            ++widgetCursor.currentState;
        }
    }

    auto savedWidget = widgetCursor.widget;

    Overlay *overlay = nullptr;
    if (isOverlay(widgetCursor)) {
        overlay = getOverlay(widgetCursor);
    }

    for (uint32_t index = 0; index < widgets.count; ++index) {
        widgetCursor.widget = GET_WIDGET_LIST_ELEMENT(widgets, index);

#if OPTION_SDRAM
        int xSaved = 0;
        int ySaved = 0;
        int wSaved = 0;
        int hSaved = 0;
#endif

        if (overlay && overlay->widgetOverrides) {
            if (!overlay->widgetOverrides[index].isVisible) {
                continue;
            }

#if OPTION_SDRAM
            xSaved = widgetCursor.widget->x;
            ySaved = widgetCursor.widget->y;
            wSaved = widgetCursor.widget->w;
            hSaved = widgetCursor.widget->h;

            ((Widget*)widgetCursor.widget)->x = overlay->widgetOverrides[index].x;
            ((Widget*)widgetCursor.widget)->y = overlay->widgetOverrides[index].y;
            ((Widget*)widgetCursor.widget)->w = overlay->widgetOverrides[index].w;
            ((Widget*)widgetCursor.widget)->h = overlay->widgetOverrides[index].h;
#endif
        }

        enumWidget(widgetCursor, callback);

#if OPTION_SDRAM
        if (overlay && overlay->widgetOverrides) {
            ((Widget*)widgetCursor.widget)->x = xSaved;
            ((Widget*)widgetCursor.widget)->y = ySaved;
            ((Widget*)widgetCursor.widget)->w = wSaved;
            ((Widget*)widgetCursor.widget)->h = hSaved;
        }
#endif

        if (widgetCursor.previousState) {
			widgetCursor.previousState = nextWidgetState(widgetCursor.previousState);
            if (widgetCursor.previousState >= endOfContainerInPreviousState) {
				widgetCursor.previousState = 0;
            }
        }

        if (widgetCursor.currentState) {
			widgetCursor.currentState = nextWidgetState(widgetCursor.currentState);
        }
    }

    widgetCursor.widget = savedWidget;

    if (widgetCursor.currentState) {
        savedCurrentState->size = ((uint8_t *)widgetCursor.currentState) - ((uint8_t *)savedCurrentState);
    }

	widgetCursor.currentState = savedCurrentState;
	widgetCursor.previousState = savedPreviousState;
}

void ContainerWidget_enum(WidgetCursor &widgetCursor, EnumWidgetsCallback callback) {
    Overlay *overlay = nullptr;
    if (isOverlay(widgetCursor)) {
        overlay = getOverlay(widgetCursor);

        auto currentState = (ContainerWidgetState *)widgetCursor.currentState;
        auto previousState = (ContainerWidgetState *)widgetCursor.previousState;

        if (currentState) {
            currentState->overlayState = overlay ? overlay->state : 1;

            if (previousState && previousState->overlayState != currentState->overlayState) {
                widgetCursor.previousState = 0;
            }
        }

        if (overlay && !overlay->state) {
            if (widgetCursor.currentState) {
                widgetCursor.currentState->size = sizeof(ContainerWidgetState);
            }
            return;
        }
    }

    const ContainerWidget *containerWidget = GET_WIDGET_PROPERTY(widgetCursor.widget, specific, const ContainerWidget *);
    enumContainer(widgetCursor, callback, containerWidget->widgets);

#if OPTION_SDRAM
    if (isOverlay(widgetCursor)) {
        auto currentState = (ContainerWidgetState *)widgetCursor.currentState;
        if (currentState) {
            int xOffset = 0;
            int yOffset = 0;
            getOverlayOffset(widgetCursor, xOffset, yOffset);

            const Style *style = getStyle(widgetCursor.widget->style);

            mcu::display::setBufferBounds(currentState->displayBufferIndex, widgetCursor.x, widgetCursor.y, overlay ? overlay->width: widgetCursor.widget->w, overlay ? overlay->height : widgetCursor.widget->h, (containerWidget->flags & SHADOW_FLAG) != 0, style->opacity, xOffset, yOffset);
        }
    }
#endif
}

void ContainerWidget_draw(const WidgetCursor &widgetCursor) {
    const Widget *widget = widgetCursor.widget;

    bool refresh = 
        !widgetCursor.previousState ||
        widgetCursor.previousState->flags.active != widgetCursor.currentState->flags.active;

    int w = (int)widget->w;
    int h = (int)widget->h;

    if (isOverlay(widgetCursor)) {
        auto currentState = (ContainerWidgetState *)widgetCursor.currentState;
        auto previousState = (ContainerWidgetState *)widgetCursor.previousState;

        Overlay *overlay = getOverlay(widgetCursor);

        if (overlay && overlay->state == 0) {
            currentState->displayBufferIndex = -1;
            return;
        }

        if (previousState && (overlay ? overlay->state : 1) != previousState->overlayState) {
            refresh = true;
        }

        w = overlay ? overlay->width : widget->w;
        h = overlay ? overlay->height : widget->h;

#if OPTION_SDRAM
        if (!previousState || previousState->displayBufferIndex == -1) {
            currentState->displayBufferIndex = mcu::display::allocBuffer();
            refresh = true;
        } else {
            currentState->displayBufferIndex = previousState->displayBufferIndex;
        }
        mcu::display::selectBuffer(currentState->displayBufferIndex);
#endif
    }

    if (refresh) {
        drawRectangle(
            widgetCursor.x, widgetCursor.y, w, h, 
            getStyle(widget->style), widgetCursor.currentState->flags.active, false, true
        );
    }
}

} // namespace gui
} // namespace eez

#endif
