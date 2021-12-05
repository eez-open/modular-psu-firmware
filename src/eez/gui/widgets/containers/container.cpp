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
#include <eez/gui/widgets/containers/container.h>

namespace eez {
namespace gui {

void ContainerWidgetState::draw(WidgetState *previousStateBase) {
    Overlay *overlay = getOverlay(widgetCursor);
    if (overlay) {
        drawOverlay(previousStateBase, overlay);
        return;
    }

	auto previousState = (ContainerWidgetState *)previousStateBase;
    auto widget = (const ContainerWidget *)widgetCursor.widget;

    bool refresh = !previousState || previousState->flags.active != flags.active;

    if (refresh) {
        drawRectangle(
            widgetCursor.x, widgetCursor.y, (int)widget->w, (int)widget->h, 
            getStyle(widget->style), flags.active, false, true
        );
    }

    WidgetState *childCurrentState = this;
	WidgetState *childPreviousState = previousState;
    WidgetCursor childWidgetCursor = getFirstChildWidgetCursor(widgetCursor, childCurrentState, childPreviousState);

    auto &widgets = widget->widgets;

    WidgetState *endOfContainerInPreviousState = 0;
    if (previousState) {
        endOfContainerInPreviousState = nextWidgetState(previousState);
    }

	auto widgetPtr = widgets.itemsPtr(widgetCursor.assets);
	for (uint32_t index = 0; index < widgets.count; ++index, ++widgetPtr) {
		childWidgetCursor.widget = (const Widget *)widgetPtr->ptr(widgetCursor.assets);

        enumWidget(childWidgetCursor, childCurrentState, childPreviousState);

        if (childPreviousState) {
            childPreviousState = nextWidgetState(childPreviousState);
            if (childPreviousState > endOfContainerInPreviousState) {
                childPreviousState = 0;
            }
        }
        childCurrentState = nextWidgetState(childCurrentState);
    }

    widgetStateSize = (uint8_t *)childCurrentState - (uint8_t *)this;

}

void ContainerWidgetState::drawOverlay(WidgetState *previousStateBase, Overlay *overlay) {
	auto previousState = (ContainerWidgetState *)previousStateBase;
    auto widget = (const ContainerWidget *)widgetCursor.widget;

    bool refresh = 
        !previousState ||
        previousState->flags.active != flags.active;

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

    if (refresh || previousState->displayBufferIndex == -1) {
        displayBufferIndex = display::allocBuffer();
        refresh = true;
    } else {
        displayBufferIndex = previousState->displayBufferIndex;
    }
    
    display::selectBuffer(displayBufferIndex);

    if (refresh) {
        drawRectangle(
            widgetCursor.x, widgetCursor.y, overlay->width, overlay->height, 
            getStyle(widget->style), flags.active, false, true
        );
    }

    WidgetState *childCurrentState = this;
	WidgetState *childPreviousState = previousState;
    WidgetCursor childWidgetCursor = getFirstChildWidgetCursor(widgetCursor, childCurrentState, childPreviousState);

    if (previousState && previousState->overlayState != overlayState) {
		childPreviousState = 0;
    }

    auto &widgets = widget->widgets;

    WidgetState *endOfContainerInPreviousState = 0;
    if (previousState) {
        endOfContainerInPreviousState = nextWidgetState(previousState);
    }

    auto widgetOverrides = overlay->widgetOverrides;
    auto widgetPtr = widgets.itemsPtr(widgetCursor.assets);
    for (uint32_t index = 0; index < widgets.count; ++index, ++widgetPtr) {
        if (widgetOverrides) {
			if (!widgetOverrides->isVisible) {
				widgetOverrides++;
				continue;
			}
        }

        childWidgetCursor.widget = (const Widget *)widgetPtr->ptr(widgetCursor.assets);

        int xSaved = childWidgetCursor.widget->x;
        int ySaved = childWidgetCursor.widget->y;
        int wSaved = childWidgetCursor.widget->w;
        int hSaved = childWidgetCursor.widget->h;

		if (widgetOverrides) {
			((Widget*)childWidgetCursor.widget)->x = widgetOverrides->x;
			((Widget*)childWidgetCursor.widget)->y = widgetOverrides->y;
			((Widget*)childWidgetCursor.widget)->w = widgetOverrides->w;
			((Widget*)childWidgetCursor.widget)->h = widgetOverrides->h;
		}

        enumWidget(childWidgetCursor, childCurrentState, childPreviousState);

		if (widgetOverrides) {
			((Widget*)childWidgetCursor.widget)->x = xSaved;
			((Widget*)childWidgetCursor.widget)->y = ySaved;
			((Widget*)childWidgetCursor.widget)->w = wSaved;
			((Widget*)childWidgetCursor.widget)->h = hSaved;
			
			widgetOverrides++;
		}

        if (childPreviousState) {
            childPreviousState = nextWidgetState(childPreviousState);
            if (childPreviousState > endOfContainerInPreviousState) {
                childPreviousState = 0;
            }
        }
        childCurrentState = nextWidgetState(childCurrentState);
    }

    widgetStateSize = (uint8_t *)childCurrentState - (uint8_t *)this;

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

} // namespace gui
} // namespace eez
