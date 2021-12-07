/*
 * EEZ Modular Firmware
 * Copyright (C) 2020-present, Envox d.o.o.
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

#include <eez/util.h>

#include <eez/gui/gui.h>
#include <eez/gui/widgets/canvas.h>

namespace eez {
namespace gui {

bool CanvasWidgetState::updateState() {
    const WidgetCursor &widgetCursor = g_widgetCursor;

    bool hasPreviousState = widgetCursor.hasPreviousState;
    auto widget = (const CanvasWidget *)widgetCursor.widget;

    WIDGET_STATE(data, get(widgetCursor, widget->data));

    return !hasPreviousState;
}

void CanvasWidgetState::render() {
    const WidgetCursor &widgetCursor = g_widgetCursor;

    auto widget = (const CanvasWidget *)widgetCursor.widget;

    Value value;
    DATA_OPERATION_FUNCTION(widget->data, DATA_OPERATION_GET_CANVAS_DRAW_FUNCTION, widgetCursor, value);
    auto drawFunction = (void (*)(const WidgetCursor &widgetCursor))value.getVoidPointer();
    drawFunction(widgetCursor);
}

} // namespace gui
} // namespace eez
