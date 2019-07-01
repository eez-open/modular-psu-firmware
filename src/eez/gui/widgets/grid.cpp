/*
 * EEZ Generic Firmware
 * Copyright (C) 2018-present, Envox d.o.o.
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

#include <eez/gui/widgets/grid.h>

#include <eez/gui/assets.h>

namespace eez {
namespace gui {

void GridWidget_enum(OBJ_OFFSET widgetOffset, int16_t x, int16_t y, data::Cursor &cursor,
                     WidgetState *previousState, WidgetState *currentState,
                     EnumWidgetsCallback callback) {
    DECL_WIDGET(widget, widgetOffset);

    WidgetState *savedCurrentState = currentState;

    WidgetState *endOfContainerInPreviousState = 0;
    if (previousState)
        endOfContainerInPreviousState = nextWidgetState(previousState);

    // move to the first child widget state
    if (previousState)
        ++previousState;
    if (currentState)
        ++currentState;

    int xOffset = 0;
    int yOffset = 0;
    int count = data::count(widget->data);
    for (int index = 0; index < count; ++index) {
        data::select(cursor, widget->data, index);

        DECL_WIDGET_SPECIFIC(GridWidget, gridWidget, widget);
        OBJ_OFFSET childWidgetOffset = gridWidget->item_widget;

        enumWidget(childWidgetOffset, x + xOffset, y + yOffset, cursor, previousState, currentState,
                   callback);

        DECL_WIDGET(childWidget, childWidgetOffset);

        if (xOffset + childWidget->w < widget->w) {
            xOffset += childWidget->w;
        } else {
            if (yOffset + childWidget->h < widget->h) {
                yOffset += childWidget->h;
                xOffset = 0;
            } else {
                // TODO: add horizontal scroll
                break;
            }
        }

        if (previousState) {
            previousState = nextWidgetState(previousState);
            if (previousState >= endOfContainerInPreviousState)
                previousState = 0;
        }

        if (currentState)
            currentState = nextWidgetState(currentState);
    }

    if (currentState) {
        savedCurrentState->size = ((uint8_t *)currentState) - ((uint8_t *)savedCurrentState);
    }

    data::select(cursor, widget->data, -1);
}

} // namespace gui
} // namespace eez
