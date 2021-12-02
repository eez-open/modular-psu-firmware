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
#include <eez/gui/widgets/grid.h>

namespace eez {
namespace gui {

#define GRID_FLOW_ROW 1
#define GRID_FLOW_COLUMN 2

EnumFunctionType GRID_enum = [](WidgetCursor &widgetCursor) {
	auto savedCurrentState = widgetCursor.currentState;
	auto savedPreviousState = widgetCursor.previousState;

    WidgetState *endOfContainerInPreviousState = 0;
    if (widgetCursor.previousState) {
        endOfContainerInPreviousState = nextWidgetState(widgetCursor.previousState);
    }

	auto savedWidget = widgetCursor.widget;

    auto parentWidget = savedWidget;

    auto gridWidget = (const GridWidget *)widgetCursor.widget;

    int startPosition = ytDataGetPosition(widgetCursor, widgetCursor.widget->data);

    // refresh when startPosition changes
    widgetCursor.currentState->data.clear();
    widgetCursor.currentState->data = startPosition;

    if (widgetCursor.previousState && widgetCursor.previousState->data != widgetCursor.currentState->data) {
        widgetCursor.previousState = 0;
    }

	// move to the first child widget state
	if (widgetCursor.previousState) {
		++widgetCursor.previousState;
	}
    ++widgetCursor.currentState;

	const Widget *childWidget = gridWidget->itemWidget.ptr(widgetCursor.assets);
    widgetCursor.widget = childWidget;

	auto savedX = widgetCursor.x;
	auto savedY = widgetCursor.y;

    auto savedCursor = widgetCursor.cursor;

    int xOffset = 0;
    int yOffset = 0;
    int count = eez::gui::count(widgetCursor, parentWidget->data);

    Value oldValue;

    for (int index = startPosition; index < count; ++index) {
        select(widgetCursor, parentWidget->data, index, oldValue);

		widgetCursor.x = savedX + xOffset;
		widgetCursor.y = savedY + yOffset;

		widgetCursor.pushIterator(index);
		enumWidget(widgetCursor);
		widgetCursor.popIterator();

        if (widgetCursor.previousState) {
			widgetCursor.previousState = nextWidgetState(widgetCursor.previousState);
            if (widgetCursor.previousState > endOfContainerInPreviousState) {
				widgetCursor.previousState = 0;
            }
        }

        widgetCursor.currentState = nextWidgetState(widgetCursor.currentState);

        if (gridWidget->gridFlow == GRID_FLOW_ROW) {
            if (xOffset + childWidget->w < parentWidget->w) {
                xOffset += childWidget->w;
            } else {
                if (yOffset + childWidget->h < parentWidget->h) {
                    yOffset += childWidget->h;
                    xOffset = 0;
                } else {
                    break;
                }
            }
        } else {
            if (yOffset + childWidget->h < parentWidget->h) {
                yOffset += childWidget->h;
            } else {
                if (xOffset + childWidget->w < parentWidget->w) {
                    yOffset = 0;
                    xOffset += childWidget->w;
                } else {
                    break;
                }
            }
        }
    }

	widgetCursor.x = savedX;
	widgetCursor.y = savedY;

    widgetCursor.cursor = savedCursor;

    widgetCursor.widget = savedWidget;

    savedCurrentState->size = ((uint8_t *)widgetCursor.currentState) - ((uint8_t *)savedCurrentState);

    if (count > 0) {
        deselect(widgetCursor, widgetCursor.widget->data, oldValue);
    }

	widgetCursor.currentState = savedCurrentState;
	widgetCursor.previousState = savedPreviousState;

    widgetCursor.currentState->data.freeRef();
};

DrawFunctionType GRID_draw = nullptr;

OnTouchFunctionType GRID_onTouch = nullptr;

OnKeyboardFunctionType GRID_onKeyboard = nullptr;

} // namespace gui
} // namespace eez
