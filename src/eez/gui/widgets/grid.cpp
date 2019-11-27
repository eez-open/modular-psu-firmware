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

#include <eez/gui/widgets/grid.h>

#include <eez/gui/assets.h>

namespace eez {
namespace gui {

#if OPTION_SDRAM
void GridWidget_fixPointers(Widget *widget) {
    GridWidget *gridWidget = (GridWidget *)widget->specific;
	gridWidget->itemWidget = (Widget *)((uint8_t *)g_document + (uint32_t)gridWidget->itemWidget);
    Widget_fixPointers((Widget *)gridWidget->itemWidget);
}
#endif

void GridWidget_enum(WidgetCursor &widgetCursor, EnumWidgetsCallback callback) {
	auto savedCurrentState = widgetCursor.currentState;
	auto savedPreviousState = widgetCursor.previousState;

    WidgetState *endOfContainerInPreviousState = 0;
    if (widgetCursor.previousState) {
        endOfContainerInPreviousState = nextWidgetState(widgetCursor.previousState);
    }

    // move to the first child widget state
    if (widgetCursor.previousState) {
        ++widgetCursor.previousState;
    }
    if (widgetCursor.currentState) {
        ++widgetCursor.currentState;
    }

	auto savedWidget = widgetCursor.widget;

    auto parentWidget = savedWidget;

    const GridWidget *gridWidget = GET_WIDGET_PROPERTY(widgetCursor.widget, specific, const GridWidget *);

	const Widget *childWidget = GET_WIDGET_PROPERTY(gridWidget, itemWidget, const Widget *);

    widgetCursor.widget = childWidget;

	auto savedX = widgetCursor.x;
	auto savedY = widgetCursor.y;

    int xOffset = 0;
    int yOffset = 0;
    int count = data::count(parentWidget->data);

    Value oldValue;

    for (int index = 0; index < count; ++index) {
        data::select(widgetCursor.cursor, parentWidget->data, index, oldValue);

		widgetCursor.x = savedX + xOffset;
		widgetCursor.y = savedY + yOffset;

		enumWidget(widgetCursor, callback);
		
        if (gridWidget->gridFlow == GRID_FLOW_ROW) {
            if (xOffset + childWidget->w < parentWidget->w) {
                xOffset += childWidget->w;
            } else {
                if (yOffset + childWidget->h < parentWidget->h) {
                    yOffset += childWidget->h;
                    xOffset = 0;
                } else {
                    // TODO: add vertical scroll
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
                    // TODO: add horizontal scroll
                    break;
                }
            }
        }

        widgetCursor.nextState();
        if (widgetCursor.previousState >= endOfContainerInPreviousState) {
            widgetCursor.previousState = 0;
        }
    }

	widgetCursor.x = savedX;
	widgetCursor.y = savedY;

    widgetCursor.widget = savedWidget;

    if (widgetCursor.currentState) {
        savedCurrentState->size = ((uint8_t *)widgetCursor.currentState) - ((uint8_t *)savedCurrentState);
    }

    if (count > 0) {
        data::deselect(widgetCursor.cursor, widgetCursor.widget->data, oldValue);
    }

	widgetCursor.currentState = savedCurrentState;
	widgetCursor.previousState = savedPreviousState;
}

} // namespace gui
} // namespace eez

#endif