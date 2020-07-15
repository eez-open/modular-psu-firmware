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

#if OPTION_DISPLAY

#include <eez/util.h>

#include <eez/gui/gui.h>

namespace eez {
namespace gui {

FixPointersFunctionType CANVAS_fixPointers = nullptr;

EnumFunctionType CANVAS_enum = nullptr;

DrawFunctionType CANVAS_draw = [](const WidgetCursor &widgetCursor) {
    const Widget *widget = widgetCursor.widget;

    widgetCursor.currentState->size = sizeof(WidgetState);

    widgetCursor.currentState->data = get(widgetCursor.cursor, widget->data);

    bool refresh = !widgetCursor.previousState || widgetCursor.previousState->data != widgetCursor.currentState->data;

    if (refresh) {
        Value value;
        DATA_OPERATION_FUNCTION(widget->data, DATA_OPERATION_GET_CANVAS_DRAW_FUNCTION, widgetCursor.cursor, value);
        auto drawFunction = (void (*)(const WidgetCursor &widgetCursor))value.getVoidPointer();
        drawFunction(widgetCursor);
    }
};

OnTouchFunctionType CANVAS_onTouch = nullptr;

OnKeyboardFunctionType CANVAS_onKeyboard = nullptr;

} // namespace gui
} // namespace eez

#endif
