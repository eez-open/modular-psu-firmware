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

#include <eez/debug.h>
#include <eez/os.h>

#include <eez/gui/gui.h>
#include <eez/gui/widgets/container.h>

namespace eez {
namespace gui {

void ContainerWidgetState::draw() {
    auto widget = (const ContainerWidget *)widgetCursor.widget;

	auto previousState = (ContainerWidgetState *)widgetCursor.previousState;

    bool refresh = 
        !widgetCursor.previousState ||
        widgetCursor.previousState->flags.active != flags.active;

    int w;
    int h;

    Overlay *overlay = getOverlay(widgetCursor);
    if (overlay) {
		// update overlay data
		auto containerWidget = (const ContainerWidget *)widget;
		Value widgetCursorValue((void *)&widgetCursor, VALUE_TYPE_POINTER);
		DATA_OPERATION_FUNCTION(containerWidget->overlay, DATA_OPERATION_UPDATE_OVERLAY_DATA, widgetCursor, widgetCursorValue);

		overlayState = overlay->state;
		
        if (overlayState == 0) {
            displayBufferIndex = -1;
            return;
        }

        if (previousState && overlayState != previousState->overlayState) {
            refresh = true;
        }

        w = overlay->width;
        h = overlay->height;

        if (refresh || previousState->displayBufferIndex == -1) {
            displayBufferIndex = display::allocBuffer();
            refresh = true;
        } else {
            displayBufferIndex = previousState->displayBufferIndex;
        }
        
        display::selectBuffer(displayBufferIndex);
    } else {
        w = (int)widget->w;
        h = (int)widget->h;
    }

    if (refresh) {
        drawRectangle(
            widgetCursor.x, widgetCursor.y, w, h, 
            getStyle(widget->style), flags.active, false, true
        );
    }

    WidgetCursor childWidgetCursor = getFirstChildWidgetCursor();

    if (overlay && previousState && previousState->overlayState != overlayState) {
		childWidgetCursor.previousState = 0;
    }

    auto &widgets = widget->widgets;

    WidgetState *endOfContainerInPreviousState = 0;
    if (widgetCursor.previousState) {
        endOfContainerInPreviousState = nextWidgetState(widgetCursor.previousState);
    }

    auto widgetOverrides = overlay && overlay->widgetOverrides;

    for (uint32_t index = 0; index < widgets.count; ++index) {
        childWidgetCursor.widget = widgets.item(widgetCursor.assets, index);

        int xSaved = 0;
        int ySaved = 0;
        int wSaved = 0;
        int hSaved = 0;

        if (widgetOverrides) {
            if (!overlay->widgetOverrides[index].isVisible) {
                continue;
            }

            xSaved = childWidgetCursor.widget->x;
            ySaved = childWidgetCursor.widget->y;
            wSaved = childWidgetCursor.widget->w;
            hSaved = childWidgetCursor.widget->h;

            ((Widget*)childWidgetCursor.widget)->x = overlay->widgetOverrides[index].x;
            ((Widget*)childWidgetCursor.widget)->y = overlay->widgetOverrides[index].y;
            ((Widget*)childWidgetCursor.widget)->w = overlay->widgetOverrides[index].w;
            ((Widget*)childWidgetCursor.widget)->h = overlay->widgetOverrides[index].h;
        }

        enumWidget(childWidgetCursor);

        if (widgetOverrides) {
            ((Widget*)childWidgetCursor.widget)->x = xSaved;
            ((Widget*)childWidgetCursor.widget)->y = ySaved;
            ((Widget*)childWidgetCursor.widget)->w = wSaved;
            ((Widget*)childWidgetCursor.widget)->h = hSaved;
        }

        if (childWidgetCursor.previousState) {
            childWidgetCursor.previousState = nextWidgetState(childWidgetCursor.previousState);
            if (childWidgetCursor.previousState > endOfContainerInPreviousState) {
                childWidgetCursor.previousState = 0;
            }
        }

        childWidgetCursor.currentState = nextWidgetState(childWidgetCursor.currentState);
    }

    widgetStateSize = (uint8_t *)childWidgetCursor.currentState - (uint8_t *)this;

    if (overlay) {
        int xOffset = 0;
        int yOffset = 0;
        getOverlayOffset(widgetCursor, xOffset, yOffset);

        const Style *style = getStyle(widgetCursor.widget->style);

        display::setBufferBounds(
			displayBufferIndex,
            widgetCursor.x,
            widgetCursor.y,
            overlay->width,
            overlay->height,
            (widget->flags & SHADOW_FLAG) != 0, 
            style->opacity,
            xOffset,
            yOffset,
            nullptr
        );
    }
}

} // namespace gui
} // namespace eez
