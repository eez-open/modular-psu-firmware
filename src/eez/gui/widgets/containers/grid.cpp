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

#include <eez/gui/gui.h>
#include <eez/gui/widgets/containers/grid.h>

namespace eez {
namespace gui {

#define GRID_FLOW_ROW 1
#define GRID_FLOW_COLUMN 2

void GridWidgetState::draw(WidgetState *previousState) {
    auto widget = (const GridWidget *)widgetCursor.widget;

    int startPosition = ytDataGetPosition(widgetCursor, widget->data);

    // refresh when startPosition changes
    data = startPosition;

    int count = eez::gui::count(widgetCursor, widget->data);

    if (count > 0) {
        WidgetState *childCurrentState = this;
        WidgetState *childPreviousState = previousState;
        WidgetCursor childWidgetCursor = getFirstChildWidgetCursor(widgetCursor, childCurrentState, childPreviousState);

		if (previousState && previousState->data != data) {
			childPreviousState = 0;
		}

		WidgetState *endOfContainerInPreviousState = 0;
        if (previousState) {
            endOfContainerInPreviousState = nextWidgetState(previousState);
        }

        const Widget *childWidget = widget->itemWidget.ptr(widgetCursor.assets);
        childWidgetCursor.widget = childWidget;

        auto savedX = widgetCursor.x;
        auto savedY = widgetCursor.y;

        int xOffset = 0;
        int yOffset = 0;

        Value oldValue;

        for (int index = startPosition; index < count; ++index) {
            select(childWidgetCursor, widget->data, index, oldValue);

            childWidgetCursor.x = savedX + xOffset;
            childWidgetCursor.y = savedY + yOffset;

            childWidgetCursor.pushIterator(index);
            enumWidget(childWidgetCursor, childCurrentState, childPreviousState);
            childWidgetCursor.popIterator();

            if (childPreviousState) {
                childPreviousState = nextWidgetState(childPreviousState);
                if (childPreviousState > endOfContainerInPreviousState) {
                    childPreviousState = 0;
                }
            }
            childCurrentState = nextWidgetState(childCurrentState);

            if (widget->gridFlow == GRID_FLOW_ROW) {
                if (xOffset + childWidget->w < widget->w) {
                    xOffset += childWidget->w;
                } else {
                    if (yOffset + childWidget->h < widget->h) {
                        yOffset += childWidget->h;
                        xOffset = 0;
                    } else {
                        break;
                    }
                }
            } else {
                if (yOffset + childWidget->h < widget->h) {
                    yOffset += childWidget->h;
                } else {
                    if (xOffset + childWidget->w < widget->w) {
                        yOffset = 0;
                        xOffset += childWidget->w;
                    } else {
                        break;
                    }
                }
            }
        }

        deselect(childWidgetCursor, widget->data, oldValue);
	
		widgetStateSize = (uint8_t *)childCurrentState - (uint8_t *)this;
	}
}

} // namespace gui
} // namespace eez
