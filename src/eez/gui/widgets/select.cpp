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

#include <eez/gui/widgets/select.h>

#include <eez/gui/assets.h>
#include <eez/gui/draw.h>
#include <eez/gui/gui.h>
#include <eez/gui/widgets/container.h>

namespace eez {
namespace gui {

void SelectWidget_enum(OBJ_OFFSET widgetOffset, int16_t x, int16_t y, data::Cursor &cursor,
                       WidgetState *previousState, WidgetState *currentState,
                       EnumWidgetsCallback callback) {
    DECL_WIDGET(widget, widgetOffset);

    data::Value indexValue = data::get(cursor, widget->data);
    if (indexValue.getType() == VALUE_TYPE_NONE) {
        indexValue = data::Value(0);
    }

    if (currentState) {
        currentState->data = indexValue;
    }

    if (previousState && previousState->data != currentState->data) {
        previousState = 0;
    }

    WidgetState *savedCurrentState = currentState;

    // move to the selected widget state
    if (previousState)
        ++previousState;
    if (currentState)
        ++currentState;

    int index = indexValue.getInt();
    DECL_WIDGET_SPECIFIC(ContainerWidget, containerWidget, widget);
    OBJ_OFFSET selectedWidgetOffset =
        getListItemOffset(containerWidget->widgets, index, sizeof(Widget));

    enumWidget(selectedWidgetOffset, x, y, cursor, previousState, currentState, callback);

    if (currentState) {
        savedCurrentState->size = sizeof(WidgetState) + currentState->size;
    }
}

} // namespace gui
} // namespace eez
